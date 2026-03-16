#include "ui/imageCanvas.h"
#include "formats/png.h"
#include <algorithm>
#include <cmath>

namespace shoecomp
{
    ImageData::~ImageData()
    {
        if (textureId) freeTexture(textureId);
    }

    ImageCanvas::ImageCanvas()
        : image(std::make_shared<ImageData>())
    {
    }

    ImageCanvas::ImageCanvas(
        std::shared_ptr<ImageData> img)
        : image(std::move(img))
    {
    }

    ImVec2 ImageCanvas::screenToImageCoord(
        ImVec2 sp, ImVec2 canvasPos, ImVec2 avail,
        ImVec2 pan, float zoom, float baseScale,
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

    ImVec2 ImageCanvas::imageToScreenCoord(
        float ix, float iy, float cx, float cy,
        float scale, float cosR, float sinR, int imgW,
        int imgH)
    {
        float lx = (ix - imgW * 0.5f) * scale;
        float ly = (iy - imgH * 0.5f) * scale;
        return ImVec2(cx + lx * cosR - ly * sinR,
                      cy + lx * sinR + ly * cosR);
    }

    bool ImageCanvas::pointInPolygon(
        float px, float py,
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
                (px < (xj - xi) * (py - yi) /
                          (yj - yi) +
                      xi))
                inside = !inside;
        }
        return inside;
    }

    void ImageCanvas::removePointsOutsideBounds()
    {
        if (!image->annotations.isObject() ||
            !image->annotations.contains("bounds") ||
            !image->annotations["bounds"].isArray())
            return;
        auto& bnd =
            image->annotations["bounds"].getArray();
        if (bnd.size() < 3) return;
        if (!image->annotations.contains("points") ||
            !image->annotations["points"].isArray())
            return;
        auto& pts =
            image->annotations["points"].getArray();
        pts.erase(
            std::remove_if(
                pts.begin(), pts.end(),
                [&bnd](jt::Json& p)
                {
                    return !pointInPolygon(
                        p["x"].getFloat(),
                        p["y"].getFloat(), bnd);
                }),
            pts.end());
    }

    void ImageCanvas::renderCanvas(
        const char* canvasId, ImageViewState* linked)
    {
        AnnotationMode& mode =
            image->annotationMode;
        ImVec2 avail = ImGui::GetContentRegionAvail();

        float scaleX =
            avail.x / (float)image->width;
        float scaleY =
            avail.y / (float)image->height;
        float baseScale = std::min(scaleX, scaleY);
        if (viewState.zoomTarget <= 0.0f)
            viewState.zoomTarget =
                scaleX / baseScale;
        if (viewState.zoom <= 0.0f)
            viewState.zoom = viewState.zoomTarget;

        ImVec2 canvasPos =
            ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(
            canvasId, avail,
            ImGuiButtonFlags_MouseButtonLeft);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        ImGuiIO& io = ImGui::GetIO();

        if (hovered && io.KeyCtrl &&
            io.MouseWheel != 0.0f)
        {
            float oldTarget = viewState.zoomTarget;
            viewState.zoomTarget *=
                (io.MouseWheel > 0) ? 1.15f : 0.87f;
            viewState.zoomTarget = std::clamp(
                viewState.zoomTarget, 0.1f, 50.0f);
            ImVec2 mouse =
                ImVec2(io.MousePos.x - canvasPos.x,
                       io.MousePos.y - canvasPos.y);
            float ratio =
                viewState.zoomTarget / oldTarget;
            viewState.panTarget.x =
                (1.0f - ratio) *
                    (mouse.x - avail.x * 0.5f) +
                ratio * viewState.panTarget.x;
            viewState.panTarget.y =
                (1.0f - ratio) *
                    (mouse.y - avail.y * 0.5f) +
                ratio * viewState.panTarget.y;
            if (linked)
            {
                linked->zoomTarget =
                    viewState.zoomTarget;
                linked->panTarget =
                    viewState.panTarget;
            }
        }

        if (hovered && !io.KeyCtrl &&
            io.MouseWheel != 0.0f)
        {
            viewState.panTarget.y +=
                io.MouseWheel * 30.0f;
            if (linked)
                linked->panTarget.y =
                    viewState.panTarget.y;
        }

        if (active && io.KeyCtrl &&
            ImGui::IsMouseDragging(
                ImGuiMouseButton_Left))
        {
            viewState.panTarget.x += io.MouseDelta.x;
            viewState.panTarget.y += io.MouseDelta.y;
            viewState.pan.x += io.MouseDelta.x;
            viewState.pan.y += io.MouseDelta.y;
            if (linked)
            {
                linked->panTarget.x +=
                    io.MouseDelta.x;
                linked->panTarget.y +=
                    io.MouseDelta.y;
                linked->pan.x += io.MouseDelta.x;
                linked->pan.y += io.MouseDelta.y;
            }
        }

        if (hovered &&
            ImGui::IsMouseClicked(
                ImGuiMouseButton_Left) &&
            !io.KeyCtrl &&
            mode != AnnotationMode::None)
        {
            ImVec2 ic = screenToImageCoord(
                io.MousePos, canvasPos, avail,
                viewState.pan, viewState.zoom,
                baseScale, viewState.rotation,
                image->width, image->height);
            const char* key =
                (mode == AnnotationMode::AddPoint)
                    ? "points"
                    : "bounds";

            // Screen-pixel threshold converted to
            // image-space
            float screenPx = 10.0f;
            float imgPx = screenPx /
                (baseScale * viewState.zoom);
            float thresh2 = imgPx * imgPx;

            // Close bounds polygon by clicking
            // near the first vertex
            if (mode == AnnotationMode::AddBounds &&
                !io.KeyShift &&
                image->annotations.contains(
                    "bounds") &&
                image->annotations["bounds"]
                    .isArray() &&
                image->annotations["bounds"]
                        .getArray()
                        .size() >= 3)
            {
                auto& bnd =
                    image->annotations["bounds"]
                        .getArray();
                float dx =
                    bnd[0]["x"].getFloat() - ic.x;
                float dy =
                    bnd[0]["y"].getFloat() - ic.y;
                if (dx * dx + dy * dy < thresh2)
                {
                    mode = AnnotationMode::None;
                    removePointsOutsideBounds();
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
                if (image->annotations.contains(
                        key) &&
                    image->annotations[key]
                        .isArray())
                {
                    auto& arr =
                        image->annotations[key]
                            .getArray();
                    float bestDist = thresh2;
                    int bestIdx = -1;
                    for (int ai = 0;
                         ai < (int)arr.size();
                         ++ai)
                    {
                        float dx =
                            arr[ai]["x"]
                                .getFloat() -
                            ic.x;
                        float dy =
                            arr[ai]["y"]
                                .getFloat() -
                            ic.y;
                        float d2 =
                            dx * dx + dy * dy;
                        if (d2 < bestDist)
                        {
                            bestDist = d2;
                            bestIdx = ai;
                        }
                    }
                    if (bestIdx >= 0)
                        arr.erase(
                            arr.begin() + bestIdx);
                }
            }
            else
            {
                bool allow = true;
                if (mode ==
                        AnnotationMode::AddPoint &&
                    image->annotations.contains(
                        "bounds") &&
                    image->annotations["bounds"]
                        .isArray() &&
                    image->annotations["bounds"]
                            .getArray()
                            .size() >= 3)
                {
                    allow = pointInPolygon(
                        ic.x, ic.y,
                        image->annotations["bounds"]
                            .getArray());
                }
                if (allow)
                {
                    jt::Json pt;
                    pt.setObject();
                    pt["x"] = ic.x;
                    pt["y"] = ic.y;
                    image->annotations[key]
                        .getArray()
                        .push_back(std::move(pt));
                }
            }
        }

        float speed = 12.0f * io.DeltaTime;
        speed = std::clamp(speed, 0.0f, 1.0f);
        viewState.zoom +=
            (viewState.zoomTarget - viewState.zoom) *
            speed;
        viewState.pan.x +=
            (viewState.panTarget.x -
             viewState.pan.x) *
            speed;
        viewState.pan.y +=
            (viewState.panTarget.y -
             viewState.pan.y) *
            speed;
        viewState.rotation +=
            (viewState.rotationTarget -
             viewState.rotation) *
            speed;

        float dispW = image->width * baseScale *
            viewState.zoom;
        float dispH = image->height * baseScale *
            viewState.zoom;

        float limX =
            std::max(avail.x, dispW) * 0.5f;
        float limY =
            std::max(avail.y, dispH) * 0.5f;
        viewState.panTarget.x = std::clamp(
            viewState.panTarget.x, -limX, limX);
        viewState.panTarget.y = std::clamp(
            viewState.panTarget.y, -limY, limY);
        viewState.pan.x = std::clamp(
            viewState.pan.x, -limX, limX);
        viewState.pan.y = std::clamp(
            viewState.pan.y, -limY, limY);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(
            canvasPos,
            ImVec2(canvasPos.x + avail.x,
                   canvasPos.y + avail.y),
            true);
        float cx = canvasPos.x + avail.x * 0.5f +
            viewState.pan.x;
        float cy = canvasPos.y + avail.y * 0.5f +
            viewState.pan.y;
        float hw = dispW * 0.5f;
        float hh = dispH * 0.5f;
        float cosR = cosf(viewState.rotation);
        float sinR = sinf(viewState.rotation);
        auto rot = [&](float lx, float ly) -> ImVec2
        {
            return ImVec2(
                cx + lx * cosR - ly * sinR,
                cy + lx * sinR + ly * cosR);
        };
        ImVec2 tl = rot(-hw, -hh);
        ImVec2 tr = rot(hw, -hh);
        ImVec2 br = rot(hw, hh);
        ImVec2 bl = rot(-hw, hh);
        dl->AddImageQuad(
            image->textureId, tl, tr, br, bl,
            ImVec2(0, 0), ImVec2(1, 0),
            ImVec2(1, 1), ImVec2(0, 1));
        dl->AddQuad(tl, tr, br, bl,
                    IM_COL32(180, 180, 180, 200));

        // Dim area outside bounds when not editing
        float annScale =
            baseScale * viewState.zoom;
        if (mode != AnnotationMode::AddBounds &&
            image->annotations.isObject() &&
            image->annotations.contains("bounds") &&
            image->annotations["bounds"].isArray() &&
            image->annotations["bounds"]
                    .getArray()
                    .size() >= 3)
        {
            auto& bnd =
                image->annotations["bounds"]
                    .getArray();
            // Build screen-space bounds polygon
            std::vector<ImVec2> screenBnd;
            screenBnd.reserve(bnd.size());
            for (auto& v : bnd)
            {
                screenBnd.push_back(
                    imageToScreenCoord(
                        v["x"].getFloat(),
                        v["y"].getFloat(), cx, cy,
                        annScale, cosR, sinR,
                        image->width,
                        image->height));
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
            for (int si =
                     (int)screenBnd.size() - 1;
                 si >= 0; --si)
                frame.push_back(screenBnd[si]);
            frame.push_back(cBL);
            dl->AddConcavePolyFilled(
                frame.data(), (int)frame.size(),
                IM_COL32(0, 0, 0, 120));
        }

        // Annotation overlays
        if (image->annotations.isObject())
        {
            if (image->annotations.contains(
                    "points") &&
                image->annotations["points"]
                    .isArray())
            {
                auto& pts =
                    image->annotations["points"]
                        .getArray();
                for (auto& p : pts)
                {
                    ImVec2 sp = imageToScreenCoord(
                        p["x"].getFloat(),
                        p["y"].getFloat(), cx, cy,
                        annScale, cosR, sinR,
                        image->width,
                        image->height);
                    dl->AddCircleFilled(
                        sp, 5.0f,
                        IM_COL32(
                            255, 50, 50, 220));
                    dl->AddCircle(
                        sp, 5.0f,
                        IM_COL32(
                            255, 255, 255, 200),
                        12, 1.5f);
                }
            }
            if (image->annotations.contains(
                    "bounds") &&
                image->annotations["bounds"]
                    .isArray())
            {
                auto& bnd =
                    image->annotations["bounds"]
                        .getArray();
                bool editing =
                    (mode ==
                     AnnotationMode::AddBounds);
                // Solid edges between consecutive
                // vertices
                if (bnd.size() >= 2)
                {
                    for (size_t i = 0;
                         i + 1 < bnd.size(); ++i)
                    {
                        ImVec2 a =
                            imageToScreenCoord(
                                bnd[i]["x"]
                                    .getFloat(),
                                bnd[i]["y"]
                                    .getFloat(),
                                cx, cy, annScale,
                                cosR, sinR,
                                image->width,
                                image->height);
                        ImVec2 b =
                            imageToScreenCoord(
                                bnd[i + 1]["x"]
                                    .getFloat(),
                                bnd[i + 1]["y"]
                                    .getFloat(),
                                cx, cy, annScale,
                                cosR, sinR,
                                image->width,
                                image->height);
                        dl->AddLine(
                            a, b,
                            IM_COL32(
                                50, 255, 50, 220),
                            2.0f);
                    }
                }
                // Closing segment: solid when not
                // editing, dashed when editing
                if (bnd.size() >= 2)
                {
                    ImVec2 last =
                        imageToScreenCoord(
                            bnd.back()["x"]
                                .getFloat(),
                            bnd.back()["y"]
                                .getFloat(),
                            cx, cy, annScale,
                            cosR, sinR,
                            image->width,
                            image->height);
                    ImVec2 first =
                        imageToScreenCoord(
                            bnd[0]["x"]
                                .getFloat(),
                            bnd[0]["y"]
                                .getFloat(),
                            cx, cy, annScale,
                            cosR, sinR,
                            image->width,
                            image->height);
                    if (!editing)
                    {
                        dl->AddLine(
                            last, first,
                            IM_COL32(
                                50, 255, 50, 220),
                            2.0f);
                    }
                    else
                    {
                        // Dashed closing segment
                        float dashLen = 8.0f;
                        float gapLen = 6.0f;
                        auto drawDashed =
                            [&](ImVec2 p0,
                                ImVec2 p1)
                        {
                            float dx =
                                p1.x - p0.x;
                            float dy =
                                p1.y - p0.y;
                            float len = sqrtf(
                                dx * dx +
                                dy * dy);
                            if (len < 1.0f)
                                return;
                            float nx = dx / len;
                            float ny = dy / len;
                            float t = 0.0f;
                            while (t < len)
                            {
                                float t1 =
                                    std::min(
                                        t +
                                            dashLen,
                                        len);
                                dl->AddLine(
                                    ImVec2(
                                        p0.x +
                                            nx *
                                                t,
                                        p0.y +
                                            ny *
                                                t),
                                    ImVec2(
                                        p0.x +
                                            nx *
                                                t1,
                                        p0.y +
                                            ny *
                                                t1),
                                    IM_COL32(
                                        50, 255,
                                        50, 120),
                                    1.5f);
                                t = t1 + gapLen;
                            }
                        };
                        if (hovered)
                        {
                            drawDashed(
                                last,
                                io.MousePos);
                            drawDashed(
                                io.MousePos,
                                first);
                        }
                        else
                        {
                            drawDashed(
                                last, first);
                        }
                    }
                }
                for (auto& v : bnd)
                {
                    ImVec2 sp =
                        imageToScreenCoord(
                            v["x"].getFloat(),
                            v["y"].getFloat(),
                            cx, cy, annScale,
                            cosR, sinR,
                            image->width,
                            image->height);
                    dl->AddCircleFilled(
                        sp, 4.0f,
                        IM_COL32(
                            50, 255, 50, 220));
                }
            }
        }

        float barThick = 8.0f;
        float barPad = 2.0f;
        ImU32 barCol =
            IM_COL32(200, 200, 200, 100);
        ImU32 barColHov =
            IM_COL32(200, 200, 200, 180);
        ImVec2 mpos = io.MousePos;

        if (dispW > avail.x)
        {
            float viewRatio = avail.x / dispW;
            float barW = avail.x * viewRatio;
            float t =
                (limX - viewState.pan.x) /
                (2.0f * limX);
            float barX =
                canvasPos.x +
                t * (avail.x - barW);
            float barY = canvasPos.y + avail.y -
                         barThick - barPad;
            ImVec2 bMin(barX, barY);
            ImVec2 bMax(barX + barW,
                        barY + barThick);

            bool barHov =
                mpos.x >= bMin.x &&
                mpos.x <= bMax.x &&
                mpos.y >= bMin.y &&
                mpos.y <= bMax.y;
            bool barDrag =
                barHov &&
                ImGui::IsMouseDragging(
                    ImGuiMouseButton_Left);
            if (barDrag)
            {
                float dx =
                    -io.MouseDelta.x /
                    (avail.x - barW) *
                    (2.0f * limX);
                viewState.pan.x += dx;
                viewState.panTarget.x += dx;
                if (linked)
                {
                    linked->pan.x += dx;
                    linked->panTarget.x += dx;
                }
            }
            dl->AddRectFilled(
                bMin, bMax,
                barDrag || barHov ? barColHov
                                  : barCol,
                barThick * 0.5f);
        }

        if (dispH > avail.y)
        {
            float viewRatio = avail.y / dispH;
            float barH = avail.y * viewRatio;
            float t =
                (limY - viewState.pan.y) /
                (2.0f * limY);
            float barX = canvasPos.x + avail.x -
                         barThick - barPad;
            float barY =
                canvasPos.y +
                t * (avail.y - barH);
            ImVec2 bMin(barX, barY);
            ImVec2 bMax(barX + barThick,
                        barY + barH);

            bool barHov =
                mpos.x >= bMin.x &&
                mpos.x <= bMax.x &&
                mpos.y >= bMin.y &&
                mpos.y <= bMax.y;
            bool barDrag =
                barHov &&
                ImGui::IsMouseDragging(
                    ImGuiMouseButton_Left);
            if (barDrag)
            {
                float dy =
                    -io.MouseDelta.y /
                    (avail.y - barH) *
                    (2.0f * limY);
                viewState.pan.y += dy;
                viewState.panTarget.y += dy;
                if (linked)
                {
                    linked->pan.y += dy;
                    linked->panTarget.y += dy;
                }
            }
            dl->AddRectFilled(
                bMin, bMax,
                barDrag || barHov ? barColHov
                                  : barCol,
                barThick * 0.5f);
        }

        viewState.panTarget.x = std::clamp(
            viewState.panTarget.x, -limX, limX);
        viewState.panTarget.y = std::clamp(
            viewState.panTarget.y, -limY, limY);
        viewState.pan.x = std::clamp(
            viewState.pan.x, -limX, limX);
        viewState.pan.y = std::clamp(
            viewState.pan.y, -limY, limY);

        dl->PopClipRect();
    }

    void ImageCanvas::renderToolbar(
        const char* toolbarId,
        ImageViewState* linked)
    {
        AnnotationMode& mode =
            image->annotationMode;
        ImGui::PushID(toolbarId);

        float frameH = ImGui::GetFrameHeight();

        // Home: fit image vertically, reset pan
        if (ImGui::Button("Home"))
        {
            viewState.zoomTarget = 1.0f;
            viewState.panTarget = ImVec2(0, 0);
            viewState.rotationTarget = 0.0f;
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
            viewState.zoomTarget = std::clamp(
                viewState.zoomTarget * 1.25f,
                0.1f, 50.0f);
            if (linked)
                linked->zoomTarget =
                    viewState.zoomTarget;
        }
        ImGui::SameLine();

        // Zoom out
        if (ImGui::Button("-"))
        {
            viewState.zoomTarget = std::clamp(
                viewState.zoomTarget * 0.8f,
                0.1f, 50.0f);
            if (linked)
                linked->zoomTarget =
                    viewState.zoomTarget;
        }
        ImGui::SameLine();

        // Rotation dial
        float dialR = frameH * 0.5f;
        ImVec2 cursor =
            ImGui::GetCursorScreenPos();
        ImVec2 center(cursor.x + dialR,
                      cursor.y + dialR);
        ImGui::InvisibleButton(
            "##dial",
            ImVec2(dialR * 2.0f, dialR * 2.0f));
        bool dialHov = ImGui::IsItemHovered();
        bool dialAct = ImGui::IsItemActive();

        if (dialAct)
        {
            ImGuiIO& io = ImGui::GetIO();
            float angle = atan2f(
                io.MousePos.y - center.y,
                io.MousePos.x - center.x);
            viewState.rotationTarget = angle;
            viewState.rotation = angle;
            if (linked)
            {
                linked->rotationTarget = angle;
                linked->rotation = angle;
            }
        }

        ImDrawList* dl =
            ImGui::GetWindowDrawList();
        ImU32 ringCol =
            dialHov || dialAct
                ? IM_COL32(200, 200, 200, 255)
                : IM_COL32(150, 150, 150, 200);
        dl->AddCircle(center, dialR, ringCol, 24,
                      2.0f);
        float indX = center.x +
            cosf(viewState.rotation) * dialR;
        float indY = center.y +
            sinf(viewState.rotation) * dialR;
        dl->AddLine(
            center, ImVec2(indX, indY),
            IM_COL32(255, 180, 50, 255), 2.0f);
        dl->AddCircleFilled(
            ImVec2(indX, indY), 3.0f,
            IM_COL32(255, 180, 50, 255));

        ImGui::SameLine();
        ImGui::Text(
            "%.0f deg",
            viewState.rotation * 180.0f /
                3.14159265f);

        bool pointActive =
            (mode == AnnotationMode::AddPoint);
        if (pointActive)
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                ImVec4(0.8f, 0.2f, 0.2f, 0.7f));
        if (ImGui::Button("+ Point"))
            mode = pointActive
                       ? AnnotationMode::None
                       : AnnotationMode::AddPoint;
        if (pointActive) ImGui::PopStyleColor();

        ImGui::SameLine();

        bool boundsActive =
            (mode == AnnotationMode::AddBounds);
        if (boundsActive)
            ImGui::PushStyleColor(
                ImGuiCol_Button,
                ImVec4(0.2f, 0.8f, 0.2f, 0.7f));
        if (ImGui::Button("+ Bounds"))
        {
            mode =
                boundsActive
                    ? AnnotationMode::None
                    : AnnotationMode::AddBounds;
            if (boundsActive)
                removePointsOutsideBounds();
        }
        if (boundsActive) ImGui::PopStyleColor();

        ImGui::SameLine();

        if (ImGui::Button("Undo"))
        {
            const char* key = nullptr;
            if (mode == AnnotationMode::AddPoint)
                key = "points";
            else if (mode ==
                     AnnotationMode::AddBounds)
                key = "bounds";
            if (key &&
                image->annotations.contains(key) &&
                image->annotations[key].isArray())
            {
                auto& arr =
                    image->annotations[key]
                        .getArray();
                if (!arr.empty()) arr.pop_back();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear"))
        {
            image->annotations["bounds"]
                .setArray();
            image->annotations["points"]
                .setArray();
        }

        ImGui::PopID();
    }

}  // namespace shoecomp
