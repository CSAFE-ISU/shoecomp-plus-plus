#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"
#include "formats/png.h"
#include <algorithm>
#include <cmath>

namespace shoecomp
{
    AnnotationStyle g_annotationStyle;

    const char* pointTypeToString(PointType t)
    {
        switch (t)
        {
            case PointType::Center:
                return "Center";
            case PointType::Corner:
            default:
                return "Corner";
        }
    }

    PointType stringToPointType(const std::string& s)
    {
        if (s == "Center") return PointType::Center;
        return PointType::Corner;
    }

    ImageData::~ImageData()
    {
        if (textureId) freeTexture(textureId);
    }

    ImageCanvas::ImageCanvas() : image(std::make_shared<ImageData>()) {}

    ImageCanvas::ImageCanvas(std::shared_ptr<ImageData> img)
        : image(std::move(img))
    {
    }

    ImVec2 ImageCanvas::screenToImageCoord(ImVec2 sp, ImVec2 canvasPos,
                                           ImVec2 avail, ImVec2 pan,
                                           float zoom, float baseScale,
                                           float rotation, int imgW,
                                           int imgH)
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

    ImVec2 ImageCanvas::imageToScreenCoord(float ix, float iy, float cx,
                                           float cy, float scale,
                                           float cosR, float sinR,
                                           int imgW, int imgH)
    {
        float lx = (ix - imgW * 0.5f) * scale;
        float ly = (iy - imgH * 0.5f) * scale;
        return ImVec2(cx + lx * cosR - ly * sinR,
                      cy + lx * sinR + ly * cosR);
    }

    bool ImageCanvas::pointInPolygon(float px, float py,
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

    void ImageCanvas::removePointsOutsideBounds()
    {
        if (!hasAnnotationArray(image->annotations, "bounds")) return;
        auto& bnd = image->annotations["bounds"].getArray();
        if (bnd.size() < 3) return;
        if (!hasAnnotationArray(image->annotations, "points")) return;
        auto& pts = image->annotations["points"].getArray();
        pts.erase(std::remove_if(pts.begin(), pts.end(),
                                 [&bnd](jt::Json& p)
                                 {
                                     return !pointInPolygon(
                                         p["x"].getFloat(),
                                         p["y"].getFloat(), bnd);
                                 }),
                  pts.end());
    }

    // --- Static helpers for renderCanvas ---

    static void drawDashedLine(ImDrawList* dl, ImVec2 p0, ImVec2 p1,
                               float dashLen, float gapLen)
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
            dl->AddLine(ImVec2(p0.x + nx * t, p0.y + ny * t),
                        ImVec2(p0.x + nx * t1, p0.y + ny * t1),
                        IM_COL32(50, 255, 50, 120), 1.5f);
            t = t1 + gapLen;
        }
    }

    static void renderScrollbar(
        ImDrawList* dl, bool isVertical, float dispSize,
        float availSize, float limSize, float panVal,
        float panTargetVal, ImVec2 canvasPos, ImVec2 avail, ImGuiIO& io,
        float barThick, float barPad, ImU32 barCol, ImU32 barColHov,
        ImageViewState& vs, ImageViewState* linked)
    {
        if (dispSize <= availSize) return;
        float viewRatio = availSize / dispSize;
        float barLen = availSize * viewRatio;
        float t = (limSize - panVal) / (2.0f * limSize);
        float barX, barY;
        ImVec2 bMin, bMax;
        if (isVertical)
        {
            barX = canvasPos.x + avail.x - barThick - barPad;
            barY = canvasPos.y + t * (availSize - barLen);
            bMin = ImVec2(barX, barY);
            bMax = ImVec2(barX + barThick, barY + barLen);
        }
        else
        {
            barX = canvasPos.x + t * (availSize - barLen);
            barY = canvasPos.y + avail.y - barThick - barPad;
            bMin = ImVec2(barX, barY);
            bMax = ImVec2(barX + barLen, barY + barThick);
        }
        ImVec2 mpos = io.MousePos;
        bool barHov = mpos.x >= bMin.x && mpos.x <= bMax.x &&
                      mpos.y >= bMin.y && mpos.y <= bMax.y;
        bool barDrag =
            barHov && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        if (barDrag)
        {
            float delta =
                isVertical ? io.MouseDelta.y : io.MouseDelta.x;
            float d = -delta / (availSize - barLen) * (2.0f * limSize);
            if (isVertical)
            {
                vs.pan.y += d;
                vs.panTarget.y += d;
                if (linked)
                {
                    linked->pan.y += d;
                    linked->panTarget.y += d;
                }
            }
            else
            {
                vs.pan.x += d;
                vs.panTarget.x += d;
                if (linked)
                {
                    linked->pan.x += d;
                    linked->panTarget.x += d;
                }
            }
        }
        dl->AddRectFilled(bMin, bMax,
                          barDrag || barHov ? barColHov : barCol,
                          barThick * 0.5f);
    }

    static void renderBoundsDimming(ImDrawList* dl,
                                    jt::Json& annotations, float cx,
                                    float cy, float annScale,
                                    float cosR, float sinR, int imgW,
                                    int imgH, ImVec2 canvasPos,
                                    ImVec2 avail, ImVec2 tl, ImVec2 tr,
                                    ImVec2 br, ImVec2 bl)
    {
        auto& bnd = annotations["bounds"].getArray();
        std::vector<ImVec2> screenBnd;
        screenBnd.reserve(bnd.size());
        for (auto& v : bnd)
        {
            screenBnd.push_back(ImageCanvas::imageToScreenCoord(
                v["x"].getFloat(), v["y"].getFloat(), cx, cy, annScale,
                cosR, sinR, imgW, imgH));
        }
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
        ImVec2 imgCorners[4] = {tl, tr, br, bl};
        for (auto& ic : imgCorners)
        {
            oMinX = std::min(oMinX, ic.x);
            oMinY = std::min(oMinY, ic.y);
            oMaxX = std::max(oMaxX, ic.x);
            oMaxY = std::max(oMaxY, ic.y);
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
        int maxXIdx = 0;
        for (int si = 1; si < (int)screenBnd.size(); ++si)
        {
            if (screenBnd[si].x > screenBnd[maxXIdx].x) maxXIdx = si;
        }
        ImVec2 bridge(oMaxX, screenBnd[maxXIdx].y);
        int n = (int)screenBnd.size();
        float signedArea = 0.0f;
        for (int si = 0; si < n; ++si)
        {
            auto& a2 = screenBnd[si];
            auto& b2 = screenBnd[(si + 1) % n];
            signedArea += (b2.x - a2.x) * (b2.y + a2.y);
        }
        bool isCW = signedArea < 0.0f;
        std::vector<ImVec2> frame;
        frame.reserve(n + 8);
        frame.push_back(cTL);
        frame.push_back(cTR);
        frame.push_back(bridge);
        for (int si = 0; si <= n; ++si)
        {
            int idx =
                isCW ? (maxXIdx - si + n) % n : (maxXIdx + si) % n;
            frame.push_back(screenBnd[idx]);
        }
        frame.push_back(bridge);
        frame.push_back(cBR);
        frame.push_back(cBL);
        dl->AddConcavePolyFilled(frame.data(), (int)frame.size(),
                                 IM_COL32(0, 0, 0, 120));
    }

    void ImageCanvas::renderAnnotations(ImDrawList* dl, float cx,
                                        float cy, float annScale,
                                        float cosR, float sinR,
                                        bool hovered,
                                        AnnotationMode mode,
                                        const ImGuiIO& io)
    {
        if (!image->annotations.isObject()) return;

        int imgW = image->width;
        int imgH = image->height;

        if (hasAnnotationArray(image->annotations, "points"))
        {
            auto& pts = image->annotations["points"].getArray();
            for (auto& p : pts)
            {
                ImVec2 sp = imageToScreenCoord(
                    p["x"].getFloat(), p["y"].getFloat(), cx, cy,
                    annScale, cosR, sinR, imgW, imgH);
                PointType pType = PointType::Corner;
                if (p.contains("type") && p["type"].isString())
                {
                    pType = stringToPointType(p["type"].getString());
                }
                if (pType == PointType::Center)
                {
                    ImU32 cCol = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_annotationStyle.centerColor[0],
                               g_annotationStyle.centerColor[1],
                               g_annotationStyle.centerColor[2],
                               g_annotationStyle.centerColor[3]));
                    dl->AddCircleFilled(
                        sp, g_annotationStyle.pointRadius, cCol);
                    dl->AddCircle(sp, g_annotationStyle.pointRadius,
                                  IM_COL32(255, 255, 255, 200), 12,
                                  1.5f);
                }
                else
                {
                    ImU32 cCol = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(g_annotationStyle.cornerColor[0],
                               g_annotationStyle.cornerColor[1],
                               g_annotationStyle.cornerColor[2],
                               g_annotationStyle.cornerColor[3]));
                    float d = g_annotationStyle.pointRadius;
                    ImVec2 top(sp.x, sp.y - d);
                    ImVec2 right(sp.x + d, sp.y);
                    ImVec2 bot(sp.x, sp.y + d);
                    ImVec2 left(sp.x - d, sp.y);
                    ImVec2 diamond[4] = {top, right, bot, left};
                    dl->AddConvexPolyFilled(diamond, 4, cCol);
                    dl->AddPolyline(diamond, 4,
                                    IM_COL32(255, 255, 255, 200),
                                    ImDrawFlags_Closed, 1.5f);
                }
            }
        }
        if (hasAnnotationArray(image->annotations, "bounds"))
        {
            auto& bnd = image->annotations["bounds"].getArray();
            bool editing = (mode == AnnotationMode::AddBounds);
            ImU32 bndCol = ImGui::ColorConvertFloat4ToU32(
                ImVec4(g_annotationStyle.boundsColor[0],
                       g_annotationStyle.boundsColor[1],
                       g_annotationStyle.boundsColor[2],
                       g_annotationStyle.boundsColor[3]));
            if (bnd.size() >= 2)
            {
                for (size_t i = 0; i + 1 < bnd.size(); ++i)
                {
                    ImVec2 a = imageToScreenCoord(
                        bnd[i]["x"].getFloat(), bnd[i]["y"].getFloat(),
                        cx, cy, annScale, cosR, sinR, imgW, imgH);
                    ImVec2 b = imageToScreenCoord(
                        bnd[i + 1]["x"].getFloat(),
                        bnd[i + 1]["y"].getFloat(), cx, cy, annScale,
                        cosR, sinR, imgW, imgH);
                    dl->AddLine(a, b, bndCol,
                                g_annotationStyle.boundsLineThickness);
                }
            }
            if (bnd.size() >= 2)
            {
                ImVec2 last = imageToScreenCoord(
                    bnd.back()["x"].getFloat(),
                    bnd.back()["y"].getFloat(), cx, cy, annScale, cosR,
                    sinR, imgW, imgH);
                ImVec2 first = imageToScreenCoord(
                    bnd[0]["x"].getFloat(), bnd[0]["y"].getFloat(), cx,
                    cy, annScale, cosR, sinR, imgW, imgH);
                if (!editing)
                {
                    dl->AddLine(last, first, bndCol,
                                g_annotationStyle.boundsLineThickness);
                }
                else
                {
                    bool shift = io.KeyShift;
                    if (hovered && !shift)
                    {
                        drawDashedLine(dl, last, io.MousePos, 8.0f,
                                       6.0f);
                    }
                    else if (!shift)
                    {
                        drawDashedLine(dl, last, first, 8.0f, 6.0f);
                    }
                }
            }
            for (auto& v : bnd)
            {
                ImVec2 sp = imageToScreenCoord(
                    v["x"].getFloat(), v["y"].getFloat(), cx, cy,
                    annScale, cosR, sinR, imgW, imgH);
                dl->AddCircleFilled(
                    sp, g_annotationStyle.pointRadius - 1.0f, bndCol);
            }
        }
    }

    void ImageCanvas::renderCanvas(const char* canvasId,
                                   ImageViewState* linked)
    {
        AnnotationMode& mode = image->annotationMode;
        ImVec2 avail = ImGui::GetContentRegionAvail();

        float scaleX = avail.x / (float)image->width;
        float scaleY = avail.y / (float)image->height;
        float baseScale = std::min(scaleX, scaleY);
        if (viewState.zoomTarget <= 0.0f)
            viewState.zoomTarget = scaleX / baseScale;
        if (viewState.zoom <= 0.0f)
            viewState.zoom = viewState.zoomTarget;

        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(canvasId, avail,
                               ImGuiButtonFlags_MouseButtonLeft);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        ImGuiIO& io = ImGui::GetIO();

        if (hovered && io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            float oldTarget = viewState.zoomTarget;
            viewState.zoomTarget *= (io.MouseWheel > 0) ? 1.15f : 0.87f;
            viewState.zoomTarget =
                std::clamp(viewState.zoomTarget, 0.1f, 50.0f);
            ImVec2 mouse = ImVec2(io.MousePos.x - canvasPos.x,
                                  io.MousePos.y - canvasPos.y);
            float ratio = viewState.zoomTarget / oldTarget;
            viewState.panTarget.x =
                (1.0f - ratio) * (mouse.x - avail.x * 0.5f) +
                ratio * viewState.panTarget.x;
            viewState.panTarget.y =
                (1.0f - ratio) * (mouse.y - avail.y * 0.5f) +
                ratio * viewState.panTarget.y;
            if (linked)
            {
                linked->zoomTarget = viewState.zoomTarget;
                linked->panTarget = viewState.panTarget;
            }
        }

        if (hovered && !io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            viewState.panTarget.y += io.MouseWheel * 30.0f;
            if (linked) linked->panTarget.y = viewState.panTarget.y;
        }

        if (active && io.KeyCtrl &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            viewState.panTarget.x += io.MouseDelta.x;
            viewState.panTarget.y += io.MouseDelta.y;
            viewState.pan.x += io.MouseDelta.x;
            viewState.pan.y += io.MouseDelta.y;
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
                io.MousePos, canvasPos, avail, viewState.pan,
                viewState.zoom, baseScale, viewState.rotation,
                image->width, image->height);
            const char* key = (mode == AnnotationMode::AddPoint)
                                  ? "points"
                                  : "bounds";

            float screenPx = 10.0f;
            float imgPx = screenPx / (baseScale * viewState.zoom);
            float thresh2 = imgPx * imgPx;

            if (mode == AnnotationMode::AddBounds && !io.KeyShift &&
                hasAnnotationArray(image->annotations, "bounds") &&
                image->annotations["bounds"].getArray().size() >= 3)
            {
                auto& bnd = image->annotations["bounds"].getArray();
                float dx = bnd[0]["x"].getFloat() - ic.x;
                float dy = bnd[0]["y"].getFloat() - ic.y;
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
                if (hasAnnotationArray(image->annotations, key))
                {
                    auto& arr = image->annotations[key].getArray();
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
                    hasAnnotationArray(image->annotations, "bounds") &&
                    image->annotations["bounds"].getArray().size() >= 3)
                {
                    allow = pointInPolygon(
                        ic.x, ic.y,
                        image->annotations["bounds"].getArray());
                }
                if (allow)
                {
                    jt::Json pt;
                    pt.setObject();
                    pt["x"] = ic.x;
                    pt["y"] = ic.y;
                    if (mode == AnnotationMode::AddPoint)
                    {
                        pt["type"] =
                            pointTypeToString(image->selectedPointType);
                    }
                    image->annotations[key].getArray().push_back(
                        std::move(pt));
                }
            }
        }

        float speed = 12.0f * io.DeltaTime;
        speed = std::clamp(speed, 0.0f, 1.0f);
        viewState.zoom +=
            (viewState.zoomTarget - viewState.zoom) * speed;
        viewState.pan.x +=
            (viewState.panTarget.x - viewState.pan.x) * speed;
        viewState.pan.y +=
            (viewState.panTarget.y - viewState.pan.y) * speed;
        viewState.rotation +=
            (viewState.rotationTarget - viewState.rotation) * speed;

        float dispW = image->width * baseScale * viewState.zoom;
        float dispH = image->height * baseScale * viewState.zoom;

        float limX = std::max(avail.x, dispW) * 0.5f;
        float limY = std::max(avail.y, dispH) * 0.5f;
        viewState.panTarget.x =
            std::clamp(viewState.panTarget.x, -limX, limX);
        viewState.panTarget.y =
            std::clamp(viewState.panTarget.y, -limY, limY);
        viewState.pan.x = std::clamp(viewState.pan.x, -limX, limX);
        viewState.pan.y = std::clamp(viewState.pan.y, -limY, limY);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(
            canvasPos,
            ImVec2(canvasPos.x + avail.x, canvasPos.y + avail.y), true);
        float cx = canvasPos.x + avail.x * 0.5f + viewState.pan.x;
        float cy = canvasPos.y + avail.y * 0.5f + viewState.pan.y;
        float hw = dispW * 0.5f;
        float hh = dispH * 0.5f;
        float cosR = cosf(viewState.rotation);
        float sinR = sinf(viewState.rotation);
        auto rot = [&](float lx, float ly) -> ImVec2
        {
            return ImVec2(cx + lx * cosR - ly * sinR,
                          cy + lx * sinR + ly * cosR);
        };
        ImVec2 tl = rot(-hw, -hh);
        ImVec2 tr = rot(hw, -hh);
        ImVec2 br = rot(hw, hh);
        ImVec2 bl = rot(-hw, hh);
        dl->AddImageQuad(image->textureId, tl, tr, br, bl, ImVec2(0, 0),
                         ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1));
        dl->AddQuad(tl, tr, br, bl, IM_COL32(180, 180, 180, 200));

        // Dim area outside bounds when not editing
        float annScale = baseScale * viewState.zoom;
        if (mode != AnnotationMode::AddBounds &&
            hasAnnotationArray(image->annotations, "bounds") &&
            image->annotations["bounds"].getArray().size() >= 3)
        {
            renderBoundsDimming(dl, image->annotations, cx, cy,
                                annScale, cosR, sinR, image->width,
                                image->height, canvasPos, avail, tl, tr,
                                br, bl);
        }

        // Annotation overlays
        renderAnnotations(dl, cx, cy, annScale, cosR, sinR, hovered,
                          mode, io);

        // Scrollbars
        float barThick = 8.0f;
        float barPad = 2.0f;
        ImU32 barCol = IM_COL32(200, 200, 200, 100);
        ImU32 barColHov = IM_COL32(200, 200, 200, 180);
        renderScrollbar(dl, false, dispW, avail.x, limX,
                        viewState.pan.x, viewState.panTarget.x,
                        canvasPos, avail, io, barThick, barPad, barCol,
                        barColHov, viewState, linked);
        renderScrollbar(dl, true, dispH, avail.y, limY, viewState.pan.y,
                        viewState.panTarget.y, canvasPos, avail, io,
                        barThick, barPad, barCol, barColHov, viewState,
                        linked);

        viewState.panTarget.x =
            std::clamp(viewState.panTarget.x, -limX, limX);
        viewState.panTarget.y =
            std::clamp(viewState.panTarget.y, -limY, limY);
        viewState.pan.x = std::clamp(viewState.pan.x, -limX, limX);
        viewState.pan.y = std::clamp(viewState.pan.y, -limY, limY);

        dl->PopClipRect();
    }

    void ImageCanvas::renderToolbar(const char* toolbarId,
                                    ImageViewState* linked)
    {
        AnnotationMode& mode = image->annotationMode;
        ImGui::PushID(toolbarId);

        float frameH = ImGui::GetFrameHeight();

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

        if (ImGui::Button("Zoom+"))
        {
            viewState.zoomTarget =
                std::clamp(viewState.zoomTarget * 1.25f, 0.1f, 50.0f);
            if (linked) linked->zoomTarget = viewState.zoomTarget;
        }
        ImGui::SameLine();

        if (ImGui::Button("Zoom-"))
        {
            viewState.zoomTarget =
                std::clamp(viewState.zoomTarget * 0.8f, 0.1f, 50.0f);
            if (linked) linked->zoomTarget = viewState.zoomTarget;
        }
        ImGui::SameLine();

        float dialR = frameH * 0.5f;
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 center(cursor.x + dialR, cursor.y + dialR);
        ImGui::InvisibleButton("##dial",
                               ImVec2(dialR * 2.0f, dialR * 2.0f));
        bool dialHov = ImGui::IsItemHovered();
        bool dialAct = ImGui::IsItemActive();

        if (dialAct)
        {
            ImGuiIO& dio = ImGui::GetIO();
            float angle = atan2f(dio.MousePos.y - center.y,
                                 dio.MousePos.x - center.x);
            viewState.rotationTarget = angle;
            viewState.rotation = angle;
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
        float indX = center.x + cosf(viewState.rotation) * dialR;
        float indY = center.y + sinf(viewState.rotation) * dialR;
        dl->AddLine(center, ImVec2(indX, indY),
                    IM_COL32(255, 180, 50, 255), 2.0f);
        dl->AddCircleFilled(ImVec2(indX, indY), 3.0f,
                            IM_COL32(255, 180, 50, 255));

        ImGui::SameLine();
        ImGui::Text("%.0f deg",
                    viewState.rotation * 180.0f / 3.14159265f);

        bool pointActive = (mode == AnnotationMode::AddPoint);
        if (pointActive)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.8f, 0.2f, 0.2f, 0.7f));
        if (ImGui::Button("+ Point"))
            mode = pointActive ? AnnotationMode::None
                               : AnnotationMode::AddPoint;
        if (pointActive) ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::BeginDisabled(!pointActive);
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("Corner__").x);
        int cur = static_cast<int>(image->selectedPointType);
        const char* items[] = {"Corner", "Center"};
        if (ImGui::Combo("##ptType", &cur, items, 2))
        {
            image->selectedPointType = static_cast<PointType>(cur);
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        bool boundsActive = (mode == AnnotationMode::AddBounds);
        if (boundsActive)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.2f, 0.8f, 0.2f, 0.7f));
        if (ImGui::Button("+ Bounds"))
        {
            mode = boundsActive ? AnnotationMode::None
                                : AnnotationMode::AddBounds;
            if (boundsActive) removePointsOutsideBounds();
        }
        if (boundsActive) ImGui::PopStyleColor();

        if (ImGui::Button("Undo"))
        {
            const char* key = nullptr;
            if (mode == AnnotationMode::AddPoint)
                key = "points";
            else if (mode == AnnotationMode::AddBounds)
                key = "bounds";
            if (key && hasAnnotationArray(image->annotations, key))
            {
                auto& arr = image->annotations[key].getArray();
                if (!arr.empty()) arr.pop_back();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear"))
        {
            image->annotations["bounds"].setArray();
            image->annotations["points"].setArray();
        }

        ImGui::PopID();
    }

}  // namespace shoecomp
