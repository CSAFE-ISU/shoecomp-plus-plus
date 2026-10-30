#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "cli/cli.h"
#include "core/eulerAngles.h"

namespace shoecomp
{
    namespace
    {
        constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

        std::string fmt(const char* f, double v)
        {
            char buf[64];
            std::snprintf(buf, sizeof(buf), f, v);
            return buf;
        }

        // pitch/yaw/roll degrees and translation micrometres.
        void decompose(const StriationAlignment& a, double ang[3],
                       double t[3])
        {
            const Eigen::Vector3f e =
                matrixToEuler(a.leftToRight.linear().cast<float>());
            const Eigen::Vector3d tr =
                a.leftToRight.translation() * 1e6;
            for (int k = 0; k < 3; ++k)
            {
                ang[k] = e[k];
                t[k] = tr[k];
            }
        }
    }  // namespace

    jt::Json pairDocument(ImageType type, const Item& left,
                          const Item& right, const PairResult& r)
    {
        jt::Json d;
        d.setObject();
        d["type"] = jt::Json(imageTypeName(type));
        d["left"] = jt::Json(left.label);
        d["right"] = jt::Json(right.label);
        d["status"] = jt::Json(pairStatusName(r.status));
        if (!r.message.empty()) d["message"] = jt::Json(r.message);
        d["timeMs"] = jt::Json(std::round(r.timeMs * 10.0) / 10.0);
        d["leftCount"] = jt::Json((long)left.count);
        d["rightCount"] = jt::Json((long)right.count);
        d["leftSource"] = jt::Json(left.annSource);
        d["rightSource"] = jt::Json(right.annSource);
        d["alignments"] = r.alignments;
        return d;
    }

    void printSummary(std::FILE* f, const Item& left, const Item& right,
                      const PairResult& r)
    {
        const size_t n = r.matched.size();
        std::fprintf(f, "%s vs %s: %s", left.label.c_str(),
                     right.label.c_str(), pairStatusName(r.status));
        if (r.status == PairStatus::Ok)
            std::fprintf(f, ", %zu alignment(s)", n);
        else if (!r.message.empty())
            std::fprintf(f, " (%s)", r.message.c_str());
        std::fprintf(f, ", %.0f ms\n", r.timeMs);

        for (size_t i = 0; i < r.align2d.size(); ++i)
        {
            const AlignState& a = r.align2d[i];
            std::fprintf(f,
                         "  #%zu  matched %zu of %zu/%zu points  "
                         "rot %.3f deg  shift (%.2f, %.2f) px  "
                         "scale %.4f\n",
                         i + 1, r.matched[i].size(), left.count,
                         right.count, a.rotation * kRadToDeg, a.dx,
                         a.dy, a.scale);
        }
        for (size_t i = 0; i < r.align3d.size(); ++i)
        {
            double ang[3], t[3];
            decompose(r.align3d[i], ang, t);
            std::fprintf(f,
                         "  #%zu  matched %zu of %zu/%zu striations  "
                         "rot (%.3f, %.3f, %.3f) deg  "
                         "shift (%.2f, %.2f, %.2f) um  rms %.2f um%s\n",
                         i + 1, r.matched[i].size(), left.count,
                         right.count, ang[0], ang[1], ang[2], t[0],
                         t[1], t[2], r.align3d[i].rmsPerp * 1e6,
                         r.align3d[i].axialWeak ? "  axial-weak" : "");
        }
    }

    std::string csvField(const std::string& s)
    {
        if (s.find_first_of(",\"\n") == std::string::npos) return s;
        std::string q = "\"";
        for (char c : s)
        {
            if (c == '"') q += '"';
            q += c;
        }
        return q + "\"";
    }

    std::string csvHeader(ImageType type)
    {
        if (is3DType(type))
            return "id,left,right,status,n_results,matched,explained,"
                   "matched_2,ratio,left_lines,right_lines,pitch_deg,"
                   "yaw_deg,"
                   "roll_deg,tx_um,ty_um,tz_um,rms_um,axial_weak,"
                   "time_ms,message";
        return "id,left,right,status,n_results,matched,explained,"
               "matched_2,ratio,left_points,right_points,rotation_deg,"
               "dx,dy,scale,"
               "time_ms,message";
    }

    std::string csvRow(size_t id, const Item& left, const Item& right,
                       const PairResult& r)
    {
        std::ostringstream os;
        os << id << ',' << csvField(left.label) << ','
           << csvField(right.label) << ',' << pairStatusName(r.status)
           << ',' << r.matched.size() << ','
           << (r.matched.empty() ? 0 : r.matched[0].size()) << ','
           << (r.explained.empty() ? 0 : r.explained[0]) << ',';
        // Results are distinct alignments: how far the best stands
        // above the second best (inf: no second one reached 70% of the
        // best).
        if (r.matched.empty())
            os << ",,";
        else
        {
            const size_t m1 = r.matched[0].size();
            const size_t m2 =
                r.matched.size() > 1 ? r.matched[1].size() : 0;
            os << m2 << ','
               << (m2 > 0 ? fmt("%.3f", double(m1) / double(m2))
                          : std::string("inf"))
               << ',';
        }
        os << left.count << ',' << right.count << ',';
        if (!r.align3d.empty())
        {
            double ang[3], t[3];
            decompose(r.align3d[0], ang, t);
            for (double v : ang) os << fmt("%.4f", v) << ',';
            for (double v : t) os << fmt("%.3f", v) << ',';
            os << fmt("%.3f", r.align3d[0].rmsPerp * 1e6) << ','
               << (r.align3d[0].axialWeak ? "true" : "false") << ',';
        }
        else if (!r.align2d.empty())
        {
            const AlignState& a = r.align2d[0];
            os << fmt("%.4f", a.rotation * kRadToDeg) << ','
               << fmt("%.3f", a.dx) << ',' << fmt("%.3f", a.dy) << ','
               << fmt("%.6f", a.scale) << ',';
        }
        else { os << (is3DType(left.type) ? ",,,,,,,," : ",,,,"); }
        std::string msg = r.message;
        auto unitNote = [&msg](X3PUnits u, const char* side)
        {
            if (u == X3PUnits::Metres) return;
            msg += (msg.empty() ? "" : "; ") +
                   std::string("x3p units read as ") + x3pUnitsName(u) +
                   " (" + side + ")";
        };
        if (left.units == right.units)
            unitNote(left.units, "both");
        else
        {
            unitNote(left.units, "left");
            unitNote(right.units, "right");
        }
        os << fmt("%.1f", r.timeMs) << ',' << csvField(msg);
        return os.str();
    }

    bool writeTextFile(const std::string& path, const std::string& text,
                       std::string& err)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (out) out << text;
        if (!out)
        {
            err = "cannot write " + path;
            return false;
        }
        return true;
    }
}  // namespace shoecomp
