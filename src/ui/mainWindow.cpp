#include "ui/mainWindow.h"
#include "formats.h"
#include "json.h"
#include "hello_imgui/hello_imgui_include_opengl.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace shoecomp
{
    namespace fs = std::filesystem;

    static void runSplash(double duration)
    {
        double startTime = 0.0;

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle = "ShoeComp";
        params.appWindowParams.windowGeometry.size = {640, 360};
        params.appWindowParams.borderless = true;
        params.appWindowParams.borderlessMovable = false;
        params.appWindowParams.borderlessResizable = false;
        params.appWindowParams.borderlessClosable = false;
        params.appWindowParams.resizable = false;
        params.appWindowParams.windowGeometry.positionMode =
            HelloImGui::WindowPositionMode::MonitorCenter;
        params.imGuiWindowParams.defaultImGuiWindowType =
            HelloImGui::DefaultImGuiWindowType::ProvideFullScreenWindow;
        params.callbacks.PostInit = []()
        { ImGui::GetIO().FontGlobalScale = 2.5f; };
        params.callbacks.ShowGui = [&startTime, duration]()
        {
            if (startTime == 0.0) startTime = ImGui::GetTime();

            double elapsed = ImGui::GetTime() - startTime;

            ImVec2 winSize = ImGui::GetWindowSize();

            const char* title = "ShoeComp";
            ImVec2 titleSize = ImGui::CalcTextSize(title);
            ImGui::SetCursorPos(
                ImVec2((winSize.x - titleSize.x) * 0.5f,
                       (winSize.y - titleSize.y) * 0.5f - 30.0f));
            ImGui::Text("%s", title);

            // Animated dots: cycle 1-3
            int dots = (int)(elapsed / 0.4) % 3 + 1;
            char subtitle[16];
            snprintf(subtitle, sizeof(subtitle), "Loading%.*s", dots,
                     "...");
            // Use fixed width so text doesn't shift
            const char* widest = "Loading...";
            ImVec2 wSize = ImGui::CalcTextSize(widest);
            ImGui::SetCursorPos(
                ImVec2((winSize.x - wSize.x) * 0.5f,
                       (winSize.y - wSize.y) * 0.5f + 30.0f));
            ImGui::Text("%s", subtitle);

            if (elapsed >= duration)
                HelloImGui::GetRunnerParams()->appShallExit = true;
        };

        HelloImGui::Run(params);
    }

    static void renderSettings(AppState& state)
    {
        ImGui::Text("Loaded images:");
        int removeIdx = -1;
        for (int i = 0; i < (int)state.images.size(); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Button("X")) removeIdx = i;
            ImGui::SameLine();
            ImGui::Text("%s", state.images[i].name.c_str());
            ImGui::PopID();
        }
        if (removeIdx >= 0)
        {
            freeTexture(state.images[removeIdx].textureId);
            state.images.erase(state.images.begin() + removeIdx);
            if (state.viewerLeftIdx >= (int)state.images.size())
                state.viewerLeftIdx = (int)state.images.size() - 1;
            if (state.viewerRightIdx >= (int)state.images.size())
                state.viewerRightIdx = (int)state.images.size() - 1;
        }
    }

    static void renderFilesAndSettings(AppState& state)
    {
        ImGui::BeginChild("SettingsPane", ImVec2(0, 0),
                          ImGuiChildFlags_Borders);
        renderSettings(state);
        ImGui::EndChild();
    }

    static ImVec2 screenToImageCoord(ImVec2 sp, ImVec2 canvasPos,
                                     ImVec2 avail, ImVec2 pan,
                                     float zoom, float baseScale,
                                     float rotation, int imgW, int imgH)
    {
        float cx = canvasPos.x + avail.x * 0.5f + pan.x;
        float cy = canvasPos.y + avail.y * 0.5f + pan.y;
        float dx = sp.x - cx;
        float dy = sp.y - cy;
        float cosR = cosf(-rotation);
        float sinR = sinf(-rotation);
        float lx = dx * cosR - dy * sinR;
        float ly = dx * sinR + dy * cosR;
        float scale = baseScale * zoom;
        return ImVec2(lx / scale + imgW * 0.5f,
                      ly / scale + imgH * 0.5f);
    }

    static ImVec2 imageToScreenCoord(float ix, float iy, float cx,
                                     float cy, float scale, float cosR,
                                     float sinR, int imgW, int imgH)
    {
        float lx = (ix - imgW * 0.5f) * scale;
        float ly = (iy - imgH * 0.5f) * scale;
        return ImVec2(cx + lx * cosR - ly * sinR,
                      cy + lx * sinR + ly * cosR);
    }

    static bool pointInPolygon(float px, float py,
                               std::vector<jt::Json>& poly)
    {
        bool inside = false;
        int n = (int)poly.size();
        for (int i = 0, j = n - 1; i < n; j = i++)
        {
            float xi = poly[i]["x"].getFloat();
            float yi = poly[i]["y"].getFloat();
            float xj = poly[j]["x"].getFloat();
            float yj = poly[j]["y"].getFloat();
            if (((yi > py) != (yj > py)) &&
                (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
                inside = !inside;
        }
        return inside;
    }

    static void removePointsOutsideBounds(LoadedImage& img)
    {
        if (!img.annotations.isObject() ||
            !img.annotations.contains("bounds") ||
            !img.annotations["bounds"].isArray())
            return;
        auto& bnd = img.annotations["bounds"].getArray();
        if (bnd.size() < 3) return;
        if (!img.annotations.contains("points") ||
            !img.annotations["points"].isArray())
            return;
        auto& pts = img.annotations["points"].getArray();
        pts.erase(std::remove_if(pts.begin(), pts.end(),
                                 [&bnd](jt::Json& p)
                                 {
                                     return !pointInPolygon(
                                         p["x"].getFloat(),
                                         p["y"].getFloat(), bnd);
                                 }),
                  pts.end());
    }

    static void renderImageCanvas(LoadedImage& img, ImageViewState& vs,
                                  const char* canvasId,
                                  ImageViewState* linked = nullptr,
                                  AnnotationMode* pMode = nullptr)
    {
        AnnotationMode modeVal = pMode ? *pMode : AnnotationMode::None;
        AnnotationMode& mode = pMode ? *pMode : modeVal;
        ImVec2 avail = ImGui::GetContentRegionAvail();

        float scaleX = avail.x / (float)img.width;
        float scaleY = avail.y / (float)img.height;
        float baseScale = std::min(scaleX, scaleY);
        if (vs.zoomTarget <= 0.0f) vs.zoomTarget = scaleX / baseScale;
        if (vs.zoom <= 0.0f) vs.zoom = vs.zoomTarget;

        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(canvasId, avail,
                               ImGuiButtonFlags_MouseButtonLeft);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        ImGuiIO& io = ImGui::GetIO();

        if (hovered && io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            float oldTarget = vs.zoomTarget;
            vs.zoomTarget *= (io.MouseWheel > 0) ? 1.15f : 0.87f;
            vs.zoomTarget = std::clamp(vs.zoomTarget, 0.1f, 50.0f);
            ImVec2 mouse = ImVec2(io.MousePos.x - canvasPos.x,
                                  io.MousePos.y - canvasPos.y);
            float ratio = vs.zoomTarget / oldTarget;
            vs.panTarget.x =
                (1.0f - ratio) * (mouse.x - avail.x * 0.5f) +
                ratio * vs.panTarget.x;
            vs.panTarget.y =
                (1.0f - ratio) * (mouse.y - avail.y * 0.5f) +
                ratio * vs.panTarget.y;
            if (linked)
            {
                linked->zoomTarget = vs.zoomTarget;
                linked->panTarget = vs.panTarget;
            }
        }

        if (hovered && !io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            vs.panTarget.y += io.MouseWheel * 30.0f;
            if (linked) linked->panTarget.y = vs.panTarget.y;
        }

        if (active && io.KeyCtrl &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            vs.panTarget.x += io.MouseDelta.x;
            vs.panTarget.y += io.MouseDelta.y;
            vs.pan.x += io.MouseDelta.x;
            vs.pan.y += io.MouseDelta.y;
            if (linked)
            {
                linked->panTarget.x += io.MouseDelta.x;
                linked->panTarget.y += io.MouseDelta.y;
                linked->pan.x += io.MouseDelta.x;
                linked->pan.y += io.MouseDelta.y;
            }
        }

        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !io.KeyCtrl && mode != AnnotationMode::None)
        {
            ImVec2 ic = screenToImageCoord(
                io.MousePos, canvasPos, avail, vs.pan, vs.zoom,
                baseScale, vs.rotation, img.width, img.height);
            const char* key = (mode == AnnotationMode::AddPoint)
                                  ? "points"
                                  : "bounds";

            // Screen-pixel threshold converted to
            // image-space
            float screenPx = 10.0f;
            float imgPx = screenPx / (baseScale * vs.zoom);
            float thresh2 = imgPx * imgPx;

            // Close bounds polygon by clicking
            // near the first vertex
            if (mode == AnnotationMode::AddBounds && !io.KeyShift &&
                img.annotations.contains("bounds") &&
                img.annotations["bounds"].isArray() &&
                img.annotations["bounds"].getArray().size() >= 3)
            {
                auto& bnd = img.annotations["bounds"].getArray();
                float dx = bnd[0]["x"].getFloat() - ic.x;
                float dy = bnd[0]["y"].getFloat() - ic.y;
                if (dx * dx + dy * dy < thresh2)
                {
                    mode = AnnotationMode::None;
                    removePointsOutsideBounds(img);
                }
            }

            if (mode == AnnotationMode::None)
            {
                // Already closed bounds above
            }
            else if (io.KeyShift)
            {
                // Remove nearest annotation within
                // 10 image-pixels
                if (img.annotations.contains(key) &&
                    img.annotations[key].isArray())
                {
                    auto& arr = img.annotations[key].getArray();
                    float bestDist = thresh2;
                    int bestIdx = -1;
                    for (int ai = 0; ai < (int)arr.size(); ++ai)
                    {
                        float dx = arr[ai]["x"].getFloat() - ic.x;
                        float dy = arr[ai]["y"].getFloat() - ic.y;
                        float d2 = dx * dx + dy * dy;
                        if (d2 < bestDist)
                        {
                            bestDist = d2;
                            bestIdx = ai;
                        }
                    }
                    if (bestIdx >= 0) arr.erase(arr.begin() + bestIdx);
                }
            }
            else
            {
                bool allow = true;
                if (mode == AnnotationMode::AddPoint &&
                    img.annotations.contains("bounds") &&
                    img.annotations["bounds"].isArray() &&
                    img.annotations["bounds"].getArray().size() >= 3)
                {
                    allow = pointInPolygon(
                        ic.x, ic.y,
                        img.annotations["bounds"].getArray());
                }
                if (allow)
                {
                    jt::Json pt;
                    pt.setObject();
                    pt["x"] = ic.x;
                    pt["y"] = ic.y;
                    img.annotations[key].getArray().push_back(
                        std::move(pt));
                }
            }
        }

        float speed = 12.0f * io.DeltaTime;
        speed = std::clamp(speed, 0.0f, 1.0f);
        vs.zoom += (vs.zoomTarget - vs.zoom) * speed;
        vs.pan.x += (vs.panTarget.x - vs.pan.x) * speed;
        vs.pan.y += (vs.panTarget.y - vs.pan.y) * speed;
        vs.rotation += (vs.rotationTarget - vs.rotation) * speed;

        float dispW = img.width * baseScale * vs.zoom;
        float dispH = img.height * baseScale * vs.zoom;

        float limX = std::max(avail.x, dispW) * 0.5f;
        float limY = std::max(avail.y, dispH) * 0.5f;
        vs.panTarget.x = std::clamp(vs.panTarget.x, -limX, limX);
        vs.panTarget.y = std::clamp(vs.panTarget.y, -limY, limY);
        vs.pan.x = std::clamp(vs.pan.x, -limX, limX);
        vs.pan.y = std::clamp(vs.pan.y, -limY, limY);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(
            canvasPos,
            ImVec2(canvasPos.x + avail.x, canvasPos.y + avail.y), true);
        float cx = canvasPos.x + avail.x * 0.5f + vs.pan.x;
        float cy = canvasPos.y + avail.y * 0.5f + vs.pan.y;
        float hw = dispW * 0.5f;
        float hh = dispH * 0.5f;
        float cosR = cosf(vs.rotation);
        float sinR = sinf(vs.rotation);
        auto rot = [&](float lx, float ly) -> ImVec2
        {
            return ImVec2(cx + lx * cosR - ly * sinR,
                          cy + lx * sinR + ly * cosR);
        };
        ImVec2 tl = rot(-hw, -hh);
        ImVec2 tr = rot(hw, -hh);
        ImVec2 br = rot(hw, hh);
        ImVec2 bl = rot(-hw, hh);
        dl->AddImageQuad(img.textureId, tl, tr, br, bl, ImVec2(0, 0),
                         ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1));
        dl->AddQuad(tl, tr, br, bl, IM_COL32(180, 180, 180, 200));

        // Dim area outside bounds when not editing
        float annScale = baseScale * vs.zoom;
        if (mode != AnnotationMode::AddBounds &&
            img.annotations.isObject() &&
            img.annotations.contains("bounds") &&
            img.annotations["bounds"].isArray() &&
            img.annotations["bounds"].getArray().size() >= 3)
        {
            auto& bnd = img.annotations["bounds"].getArray();
            // Build screen-space bounds polygon
            std::vector<ImVec2> screenBnd;
            screenBnd.reserve(bnd.size());
            for (auto& v : bnd)
            {
                screenBnd.push_back(imageToScreenCoord(
                    v["x"].getFloat(), v["y"].getFloat(), cx, cy,
                    annScale, cosR, sinR, img.width, img.height));
            }
            // Outer rect large enough to cover all
            // bounds vertices and the canvas
            float oMinX = canvasPos.x;
            float oMinY = canvasPos.y;
            float oMaxX = canvasPos.x + avail.x;
            float oMaxY = canvasPos.y + avail.y;
            for (auto& sp : screenBnd)
            {
                oMinX = std::min(oMinX, sp.x);
                oMinY = std::min(oMinY, sp.y);
                oMaxX = std::max(oMaxX, sp.x);
                oMaxY = std::max(oMaxY, sp.y);
            }
            float pad = 10.0f;
            oMinX -= pad;
            oMinY -= pad;
            oMaxX += pad;
            oMaxY += pad;
            ImVec2 cTL(oMinX, oMinY);
            ImVec2 cTR(oMaxX, oMinY);
            ImVec2 cBR(oMaxX, oMaxY);
            ImVec2 cBL(oMinX, oMaxY);
            std::vector<ImVec2> frame;
            frame.reserve(screenBnd.size() + 6);
            frame.push_back(cTL);
            frame.push_back(cTR);
            frame.push_back(cBR);
            frame.push_back(cBL);
            frame.push_back(screenBnd[0]);
            for (int si = (int)screenBnd.size() - 1; si >= 0; --si)
                frame.push_back(screenBnd[si]);
            frame.push_back(cBL);
            dl->AddConcavePolyFilled(frame.data(), (int)frame.size(),
                                     IM_COL32(0, 0, 0, 120));
        }

        // Annotation overlays
        if (img.annotations.isObject())
        {
            if (img.annotations.contains("points") &&
                img.annotations["points"].isArray())
            {
                auto& pts = img.annotations["points"].getArray();
                for (auto& p : pts)
                {
                    ImVec2 sp = imageToScreenCoord(
                        p["x"].getFloat(), p["y"].getFloat(), cx, cy,
                        annScale, cosR, sinR, img.width, img.height);
                    dl->AddCircleFilled(sp, 5.0f,
                                        IM_COL32(255, 50, 50, 220));
                    dl->AddCircle(sp, 5.0f,
                                  IM_COL32(255, 255, 255, 200), 12,
                                  1.5f);
                }
            }
            if (img.annotations.contains("bounds") &&
                img.annotations["bounds"].isArray())
            {
                auto& bnd = img.annotations["bounds"].getArray();
                bool editing = (mode == AnnotationMode::AddBounds);
                // Solid edges between consecutive
                // vertices
                if (bnd.size() >= 2)
                {
                    for (size_t i = 0; i + 1 < bnd.size(); ++i)
                    {
                        ImVec2 a = imageToScreenCoord(
                            bnd[i]["x"].getFloat(),
                            bnd[i]["y"].getFloat(), cx, cy, annScale,
                            cosR, sinR, img.width, img.height);
                        ImVec2 b = imageToScreenCoord(
                            bnd[i + 1]["x"].getFloat(),
                            bnd[i + 1]["y"].getFloat(), cx, cy,
                            annScale, cosR, sinR, img.width,
                            img.height);
                        dl->AddLine(a, b, IM_COL32(50, 255, 50, 220),
                                    2.0f);
                    }
                }
                // Closing segment: solid when not
                // editing, dashed when editing
                if (bnd.size() >= 2)
                {
                    ImVec2 last = imageToScreenCoord(
                        bnd.back()["x"].getFloat(),
                        bnd.back()["y"].getFloat(), cx, cy, annScale,
                        cosR, sinR, img.width, img.height);
                    ImVec2 first = imageToScreenCoord(
                        bnd[0]["x"].getFloat(), bnd[0]["y"].getFloat(),
                        cx, cy, annScale, cosR, sinR, img.width,
                        img.height);
                    if (!editing)
                    {
                        dl->AddLine(last, first,
                                    IM_COL32(50, 255, 50, 220), 2.0f);
                    }
                    else
                    {
                        // Dashed closing segment
                        float dashLen = 8.0f;
                        float gapLen = 6.0f;
                        auto drawDashed = [&](ImVec2 p0, ImVec2 p1)
                        {
                            float dx = p1.x - p0.x;
                            float dy = p1.y - p0.y;
                            float len = sqrtf(dx * dx + dy * dy);
                            if (len < 1.0f) return;
                            float nx = dx / len;
                            float ny = dy / len;
                            float t = 0.0f;
                            while (t < len)
                            {
                                float t1 = std::min(t + dashLen, len);
                                dl->AddLine(ImVec2(p0.x + nx * t,
                                                   p0.y + ny * t),
                                            ImVec2(p0.x + nx * t1,
                                                   p0.y + ny * t1),
                                            IM_COL32(50, 255, 50, 120),
                                            1.5f);
                                t = t1 + gapLen;
                            }
                        };
                        if (hovered)
                        {
                            drawDashed(last, io.MousePos);
                            drawDashed(io.MousePos, first);
                        }
                        else { drawDashed(last, first); }
                    }
                }
                for (auto& v : bnd)
                {
                    ImVec2 sp = imageToScreenCoord(
                        v["x"].getFloat(), v["y"].getFloat(), cx, cy,
                        annScale, cosR, sinR, img.width, img.height);
                    dl->AddCircleFilled(sp, 4.0f,
                                        IM_COL32(50, 255, 50, 220));
                }
            }
        }

        float barThick = 8.0f;
        float barPad = 2.0f;
        ImU32 barCol = IM_COL32(200, 200, 200, 100);
        ImU32 barColHov = IM_COL32(200, 200, 200, 180);
        ImVec2 mpos = io.MousePos;

        if (dispW > avail.x)
        {
            float viewRatio = avail.x / dispW;
            float barW = avail.x * viewRatio;
            float t = (limX - vs.pan.x) / (2.0f * limX);
            float barX = canvasPos.x + t * (avail.x - barW);
            float barY = canvasPos.y + avail.y - barThick - barPad;
            ImVec2 bMin(barX, barY);
            ImVec2 bMax(barX + barW, barY + barThick);

            bool barHov = mpos.x >= bMin.x && mpos.x <= bMax.x &&
                          mpos.y >= bMin.y && mpos.y <= bMax.y;
            bool barDrag =
                barHov && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
            if (barDrag)
            {
                float dx =
                    -io.MouseDelta.x / (avail.x - barW) * (2.0f * limX);
                vs.pan.x += dx;
                vs.panTarget.x += dx;
                if (linked)
                {
                    linked->pan.x += dx;
                    linked->panTarget.x += dx;
                }
            }
            dl->AddRectFilled(bMin, bMax,
                              barDrag || barHov ? barColHov : barCol,
                              barThick * 0.5f);
        }

        if (dispH > avail.y)
        {
            float viewRatio = avail.y / dispH;
            float barH = avail.y * viewRatio;
            float t = (limY - vs.pan.y) / (2.0f * limY);
            float barX = canvasPos.x + avail.x - barThick - barPad;
            float barY = canvasPos.y + t * (avail.y - barH);
            ImVec2 bMin(barX, barY);
            ImVec2 bMax(barX + barThick, barY + barH);

            bool barHov = mpos.x >= bMin.x && mpos.x <= bMax.x &&
                          mpos.y >= bMin.y && mpos.y <= bMax.y;
            bool barDrag =
                barHov && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
            if (barDrag)
            {
                float dy =
                    -io.MouseDelta.y / (avail.y - barH) * (2.0f * limY);
                vs.pan.y += dy;
                vs.panTarget.y += dy;
                if (linked)
                {
                    linked->pan.y += dy;
                    linked->panTarget.y += dy;
                }
            }
            dl->AddRectFilled(bMin, bMax,
                              barDrag || barHov ? barColHov : barCol,
                              barThick * 0.5f);
        }

        vs.panTarget.x = std::clamp(vs.panTarget.x, -limX, limX);
        vs.panTarget.y = std::clamp(vs.panTarget.y, -limY, limY);
        vs.pan.x = std::clamp(vs.pan.x, -limX, limX);
        vs.pan.y = std::clamp(vs.pan.y, -limY, limY);

        dl->PopClipRect();
    }

    static void renderLockToggle(bool& locked)
    {
        if (ImGui::Button(locked ? "Unlock" : "Lock")) locked = !locked;
    }

    static void renderImageToolbar(LoadedImage& img, ImageViewState& vs,
                                   const char* toolbarId,
                                   AnnotationMode* mode,
                                   ImageViewState* linked = nullptr)
    {
        ImGui::PushID(toolbarId);

        float frameH = ImGui::GetFrameHeight();

        // Home: fit image vertically, reset pan
        if (ImGui::Button("Home"))
        {
            vs.zoomTarget = 1.0f;
            vs.panTarget = ImVec2(0, 0);
            vs.rotationTarget = 0.0f;
            if (linked)
            {
                linked->zoomTarget = 1.0f;
                linked->panTarget = ImVec2(0, 0);
                linked->rotationTarget = 0.0f;
            }
        }
        ImGui::SameLine();

        // Zoom in
        if (ImGui::Button("+"))
        {
            vs.zoomTarget =
                std::clamp(vs.zoomTarget * 1.25f, 0.1f, 50.0f);
            if (linked) linked->zoomTarget = vs.zoomTarget;
        }
        ImGui::SameLine();

        // Zoom out
        if (ImGui::Button("-"))
        {
            vs.zoomTarget =
                std::clamp(vs.zoomTarget * 0.8f, 0.1f, 50.0f);
            if (linked) linked->zoomTarget = vs.zoomTarget;
        }
        ImGui::SameLine();

        // Rotation dial
        float dialR = frameH * 0.5f;
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 center(cursor.x + dialR, cursor.y + dialR);
        ImGui::InvisibleButton("##dial",
                               ImVec2(dialR * 2.0f, dialR * 2.0f));
        bool dialHov = ImGui::IsItemHovered();
        bool dialAct = ImGui::IsItemActive();

        if (dialAct)
        {
            ImGuiIO& io = ImGui::GetIO();
            float angle = atan2f(io.MousePos.y - center.y,
                                 io.MousePos.x - center.x);
            vs.rotationTarget = angle;
            vs.rotation = angle;
            if (linked)
            {
                linked->rotationTarget = angle;
                linked->rotation = angle;
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 ringCol = dialHov || dialAct
                            ? IM_COL32(200, 200, 200, 255)
                            : IM_COL32(150, 150, 150, 200);
        dl->AddCircle(center, dialR, ringCol, 24, 2.0f);
        float indX = center.x + cosf(vs.rotation) * dialR;
        float indY = center.y + sinf(vs.rotation) * dialR;
        dl->AddLine(center, ImVec2(indX, indY),
                    IM_COL32(255, 180, 50, 255), 2.0f);
        dl->AddCircleFilled(ImVec2(indX, indY), 3.0f,
                            IM_COL32(255, 180, 50, 255));

        ImGui::SameLine();
        ImGui::Text("%.0f deg", vs.rotation * 180.0f / 3.14159265f);

        if (mode)
        {
            bool pointActive = (*mode == AnnotationMode::AddPoint);
            if (pointActive)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImVec4(0.8f, 0.2f, 0.2f, 0.7f));
            if (ImGui::Button("+ Point"))
                *mode = pointActive ? AnnotationMode::None
                                    : AnnotationMode::AddPoint;
            if (pointActive) ImGui::PopStyleColor();

            ImGui::SameLine();

            bool boundsActive = (*mode == AnnotationMode::AddBounds);
            if (boundsActive)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImVec4(0.2f, 0.8f, 0.2f, 0.7f));
            if (ImGui::Button("+ Bounds"))
            {
                *mode = boundsActive ? AnnotationMode::None
                                     : AnnotationMode::AddBounds;
                if (boundsActive) removePointsOutsideBounds(img);
            }
            if (boundsActive) ImGui::PopStyleColor();

            ImGui::SameLine();

            if (ImGui::Button("Undo"))
            {
                const char* key = nullptr;
                if (*mode == AnnotationMode::AddPoint)
                    key = "points";
                else if (*mode == AnnotationMode::AddBounds)
                    key = "bounds";
                if (key && img.annotations.contains(key) &&
                    img.annotations[key].isArray())
                {
                    auto& arr = img.annotations[key].getArray();
                    if (!arr.empty()) arr.pop_back();
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear"))
            {
                img.annotations["bounds"].setArray();
                img.annotations["points"].setArray();
            }
        }

        ImGui::PopID();
    }

    static void renderSingleViewer(AppState& state,
                                   int& selectedIdx,
                                   int otherIdx,
                                   ImageViewState& vs,
                                   const char* label,
                                   ImageViewState* linked =
                                       nullptr)
    {
        const char* preview =
            (selectedIdx >= 0 &&
             selectedIdx < (int)state.images.size())
                ? state.images[selectedIdx].name.c_str()
                : "<none>";

        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("<none>",
                                  selectedIdx < 0))
            {
                selectedIdx = -1;
                vs.zoom = vs.zoomTarget = 1.0f;
                vs.pan = vs.panTarget = ImVec2(0, 0);
            }
            for (int i = 0;
                 i < (int)state.images.size(); ++i)
            {
                if (i == otherIdx) continue;
                bool selected = (i == selectedIdx);
                if (ImGui::Selectable(
                        state.images[i].name.c_str(),
                        selected))
                {
                    selectedIdx = i;
                    vs.zoom = vs.zoomTarget = 0.0f;
                    vs.pan = vs.panTarget =
                        ImVec2(0, 0);
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (selectedIdx < 0 ||
            selectedIdx >= (int)state.images.size())
            return;

        auto& img = state.images[selectedIdx];
        float toolbarH =
            ImGui::GetFrameHeightWithSpacing() * 3.0f;
        ImVec2 region = ImGui::GetContentRegionAvail();
        float canvasH = region.y - toolbarH;
        if (canvasH > 0.0f)
        {
            ImGui::BeginChild("##cvs",
                              ImVec2(0, canvasH),
                              ImGuiChildFlags_None);
            renderImageCanvas(img, vs, "##canvas",
                              linked,
                              &img.annotationMode);
            ImGui::EndChild();
        }
        renderImageToolbar(img, vs, label,
                           &img.annotationMode,
                           linked);
    }

    static void renderImageViewer(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float dockH =
            ImGui::GetFrameHeightWithSpacing() +
            ImGui::GetStyle().ItemSpacing.y;
        float contentH = avail.y - dockH;

        ImGui::BeginChild("ComparisonContent",
                          ImVec2(avail.x, contentH),
                          ImGuiChildFlags_None);
        {
            float totalW =
                ImGui::GetContentRegionAvail().x;
            float splitterW = 8.0f;
            float leftW =
                totalW * state.viewerSplitRatio -
                splitterW * 0.5f;
            float rightW =
                totalW *
                    (1.0f - state.viewerSplitRatio) -
                splitterW * 0.5f;

            ImGui::BeginChild("LeftViewer",
                              ImVec2(leftW, 0),
                              ImGuiChildFlags_Borders);
            renderSingleViewer(
                state, state.viewerLeftIdx,
                state.viewerRightIdx,
                state.viewerLeftState, "##Left",
                state.viewerLocked
                    ? &state.viewerRightState
                    : nullptr);
            ImGui::EndChild();

            ImGui::SameLine();

            // Draggable splitter
            float height =
                ImGui::GetContentRegionAvail().y;
            ImGui::Button("##Splitter",
                          ImVec2(splitterW, height));
            if (ImGui::IsItemActive())
            {
                float delta =
                    ImGui::GetIO().MouseDelta.x;
                state.viewerSplitRatio +=
                    delta / totalW;
                state.viewerSplitRatio = std::clamp(
                    state.viewerSplitRatio, 0.1f,
                    0.9f);
            }
            if (ImGui::IsItemHovered() ||
                ImGui::IsItemActive())
                ImGui::SetMouseCursor(
                    ImGuiMouseCursor_ResizeEW);

            ImGui::SameLine();

            ImGui::BeginChild("RightViewer",
                              ImVec2(rightW, 0),
                              ImGuiChildFlags_Borders);
            renderSingleViewer(
                state, state.viewerRightIdx,
                state.viewerLeftIdx,
                state.viewerRightState, "##Right",
                state.viewerLocked
                    ? &state.viewerLeftState
                    : nullptr);
            ImGui::EndChild();
        }
        ImGui::EndChild();

        // --- Dock bar ---
        ImGui::Separator();
        if (ImGui::Button(state.viewerLocked
                              ? "Unlock"
                              : "Lock"))
            state.viewerLocked = !state.viewerLocked;
    }

    static void renderImageGallery(AppState& state)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 origin = ImGui::GetCursorScreenPos();

        // Reserve space for dock bar at bottom
        float dockH =
            ImGui::GetFrameHeightWithSpacing() +
            ImGui::GetStyle().ItemSpacing.y;
        float galleryH = avail.y - dockH;

        ImGui::BeginChild("GalleryArea",
                          ImVec2(avail.x, galleryH),
                          ImGuiChildFlags_None);

        // Title bar height for sizing the window
        float titleH =
            ImGui::GetFrameHeight() +
            ImGui::GetStyle().FramePadding.y;
        ImVec2 pad = ImGui::GetStyle().WindowPadding;
        float maxW = avail.x * 0.5f;
        float maxH = galleryH * 0.5f;

        int removeIdx = -1;
        for (int i = 0; i < (int)state.images.size(); ++i)
        {
            auto& img = state.images[i];

            // Scale image to fit within max bounds
            float scale =
                std::min(maxW / (float)img.width,
                         maxH / (float)img.height);
            scale = std::min(scale, 1.0f);
            float dispW = img.width * scale;
            float dispH = img.height * scale;

            float tbRows =
                ImGui::GetFrameHeightWithSpacing() *
                3.0f;
            float minW = 400.0f;
            float winW =
                std::max(dispW + pad.x * 2, minW);
            ImGui::SetNextWindowSize(
                ImVec2(winW,
                       dispH + pad.y * 2 + titleH +
                           tbRows),
                ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(
                ImVec2(origin.x + 20 + i * 30,
                       origin.y + 20 + i * 30),
                ImGuiCond_FirstUseEver);

            char winId[128];
            snprintf(winId, sizeof(winId),
                     "%s###gallery_%d",
                     img.name.c_str(), i);

            char canvasId[64];
            snprintf(canvasId, sizeof(canvasId),
                     "##gcanvas_%d", i);

            if (img.minimized)
                ImGui::SetNextWindowCollapsed(
                    true, ImGuiCond_Always);

            bool open = true;
            if (ImGui::Begin(
                    winId, &open,
                    ImGuiWindowFlags_NoSavedSettings))
            {
                if (img.minimized)
                    img.minimized = false;
                if (ImGui::IsWindowFocused(
                        ImGuiFocusedFlags_ChildWindows))
                    state.activeGalleryImage = i;

                float toolbarH =
                    ImGui::GetFrameHeightWithSpacing() *
                    3.0f;
                ImVec2 region =
                    ImGui::GetContentRegionAvail();
                float canvasH = region.y - toolbarH;
                if (canvasH > 0.0f)
                {
                    ImGui::BeginChild(
                        canvasId, ImVec2(0, canvasH),
                        ImGuiChildFlags_None);
                    char cid[64];
                    snprintf(cid, sizeof(cid),
                             "##gc_%d", i);
                    renderImageCanvas(
                        img, img.viewState, cid,
                        nullptr, &img.annotationMode);
                    ImGui::EndChild();
                }
                char tbId[64];
                snprintf(tbId, sizeof(tbId),
                         "##gtb_%d", i);
                renderImageToolbar(img, img.viewState,
                                   tbId,
                                   &img.annotationMode);
            }
            ImGui::End();

            if (!open) removeIdx = i;
        }

        if (removeIdx >= 0)
        {
            freeTexture(
                state.images[removeIdx].textureId);
            state.images.erase(state.images.begin() +
                               removeIdx);
            if (state.viewerLeftIdx >=
                (int)state.images.size())
                state.viewerLeftIdx =
                    (int)state.images.size() - 1;
            if (state.viewerRightIdx >=
                (int)state.images.size())
                state.viewerRightIdx =
                    (int)state.images.size() - 1;
            if (state.activeGalleryImage == removeIdx)
                state.activeGalleryImage = -1;
            else if (state.activeGalleryImage >
                     removeIdx)
                state.activeGalleryImage--;
        }

        if (state.images.empty())
            ImGui::Text(
                "Use Load Image to add images");

        ImGui::EndChild();

        // --- Dock bar ---
        ImGui::Separator();
        bool hasActive =
            state.activeGalleryImage >= 0 &&
            state.activeGalleryImage <
                (int)state.images.size();

        if (ImGui::Button("Load Image"))
            state.imageLoadBrowser.show = true;

        ImGui::SameLine();
        ImGui::BeginDisabled(!hasActive);
        if (ImGui::Button("Save PNG"))
        {
            state.imageSaveBrowser.show = true;
            state.imageSaveTarget =
                state.activeGalleryImage;
            state.imageSaveBrowser.dirNeedsRefresh =
                true;
            state.imageSaveBrowser.fileName.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load JSON"))
        {
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = false;
            state.annotationFileTarget =
                state.activeGalleryImage;
            state.annotationFileBrowser
                .dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName
                .clear();
            state.annotationFileBrowser.title =
                "Load Annotations";
        }
        ImGui::SameLine();
        if (ImGui::Button("Save JSON"))
        {
            state.annotationFileBrowser.show = true;
            state.annotationFileSave = true;
            state.annotationFileTarget =
                state.activeGalleryImage;
            state.annotationFileBrowser
                .dirNeedsRefresh = true;
            state.annotationFileBrowser.fileName
                .clear();
            state.annotationFileBrowser.title =
                "Save Annotations";
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(state.images.empty());
        if (ImGui::Button("Images"))
            state.imageListDialog.show = true;
        ImGui::EndDisabled();
    }

    static void renderAbout()
    {
        static std::string aboutText;
        if (aboutText.empty())
        {
            auto data = HelloImGui::LoadAssetFileData("about.txt");
            if (data.data)
            {
                aboutText.assign((const char*)data.data, data.dataSize);
                HelloImGui::FreeAssetFileData(&data);
            }
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::InputTextMultiline(
            "##about", const_cast<char*>(aboutText.c_str()),
            aboutText.size() + 1, avail, ImGuiInputTextFlags_ReadOnly);
    }

    static void renderImageSaveProgressPopup(
        AppState& state)
    {
        if (state.imageSaveInProgress.load())
        {
            ImGui::OpenPopup("Saving Image...");
        }
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowSize(
            ImVec2(ds.x * 0.4f, ds.y * 0.2f),
            ImGuiCond_Always);
        ImGui::SetNextWindowPos(
            ImVec2(ds.x * 0.3f, ds.y * 0.4f),
            ImGuiCond_Always);
        if (ImGui::BeginPopupModal(
                "Saving Image...", nullptr,
                ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove))
        {
            ImGui::Text("Saving to: %s",
                        state.imageSaveProgressPath
                            .c_str());
            ImGui::Spacing();

            // Animated spinner dots
            int dots =
                (int)(ImGui::GetTime() / 0.4) % 3 + 1;
            ImGui::Text("Please wait%.*s", dots, "...");

            if (state.imageSaveDone.load())
            {
                if (state.imageSaveThread.joinable())
                    state.imageSaveThread.join();
                if (state.imageSaveResult.load() != 0)
                {
                    state.imageSaveError.show = true;
                    state.imageSaveError.message =
                        "Failed to save image "
                        "to:\n" +
                        state.imageSaveProgressPath;
                }
                state.imageSaveInProgress = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    static void renderGui(AppState& state)
    {
        state.annotationError.render();
        state.imageSaveError.render();
        state.imageLoadBrowser.render();
        state.imageSaveBrowser.render();
        state.annotationFileBrowser.render();
        renderImageSaveProgressPopup(state);
        state.imageListDialog.render(
            state.images, state.viewerLeftIdx,
            state.viewerRightIdx,
            state.activeGalleryImage);

        if (ImGui::BeginTabBar("MainTabs"))
        {
            if (ImGui::BeginTabItem("Image Viewer"))
            {
                renderImageGallery(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Image Comparison"))
            {
                renderImageViewer(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings"))
            {
                renderFilesAndSettings(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("About"))
            {
                renderAbout();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void submain(void)
    {
        runSplash(1.0);

        AppState state;

        // Configure error popup titles
        state.imageSaveError.title = "Image Save Error";
        state.annotationError.title = "Annotation Error";

        // Configure image save browser
        state.imageSaveBrowser.extension = ".png";
        state.imageSaveBrowser.title = "Save Image";
        state.imageSaveBrowser.onOk =
            [&state](const std::string& fullPath)
        {
            if (state.imageSaveTarget >= 0 &&
                state.imageSaveTarget <
                    (int)state.images.size() &&
                !state.imageSaveInProgress.load())
            {
                auto& img =
                    state.images[state.imageSaveTarget];

                // GL readback on UI thread
                int w = img.width;
                int h = img.height;
                auto buffer =
                    std::make_shared<
                        std::vector<unsigned char>>(
                        w * h * 4);
                GLuint tex =
                    (GLuint)(intptr_t)img.textureId;
                glBindTexture(GL_TEXTURE_2D, tex);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              buffer->data());

                // Launch background thread
                state.imageSaveInProgress = true;
                state.imageSaveDone = false;
                state.imageSaveProgressPath = fullPath;
                if (state.imageSaveThread.joinable())
                    state.imageSaveThread.join();
                state.imageSaveThread = std::thread(
                    [&state, fullPath, buffer, w, h]()
                    {
                        int res = savePngToDisk(
                            fullPath, buffer->data(),
                            w, h);
                        state.imageSaveResult = res;
                        state.imageSaveDone = true;
                    });
            }
        };

        // Configure annotation file browser
        state.annotationFileBrowser.extension = ".json";
        state.annotationFileBrowser.title =
            "Annotation File";
        state.annotationFileBrowser.onOk =
            [&state](const std::string& fullPath)
        {
            if (state.annotationFileTarget >= 0 &&
                state.annotationFileTarget <
                    (int)state.images.size())
            {
                auto& img =
                    state
                        .images[state.annotationFileTarget];
                if (state.annotationFileSave)
                {
                    if (saveAnnotationsToFile(
                            fullPath,
                            img.annotations) != 0)
                    {
                        state.annotationError.show = true;
                        state.annotationError.message =
                            "Failed to save "
                            "annotations to:\n" +
                            fullPath;
                    }
                }
                else
                {
                    if (loadAnnotationsFromFile(
                            fullPath,
                            img.annotations) != 0)
                    {
                        state.annotationError.show = true;
                        state.annotationError.message =
                            "Failed to load "
                            "annotations from:\n" +
                            fullPath;
                    }
                }
            }
        };

        // Configure image load browser
        state.imageLoadBrowser.extension = ".png";
        state.imageLoadBrowser.title = "Load Image";
        state.imageLoadBrowser.onSelect =
            [&state](const std::string& fullPath,
                     const std::string& name)
        {
            bool alreadyLoaded = false;
            for (auto& img : state.images)
            {
                if (img.path == fullPath)
                {
                    alreadyLoaded = true;
                    break;
                }
            }
            if (!alreadyLoaded)
            {
                LoadedImage img;
                img.name = name;
                img.path = fullPath;
                if (loadPngFromDisk(fullPath,
                                    img.textureId,
                                    img.width,
                                    img.height))
                {
                    img.annotations.setObject();
                    img.annotations["bounds"]
                        .setArray();
                    img.annotations["points"]
                        .setArray();
                    state.images.push_back(img);
                }
            }
        };

        HelloImGui::RunnerParams params;
        params.appWindowParams.windowTitle = "ShoeComp";
        params.appWindowParams.windowGeometry.fullScreenMode =
            HelloImGui::FullScreenMode::FullMonitorWorkArea;
        params.imGuiWindowParams.defaultImGuiWindowType =
            HelloImGui::DefaultImGuiWindowType::ProvideFullScreenWindow;
        params.callbacks.PostInit = []()
        { ImGui::GetIO().FontGlobalScale = 2.5f; };
        params.callbacks.ShowGui = [&state]() { renderGui(state); };
        params.callbacks.BeforeExit = [&state]()
        {
            if (state.imageSaveThread.joinable())
                state.imageSaveThread.join();
            for (auto& img : state.images) freeTexture(img.textureId);
            state.images.clear();
        };

        HelloImGui::Run(params);
    }

}  // namespace shoecomp
