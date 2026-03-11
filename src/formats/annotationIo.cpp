#include "formats.h"
#include <fstream>
#include <sstream>

namespace shoecomp
{
    int saveAnnotationsToFile(const std::string& filePath,
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

    int loadAnnotationsFromFile(const std::string& filePath,
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
        if (!parsed.contains("bounds") || !parsed["bounds"].isArray())
            return -1;
        if (!parsed.contains("points") || !parsed["points"].isArray())
            return -1;

        for (size_t i = 0; i < parsed["bounds"].getArray().size(); ++i)
        {
            auto& el = parsed["bounds"].getArray()[i];
            if (!el.isObject()) return -1;
            if (!el.contains("x") || !el["x"].isNumber()) return -1;
            if (!el.contains("y") || !el["y"].isNumber()) return -1;
        }

        for (size_t i = 0; i < parsed["points"].getArray().size(); ++i)
        {
            auto& el = parsed["points"].getArray()[i];
            if (!el.isObject()) return -1;
            if (!el.contains("x") || !el["x"].isNumber()) return -1;
            if (!el.contains("y") || !el["y"].isNumber()) return -1;
        }

        annotations = std::move(parsed);
        return 0;
    }
} /* namespace shoecomp */
