#include "formats/x3pAnnotationIo.h"
#include <fstream>
#include <sstream>

namespace shoecomp
{
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

    static bool validElement(const jt::Json& el, bool wantType)
    {
        if (!el.isObject()) return false;
        for (const char* key : {"col", "row", "x", "y", "z"})
        {
            if (!el.contains(key) || !el[key].isNumber()) return false;
        }
        if (wantType)
        {
            if (!el.contains("type") || !el["type"].isString())
                return false;
            if (el["type"].getString() != "Peak") return false;
        }
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
        if (!parsed.contains("points") || !parsed["points"].isArray())
            return -1;
        if (!parsed.contains("bounds") || !parsed["bounds"].isArray())
            return -1;

        for (auto& el : parsed["points"].getArray())
        {
            if (!validElement(el, /*wantType=*/true)) return -1;
        }
        for (auto& el : parsed["bounds"].getArray())
        {
            if (!validElement(el, /*wantType=*/false)) return -1;
        }

        annotations = std::move(parsed);
        return 0;
    }
} /* namespace shoecomp */
