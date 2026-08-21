#include "ui/imageCanvas2d.h"
#include "ui/uiHelpers.h"
#include "ui/calcHelpers.h"
#include "formats/png.h"
#include "formats/annotationIo.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace shoecomp
{
    ImageCanvas2D::AnnotationStyle ImageCanvas2D::style;
    ImageCanvas2D::AnnotationMode ImageCanvas2D::annotationMode =
        ImageCanvas2D::AnnotationMode::None;
    ImageCanvas2D::PointType ImageCanvas2D::selectedPointType =
        ImageCanvas2D::PointType::Corner;

    const std::vector<std::string>& ImageCanvas2D::imageExtensions()
        const
    {
        // Plain 2D canvas accepts PNG only. Subclasses widen this.
        static const std::vector<std::string> exts = {".png", ".PNG"};
        return exts;
    }

    std::vector<ImageCanvas2D::PointType>
    ImageCanvas2D::allowedPointTypes() const
    {
        return {PointType::Corner,      PointType::Center,
                PointType::RidgeEnding, PointType::Bifurcation,
                PointType::Other,       PointType::Core,
                PointType::Delta,       PointType::WordStart,
                PointType::WordEnd,     PointType::Intersection,
                PointType::CurveTurn};
    }

    const char* ImageCanvas2D::pointTypeToString(PointType t)
    {
        switch (t)
        {
            case PointType::Center:
                return "Center";
            case PointType::RidgeEnding:
                return "RidgeEnding";
            case PointType::Bifurcation:
                return "Bifurcation";
            case PointType::Other:
                return "Other";
            case PointType::Core:
                return "Core";
            case PointType::Delta:
                return "Delta";
            case PointType::WordStart:
                return "WordStart";
            case PointType::WordEnd:
                return "WordEnd";
            case PointType::Intersection:
                return "Intersection";
            case PointType::CurveTurn:
                return "CurveTurn";
            case PointType::Corner:
            default:
                return "Corner";
        }
    }

    bool ImageCanvas2D::ImageViewState::contains(const ImVec2& pt) const
    {
        return ((pt.x >= this->canvasPos.x) &&
                (pt.x < (this->canvasPos.x + this->canvasSize.x)) &&
                (pt.y >= this->canvasPos.y) &&
                (pt.y < (this->canvasPos.y + this->canvasSize.y)));
    }

    ImVec2 ImageCanvas2D::ImageViewState::canvasCenter() const
    {
        ImVec2 result(canvasPos.x + canvasSize.x * 0.5f,
                      canvasPos.y + canvasSize.y * 0.5f);
        return result;
    }

    ImageCanvas2D::PointType ImageCanvas2D::stringToPointType(
        const std::string& s)
    {
        if (s == "Center") return PointType::Center;
        if (s == "RidgeEnding") return PointType::RidgeEnding;
        if (s == "Bifurcation") return PointType::Bifurcation;
        if (s == "Other") return PointType::Other;
        if (s == "Core") return PointType::Core;
        if (s == "Delta") return PointType::Delta;
        if (s == "WordStart") return PointType::WordStart;
        if (s == "WordEnd") return PointType::WordEnd;
        if (s == "Intersection") return PointType::Intersection;
        if (s == "CurveTurn") return PointType::CurveTurn;
        return PointType::Corner;
    }

    ImageCanvas2D::ImageData::~ImageData()
    {
        if (textureId) freeTexture(textureId);
    }

    void ImageCanvas2D::ImageData::resetAnnotations()
    {
        annotations.setObject();
        annotations["bounds"].setArray();
        annotations["points"].setArray();
    }

    ImageCanvas2D::ImageCanvas2D()
        : image(std::make_shared<ImageData>())
    {
    }

    ImageCanvas2D::ImageCanvas2D(std::shared_ptr<ImageData> img)
        : image(std::move(img))
    {
    }

    const std::string& ImageCanvas2D::name() const
    {
        static const std::string empty;
        return image ? image->name : empty;
    }

    const std::string& ImageCanvas2D::path() const
    {
        static const std::string empty;
        return image ? image->path : empty;
    }

    void ImageCanvas2D::resetView()
    {
        viewState.zoom = viewState.zoomTarget = 1.0f;
        viewState.pan = viewState.panTarget = ImVec2(0, 0);
        viewState.rotation = viewState.rotationTarget = 0.0f;
    }

    void ImageCanvas2D::clearHomeRequested()
    {
        viewState.homeRequested = false;
    }

    std::string ImageCanvas2D::lowerExt(const std::string& path)
    {
        auto dot = path.rfind('.');
        if (dot == std::string::npos) return std::string();
        std::string ext = path.substr(dot);
        for (auto& c : ext) c = (char)std::tolower(c);
        return ext;
    }

    void ImageCanvas2D::fillRaster(const std::string& path,
                                   ImTextureID tex, int width,
                                   int height)
    {
        image->name = std::filesystem::path(path).filename().string();
        image->path = path;
        image->textureId = tex;
        image->width = width;
        image->height = height;
        image->resetAnnotations();
    }

    int ImageCanvas2D::loadImages(
        const std::string& path,
        std::vector<std::unique_ptr<ImageCanvas>>& out,
        std::string& err) const
    {
        // Plain 2D canvas: PNG only.
        if (lowerExt(path) != ".png")
        {
            err = "ImageCanvas2D only supports PNG:\n" + path;
            return -1;
        }
        ImTextureID tex = 0;
        int w = 0, h = 0;
        if (!loadPngFromDisk(path, tex, w, h))
        {
            err = "Failed to load image from:\n" + path;
            return -1;
        }
        auto c = std::make_unique<ImageCanvas2D>();
        c->fillRaster(path, tex, w, h);
        out.push_back(std::move(c));
        return 1;
    }

    int ImageCanvas2D::saveAnnotations(const std::string& path) const
    {
        return saveAnnotationsToFile(path, image->annotations);
    }

    int ImageCanvas2D::loadAnnotations(const std::string& path,
                                       std::string& err)
    {
        if (loadAnnotationsFromFile(path, image->annotations) != 0)
        {
            err = "Failed to load annotations from:\n" + path;
            return -1;
        }

        auto allowed = allowedPointTypes();
        for (auto& pt : image->annotations["points"].getArray())
        {
            if (pt.contains("type") && pt["type"].isString())
            {
                PointType t = stringToPointType(pt["type"].getString());
                if (std::find(allowed.begin(), allowed.end(), t) ==
                    allowed.end())
                {
                    err = "Annotation type '" + pt["type"].getString() +
                          "' is not allowed for this canvas "
                          "type.\nFailed to load annotations "
                          "from:\n" +
                          path;
                    image->resetAnnotations();
                    return -1;
                }
            }
        }
        return 0;
    }

    ImVec2 ImageCanvas2D::screenToImageCoord(
        ImVec2 sp, ImVec2 canvasPos, ImVec2 avail, ImVec2 pan,
        float zoom, float baseScale, float rotation, int imgW, int imgH)
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

    ImVec2 ImageCanvas2D::getImageCoord(ImVec2 sp) const
    {
        return ImageCanvas2D::screenToImageCoord(
            sp, viewState.canvasPos, viewState.canvasSize,
            viewState.panTarget, viewState.zoomTarget,
            viewState.baseScale, viewState.rotationTarget, image->width,
            image->height);
    }

    ImVec2 ImageCanvas2D::imageToScreenCoord(float ix, float iy,
                                             float cx, float cy,
                                             float scale,
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

    bool ImageCanvas2D::pointInPolygon(
        float px, float py, const std::vector<jt::Json>& poly)
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

    void ImageCanvas2D::removePointsOutsideBounds()
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
            dl->AddLine(p0 + nd * t, p0 + nd * t1, kColorBoundsEditDash,
                        1.5f);
            t = t1 + gapLen;
        }
    }

    static void renderScrollbar(ImDrawList* dl, bool isVertical,
                                float dispSize, float availSize,
                                float limSize, float panVal,
                                float panTargetVal, ImVec2 canvasPos,
                                ImVec2 avail, ImGuiIO& io,
                                float barThick, float barPad,
                                ImU32 barCol, ImU32 barColHov,
                                ImageCanvas2D::ImageViewState& vs)
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
            ImVec2 sp = ImageCanvas2D::imageToScreenCoord(
                v["x"].getNumber(), v["y"].getNumber(), cx, cy,
                annScale, rotation, imgW, imgH);
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
                                 kColorBoundsDimOverlay);
    }

    void ImageCanvas2D::renderAnnotations(ImDrawList* dl, float cx,
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
                        style.centerColor);
                    dl->AddCircleFilled(sp, style.pointRadius, cCol);
                    dl->AddCircle(sp, style.pointRadius,
                                  kColorPointOutline, 12, 1.5f);
                }
                else if (pType == PointType::RidgeEnding ||
                         pType == PointType::Bifurcation ||
                         pType == PointType::Other)
                {
                    ImVec4 col = pType == PointType::RidgeEnding
                                     ? style.ridgeEndingColor
                                 : pType == PointType::Bifurcation
                                     ? style.bifurcationColor
                                     : style.otherColor;
                    ImU32 cCol = ImGui::ColorConvertFloat4ToU32(col);
                    dl->AddCircleFilled(sp, style.pointRadius, cCol);
                    dl->AddCircle(sp, style.pointRadius,
                                  kColorPointOutline, 12, 1.5f);
                }
                else if (pType == PointType::Core)
                {
                    ImU32 cCol =
                        ImGui::ColorConvertFloat4ToU32(style.coreColor);
                    float r = style.pointRadius;
                    dl->AddCircleFilled(sp, r, cCol);
                    dl->AddCircle(sp, r, kColorPointOutline, 12, 1.5f);
                    dl->AddCircle(sp, r * 1.6f, cCol, 12, 1.5f);
                }
                else if (pType == PointType::Delta)
                {
                    ImU32 cCol = ImGui::ColorConvertFloat4ToU32(
                        style.deltaColor);
                    float d = style.pointRadius;
                    ImVec2 top(sp.x, sp.y - d);
                    ImVec2 bl(sp.x - d, sp.y + d);
                    ImVec2 br(sp.x + d, sp.y + d);
                    ImVec2 tri[3] = {top, br, bl};
                    dl->AddConvexPolyFilled(tri, 3, cCol);
                    dl->AddPolyline(tri, 3, kColorPointOutline,
                                    ImDrawFlags_Closed, 1.5f);
                }
                else
                {
                    ImU32 cCol = ImGui::ColorConvertFloat4ToU32(
                        style.cornerColor);
                    float d = style.pointRadius;
                    ImVec2 top(sp.x, sp.y - d);
                    ImVec2 right(sp.x + d, sp.y);
                    ImVec2 bot(sp.x, sp.y + d);
                    ImVec2 left(sp.x - d, sp.y);
                    ImVec2 diamond[4] = {top, right, bot, left};
                    dl->AddConvexPolyFilled(diamond, 4, cCol);
                    dl->AddPolyline(diamond, 4, kColorPointOutline,
                                    ImDrawFlags_Closed, 1.5f);
                }
            }
        }
        if (hasAnnotationArray(image->annotations, "bounds"))
        {
            auto& bnd = image->annotations["bounds"].getArray();
            bool editing = (mode == AnnotationMode::AddBounds);
            ImU32 bndCol =
                ImGui::ColorConvertFloat4ToU32(style.boundsColor);
            if (bnd.size() >= 2)
            {
                for (size_t i = 0; i + 1 < bnd.size(); ++i)
                {
                    ImVec2 a = imageToScreenCoord(
                        bnd[i]["x"].getNumber(),
                        bnd[i]["y"].getNumber(), cx, cy, annScale,
                        rotation, imgW, imgH);
                    ImVec2 b = imageToScreenCoord(
                        bnd[i + 1]["x"].getNumber(),
                        bnd[i + 1]["y"].getNumber(), cx, cy, annScale,
                        rotation, imgW, imgH);
                    dl->AddLine(a, b, bndCol,
                                style.boundsLineThickness);
                }
            }
            if (bnd.size() >= 2)
            {
                ImVec2 last = imageToScreenCoord(
                    bnd.back()["x"].getNumber(),
                    bnd.back()["y"].getNumber(), cx, cy, annScale,
                    rotation, imgW, imgH);
                ImVec2 first = imageToScreenCoord(
                    bnd[0]["x"].getNumber(), bnd[0]["y"].getNumber(),
                    cx, cy, annScale, rotation, imgW, imgH);
                if (!editing)
                {
                    dl->AddLine(last, first, bndCol,
                                style.boundsLineThickness);
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
                dl->AddCircleFilled(sp, style.pointRadius - 1.0f,
                                    bndCol);
            }
        }
    }

    static ImVec2 getRotatedVec2(const ImVec2& c, float rotation,
                                 float lx, float ly)
    {
        return ImVec2(c.x + lx * cosf(rotation) - ly * sinf(rotation),
                      c.y + lx * sinf(rotation) + ly * cosf(rotation));
    }

    void ImageCanvas2D::renderCanvas(const char* canvasId)
    {
        AnnotationMode& mode = annotationMode;
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
                            pointTypeToString(selectedPointType);
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
        dl->AddQuad(tl, tr, br, bl, kColorImageQuadOutline);

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
        float barThick = 5.0f;
        float barPad = 2.0f;
        ImU32 barCol = kColorScrollbar;
        ImU32 barColHov = kColorScrollbarHover;
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

    void ImageCanvas2D::renderToolbar(const char* toolbarId)
    {
        ImGui::PushID(toolbarId);
        ImGui::SetWindowFontScale(0.85f);

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
        if (dialHov) ImGui::SetTooltip("Drag to rotate");

        if (dialAct)
        {
            ImGuiIO& dio = ImGui::GetIO();
            float angle = atan2f(dio.MousePos.y - dialCenter.y,
                                 dio.MousePos.x - dialCenter.x);
            viewState.rotationTarget = angle;
            viewState.rotation = angle;
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 ringCol =
            dialHov || dialAct ? kColorDialRingHover : kColorDialRing;
        dl->AddCircle(dialCenter, dialR, ringCol, 24, 2.0f);

        ImVec2 ind =
            dialCenter + (direction(viewState.rotation) * dialR);
        dl->AddLine(dialCenter, ind, kColorDialIndicator, 2.0f);
        dl->AddCircleFilled(ind, 3.0f, kColorDialIndicator);

        ImGui::SameLine();
        float degs = viewState.rotationTarget / kDegToRad;
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("-360.0___").x);
        if (ImGui::InputFloat("deg", &degs, 0.0f, 0.0f, "%.1f",
                              ImGuiInputTextFlags_EnterReturnsTrue))
        {
            viewState.rotationTarget = degs * kDegToRad;
            viewState.rotation = viewState.rotationTarget;
        }

        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopID();
    }

    void ImageCanvas2D::renderMarkupControls(ImageCanvas2D* active)
    {
        ImGui::SeparatorText("Markup");
        AnnotationMode& mode = annotationMode;

        // Per-image edits need a live canvas; the mode toggles are
        // shared and stay usable, but anything touching annotations
        // is disabled when no canvas is active.
        bool hasActive =
            (active != nullptr && active->image != nullptr);
        float fullW = ImGui::GetContentRegionAvail().x;

        bool pointActive = (mode == AnnotationMode::AddPoint);
        if (pointActive)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.8f, 0.2f, 0.2f, 0.7f));
        if (ImGui::Button("+ Point", ImVec2(fullW, 0)))
            mode = pointActive ? AnnotationMode::None
                               : AnnotationMode::AddPoint;
        if (pointActive) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Click on the image to add a point.\n"
                "Shift+click removes the nearest point.");

        std::vector<PointType> allowed =
            hasActive ? active->allowedPointTypes()
                      : std::vector<PointType>{};
        int cur = 0;
        for (int i = 0; i < (int)allowed.size(); ++i)
        {
            if (allowed[i] == selectedPointType)
            {
                cur = i;
                break;
            }
        }
        if (!allowed.empty() && allowed[cur] != selectedPointType)
            selectedPointType = allowed[cur];

        ImGui::BeginDisabled(!pointActive || !hasActive);
        ImGui::SetNextItemWidth(fullW);
        const char* preview =
            allowed.empty() ? "" : pointTypeToString(selectedPointType);
        if (ImGui::BeginCombo("##ptType", preview))
        {
            for (int i = 0; i < (int)allowed.size(); ++i)
            {
                bool sel = (i == cur);
                if (ImGui::Selectable(pointTypeToString(allowed[i]),
                                      sel))
                    selectedPointType = allowed[i];
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        bool boundsActive = (mode == AnnotationMode::AddBounds);
        if (boundsActive)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(0.2f, 0.8f, 0.2f, 0.7f));
        if (ImGui::Button("+ Bounds", ImVec2(fullW, 0)))
        {
            mode = boundsActive ? AnnotationMode::None
                                : AnnotationMode::AddBounds;
            if (boundsActive && hasActive)
                active->removePointsOutsideBounds();
        }
        if (boundsActive) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Click on the image to add boundary vertices;\n"
                "click the first vertex to close. Shift+click "
                "removes.");

        ImGui::Spacing();
        ImGui::BeginDisabled(!hasActive);
        float halfW = (fullW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Undo", ImVec2(halfW, 0)))
        {
            const char* key = nullptr;
            if (mode == AnnotationMode::AddPoint)
                key = "points";
            else if (mode == AnnotationMode::AddBounds)
                key = "bounds";
            if (key &&
                hasAnnotationArray(active->image->annotations, key))
            {
                auto& arr = active->image->annotations[key].getArray();
                if (!arr.empty()) arr.pop_back();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear", ImVec2(halfW, 0)))
        {
            active->image->annotations["bounds"].setArray();
            active->image->annotations["points"].setArray();
        }
        ImGui::EndDisabled();
    }

    void ImageCanvas2D::renderAnnotationStyleSettings()
    {
        renderStyleSettings(allowedPointTypes());
    }

    void ImageCanvas2D::renderStyleSettings(
        const std::vector<PointType>& allowed)
    {
        if (!ImGui::CollapsingHeader("Annotations")) return;
        if (!ImGui::BeginTable("##annSettings", 3)) return;

        ImGui::TableSetupColumn(
            "Label", ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableSetupColumn(
            "Spacer", ImGuiTableColumnFlags_WidthFixed, 20.0f);
        ImGui::TableSetupColumn("Widget",
                                ImGuiTableColumnFlags_WidthStretch);

        auto has = [&](PointType t)
        {
            return std::find(allowed.begin(), allowed.end(), t) !=
                   allowed.end();
        };

        settingsTableRow("Point Radius");
        ImGui::SliderFloat("##PointRadius", &style.pointRadius, 2.0f,
                           15.0f, "%.1f");

        if (has(PointType::Corner))
        {
            settingsTableRow("Corner Color");
            ImGui::ColorEdit4("##CornerColor", &style.cornerColor.x);
        }
        if (has(PointType::Center))
        {
            settingsTableRow("Center Color");
            ImGui::ColorEdit4("##CenterColor", &style.centerColor.x);
        }
        if (has(PointType::RidgeEnding))
        {
            settingsTableRow("Ridge Ending Color");
            ImGui::ColorEdit4("##RidgeEndingColor",
                              &style.ridgeEndingColor.x);
        }
        if (has(PointType::Bifurcation))
        {
            settingsTableRow("Bifurcation Color");
            ImGui::ColorEdit4("##BifurcationColor",
                              &style.bifurcationColor.x);
        }
        if (has(PointType::Other))
        {
            settingsTableRow("Other Color");
            ImGui::ColorEdit4("##OtherColor", &style.otherColor.x);
        }
        if (has(PointType::Core))
        {
            settingsTableRow("Core Color");
            ImGui::ColorEdit4("##CoreColor", &style.coreColor.x);
        }
        if (has(PointType::Delta))
        {
            settingsTableRow("Delta Color");
            ImGui::ColorEdit4("##DeltaColor", &style.deltaColor.x);
        }
        if (has(PointType::WordStart))
        {
            settingsTableRow("Word Start Color");
            ImGui::ColorEdit4("##WordStartColor",
                              &style.wordStartColor.x);
        }
        if (has(PointType::WordEnd))
        {
            settingsTableRow("Word End Color");
            ImGui::ColorEdit4("##WordEndColor", &style.wordEndColor.x);
        }
        if (has(PointType::Intersection))
        {
            settingsTableRow("Intersection Color");
            ImGui::ColorEdit4("##IntersectionColor",
                              &style.intersectionColor.x);
        }
        if (has(PointType::CurveTurn))
        {
            settingsTableRow("CurveTurn Color");
            ImGui::ColorEdit4("##CurveTurnColor",
                              &style.curveTurnColor.x);
        }

        settingsTableRow("Bounds Thickness");
        ImGui::SliderFloat("##BoundsThickness",
                           &style.boundsLineThickness, 1.0f, 8.0f,
                           "%.1f");

        settingsTableRow("Bounds Color");
        ImGui::ColorEdit4("##BoundsColor", &style.boundsColor.x);

        ImGui::EndTable();
    }

}  // namespace shoecomp
