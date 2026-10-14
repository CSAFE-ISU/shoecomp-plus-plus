#include "ui/imageCanvas3d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "formats/x3pAnnotationIo.h"
#include "imgui.h"
#include "ui/calcHelpers.h"    // ImVec2 arithmetic operators
#include "core/eulerAngles.h"  // shared pitch/yaw/roll convention
#include "ui/icons.h"          // Material icon codepoints
#include "ui/imageCanvas2d.h"  // shared AnnotationStyle (style statics)
#include "ui/uiHelpers.h"      // kDegToRad

namespace shoecomp
{
    ImageCanvas3D::Mode ImageCanvas3D::markupMode =
        ImageCanvas3D::Mode::None;

    Eigen::Affine3f Align3DState::matrix() const
    {
        Eigen::Affine3f m = Eigen::Affine3f::Identity();
        m.translate(translation);
        m.rotate(eulerToMatrix(rotationDeg.x(), rotationDeg.y(),
                               rotationDeg.z()));
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
    ImageCanvas3D::~ImageCanvas3D()
    {
        if (meshThread_.joinable()) meshThread_.join();
        renderer.releaseGL();
    }

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

    void ImageCanvas3D::resetView()
    {
        camera.pitch = 0.0f;
        camera.yaw = 0.0f;
        camera.roll = 0.0f;
    }

    void ImageCanvas3D::ensureMesh()
    {
        if (!surface || !SurfaceRenderer::supported()) return;

        if (meshBuilding_ && meshReady_)
        {
            if (meshThread_.joinable()) meshThread_.join();
            renderer.uploadMesh(pendingMesh_);
            pendingMesh_ = SurfaceMesh{};
            meshBuilding_ = false;
        }

        if (meshSource == surface.get()) return;
        if (meshBuilding_) return;

        meshSource = surface.get();
        meshBuilding_ = true;
        meshReady_ = false;
        std::shared_ptr<Surface3D> s = surface;
        meshThread_ = std::thread(
            [this, s]()
            {
                buildSurfaceMesh(*s, pendingMesh_);
                meshReady_ = true;
            });
    }

    Eigen::Vector3f ImageCanvas3D::modelPosOf(int col, int row) const
    {
        const SurfaceModelParams& P = renderer.params();
        // Only the height lookup needs a valid cell; the x/y model
        // position is linear and safe for any index (reconstructed 5-D
        // peak corners can land just outside the grid).
        float zv = 0.0f;
        if (surface && row >= 0 && row < surface->sizeY && col >= 0 &&
            col < surface->sizeX)
            zv = surface->z(row, col);
        float z = std::isfinite(zv) ? (zv - P.zc) * P.zScale : 0.0f;
        return Eigen::Vector3f((float)((col - P.cx) * P.sx),
                               (float)(-(row - P.cy) * P.sy), z);
    }

    Eigen::Vector3f ImageCanvas3D::modelPosOfPhysical(
        const Eigen::Vector3d& p) const
    {
        const SurfaceModelParams& P = renderer.params();
        // Physical analog of modelPosOf: map metres to fractional grid
        // coordinates, then apply the same model normalization. Height
        // comes from the point itself (no grid lookup), so any position
        // is safe.
        double cf =
            surface ? (p.x() - surface->offX) / surface->incX : 0.0;
        double rf =
            surface ? (p.y() - surface->offY) / surface->incY : 0.0;
        double val =
            surface ? (p.z() - surface->offZ) / surface->incZ : 0.0;
        return Eigen::Vector3f((float)((cf - P.cx) * P.sx),
                               (float)(-(rf - P.cy) * P.sy),
                               (float)((val - P.zc) * P.zScale));
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
            Eigen::Matrix3f R =
                eulerToMatrixRad(camera.pitch, camera.yaw, camera.roll);
            Eigen::Vector3f right =
                R.transpose() * Eigen::Vector3f(1, 0, 0);
            Eigen::Vector3f up =
                R.transpose() * Eigen::Vector3f(0, 1, 0);
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
            if (meshBuilding_)
            {
                const char* msg = "Building surface...";
                ImVec2 ts = ImGui::CalcTextSize(msg);
                dl->AddText(
                    ImVec2(canvasPos.x + (avail.x - ts.x) * 0.5f,
                           canvasPos.y + (avail.y - ts.y) * 0.5f),
                    IM_COL32(200, 200, 200, 255), msg);
            }
        }

        // --- live hover pick (for the status-bar location readout) ---
        hoverValid = false;
        if (hovered && tex && !panning)
        {
            ImVec2 rel = io.MousePos - canvasPos;
            int col = 0, row = 0;
            if (renderer.pick((int)(rel.x * fbs), (int)(rel.y * fbs),
                              camera, modelXform, col, row))
            {
                hoverValid = true;
                hoverCol = col;
                hoverRow = row;
            }
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
                    handleMarkupPick(col, row, io.KeyShift);
            }
        }

        overlayHovered = hovered && !panning;

        // --- overlay: project stored annotations onto the view ---
        Eigen::Matrix4f mvp = camera.proj(avail.x / avail.y) *
                              camera.view() * modelXform.matrix();
        dl->PushClipRect(canvasPos, canvasPos + avail, true);
        drawAnnotationOverlay(dl, mvp, canvasPos, avail);
        dl->PopClipRect();
    }

    void ImageCanvas3D::handleMarkupPick(int col, int row, bool remove)
    {
        if (remove)
            removeNearestPoint(col, row);
        else
            addAnnotation(col, row);
    }

    void ImageCanvas3D::drawAnnotationOverlay(
        ImDrawList* dl, const Eigen::Matrix4f& mvp,
        const ImVec2& canvasPos, const ImVec2& avail)
    {
        if (!surface || !surface->annotations.isObject()) return;
        ImU32 peakCol = ImGui::ColorConvertFloat4ToU32(
            ImageCanvas2D::style.peakColor);
        ImU32 boundsCol = ImGui::ColorConvertFloat4ToU32(
            ImageCanvas2D::style.boundsColor);
        float rad = ImageCanvas2D::style.pointRadius;

        if (surface->annotations["points"].isArray())
        {
            for (auto& p : surface->annotations["points"].getArray())
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
                dl->AddLine(pts[i], pts[i + 1], boundsCol,
                            ImageCanvas2D::style.boundsLineThickness);
            if (pts.size() >= 3 && markupMode != Mode::AddBounds)
                dl->AddLine(pts.back(), pts.front(), boundsCol,
                            ImageCanvas2D::style.boundsLineThickness);
            for (const ImVec2& sp : pts)
                dl->AddCircleFilled(sp, rad - 1.0f, boundsCol);
        }
    }

    void ImageCanvas3D::renderCanvas(const char* canvasId)
    {
        render3DView(canvasId, Eigen::Affine3f::Identity(), true);
    }

    bool ImageCanvas3D::iconBtn(const char* icon, const ImVec2& sz,
                                const char* tip, bool allowDisabled)
    {
        ImFont* f = ImageCanvas2D::iconFont;
        if (f) ImGui::PushFont(f);
        bool clicked = ImGui::Button(icon, sz);
        if (f) ImGui::PopFont();
        ImGuiHoveredFlags hf = allowDisabled
                                   ? ImGuiHoveredFlags_AllowWhenDisabled
                                   : ImGuiHoveredFlags_None;
        if (tip && ImGui::IsItemHovered(hf))
            ImGui::SetTooltip("%s", tip);
        return clicked;
    }

    void ImageCanvas3D::vsep(float frameH)
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
    }

    void ImageCanvas3D::renderMarkupToolbar(const ImVec2& btn,
                                            bool hasImg, float frameH)
    {
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

        vsep(frameH);

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
    }

    void ImageCanvas3D::renderToolbar(const char* toolbarId)
    {
        ImGui::PushID(toolbarId);
        ImGui::SetWindowFontScale(0.85f);

        float frameH = ImGui::GetFrameHeight();
        ImVec2 btn(frameH, frameH);
        bool hasImg = surface != nullptr;

        // ---- Row 1: markup (kind-specific) ----
        renderMarkupToolbar(btn, hasImg, frameH);

        // ---- Row 2: view + numeric rotation ----
        if (iconBtn(ICON_MD_HOME, btn, "Reset view (home)"))
        {
            resetView();
            cameraDirty = true;
        }
        ImGui::SameLine();
        if (iconBtn(ICON_MD_ZOOM_IN, btn, "Zoom in"))
            camera.distance =
                std::clamp(camera.distance * 0.9f, 0.05f, 30.0f);
        ImGui::SameLine();
        if (iconBtn(ICON_MD_ZOOM_OUT, btn, "Zoom out"))
            camera.distance =
                std::clamp(camera.distance * 1.111f, 0.05f, 30.0f);

        vsep(frameH);

        ImGuiIO& io = ImGui::GetIO();
        float dialR = frameH * 0.5f;
        ImDrawList* ddl = ImGui::GetWindowDrawList();
        auto rotDial = [&](const char* id, float& angleRad,
                           const char* tip, bool clampAngle, float lo,
                           float hi)
        {
            static float prevAngle = 0.0f;
            ImVec2 c = ImGui::GetCursorScreenPos();
            ImVec2 center(c.x + dialR, c.y + dialR);
            ImGui::InvisibleButton(id,
                                   ImVec2(dialR * 2.0f, dialR * 2.0f));
            bool hov = ImGui::IsItemHovered();
            bool act = ImGui::IsItemActive();
            if (hov) ImGui::SetTooltip("%s", tip);
            if (act)
            {
                float a = atan2f(io.MousePos.y - center.y,
                                 io.MousePos.x - center.x);
                if (ImGui::IsItemActivated()) { prevAngle = a; }
                else
                {
                    const float kPi = 180.0f * kDegToRad;
                    float d = a - prevAngle;
                    if (d > kPi)
                        d -= 2.0f * kPi;
                    else if (d < -kPi)
                        d += 2.0f * kPi;
                    angleRad += d;
                    if (clampAngle)
                        angleRad = std::clamp(angleRad, lo, hi);
                    cameraDirty = true;
                    prevAngle = a;
                }
            }
            ImU32 ring =
                (hov || act) ? kColorDialRingHover : kColorDialRing;
            ddl->AddCircle(center, dialR, ring, 24, 2.0f);
            ImVec2 ind = center + (direction(angleRad) * dialR);
            ddl->AddLine(center, ind, kColorDialIndicator, 2.0f);
            ddl->AddCircleFilled(ind, 3.0f, kColorDialIndicator);
        };

        rotDial("##pitchdial", camera.pitch, "Pitch: drag to tilt",
                false, 0.0f, 0.0f);
        ImGui::SameLine();
        rotDial("##yawdial", camera.yaw, "Yaw: drag to rotate", false,
                0.0f, 0.0f);
        ImGui::SameLine();
        rotDial("##rolldial", camera.roll, "Roll: drag to spin", false,
                0.0f, 0.0f);

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
        // The loaded object may omit sections; ensure the standard
        // arrays exist so overlay/markup can index them without
        // inserting stray null keys.
        if (surface->annotations.isObject())
            for (const char* k : {"points", "bounds", "peaks"})
                if (!surface->annotations.contains(k) ||
                    !surface->annotations[k].isArray())
                    surface->annotations[k].setArray();
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
        ImGui::ColorEdit4("Valley color",
                          &ImageCanvas2D::style.valleyColor.x,
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

    // --- x3p surface loading (shared by the x3p canvas kinds) ---
    static int loadX3PSurface(
        const std::string& path,
        std::vector<std::unique_ptr<ImageCanvas>>& out,
        std::string& err,
        std::unique_ptr<ImageCanvas3D> (*make)(
            std::shared_ptr<Surface3D>))
    {
        auto surf = std::make_shared<Surface3D>();
        if (!loadX3P(path, *surf, err)) return -1;
        surf->annotations.setObject();
        surf->annotations["points"].setArray();
        surf->annotations["bounds"].setArray();
        surf->annotations["peaks"].setArray();
        auto canvas = make(std::move(surf));
        canvas->ensureMesh();
        out.push_back(std::move(canvas));
        return 1;
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
        return loadX3PSurface(
            path, out, err,
            [](std::shared_ptr<Surface3D> s)
                -> std::unique_ptr<ImageCanvas3D>
            { return std::make_unique<X3PImage>(std::move(s)); });
    }

    // --- X3PBulletLand ---
    const std::vector<std::string>& X3PBulletLand::imageExtensions()
        const
    {
        return extensionsFor(ImageType::X3P);
    }

    int X3PBulletLand::loadImages(
        const std::string& path,
        std::vector<std::unique_ptr<ImageCanvas>>& out,
        std::string& err) const
    {
        size_t before = out.size();
        int rc = loadX3PSurface(
            path, out, err,
            [](std::shared_ptr<Surface3D> s)
                -> std::unique_ptr<ImageCanvas3D>
            { return std::make_unique<X3PBulletLand>(std::move(s)); });
        // Strip view is the load default (no fit yet -> grid fallback).
        for (size_t i = before; i < out.size(); ++i)
            if (out[i]->kind() == Kind::X3PBulletLand)
                static_cast<X3PBulletLand*>(out[i].get())
                    ->applyDefaultView();
        return rc;
    }

    // Top-down strip camera: look straight down (pitch=yaw=0), rolled
    // so the given model-XY axis direction stands vertical on screen,
    // zoomed so the strip's axial extent fills the viewport height.
    // Keeps |base|'s fov; recenters the target. The exact roll sign /
    // fill margin are the likely things to tune on the user's build;
    // see .claude/x3p-notes.md.
    static OrbitCamera stripCameraFromModelAxis(const Surface3D& surf,
                                                float ax, float ay,
                                                const OrbitCamera& base)
    {
        OrbitCamera cam = base;
        cam.pitch = 0.0f;
        cam.yaw = 0.0f;
        cam.target = Eigen::Vector3f::Zero();

        float len = std::hypot(ax, ay);
        if (len < 1e-9f)
        {
            ax = 0.0f;
            ay = 1.0f;
            len = 1.0f;
        }
        ax /= len;
        ay /= len;

        // Rz(roll) turns the axis to +Y (screen-up); either vertical
        // direction reads the same, so the sign is a build-tune knob.
        const float kHalfPi = 1.5707963267948966f;
        cam.roll = kHalfPi - std::atan2(ay, ax);

        // Fill the viewport height with the strip's extent along the
        // (now vertical) axis: half-extent = corner projection onto the
        // axis; distance = halfExtent / tan(fovY/2), plus a little pad.
        SurfaceModelParams P = computeModelParams(surf);
        float Wx =
            (float)((surf.sizeX > 1 ? surf.sizeX - 1 : 1) * P.sx);
        float Wy =
            (float)((surf.sizeY > 1 ? surf.sizeY - 1 : 1) * P.sy);
        float hAxis = 0.5f * (Wx * std::fabs(ax) + Wy * std::fabs(ay));
        float tanHalf = std::tan(cam.fovY * 0.5f);
        if (tanHalf > 1e-6f && hAxis > 1e-9f)
            cam.distance = std::clamp(
                hAxis / tanHalf * kStripFillMargin, 0.05f, 30.0f);
        return cam;
    }

    void X3PBulletLand::applyDefaultView()
    {
        if (!surface) return;

        float ax = 0.0f, ay = 1.0f;
        bool haveAxis = false;
        if (hasFit)
        {
            SurfaceModelParams P = computeModelParams(*surface);
            Eigen::Vector3f a = physToModelDir(P, *surface, fit.axis);
            if (std::hypot(a.x(), a.y()) > 1e-9f)
            {
                ax = a.x();
                ay = a.y();
                haveAxis = true;
            }
        }
        if (!haveAxis)
        {
            // Fallback: stand the longer grid dimension vertical.
            double physW =
                (surface->sizeX > 1 ? surface->sizeX - 1 : 1) *
                surface->incX;
            double physH =
                (surface->sizeY > 1 ? surface->sizeY - 1 : 1) *
                surface->incY;
            if (physH >= physW)
            {
                ax = 0.0f;
                ay = 1.0f;  // rows (model Y) already vertical
            }
            else
            {
                ax = 1.0f;
                ay = 0.0f;  // cols (model X) -> rotate to vertical
            }
        }

        homeCamera = stripCameraFromModelAxis(*surface, ax, ay, camera);
        camera.pitch = homeCamera.pitch;
        camera.yaw = homeCamera.yaw;
        camera.roll = homeCamera.roll;
        camera.distance = homeCamera.distance;
        camera.target = homeCamera.target;
        cameraDirty = true;
    }

    void X3PBulletLand::computeFit()
    {
        hasFit = false;
        fitError.clear();
        if (!surface)
        {
            fitError = "No surface loaded.";
            return;
        }
        CylinderFit f;
        if (!fitCylinder(*surface, f, fitError)) return;
        fit = f;
        hasFit = true;
        if (surface->annotations.isObject())
            surface->annotations["cylinder"] = cylinderToJson(fit);
        applyDefaultView();
    }

    int X3PBulletLand::loadAnnotations(const std::string& path,
                                       std::string& err)
    {
        int rc = ImageCanvas3D::loadAnnotations(path, err);
        if (rc != 0) return rc;

        hasFit = false;
        if (surface && surface->annotations.isObject() &&
            surface->annotations.contains("cylinder"))
        {
            CylinderFit f;
            if (cylinderFromJson(surface->annotations["cylinder"], f))
            {
                fit = f;
                hasFit = true;
            }
        }
        applyDefaultView();
        return 0;
    }

    void X3PBulletLand::resetView() { applyDefaultView(); }

    void X3PBulletLand::handleMarkupPick(int col, int row, bool remove)
    {
        if (remove)
        {
            if (hasPendingStart)
                hasPendingStart = false;
            else
                removeNearestPeak(col, row);
            return;
        }
        if (!surface || col < 0 || col >= surface->sizeX || row < 0 ||
            row >= surface->sizeY)
            return;
        if (!std::isfinite(surface->z(row, col))) return;  // need z

        if (!hasPendingStart)
        {
            pendingStart = {col, row};
            hasPendingStart = true;
        }
        else { commitStriationLine(col, row); }
    }

    void X3PBulletLand::commitStriationLine(int col, int row)
    {
        hasPendingStart = false;
        if (!surface) return;
        int c0 = pendingStart.first, r0 = pendingStart.second;
        float z0 = surface->z(r0, c0);
        float z1 = surface->z(row, col);
        if (!std::isfinite(z0) || !std::isfinite(z1)) return;
        Eigen::Vector3d p0(surface->px(c0), surface->py(r0),
                           surface->pz(z0));
        Eigen::Vector3d p1(surface->px(col), surface->py(row),
                           surface->pz(z1));
        Eigen::Vector3d d = p1 - p0;
        double len = d.norm();
        if (len < 1e-12) return;  // start == end

        StriationLine ln;
        ln.start = p0;
        ln.dir = d / len;
        ln.length = len;

        if (!surface->annotations.isObject())
            surface->annotations.setObject();
        if (!surface->annotations["peaks"].isArray())
            surface->annotations["peaks"].setArray();
        surface->annotations["peaks"].getArray().push_back(
            striationLineToJson(ln));
    }

    void X3PBulletLand::removeNearestPeak(int col, int row)
    {
        if (!surface || !surface->annotations.isObject()) return;
        if (!surface->annotations["peaks"].isArray()) return;
        auto& arr = surface->annotations["peaks"].getArray();
        long best = -1;
        double bestD = 1e300;
        for (int i = 0; i < (int)arr.size(); ++i)
        {
            StriationLine ln;
            if (!striationLineFromJson(arr[i], ln)) continue;
            Eigen::Vector3d mid = ln.start + 0.5 * ln.length * ln.dir;
            double cc = (mid.x() - surface->offX) / surface->incX;
            double rr = (mid.y() - surface->offY) / surface->incY;
            double dc = cc - col, dr = rr - row;
            double d = dc * dc + dr * dr;
            if (d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
        if (best >= 0) arr.erase(arr.begin() + best);
    }

    // Screen-space distance from |p| to the segment |a|-|b|.
    static float segmentDistance(const ImVec2& p, const ImVec2& a,
                                 const ImVec2& b)
    {
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float len2 = dx * dx + dy * dy;
        float t = len2 > 0.0f
                      ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2
                      : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
    }

    void X3PBulletLand::drawAnnotationOverlay(
        ImDrawList* dl, const Eigen::Matrix4f& mvp,
        const ImVec2& canvasPos, const ImVec2& avail)
    {
        // Legacy points/bounds still drawn read-only.
        ImageCanvas3D::drawAnnotationOverlay(dl, mvp, canvasPos, avail);
        if (!surface || !surface->annotations.isObject()) return;

        ImU32 peakCol = ImGui::ColorConvertFloat4ToU32(
            ImageCanvas2D::style.peakColor);
        ImU32 valleyCol = ImGui::ColorConvertFloat4ToU32(
            ImageCanvas2D::style.valleyColor);
        auto dimmed = [](ImVec4 c)
        {
            c.w *= kUnmatchedStriationAlpha;
            return ImGui::ColorConvertFloat4ToU32(c);
        };
        ImU32 peakDim = dimmed(ImageCanvas2D::style.peakColor);
        ImU32 valleyDim = dimmed(ImageCanvas2D::style.valleyColor);
        ImU32 activeCol = IM_COL32(255, 210, 60, 255);
        float thick = ImageCanvas2D::style.boundsLineThickness;
        float rad = ImageCanvas2D::style.pointRadius;

        auto projectCell = [&](int c, int r, ImVec2& out) -> bool {
            return projectPoint(mvp, modelPosOf(c, r), canvasPos, avail,
                                out);
        };

        hoveredStriation = -1;
        if (surface->annotations["peaks"].isArray())
        {
            const auto& peaks =
                surface->annotations["peaks"].getArray();
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool linking =
                overlayHovered && !striationLinks.empty();
            float bestDist = kStriationHoverPx;
            ImVec2 hl0, hl1, ln0, ln1;
            bool haveLinked = false;
            for (size_t k = 0; k < peaks.size(); ++k)
            {
                StriationLine ln;
                if (!striationLineFromJson(peaks[k], ln)) continue;
                Eigen::Vector3d p0 = ln.start;
                Eigen::Vector3d p1 = ln.start + ln.length * ln.dir;
                ImVec2 sp0, sp1;
                if (!projectPoint(mvp, modelPosOfPhysical(p0),
                                  canvasPos, avail, sp0) ||
                    !projectPoint(mvp, modelPosOfPhysical(p1),
                                  canvasPos, avail, sp1))
                    continue;
                const bool matched =
                    k < striationLinks.size() && striationLinks[k] >= 0;
                ImU32 col = ln.isPeak ? peakCol : valleyCol;
                if (!striationLinks.empty() && !matched)
                    col = ln.isPeak ? peakDim : valleyDim;
                dl->AddLine(sp0, sp1, col, thick);
                dl->AddCircleFilled(sp0, rad - 1.0f, col);
                dl->AddCircleFilled(sp1, rad - 1.0f, col);

                if ((int)k == linkedStriation)
                {
                    ln0 = sp0;
                    ln1 = sp1;
                    haveLinked = true;
                }
                if (linking && matched)
                {
                    float d = segmentDistance(mouse, sp0, sp1);
                    if (d < bestDist)
                    {
                        bestDist = d;
                        hoveredStriation = (int)k;
                        hl0 = sp0;
                        hl1 = sp1;
                    }
                }
            }
            // drawn last so they sit on top of the other lines
            ImU32 link = ImGui::ColorConvertFloat4ToU32(linkColor);
            auto drawLink = [&](const ImVec2& a, const ImVec2& b)
            {
                dl->AddLine(a, b, link, thick + 2.0f);
                dl->AddCircleFilled(a, rad, link);
                dl->AddCircleFilled(b, rad, link);
            };
            if (hoveredStriation >= 0) drawLink(hl0, hl1);
            if (haveLinked && hoveredStriation < 0) drawLink(ln0, ln1);
        }

        // In-progress striation: the pending start cell.
        if (hasPendingStart)
        {
            ImVec2 sp;
            if (projectCell(pendingStart.first, pendingStart.second,
                            sp))
                dl->AddCircleFilled(sp, rad, activeCol);
        }
    }

    void X3PBulletLand::renderMarkupToolbar(const ImVec2& btn,
                                            bool hasImg, float frameH)
    {
        bool lineOn = markupMode == Mode::AddStriationLine;

        if (lineOn)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.2f, 0.8f, 0.2f, 0.7f));
        bool lineClick =
            iconBtn(ICON_MD_EDIT, btn,
                    "Mark a striation: click the start, then the end.");
        if (lineOn) ImGui::PopStyleColor();
        if (lineClick)
        {
            hasPendingStart = false;
            markupMode = lineOn ? Mode::None : Mode::AddStriationLine;
        }

        vsep(frameH);

        ImGui::BeginDisabled(!hasImg);
        if (iconBtn(ICON_MD_UNDO, btn,
                    "Undo the pending start / last striation", true))
        {
            if (hasPendingStart)
                hasPendingStart = false;
            else if (surface && surface->annotations.isObject() &&
                     surface->annotations["peaks"].isArray())
            {
                auto& arr = surface->annotations["peaks"].getArray();
                if (!arr.empty()) arr.pop_back();
            }
        }
        ImGui::SameLine();
        if (iconBtn(ICON_MD_DELETE, btn, "Clear all striations", true))
        {
            hasPendingStart = false;
            if (surface && surface->annotations.isObject())
                surface->annotations["peaks"].setArray();
        }
        ImGui::EndDisabled();

        vsep(frameH);

        ImGui::BeginDisabled(!hasImg);
        if (iconBtn(ICON_MD_STRAIGHTEN, btn,
                    "Fit cylinder (aligns the vertical strip view)"))
            computeFit();
        ImGui::EndDisabled();

        vsep(frameH);

        ImGui::BeginDisabled(!detectAvailable);
        if (iconBtn(ICON_MD_SEARCH, btn,
                    "Detect striations (auto-detect)", true))
            detectRequested = true;
        ImGui::EndDisabled();

        if (hasFit && fit.radius > 0.0)
        {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("R %.2f mm", fit.radius * 1000.0);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "Fitted radius %.4f mm (curvature %.3f /mm)",
                    fit.radius * 1000.0, 1.0 / (fit.radius * 1000.0));
        }
        else if (!fitError.empty())
        {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f),
                               "no fit");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", fitError.c_str());
        }
    }
}  // namespace shoecomp
