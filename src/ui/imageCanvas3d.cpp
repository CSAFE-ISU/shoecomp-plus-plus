#include "ui/imageCanvas3d.h"

#include <algorithm>
#include <cmath>

#include "formats/x3pAnnotationIo.h"
#include "imgui.h"
#include "ui/calcHelpers.h"    // ImVec2 arithmetic operators
#include "ui/icons.h"          // Material icon codepoints
#include "ui/imageCanvas2d.h"  // shared AnnotationStyle (style statics)
#include "ui/uiHelpers.h"      // kDegToRad

namespace shoecomp
{
    ImageCanvas3D::Mode ImageCanvas3D::markupMode =
        ImageCanvas3D::Mode::None;

    Eigen::Affine3f Align3DState::matrix() const
    {
        const float d2r = 3.14159265358979323846f / 180.0f;
        Eigen::Affine3f m = Eigen::Affine3f::Identity();
        m.translate(translation);
        m.rotate(Eigen::AngleAxisf(rotationDeg.z() * d2r,
                                   Eigen::Vector3f::UnitZ()));
        m.rotate(Eigen::AngleAxisf(rotationDeg.y() * d2r,
                                   Eigen::Vector3f::UnitY()));
        m.rotate(Eigen::AngleAxisf(rotationDeg.x() * d2r,
                                   Eigen::Vector3f::UnitX()));
        m.scale(scale != 0.0f ? scale : 1.0f);
        return m;
    }

    void ImageCanvas3D::sync3DCameras(ImageCanvas3D& other, bool locked)
    {
        if (!locked) return;
        if (cameraDirty)
            other.camera = camera;
        else if (other.cameraDirty)
            camera = other.camera;
    }

    ImageCanvas3D::ImageCanvas3D() = default;
    ImageCanvas3D::ImageCanvas3D(std::shared_ptr<Surface3D> surf)
        : surface(std::move(surf))
    {
    }
    ImageCanvas3D::~ImageCanvas3D() { renderer.releaseGL(); }

    const std::string& ImageCanvas3D::name() const
    {
        static const std::string empty;
        return surface ? surface->name : empty;
    }
    const std::string& ImageCanvas3D::path() const
    {
        static const std::string empty;
        return surface ? surface->path : empty;
    }

    const std::vector<std::string>& ImageCanvas3D::imageExtensions()
        const
    {
        static const std::vector<std::string> none;
        return none;
    }

    void ImageCanvas3D::resetView() { camera = OrbitCamera{}; }

    void ImageCanvas3D::ensureMesh()
    {
        if (!surface) return;
        if (meshSource == surface.get()) return;
        renderer.setSurface(*surface);
        meshSource = surface.get();
    }

    Eigen::Vector3f ImageCanvas3D::modelPosOf(int col, int row) const
    {
        const SurfaceModelParams& P = renderer.params();
        float zv = surface ? surface->z(row, col) : 0.0f;
        float z = std::isfinite(zv) ? (zv - P.zc) * P.zScale : 0.0f;
        return Eigen::Vector3f((float)((col - P.cx) * P.sx),
                               (float)(-(row - P.cy) * P.sy), z);
    }

    void ImageCanvas3D::addAnnotation(int col, int row)
    {
        if (!surface) return;
        if (!surface->annotations.isObject()) return;
        const char* key =
            (markupMode == Mode::AddPoint) ? "points" : "bounds";
        float zv = surface->z(row, col);
        jt::Json v;
        v.setObject();
        v["col"] = col;
        v["row"] = row;
        v["x"] = surface->px(col);
        v["y"] = surface->py(row);
        v["z"] = surface->pz(zv);
        if (markupMode == Mode::AddPoint) v["type"] = "Peak";
        surface->annotations[key].getArray().push_back(std::move(v));
    }

    void ImageCanvas3D::removeNearestPoint(int col, int row)
    {
        if (!surface || !surface->annotations.isObject()) return;
        const char* key =
            (markupMode == Mode::AddPoint) ? "points" : "bounds";
        if (!surface->annotations[key].isArray()) return;
        auto& arr = surface->annotations[key].getArray();
        long best = -1;
        double bestD = 1e18;
        for (int i = 0; i < (int)arr.size(); ++i)
        {
            if (!arr[i].isObject()) continue;
            double dc = arr[i]["col"].getNumber() - col;
            double dr = arr[i]["row"].getNumber() - row;
            double d = dc * dc + dr * dr;
            if (d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
        if (best >= 0) arr.erase(arr.begin() + best);
    }

    // Project a model-space point through the MVP to screen pixels.
    // Returns false if behind the camera.
    static bool projectPoint(const Eigen::Matrix4f& mvp,
                             const Eigen::Vector3f& mp,
                             const ImVec2& canvasPos,
                             const ImVec2& avail, ImVec2& out)
    {
        Eigen::Vector4f clip =
            mvp * Eigen::Vector4f(mp.x(), mp.y(), mp.z(), 1.0f);
        if (clip.w() <= 1e-6f) return false;
        Eigen::Vector3f ndc = clip.head<3>() / clip.w();
        out = ImVec2(canvasPos.x + (0.5f + 0.5f * ndc.x()) * avail.x,
                     canvasPos.y + (0.5f - 0.5f * ndc.y()) * avail.y);
        return true;
    }

    void ImageCanvas3D::render3DView(const char* id,
                                     const Eigen::Affine3f& modelXform,
                                     bool allowMarkup)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 8.0f || avail.y < 8.0f || !surface) return;
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImGui::InvisibleButton(id, avail,
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        bool hovered = ImGui::IsItemHovered();
        bool activeItem = ImGui::IsItemActive();
        ImGuiIO& io = ImGui::GetIO();
        cameraDirty = false;

        if (!SurfaceRenderer::supported())
        {
            dl->AddRectFilled(canvasPos, canvasPos + avail,
                              IM_COL32(24, 27, 31, 255));
            const char* msg = "3D view not available on this build";
            ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText(ImVec2(canvasPos.x + (avail.x - ts.x) * 0.5f,
                               canvasPos.y + (avail.y - ts.y) * 0.5f),
                        IM_COL32(200, 200, 200, 255), msg);
            return;
        }

        ensureMesh();

        bool markupActive = allowMarkup && markupMode != Mode::None;

        // --- camera input ---
        if (hovered && io.MouseWheel != 0.0f)
        {
            camera.distance *= (io.MouseWheel > 0.0f) ? 0.9f : 1.111f;
            camera.distance = std::clamp(camera.distance, 0.05f, 30.0f);
            cameraDirty = true;
        }
        bool panning =
            activeItem &&
            (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
             (io.KeyCtrl &&
              ImGui::IsMouseDragging(ImGuiMouseButton_Left)) ||
             ImGui::IsMouseDragging(ImGuiMouseButton_Right));
        if (panning)
        {
            Eigen::Vector3f e = camera.eye();
            Eigen::Vector3f fwd = (camera.target - e).normalized();
            Eigen::Vector3f right =
                fwd.cross(Eigen::Vector3f(0, 1, 0)).normalized();
            Eigen::Vector3f up = right.cross(fwd);
            float k = camera.distance * 0.0016f;
            camera.target +=
                (-io.MouseDelta.x * right + io.MouseDelta.y * up) * k;
            cameraDirty = true;
        }
        else if (activeItem && !io.KeyCtrl && !markupActive &&
                 ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            camera.yaw -= io.MouseDelta.x * 0.01f;
            camera.pitch += io.MouseDelta.y * 0.01f;
            camera.pitch = std::clamp(camera.pitch, -1.45f, 1.45f);
            cameraDirty = true;
        }

        // --- render surface to FBO, blit as an image ---
        float fbs = io.DisplayFramebufferScale.x > 0.0f
                        ? io.DisplayFramebufferScale.x
                        : 1.0f;
        int fbW = std::max(1, (int)(avail.x * fbs));
        int fbH = std::max(1, (int)(avail.y * fbs));
        uintptr_t tex = renderer.render(fbW, fbH, camera, modelXform);
        if (tex)
        {
            dl->AddImage((ImTextureID)tex, canvasPos, canvasPos + avail,
                         ImVec2(0, 1), ImVec2(1, 0));
        }
        else
        {
            dl->AddRectFilled(canvasPos, canvasPos + avail,
                              IM_COL32(24, 27, 31, 255));
        }

        // --- markup: click (no drag) places / removes a point ---
        if (markupActive && hovered &&
            ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            ImVec2 dd = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            if (std::fabs(dd.x) + std::fabs(dd.y) < 4.0f)
            {
                ImVec2 rel = io.MousePos - canvasPos;
                int px = (int)(rel.x * fbs);
                int py = (int)(rel.y * fbs);
                int col = 0, row = 0;
                if (renderer.pick(px, py, camera, modelXform, col, row))
                {
                    if (io.KeyShift)
                        removeNearestPoint(col, row);
                    else
                        addAnnotation(col, row);
                }
            }
        }

        // --- overlay: project stored annotations onto the view ---
        Eigen::Matrix4f mvp = camera.proj(avail.x / avail.y) *
                              camera.view() * modelXform.matrix();
        dl->PushClipRect(canvasPos, canvasPos + avail, true);
        if (surface->annotations.isObject())
        {
            ImU32 peakCol = ImGui::ColorConvertFloat4ToU32(
                ImageCanvas2D::style.peakColor);
            ImU32 boundsCol = ImGui::ColorConvertFloat4ToU32(
                ImageCanvas2D::style.boundsColor);
            float rad = ImageCanvas2D::style.pointRadius;

            if (surface->annotations["points"].isArray())
            {
                for (auto& p :
                     surface->annotations["points"].getArray())
                {
                    if (!p.isObject()) continue;
                    Eigen::Vector3f mp =
                        modelPosOf((int)p["col"].getNumber(),
                                   (int)p["row"].getNumber());
                    ImVec2 sp;
                    if (projectPoint(mvp, mp, canvasPos, avail, sp))
                    {
                        dl->AddCircleFilled(sp, rad, peakCol);
                        dl->AddCircle(sp, rad, IM_COL32(0, 0, 0, 200));
                    }
                }
            }
            if (surface->annotations["bounds"].isArray())
            {
                auto& bnd = surface->annotations["bounds"].getArray();
                std::vector<ImVec2> pts;
                pts.reserve(bnd.size());
                for (auto& b : bnd)
                {
                    if (!b.isObject()) continue;
                    Eigen::Vector3f mp =
                        modelPosOf((int)b["col"].getNumber(),
                                   (int)b["row"].getNumber());
                    ImVec2 sp;
                    if (projectPoint(mvp, mp, canvasPos, avail, sp))
                        pts.push_back(sp);
                }
                for (size_t i = 0; i + 1 < pts.size(); ++i)
                    dl->AddLine(
                        pts[i], pts[i + 1], boundsCol,
                        ImageCanvas2D::style.boundsLineThickness);
                if (pts.size() >= 3 && markupMode != Mode::AddBounds)
                    dl->AddLine(
                        pts.back(), pts.front(), boundsCol,
                        ImageCanvas2D::style.boundsLineThickness);
                for (const ImVec2& sp : pts)
                    dl->AddCircleFilled(sp, rad - 1.0f, boundsCol);
            }
        }
        dl->PopClipRect();
    }

    void ImageCanvas3D::renderCanvas(const char* canvasId)
    {
        render3DView(canvasId, Eigen::Affine3f::Identity(), true);
    }

    void ImageCanvas3D::renderToolbar(const char* toolbarId)
    {
        ImGui::PushID(toolbarId);
        ImGui::SetWindowFontScale(0.85f);

        float frameH = ImGui::GetFrameHeight();
        ImVec2 btn(frameH, frameH);
        bool hasImg = surface != nullptr;

        auto iconBtn = [](const char* icon, const ImVec2& sz,
                          const char* tip,
                          bool allowDisabled = false) -> bool
        {
            ImFont* f = ImageCanvas2D::iconFont;
            if (f) ImGui::PushFont(f);
            bool clicked = ImGui::Button(icon, sz);
            if (f) ImGui::PopFont();
            ImGuiHoveredFlags hf =
                allowDisabled ? ImGuiHoveredFlags_AllowWhenDisabled
                              : ImGuiHoveredFlags_None;
            if (tip && ImGui::IsItemHovered(hf))
                ImGui::SetTooltip("%s", tip);
            return clicked;
        };
        auto vsep = [&]()
        {
            ImGui::SameLine();
            float sp = ImGui::GetStyle().ItemSpacing.x;
            ImVec2 p = ImGui::GetCursorScreenPos();
            float x = p.x + sp * 0.5f;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(x, p.y + 2.0f), ImVec2(x, p.y + frameH - 2.0f),
                ImGui::GetColorU32(ImGuiCol_Separator));
            ImGui::Dummy(ImVec2(sp, frameH));
            ImGui::SameLine();
        };

        // ---- Row 1: markup ----
        bool ptOn = markupMode == Mode::AddPoint;
        bool bndOn = markupMode == Mode::AddBounds;

        if (ptOn)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.8f, 0.2f, 0.2f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImageCanvas2D::style.peakColor);
        bool ptClick =
            iconBtn(ICON_MD_FIBER_MANUAL_RECORD, btn,
                    "Place Peak points: click a surface point.\n"
                    "Shift+click removes the nearest.");
        ImGui::PopStyleColor();
        if (ptOn) ImGui::PopStyleColor();
        if (ptClick) markupMode = ptOn ? Mode::None : Mode::AddPoint;

        ImGui::SameLine();
        if (bndOn)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.2f, 0.8f, 0.2f, 0.7f));
        bool bndClick =
            iconBtn(ICON_MD_CROP_SQUARE, btn,
                    "Mark a boundary: click points around a region.");
        if (bndOn) ImGui::PopStyleColor();
        if (bndClick) markupMode = bndOn ? Mode::None : Mode::AddBounds;

        vsep();

        ImGui::BeginDisabled(!hasImg);
        bool undoClick =
            iconBtn(ICON_MD_UNDO, btn,
                    "Undo the last point / bounds vertex", true);
        if (undoClick && surface && surface->annotations.isObject())
        {
            const char* key =
                (markupMode == Mode::AddBounds) ? "bounds" : "points";
            if (surface->annotations[key].isArray())
            {
                auto& arr = surface->annotations[key].getArray();
                if (!arr.empty()) arr.pop_back();
            }
        }
        ImGui::SameLine();
        bool clearClick = iconBtn(ICON_MD_DELETE, btn,
                                  "Clear all points and bounds", true);
        if (clearClick && surface && surface->annotations.isObject())
        {
            surface->annotations["points"].setArray();
            surface->annotations["bounds"].setArray();
        }
        ImGui::EndDisabled();

        // ---- Row 2: view + numeric rotation ----
        if (iconBtn(ICON_MD_HOME, btn, "Reset view (home)"))
            resetView();
        ImGui::SameLine();
        if (iconBtn(ICON_MD_ZOOM_IN, btn, "Zoom in"))
            camera.distance =
                std::clamp(camera.distance * 0.9f, 0.05f, 30.0f);
        ImGui::SameLine();
        if (iconBtn(ICON_MD_ZOOM_OUT, btn, "Zoom out"))
            camera.distance =
                std::clamp(camera.distance * 1.111f, 0.05f, 30.0f);

        vsep();

        float itemW = ImGui::CalcTextSize("-360.0__").x;
        float yawDeg = std::fmod(camera.yaw / kDegToRad, 360.0f);
        if (yawDeg > 180.0f) yawDeg -= 360.0f;
        if (yawDeg < -180.0f) yawDeg += 360.0f;
        ImGui::SetNextItemWidth(itemW);
        if (ImGui::InputFloat("##yaw", &yawDeg, 0.0f, 0.0f, "%.1f",
                              ImGuiInputTextFlags_EnterReturnsTrue))
            camera.yaw = yawDeg * kDegToRad;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Azimuth (degrees)");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(u8"\u00B0");

        ImGui::SameLine();
        float pitchDeg = camera.pitch / kDegToRad;
        ImGui::SetNextItemWidth(itemW);
        if (ImGui::InputFloat("##pitch", &pitchDeg, 0.0f, 0.0f, "%.1f",
                              ImGuiInputTextFlags_EnterReturnsTrue))
        {
            pitchDeg = std::clamp(pitchDeg, -83.0f, 83.0f);
            camera.pitch = pitchDeg * kDegToRad;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Elevation (degrees)");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(u8"\u00B0");

        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopID();
    }

    int ImageCanvas3D::saveAnnotations(const std::string& path) const
    {
        if (!surface) return -1;
        return saveX3PAnnotations(path, surface->annotations);
    }

    int ImageCanvas3D::loadAnnotations(const std::string& path,
                                       std::string& err)
    {
        if (!surface)
        {
            err = "No surface loaded.";
            return -1;
        }
        if (loadX3PAnnotations(path, surface->annotations) != 0)
        {
            err = "Failed to load annotations from:\n" + path;
            return -1;
        }
        return 0;
    }

    bool ImageCanvas3D::captureViewRGBA(std::vector<unsigned char>& out,
                                        int& w, int& h) const
    {
        return const_cast<SurfaceRenderer&>(renderer).readPixelsRGBA(
            out, w, h);
    }

    void ImageCanvas3D::renderAnnotationStyleSettings()
    {
        if (!ImGui::CollapsingHeader("Annotations")) return;
        ImGui::ColorEdit4("Peak color",
                          &ImageCanvas2D::style.peakColor.x,
                          ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Bounds color",
                          &ImageCanvas2D::style.boundsColor.x,
                          ImGuiColorEditFlags_NoInputs);
        ImGui::SliderFloat("Point radius",
                           &ImageCanvas2D::style.pointRadius, 2.0f,
                           12.0f, "%.0f");
    }

    void ImageCanvas3D::renderViewerPanel(
        std::vector<std::unique_ptr<ImageCanvas>>& images,
        int& selectedIdx, int otherIdx, bool locked, const char* label)
    {
        const char* preview =
            (selectedIdx >= 0 && selectedIdx < (int)images.size())
                ? images[selectedIdx]->name().c_str()
                : "<none>";
        ImGui::BeginDisabled(locked);
        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("<none>", selectedIdx < 0))
                selectedIdx = -1;
            for (int i = 0; i < (int)images.size(); ++i)
            {
                if (i == otherIdx) continue;
                bool sel = (i == selectedIdx);
                if (ImGui::Selectable(images[i]->name().c_str(), sel))
                    selectedIdx = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        if (selectedIdx < 0 || selectedIdx >= (int)images.size())
            return;
        ImageCanvas3D* src = asCanvas3D(*images[selectedIdx]);
        if (!src) return;
        surface = src->surface;  // share the surface data

        float toolbarH = ImGui::GetFrameHeightWithSpacing() * 2.0f;
        ImVec2 region = ImGui::GetContentRegionAvail();
        float canvasH = region.y - toolbarH;
        if (canvasH > 0.0f)
        {
            ImGui::BeginChild("##cvs3d", ImVec2(0, canvasH),
                              ImGuiChildFlags_None);
            render3DView("##canvas3d", compareXform, true);
            ImGui::EndChild();
        }
        renderToolbar(label);
    }

    // --- X3PImage ---
    const std::vector<std::string>& X3PImage::imageExtensions() const
    {
        static const std::vector<std::string> exts = {".x3p", ".X3P"};
        return exts;
    }

    int X3PImage::loadImages(
        const std::string& path,
        std::vector<std::unique_ptr<ImageCanvas>>& out,
        std::string& err) const
    {
        auto surf = std::make_shared<Surface3D>();
        if (!loadX3P(path, *surf, err)) return -1;
        surf->annotations.setObject();
        surf->annotations["points"].setArray();
        surf->annotations["bounds"].setArray();
        auto canvas = std::make_unique<X3PImage>(surf);
        canvas->ensureMesh();
        out.push_back(std::move(canvas));
        return 1;
    }
}  // namespace shoecomp
