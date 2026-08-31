#include "formats/x3pAnnotationIo.h"
#include <fstream>
#include <sstream>

namespace shoecomp
{
    namespace
    {
        constexpr double kR2D = 180.0 / 3.14159265358979323846;
        constexpr double kD2R = 3.14159265358979323846 / 180.0;

        jt::Json vec3ToJson(const Eigen::Vector3d& v)
        {
            jt::Json a;
            a.setArray();
            a.getArray().push_back(jt::Json(v.x()));
            a.getArray().push_back(jt::Json(v.y()));
            a.getArray().push_back(jt::Json(v.z()));
            return a;
        }

        bool jsonToVec3(const jt::Json& obj, const char* key,
                        Eigen::Vector3d& v)
        {
            if (!obj.contains(key) || !obj[key].isArray()) return false;
            const auto& arr = obj[key].getArray();
            if (arr.size() != 3) return false;
            for (int i = 0; i < 3; ++i)
                if (!arr[i].isNumber()) return false;
            v = Eigen::Vector3d(arr[0].getNumber(), arr[1].getNumber(),
                                arr[2].getNumber());
            return true;
        }

        double numOr(const jt::Json& obj, const char* key, double def)
        {
            return (obj.contains(key) && obj[key].isNumber())
                       ? obj[key].getNumber()
                       : def;
        }

        bool validElement(const jt::Json& el, bool wantType)
        {
            if (!el.isObject()) return false;
            for (const char* key : {"col", "row", "x", "y", "z"})
            {
                if (!el.contains(key) || !el[key].isNumber())
                    return false;
            }
            if (wantType)
            {
                if (!el.contains("type") || !el["type"].isString())
                    return false;
                if (el["type"].getString() != "Peak") return false;
            }
            return true;
        }

        // A peak is a striation line: start[3] + direction[3] + length.
        bool validPeak(const jt::Json& pk)
        {
            StriationLine ln;
            return striationLineFromJson(pk, ln);
        }
    }  // namespace

    int saveX3PAnnotations(const std::string& filePath,
                           const jt::Json& annotations)
    {
        std::string data = annotations.toStringPretty();
        std::ofstream ofs(filePath, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) return -1;
        ofs << data;
        if (!ofs.good()) return -1;
        ofs.close();
        return 0;
    }

    jt::Json striationLineToJson(const StriationLine& s)
    {
        jt::Json pk;
        pk.setObject();
        pk["start"] = vec3ToJson(s.start);
        pk["direction"] = vec3ToJson(s.dir);
        pk["length"] = s.length;
        pk["peak"] = jt::Json(s.isPeak);
        return pk;
    }

    bool striationLineFromJson(const jt::Json& j, StriationLine& out)
    {
        if (!j.isObject()) return false;
        StriationLine s;
        if (!jsonToVec3(j, "start", s.start)) return false;
        if (!jsonToVec3(j, "direction", s.dir)) return false;
        if (!j.contains("length") || !j["length"].isNumber())
            return false;
        s.length = j["length"].getNumber();
        // Peak/valley flag; optional (older line files omit it ->
        // peak).
        if (j.contains("peak") && j["peak"].isBool())
            s.isPeak = j["peak"].getBool();
        out = s;
        return true;
    }

    jt::Json cylinderToJson(const CylinderFit& f)
    {
        jt::Json c;
        c.setObject();
        c["axis"] = vec3ToJson(f.axis);
        c["center"] = vec3ToJson(f.center);
        c["radius"] = f.radius;
        c["e1"] = vec3ToJson(f.e1);
        c["e2"] = vec3ToJson(f.e2);
        c["thetaOrigin"] = f.thetaOrigin * kR2D;
        c["sMin"] = f.sMin;
        c["sMax"] = f.sMax;
        c["thetaMin"] = f.thetaMin * kR2D;
        c["thetaMax"] = f.thetaMax * kR2D;
        return c;
    }

    bool cylinderFromJson(const jt::Json& c, CylinderFit& out)
    {
        if (!c.isObject()) return false;
        CylinderFit f;
        if (!jsonToVec3(c, "axis", f.axis)) return false;
        if (!jsonToVec3(c, "center", f.center)) return false;
        if (!jsonToVec3(c, "e1", f.e1)) return false;
        if (!jsonToVec3(c, "e2", f.e2)) return false;
        if (!c.contains("radius") || !c["radius"].isNumber())
            return false;
        f.radius = c["radius"].getNumber();
        f.thetaOrigin = numOr(c, "thetaOrigin", 0.0) * kD2R;
        f.sMin = numOr(c, "sMin", 0.0);
        f.sMax = numOr(c, "sMax", 0.0);
        f.thetaMin = numOr(c, "thetaMin", 0.0) * kD2R;
        f.thetaMax = numOr(c, "thetaMax", 0.0) * kD2R;
        f.valid = true;
        out = f;
        return true;
    }

    int loadX3PAnnotations(const std::string& filePath,
                           jt::Json& annotations)
    {
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) return -1;
        std::ostringstream ss;
        ss << ifs.rdbuf();
        if (!ifs.good() && !ifs.eof()) return -1;
        std::string contents = ss.str();

        auto [status, parsed] = jt::Json::parse(contents);
        if (status != jt::Json::Status::success) return -1;
        if (!parsed.isObject()) return -1;

        // Every section is optional; validate any that are present so a
        // legacy point-only file and a peaks/cylinder file both load.
        if (parsed.contains("points"))
        {
            if (!parsed["points"].isArray()) return -1;
            for (const auto& el : parsed["points"].getArray())
                if (!validElement(el, /*wantType=*/true)) return -1;
        }
        if (parsed.contains("bounds"))
        {
            if (!parsed["bounds"].isArray()) return -1;
            for (const auto& el : parsed["bounds"].getArray())
                if (!validElement(el, /*wantType=*/false)) return -1;
        }
        if (parsed.contains("peaks"))
        {
            if (!parsed["peaks"].isArray()) return -1;
            for (const auto& el : parsed["peaks"].getArray())
                if (!validPeak(el)) return -1;
        }
        if (parsed.contains("cylinder"))
        {
            CylinderFit tmp;
            if (!cylinderFromJson(parsed["cylinder"], tmp)) return -1;
        }

        annotations = std::move(parsed);
        return 0;
    }
} /* namespace shoecomp */
