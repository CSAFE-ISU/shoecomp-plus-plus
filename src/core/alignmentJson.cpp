#include "core/alignmentJson.h"

#include "core/eulerAngles.h"

namespace shoecomp
{
    jt::Json vec3fToJson(const Eigen::Vector3f& v)
    {
        jt::Json arr;
        arr.setArray();
        arr.getArray().push_back(jt::Json((double)v.x()));
        arr.getArray().push_back(jt::Json((double)v.y()));
        arr.getArray().push_back(jt::Json((double)v.z()));
        return arr;
    }

    jt::Json alignment2DToJson(const AlignState& a)
    {
        jt::Json obj;
        obj.setObject();
        obj["mode"] = jt::Json(
            a.mode == AlignMode::Manual ? "Manual" : "Automatic");
        obj["rotation"] = jt::Json((double)a.rotation);
        obj["dx"] = jt::Json((double)a.dx);
        obj["dy"] = jt::Json((double)a.dy);
        obj["scale"] = jt::Json((double)a.scale);
        obj["info"] = a.info;
        return obj;
    }

    jt::Json automatic3DToJson(const Eigen::Isometry3d& physical,
                               const jt::Json& info)
    {
        jt::Json obj;
        obj.setObject();
        obj["mode"] = jt::Json("Automatic");
        jt::Json m;
        m.setArray();
        for (int r = 0; r < 4; ++r)
        {
            jt::Json row;
            row.setArray();
            for (int c = 0; c < 4; ++c)
                row.getArray().push_back(
                    jt::Json(physical.matrix()(r, c)));
            m.getArray().push_back(std::move(row));
        }
        obj["matrix"] = m;
        obj["rotationDeg"] =
            vec3fToJson(matrixToEuler(physical.linear().cast<float>()));
        obj["translation"] =
            vec3fToJson(physical.translation().cast<float>());
        obj["info"] = info;
        return obj;
    }

    jt::Json striationMatchInfo(const StriationAlignment& r,
                                const std::vector<int>& leftPeakIdx,
                                const std::vector<int>& rightPeakIdx,
                                const Align3DSettings& s)
    {
        jt::Json info;
        info.setObject();
        info["matchedStriations"].setArray();
        for (const auto& pr : r.pairs)
        {
            jt::Json m;
            m.setArray();
            m.getArray().push_back(jt::Json(leftPeakIdx[pr.first]));
            m.getArray().push_back(jt::Json(rightPeakIdx[pr.second]));
            info["matchedStriations"].getArray().push_back(
                std::move(m));
        }
        info["rmsPerpUm"] = jt::Json(r.rmsPerp * 1e6);
        info["axialWeak"] = jt::Json(r.axialWeak);
        info["angleTolDeg"] = jt::Json((double)s.angleTolDeg);
        info["sepTolUm"] = jt::Json((double)s.sepTolUm);
        return info;
    }
}  // namespace shoecomp
