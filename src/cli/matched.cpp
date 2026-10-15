#include <cctype>
#include <filesystem>

#include "cli/cli.h"
#include "formats/annotationIo.h"
#include "formats/pngWrite.h"
#include "formats/x3pAnnotationIo.h"

namespace fs = std::filesystem;

namespace shoecomp
{
    namespace
    {
        // Annotations holding only the matched entries of |side|'s
        // points[] (2-D) or peaks[] (3-D), in pair order.
        jt::Json matchedAnnotations(
            const Item& it, const std::vector<std::pair<int, int>>& m,
            bool left)
        {
            const bool is3D = is3DType(it.type);
            const jt::Json& src = it.annotations;
            const auto& from =
                src[is3D ? "peaks" : "points"].getArray();
            jt::Json out;
            out.setObject();
            out["bounds"].setArray();
            out["points"].setArray();
            jt::Json& dst = out[is3D ? "peaks" : "points"];
            dst.setArray();
            for (const auto& pr : m)
                dst.getArray().push_back(
                    from[left ? pr.first : pr.second]);
            if (is3D && src.contains("cylinder"))
                out["cylinder"] = src["cylinder"];
            return out;
        }

        // |stem| = output path without the extension.
        bool writeSide(const std::string& stem, const Item& it,
                       const std::vector<std::pair<int, int>>& m,
                       bool left, std::string& err)
        {
            const jt::Json ann = matchedAnnotations(it, m, left);
            const std::string json = stem + ".json";
            const int rc = is3DType(it.type)
                               ? saveX3PAnnotations(json, ann)
                               : saveAnnotationsToFile(json, ann);
            if (rc != 0)
            {
                err = "cannot write " + json;
                return false;
            }
            if (is3DType(it.type)) return true;

            std::vector<unsigned char> rgba;
            int w = 0, h = 0;
            if (!loadItemPixels(it, rgba, w, h, err)) return false;
            std::vector<Marker> markers;
            const auto& pts = ann["points"].getArray();
            for (size_t k = 0; k < pts.size(); ++k)
            {
                const jt::Json& p = pts[k];
                const PointType t =
                    p.contains("type")
                        ? stringToPointType(p["type"].getString())
                        : PointType::Other;
                markers.push_back({p["x"].getNumber(),
                                   p["y"].getNumber(),
                                   defaultPointColor(t), (int)k + 1});
            }
            drawMarkers(rgba, w, h, markers);
            const std::string png = stem + ".png";
            if (savePngToDisk(png, rgba.data(), w, h) != 0)
            {
                err = "cannot write " + png;
                return false;
            }
            return true;
        }
    }  // namespace

    std::string fileSafe(const std::string& label)
    {
        std::string s = label;
        for (char& ch : s)
        {
            const unsigned char u = (unsigned char)ch;
            if (!std::isalnum(u) && ch != '.' && ch != '-' &&
                ch != '_' && ch != '@')
                ch = '_';
        }
        return s;
    }

    bool saveMatched(const std::string& dir, const std::string& base,
                     const std::string& leftTag,
                     const std::string& rightTag, const Item& left,
                     const Item& right, const PairResult& r,
                     int maxResults, std::string& err)
    {
        if (r.matched.empty()) return true;
        std::error_code ec;
        fs::create_directories(dir, ec);
        const size_t n = std::min(r.matched.size(), (size_t)maxResults);
        for (size_t i = 0; i < n; ++i)
        {
            std::string name = base;
            if (maxResults > 1) name += "_r" + std::to_string(i + 1);
            const std::string stem = (fs::path(dir) / name).string();
            if (!writeSide(stem + leftTag, left, r.matched[i], true,
                           err) ||
                !writeSide(stem + rightTag, right, r.matched[i], false,
                           err))
                return false;
        }
        return true;
    }
}  // namespace shoecomp
