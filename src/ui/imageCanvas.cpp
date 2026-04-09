#include "ui/imageCanvas.h"
#include "ui/uiHelpers.h"
#include "formats/png.h"

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

    bool ImageViewState::contains(const ImVec2& pt) const
    {
        return ((pt.x >= this->canvasPos.x) &&
                (pt.x < (this->canvasPos.x + this->canvasSize.x)) &&
                (pt.y >= this->canvasPos.y) &&
                (pt.y < (this->canvasPos.y + this->canvasSize.y)));
    }

    ImVec2 ImageViewState::canvasCenter() const
    {
        ImVec2 result(canvasPos.x + canvasSize.x * 0.5f,
                      canvasPos.y + canvasSize.y * 0.5f);
        return result;
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
        ImVec2 c = canvasPos + (avail * 0.5f) + pan;
        ImVec2 d = sp - c;
        float cosR = cosf(-rotation);
        float sinR = sinf(-rotation);
        float lx = d.x * cosR - d.y * sinR;
        float ly = d.x * sinR + d.y * cosR;
        float scale = baseScale * zoom;
        return ImVec2(lx / scale + imgW * 0.5f,
                      ly / scale + imgH * 0.5f);
    }

    ImVec2 ImageCanvas::getImageCoord(ImVec2 sp) const
    {
        return ImageCanvas::screenToImageCoord(
            sp, viewState.canvasPos, viewState.canvasSize,
            viewState.panTarget, viewState.zoomTarget,
            viewState.baseScale, viewState.rotationTarget, image->width,
            image->height);
    }

    ImVec2 ImageCanvas::imageToScreenCoord(float ix, float iy, float cx,
                                           float cy, float scale,
                                           float rotation, int imgW,
                                           int imgH)
    {
        float cosR = cosf(rotation);
        float sinR = sinf(rotation);
        float lx = (ix - imgW * 0.5f) * scale;
        float ly = (iy - imgH * 0.5f) * scale;
        return ImVec2(cx + lx * cosR - ly * sinR,
                      cy + lx * sinR + ly * cosR);
    }

    bool ImageCanvas::pointInPolygon(float px, float py,
                                     const std::vector<jt::Json>& poly)
    {
        bool inside = false;
        int n = (int)poly.size();
        for (int i = 0, j = n - 1; i < n; j = i++)
        {
            float xi = poly[i]["x"].getNumber();
            float yi = poly[i]["y"].getNumber();
            float xj = poly[j]["x"].getNumber();
            float yj = poly[j]["y"].getNumber();
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
                                         p["x"].getNumber(),
                                         p["y"].getNumber(), bnd);
                                 }),
                  pts.end());
    }

    // --- Static helpers for renderCanvas ---

    static void drawDashedLine(ImDrawList* dl, ImVec2 p0, ImVec2 p1,
                               float dashLen, float gapLen)
    {
        ImVec2 d = p1 - p0;
        float len = length(d);

        if (len < 1.0f) return;
        ImVec2 nd = d / len;
        float t = 0.0f;
        while (t < len)
        {
            float t1 = std::min(t + dashLen, len);
            dl->AddLine(p0 + nd * t, p0 + nd * t1,
                        IM_COL32(50, 255, 50, 120), 1.5f);
            t = t1 + gapLen;
        }
    }

    static void renderScrollbar(
        ImDrawList* dl, bool isVertical, float dispSize,
        float availSize, float limSize, float panVal,
        float panTargetVal, ImVec2 canvasPos, ImVec2 avail, ImGuiIO& io,
        float barThick, float barPad, ImU32 barCol, ImU32 barColHov,
        ImageViewState& vs)
    {
        if (dispSize <= availSize) return;
        float viewRatio = availSize / dispSize;
        float barLen = availSize * viewRatio;
        float t = (limSize - panVal) / (2.0f * limSize);
        ImVec2 bar;
        ImVec2 bMin, bMax;
        if (isVertical)
        {
            bar = ImVec2(canvasPos.x + avail.x - barThick - barPad,
                         canvasPos.y + t * (availSize - barLen));
        }
        else
        {
            bar = ImVec2(canvasPos.x + t * (availSize - barLen),
                         canvasPos.y + avail.y - barThick - barPad);
        }
        bMin = bar;
        bMax = ImVec2(bar.x + barThick, bar.y + barLen);
        //
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
            }
            else
            {
                vs.pan.x += d;
                vs.panTarget.x += d;
            }
        }
        dl->AddRectFilled(bMin, bMax,
                          barDrag || barHov ? barColHov : barCol,
                          barThick * 0.5f);
    }

    static void renderBoundsDimming(ImDrawList* dl,
                                    jt::Json& annotations, float cx,
                                    float cy, float annScale,
                                    float rotation, int imgW, int imgH,
                                    ImVec2 canvasPos, ImVec2 avail,
                                    ImVec2 tl, ImVec2 tr, ImVec2 br,
                                    ImVec2 bl)
    {
        ImVec2 oMin = canvasPos;
        ImVec2 oMax = canvasPos + avail;
        ImVec2 imgCorners[4] = {tl, tr, br, bl};
        float pad = 10.0f;
        for (auto& ic : imgCorners)
        {
            oMin = min(oMin, ic);
            oMax = max(oMax, ic);
        }
        //
        auto& bnd = annotations["bounds"].getArray();
        std::vector<ImVec2> screenBnd;
        screenBnd.reserve(bnd.size());
        for (auto& v : bnd)
        {
            ImVec2 sp = ImageCanvas::imageToScreenCoord(
                v["x"].getNumber(), v["y"].getNumber(), cx, cy, annScale,
                rotation, imgW, imgH);
            screenBnd.push_back(sp);
            oMin = min(oMin, sp);
            oMax = max(oMax, sp);
        }
        //
        oMin.x -= pad;
        oMin.y -= pad;
        oMax.x += pad;
        oMax.y += pad;
        ImVec2 cTL(oMin.x, oMin.y);
        ImVec2 cTR(oMax.x, oMin.y);
        ImVec2 cBR(oMax.x, oMax.y);
        ImVec2 cBL(oMin.x, oMax.y);
        int maxXIdx = 0;
        for (int si = 1; si < (int)screenBnd.size(); ++si)
        {
            if (screenBnd[si].x > screenBnd[maxXIdx].x) maxXIdx = si;
        }
        ImVec2 bridge(oMax.x, screenBnd[maxXIdx].y);
        //
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
                                        float rotation, bool hovered,
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
                    p["x"].getNumber(), p["y"].getNumber(), cx, cy,
                    annScale, rotation, imgW, imgH);
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
                        bnd[i]["x"].getNumber(), bnd[i]["y"].getNumber(),
                        cx, cy, annScale, rotation, imgW, imgH);
                    ImVec2 b = imageToScreenCoord(
                        bnd[i + 1]["x"].getNumber(),
                        bnd[i + 1]["y"].getNumber(), cx, cy, annScale,
                        rotation, imgW, imgH);
                    dl->AddLine(a, b, bndCol,
                                g_annotationStyle.boundsLineThickness);
                }
            }
            if (bnd.size() >= 2)
            {
                ImVec2 last = imageToScreenCoord(
                    bnd.back()["x"].getNumber(),
                    bnd.back()["y"].getNumber(), cx, cy, annScale,
                    rotation, imgW, imgH);
                ImVec2 first = imageToScreenCoord(
                    bnd[0]["x"].getNumber(), bnd[0]["y"].getNumber(), cx,
                    cy, annScale, rotation, imgW, imgH);
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
                    v["x"].getNumber(), v["y"].getNumber(), cx, cy,
                    annScale, rotation, imgW, imgH);
                dl->AddCircleFilled(
                    sp, g_annotationStyle.pointRadius - 1.0f, bndCol);
            }
        }
    }

    static ImVec2 getRotatedVec2(const ImVec2& c, float rotation,
                                 float lx, float ly)
    {
        return ImVec2(c.x + lx * cosf(rotation) - ly * sinf(rotation),
                      c.y + lx * sinf(rotation) + ly * cosf(rotation));
    }

    void ImageCanvas::renderCanvas(const char* canvasId)
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

        // Store for locked indicators
        viewState.canvasPos = canvasPos;
        viewState.canvasSize = avail;
        viewState.baseScale = baseScale;

        ImGui::InvisibleButton(canvasId, avail,
                               ImGuiButtonFlags_MouseButtonLeft);
        bool hovered = ImGui::IsItemHovered();
        viewState.isHovered = hovered;  // Store hover state
        bool active = ImGui::IsItemActive();
        ImGuiIO& io = ImGui::GetIO();

        if (hovered && io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            float oldTarget = viewState.zoomTarget;
            viewState.zoomTarget *= (io.MouseWheel > 0) ? 1.15f : 0.87f;
            viewState.zoomTarget =
                std::clamp(viewState.zoomTarget, 0.1f, 50.0f);
            ImVec2 mouse = io.MousePos - canvasPos;
            float ratio = viewState.zoomTarget / oldTarget;
            viewState.panTarget =
                (1.0f - ratio) * (mouse - avail * 0.5f) +
                ratio * viewState.panTarget;
        }

        if (hovered && !io.KeyCtrl && io.MouseWheel != 0.0f)
        {
            viewState.panTarget.y += io.MouseWheel * 30.0f;
        }

        if (active && io.KeyCtrl &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            viewState.panTarget += io.MouseDelta;
            viewState.pan += io.MouseDelta;
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
                float dx = bnd[0]["x"].getNumber() - ic.x;
                float dy = bnd[0]["y"].getNumber() - ic.y;
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
                        float dx = arr[ai]["x"].getNumber() - ic.x;
                        float dy = arr[ai]["y"].getNumber() - ic.y;
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
                    pt["x"] = (float)ic.x;
                    pt["y"] = (float)ic.y;
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
        viewState.pan += (viewState.panTarget - viewState.pan) * speed;
        viewState.rotation +=
            (viewState.rotationTarget - viewState.rotation) * speed;

        ImVec2 disp(image->width * baseScale * viewState.zoom,
                    image->height * baseScale * viewState.zoom);

        ImVec2 lim = max(avail, disp) * 0.5f;

        viewState.panTarget = clamp(viewState.panTarget, lim);
        viewState.pan = clamp(viewState.pan, lim);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(canvasPos, canvasPos + avail, true);

        ImVec2 c = canvasPos + (0.5f * avail) + viewState.pan;

        //
        float hw = disp.x * 0.5f;
        float hh = disp.y * 0.5f;
        ImVec2 tl = getRotatedVec2(c, viewState.rotation, -hw, -hh);
        ImVec2 tr = getRotatedVec2(c, viewState.rotation, hw, -hh);
        ImVec2 br = getRotatedVec2(c, viewState.rotation, hw, hh);
        ImVec2 bl = getRotatedVec2(c, viewState.rotation, -hw, hh);
        dl->AddImageQuad(image->textureId, tl, tr, br, bl, ImVec2(0, 0),
                         ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1));
        dl->AddQuad(tl, tr, br, bl, IM_COL32(180, 180, 180, 200));

        // Dim area outside bounds when not editing
        float annScale = baseScale * viewState.zoom;
        viewState.renderScale = annScale;

        if (mode != AnnotationMode::AddBounds &&
            hasAnnotationArray(image->annotations, "bounds") &&
            image->annotations["bounds"].getArray().size() >= 3)
        {
            renderBoundsDimming(dl, image->annotations, c.x, c.y,
                                annScale, viewState.rotation,
                                image->width, image->height, canvasPos,
                                avail, tl, tr, br, bl);
        }

        // Annotation overlays
        renderAnnotations(dl, c.x, c.y, annScale, viewState.rotation,
                          hovered, mode, io);

        // Scrollbars
        float barThick = 8.0f;
        float barPad = 2.0f;
        ImU32 barCol = IM_COL32(200, 200, 200, 100);
        ImU32 barColHov = IM_COL32(200, 200, 200, 180);
        renderScrollbar(dl, false, disp.x, avail.x, lim.x,
                        viewState.pan.x, viewState.panTarget.x,
                        canvasPos, avail, io, barThick, barPad, barCol,
                        barColHov, viewState);
        renderScrollbar(dl, true, disp.y, avail.y, lim.y,
                        viewState.pan.y, viewState.panTarget.y,
                        canvasPos, avail, io, barThick, barPad, barCol,
                        barColHov, viewState);

        viewState.panTarget = clamp(viewState.panTarget, lim);
        viewState.pan = clamp(viewState.pan, lim);

        dl->PopClipRect();
    }

    void ImageCanvas::renderToolbar(const char* toolbarId)
    {
        AnnotationMode& mode = image->annotationMode;
        ImGui::PushID(toolbarId);

        float frameH = ImGui::GetFrameHeight();

        if (ImGui::Button("Home"))
        {
            viewState.zoomTarget = 1.0f;
            viewState.panTarget = ImVec2(0, 0);
            viewState.rotationTarget = 0.0f;
            viewState.homeRequested = true;
        }
        ImGui::SameLine();

        if (ImGui::Button("Zoom+"))
        {
            viewState.zoomTarget =
                std::clamp(viewState.zoomTarget * 1.25f, 0.1f, 50.0f);
        }
        ImGui::SameLine();

        if (ImGui::Button("Zoom-"))
        {
            viewState.zoomTarget =
                std::clamp(viewState.zoomTarget * 0.8f, 0.1f, 50.0f);
        }
        ImGui::SameLine();

        float dialR = frameH * 0.5f;
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 dialCenter(cursor.x + dialR, cursor.y + dialR);
        ImGui::InvisibleButton("##dial",
                               ImVec2(dialR * 2.0f, dialR * 2.0f));
        bool dialHov = ImGui::IsItemHovered();
        bool dialAct = ImGui::IsItemActive();

        if (dialAct)
        {
            ImGuiIO& dio = ImGui::GetIO();
            float angle = atan2f(dio.MousePos.y - dialCenter.y,
                                 dio.MousePos.x - dialCenter.x);
            viewState.rotationTarget = angle;
            viewState.rotation = angle;
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 ringCol = dialHov || dialAct
                            ? IM_COL32(200, 200, 200, 255)
                            : IM_COL32(150, 150, 150, 200);
        dl->AddCircle(dialCenter, dialR, ringCol, 24, 2.0f);

        ImVec2 ind = dialCenter + (direction(viewState.rotation) * dialR);
        dl->AddLine(dialCenter, ind, IM_COL32(255, 180, 50, 255), 2.0f);
        dl->AddCircleFilled(ind, 3.0f, IM_COL32(255, 180, 50, 255));

        ImGui::SameLine();
        float degs = viewState.rotationTarget / kDegToRad;
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("-360.0___").x);
        if (ImGui::InputFloat("deg", &degs, 0.0f, 0.0f, "%.1f",
                              ImGuiInputTextFlags_EnterReturnsTrue))
        {
            viewState.rotationTarget = degs * kDegToRad;
            viewState.rotation = viewState.rotationTarget;
        }

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
