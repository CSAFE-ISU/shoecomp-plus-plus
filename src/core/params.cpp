#include "core/params.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace shoecomp
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        std::string trim(const std::string& s)
        {
            size_t a = 0, b = s.size();
            while (a < b && std::isspace((unsigned char)s[a])) ++a;
            while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
            return s.substr(a, b - a);
        }

        std::string lower(std::string s)
        {
            for (char& c : s) c = (char)std::tolower((unsigned char)c);
            return s;
        }

        // Shortest %g form that reads back to the same value, without
        // an exponent for ordinary magnitudes.
        std::string fmtShortest(double v, int maxPrec, bool isFloat)
        {
            const double m = std::fabs(v);
            const bool sci = m != 0.0 && (m < 1e-4 || m >= 1e15);
            char buf[64];
            for (int prec = 1; prec <= maxPrec; ++prec)
            {
                std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
                bool same = isFloat
                                ? std::strtof(buf, nullptr) == (float)v
                                : std::strtod(buf, nullptr) == v;
                bool hasExp = std::strchr(buf, 'e') != nullptr;
                if (same && (sci || !hasExp)) break;
            }
            return buf;
        }

        std::string fmtDouble(double v)
        {
            return fmtShortest(v, 17, false);
        }
        std::string fmtFloat(float v)
        {
            return fmtShortest(v, 9, true);
        }

        bool parseDouble(const std::string& s, double& out)
        {
            if (s.empty()) return false;
            char* end = nullptr;
            errno = 0;
            out = std::strtod(s.c_str(), &end);
            return errno == 0 && end && *end == '\0';
        }

        bool parseLong(const std::string& s, long long& out)
        {
            if (s.empty()) return false;
            char* end = nullptr;
            errno = 0;
            out = std::strtoll(s.c_str(), &end, 10);
            return errno == 0 && end && *end == '\0';
        }

        bool parseBool(const std::string& s, bool& out)
        {
            std::string v = lower(s);
            if (v == "true" || v == "1" || v == "yes" || v == "on")
            {
                out = true;
                return true;
            }
            if (v == "false" || v == "0" || v == "no" || v == "off")
            {
                out = false;
                return true;
            }
            return false;
        }
    }  // namespace

    StriationParams StriationSettings::toParams(int numThreads) const
    {
        StriationParams p;
        p.minLength = minLengthMm * 1e-3;
        p.minHeight = minHeightUm * 1e-6;
        p.maxSurfaceDev = maxSurfDevDeg * kPi / 180.0;
        p.maxCount = maxCount;
        p.includeValleys = includeValleys;
        p.excludeShoulders = excludeShoulders;
        p.shoulderMargin = shoulderMarginUm * 1e-6;
        p.numThreads = numThreads;
        return p;
    }

    StriationAlignParams Align3DSettings::toParams(
        size_t numWorkers, const CliqueSettings& c) const
    {
        StriationAlignParams p;
        p.angleTolDeg = angleTolDeg;
        p.sepTolUm = sepTolUm;
        p.maxLengthRatio = maxLengthRatio;
        p.extentScale = extentScale;
        p.anchorTolUm = anchorTolUm;
        p.minLines = c.minMatched;
        p.maxLines = c.maxMatched;
        p.distinct = c.distinctMode();
        p.maxRotationDeg = maxRotationDeg;
        p.numResults = (size_t)c.numResults;
        p.numWorkers = numWorkers;
        return p;
    }

    AlignCalc::DistinctMode CliqueSettings::distinctMode() const
    {
        return distinct == "disjoint"
                   ? AlignCalc::DistinctMode::Disjoint
                   : AlignCalc::DistinctMode::Explained;
    }

    AlignCalc::CliqueOptions CliqueSettings::toOptions(
        size_t numWorkers) const
    {
        AlignCalc::CliqueOptions o;
        o.minMatched = minMatched;
        o.maxMatched = maxMatched;
        o.results = (size_t)numResults;
        o.distinct = distinctMode();
        o.numWorkers = numWorkers;
        return o;
    }

    AlignCalc::PointOptions Align2DSettings::toOptions() const
    {
        AlignCalc::PointOptions o;
        o.mode = sameScale ? AlignCalc::PointMode::Rigid
                           : AlignCalc::PointMode::Similarity;
        o.epsilon = epsilon;
        o.scaleRange = scaleRange;
        o.anchorTol = anchorTol;
        return o;
    }

#define SC_REF(member) \
    [](Params& p) -> void* { return &p.member; }

    const std::vector<ParamDesc>& paramTable()
    {
        // clang-format off
        static const std::vector<ParamDesc> table = {
            {"detect2d", "model",
             "ONNX bbox model (.onnx); empty = no point detection",
             ParamKind::String, SC_REF(detect2d.model)},
            {"detect2d", "conf", "Detection confidence threshold [0,1]",
             ParamKind::Float, SC_REF(detect2d.confThreshold)},
            {"detect2d", "iou", "NMS IoU threshold [0,1]",
             ParamKind::Float, SC_REF(detect2d.iouThreshold)},

            {"align2d", "scale_range",
             "Allowed scale factor s: scales within [1/s, s]",
             ParamKind::Double, SC_REF(align2d.scaleRange)},
            {"align2d", "epsilon", "Allowed error",
             ParamKind::Double, SC_REF(align2d.epsilon)},
            {"align2d", "same_scale", "Force scale 1 (rigid)",
             ParamKind::Bool, SC_REF(align2d.sameScale)},
            {"align2d", "anchor_tol",
             "Anchored clique search: every point must fit the "
             "transform (no mirroring) of its match's first two points "
             "within this fraction of the triangle size; 0 = off",
             ParamKind::Double, SC_REF(align2d.anchorTol)},
            {"align2d", "minutia_angle_tol_deg",
             "EBTS only: max mismatch of relative minutia directions "
             "(deg); 0 = off",
             ParamKind::Float, SC_REF(minutiaAngleTolDeg)},

            {"striation", "min_length_mm", "Minimum striation length (mm)",
             ParamKind::Float, SC_REF(striation.minLengthMm)},
            {"striation", "min_height_um",
             "Minimum ridge/groove height |dr| (um)",
             ParamKind::Float, SC_REF(striation.minHeightUm)},
            {"striation", "max_dev_deg",
             "Max deviation from the surface (deg); 0 = off",
             ParamKind::Float, SC_REF(striation.maxSurfDevDeg)},
            {"striation", "max_count", "Keep the strongest N striations",
             ParamKind::Int, SC_REF(striation.maxCount)},
            {"striation", "valleys",
             "Detect valleys (grooves) as well as peaks",
             ParamKind::Bool, SC_REF(striation.includeValleys)},
            {"striation", "exclude_shoulders",
             "Skip the groove shoulders at the land edges (found from "
             "the land's cross-section profile)",
             ParamKind::Bool, SC_REF(striation.excludeShoulders)},
            {"striation", "shoulder_margin_um",
             "Also skip this much inside each found shoulder (um)",
             ParamKind::Float, SC_REF(striation.shoulderMarginUm)},

            {"align3d", "angle_tol_deg", "Angle tolerance (deg); 0 = off",
             ParamKind::Float, SC_REF(align3d.angleTolDeg)},
            {"align3d", "sep_tol_um", "Separation tolerance (um)",
             ParamKind::Float, SC_REF(align3d.sepTolUm)},
            {"align3d", "max_length_ratio",
             "Max longer/shorter striation length ratio; 0 = off",
             ParamKind::Float, SC_REF(align3d.maxLengthRatio)},
            {"align3d", "extent_scale",
             "Widen the separation tolerance for striations detected "
             "over different lengths (x sin(angle/2) x length "
             "difference); 0 = off",
             ParamKind::Float, SC_REF(align3d.extentScale)},
            {"align3d", "max_rotation_deg",
             "Reject alignments that rotate one land by more than this "
             "(deg; e.g. reversed end-for-end); 0 = off",
             ParamKind::Float, SC_REF(align3d.maxRotationDeg)},
            {"align3d", "anchor_tol_um",
             "Anchored search: every striation must fit the transform of "
             "its match's first two within this depth (um; across: "
             "sep_tol_um); 0 = off",
             ParamKind::Float, SC_REF(align3d.anchorTolUm)},

            {"clique", "min_matched",
             "Minimum matched points (2-D) or striations (x3p), >= 3",
             ParamKind::Int, SC_REF(clique.minMatched)},
            {"clique", "max_matched",
             "Maximum matched; 0 = fewer of the two counts",
             ParamKind::Int, SC_REF(clique.maxMatched)},
            {"clique", "results",
             "Distinct alignments to return, best first",
             ParamKind::Int, SC_REF(clique.numResults)},
            {"clique", "distinct",
             "How results after the best are kept apart: explained = "
             "no pair an earlier result's transform explains; "
             "disjoint = no point shared with an earlier result",
             ParamKind::String, SC_REF(clique.distinct)},
        };
        // clang-format on
        return table;
    }

#undef SC_REF

    std::string paramValueString(const ParamDesc& d, Params& p)
    {
        void* v = d.ref(p);
        switch (d.kind)
        {
            case ParamKind::Bool:
                return *(bool*)v ? "true" : "false";
            case ParamKind::Int:
                return std::to_string(*(int*)v);
            case ParamKind::Size:
                return std::to_string(*(size_t*)v);
            case ParamKind::Float:
                return fmtFloat(*(float*)v);
            case ParamKind::Double:
                return fmtDouble(*(double*)v);
            case ParamKind::String:
            default:
                return *(std::string*)v;
        }
    }

    bool setParamValue(const ParamDesc& d, Params& p,
                       const std::string& value, std::string& err)
    {
        void* v = d.ref(p);
        std::string s = trim(value);
        bool ok = true;
        switch (d.kind)
        {
            case ParamKind::Bool:
                ok = parseBool(s, *(bool*)v);
                break;
            case ParamKind::Int:
            {
                long long n = 0;
                ok =
                    parseLong(s, n) && n >= INT32_MIN && n <= INT32_MAX;
                if (ok) *(int*)v = (int)n;
                break;
            }
            case ParamKind::Size:
            {
                long long n = 0;
                ok = parseLong(s, n) && n >= 0;
                if (ok) *(size_t*)v = (size_t)n;
                break;
            }
            case ParamKind::Float:
            {
                double x = 0.0;
                ok = parseDouble(s, x);
                if (ok) *(float*)v = (float)x;
                break;
            }
            case ParamKind::Double:
                ok = parseDouble(s, *(double*)v);
                break;
            case ParamKind::String:
                if (s.size() >= 2 && s.front() == '"' &&
                    s.back() == '"')
                    s = s.substr(1, s.size() - 2);
                *(std::string*)v = s;
                break;
        }
        if (!ok)
        {
            err = std::string("invalid value '") + s + "' for [" +
                  d.section + "] " + d.key;
        }
        return ok;
    }

    std::string paramFlag(const ParamDesc& d)
    {
        std::string f = std::string("--") + d.section + "-" + d.key;
        for (char& c : f)
            if (c == '_') c = '-';
        return f;
    }

    std::string paramsToIni(const Params& p)
    {
        Params copy = p;
        std::ostringstream os;
        os << "# scpp tuning parameters. Keys may be omitted; missing "
              "keys keep their defaults.\n";
        const char* section = "";
        for (const auto& d : paramTable())
        {
            if (std::string(section) != d.section)
            {
                section = d.section;
                os << "\n[" << section << "]\n";
            }
            os << "# " << d.help << "\n";
            std::string v = paramValueString(d, copy);
            os << d.key << (v.empty() ? " =" : " = " + v) << "\n";
        }
        return os.str();
    }

    bool paramsFromIni(const std::string& text, Params& p,
                       std::string& err)
    {
        std::istringstream is(text);
        std::string line, section;
        int lineNo = 0;
        while (std::getline(is, line))
        {
            ++lineNo;
            std::string t = trim(line);
            if (t.empty() || t[0] == '#' || t[0] == ';') continue;
            auto fail = [&](const std::string& msg)
            {
                err = "line " + std::to_string(lineNo) + ": " + msg;
                return false;
            };
            if (t[0] == '[')
            {
                if (t.back() != ']') return fail("malformed section");
                section = trim(t.substr(1, t.size() - 2));
                bool known = false;
                for (const auto& d : paramTable())
                    known = known || section == d.section;
                if (!known)
                    return fail("unknown section [" + section + "]");
                continue;
            }
            size_t eq = t.find('=');
            if (eq == std::string::npos)
                return fail("expected key = value");
            if (section.empty()) return fail("key outside a section");
            std::string key = trim(t.substr(0, eq));
            const ParamDesc* desc = nullptr;
            for (const auto& d : paramTable())
                if (section == d.section && key == d.key) desc = &d;
            if (!desc)
                return fail("unknown key '" + key + "' in [" + section +
                            "]");
            std::string e;
            if (!setParamValue(*desc, p, t.substr(eq + 1), e))
                return fail(e);
        }
        return true;
    }

    bool loadParamsFile(const std::string& path, Params& p,
                        std::string& err)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            err = "cannot open " + path;
            return false;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        if (!paramsFromIni(ss.str(), p, err))
        {
            err = path + ": " + err;
            return false;
        }
        return true;
    }

    bool validateParams(const Params& p, std::string& err)
    {
        auto bad = [&](const char* what)
        {
            err = what;
            return false;
        };
        const auto& d = p.detect2d;
        if (d.confThreshold < 0.0f || d.confThreshold > 1.0f)
            return bad("[detect2d] conf must be in [0, 1]");
        if (d.iouThreshold < 0.0f || d.iouThreshold > 1.0f)
            return bad("[detect2d] iou must be in [0, 1]");

        const auto& a = p.align2d;
        if (!(a.scaleRange >= 1.0))
            return bad("[align2d] scale_range must be >= 1");
        if (!(a.epsilon > 0.0))
            return bad("[align2d] epsilon must be > 0");
        if (!(a.anchorTol >= 0.0))
            return bad("[align2d] anchor_tol must be >= 0");
        if (p.minutiaAngleTolDeg < 0.0f ||
            p.minutiaAngleTolDeg > 180.0f)
            return bad(
                "[align2d] minutia_angle_tol_deg must be in [0, 180]");

        const auto& s = p.striation;
        if (s.minLengthMm < 0.0f)
            return bad("[striation] min_length_mm must be >= 0");
        if (s.minHeightUm < 0.0f)
            return bad("[striation] min_height_um must be >= 0");
        if (s.maxSurfDevDeg < 0.0f || s.maxSurfDevDeg > 90.0f)
            return bad("[striation] max_dev_deg must be in [0, 90]");
        if (s.maxCount < 1)
            return bad("[striation] max_count must be >= 1");
        if (s.shoulderMarginUm < 0.0f)
            return bad("[striation] shoulder_margin_um must be >= 0");

        const auto& l = p.align3d;
        if (!(l.angleTolDeg >= 0.0f))
            return bad("[align3d] angle_tol_deg must be >= 0");
        if (!(l.sepTolUm > 0.0f))
            return bad("[align3d] sep_tol_um must be > 0");
        if (l.maxLengthRatio < 0.0f)
            return bad("[align3d] max_length_ratio must be >= 0");
        if (l.extentScale < 0.0f)
            return bad("[align3d] extent_scale must be >= 0");
        if (!(l.anchorTolUm >= 0.0f))
            return bad("[align3d] anchor_tol_um must be >= 0");
        if (!(l.maxRotationDeg >= 0.0f && l.maxRotationDeg <= 180.0f))
            return bad(
                "[align3d] max_rotation_deg must be in [0, 180]");
        const auto& c = p.clique;
        if (c.minMatched < 3)
            return bad("[clique] min_matched must be >= 3");
        if (c.maxMatched != 0 && c.maxMatched < c.minMatched)
            return bad(
                "[clique] max_matched must be 0 or >= min_matched");
        if (c.numResults < 1)
            return bad("[clique] results must be >= 1");
        if (c.distinct != "explained" && c.distinct != "disjoint")
            return bad(
                "[clique] distinct must be explained or disjoint");
        return true;
    }
}  // namespace shoecomp
