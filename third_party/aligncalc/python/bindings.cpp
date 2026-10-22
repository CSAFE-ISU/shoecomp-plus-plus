// aligncalc._core: the tier-1 calls (alignPoints, matchPoints,
// alignLines3D) for the Python package. Argument handling (INI keys,
// labels, defaults, validation) is in aligncalc/__init__.py; this
// layer takes the assembled row matrices and plain options.
#include "aligncalc/aligncalc.h"
#include <pybind11/eigen.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace py = pybind11;
using namespace AlignCalc;

namespace
{
    // Progress for a call made with the GIL released. report() runs on
    // the calling thread (the library's contract): it takes the GIL
    // and calls the Python callback, which may return True to cancel.
    // cancelled() is polled from every worker; on the calling thread
    // (worker 0) it also checks for signals at most every 50 ms, so
    // Ctrl-C cancels. A Python exception (from the callback or a
    // signal handler) cancels the call and is raised after it.
    class PyProgress : public Progress
    {
       public:
        explicit PyProgress(py::object callback)
            : callback_(std::move(callback))
        {
        }

        void report(float fraction, const char* stage) override
        {
            if (cancel_.load()) return;
            py::gil_scoped_acquire gil;
            if (!callback_.is_none())
            {
                try
                {
                    if (py::bool_(callback_(fraction, stage)))
                        cancel_.store(true);
                }
                catch (py::error_already_set& e)
                {
                    keepError(e);
                }
            }
            checkSignals();
        }

        bool cancelled() const override
        {
            if (cancel_.load(std::memory_order_relaxed)) return true;
            if (std::this_thread::get_id() == caller_)
            {
                const auto now = Clock::now();
                if (now >= nextCheck_)
                {
                    nextCheck_ = now + std::chrono::milliseconds(50);
                    py::gil_scoped_acquire gil;
                    checkSignals();
                }
            }
            return cancel_.load(std::memory_order_relaxed);
        }

        // With the GIL held, after the call: raise a kept exception.
        void raiseKept()
        {
            if (!error_) return;
            std::unique_ptr<py::error_already_set> e =
                std::move(error_);
            e->restore();
            throw py::error_already_set();
        }

       private:
        using Clock = std::chrono::steady_clock;

        // GIL held
        void checkSignals() const
        {
            if (error_) return;
            if (PyErr_CheckSignals() != 0)
            {
                py::error_already_set e;
                keepError(e);
            }
        }
        void keepError(py::error_already_set& e) const
        {
            if (!error_)
                error_ = std::make_unique<py::error_already_set>(
                    std::move(e));
            cancel_.store(true);
        }

        py::object callback_;
        const std::thread::id caller_ = std::this_thread::get_id();
        mutable std::atomic<bool> cancel_{false};
        mutable Clock::time_point nextCheck_ = Clock::now();
        mutable std::unique_ptr<py::error_already_set> error_;
    };

    CliqueOptions cliqueOptions(int minMatched, int maxMatched,
                                size_t results, bool disjoint,
                                double distinctMinFrac, size_t workers)
    {
        CliqueOptions c;
        c.minMatched = minMatched;
        c.maxMatched = maxMatched;
        c.results = results;
        c.distinct =
            disjoint ? DistinctMode::Disjoint : DistinctMode::Explained;
        c.distinctMinFrac = distinctMinFrac;
        c.numWorkers = workers;
        return c;
    }

    // (k, 2) int32 array of (left row, right row)
    py::array_t<int32_t> pairArray(const std::vector<Correspondence>& v)
    {
        py::array_t<int32_t> a({py::ssize_t(v.size()), py::ssize_t(2)});
        auto m = a.mutable_unchecked<2>();
        for (size_t i = 0; i < v.size(); ++i)
        {
            m(i, 0) = v[i].left;
            m(i, 1) = v[i].right;
        }
        return a;
    }

    // Runs |fn| without the GIL, then raises what |progress| kept.
    template <class Fn>
    auto runReleased(PyProgress& progress, Fn&& fn)
    {
        auto result = [&]
        {
            py::gil_scoped_release release;
            return fn();
        }();
        progress.raiseKept();
        return result;
    }

    struct PointCall
    {
        PointCols cols;
        PointOptions options;
        CliqueOptions clique;
    };

    PointCall pointCall(int dim, int labelCol, int angleCol,
                        bool similarity, double epsilon,
                        double scaleRange, double anchorTol,
                        int minSupport, double orientationTolDeg,
                        int minMatched, int maxMatched, size_t results,
                        bool disjoint, double distinctMinFrac,
                        size_t workers)
    {
        PointCall c;
        c.cols.dim = dim;
        c.cols.label = labelCol;
        c.cols.angle = angleCol;
        PointOptions& o = c.options;
        o.mode = similarity ? PointMode::Similarity : PointMode::Rigid;
        o.epsilon = epsilon;
        o.scaleRange = scaleRange;
        o.anchorTol = anchorTol;
        o.minSupport = minSupport;
        o.orientationTolDeg = orientationTolDeg;
        c.clique = cliqueOptions(minMatched, maxMatched, results,
                                 disjoint, distinctMinFrac, workers);
        return c;
    }

    py::tuple alignPointsPy(const DoubleMatrixR& left,
                            const DoubleMatrixR& right, int dim,
                            int labelCol, int angleCol, bool similarity,
                            double epsilon, double scaleRange,
                            double anchorTol, int minSupport,
                            double orientationTolDeg, int minMatched,
                            int maxMatched, size_t results,
                            bool disjoint, double distinctMinFrac,
                            size_t workers, py::object progress)
    {
        const PointCall c = pointCall(
            dim, labelCol, angleCol, similarity, epsilon, scaleRange,
            anchorTol, minSupport, orientationTolDeg, minMatched,
            maxMatched, results, disjoint, distinctMinFrac, workers);
        PyProgress p(std::move(progress));
        const PointResult r =
            runReleased(p,
                        [&] {
                            return alignPoints(left, right, c.cols,
                                               c.options, c.clique, &p);
                        });
        py::list out;
        for (const PointAlignment& a : r.alignments)
        {
            py::dict d;
            d["pairs"] = pairArray(a.pairs);
            d["rotation"] = Eigen::MatrixXd(a.transform.rotation);
            d["scale"] = a.transform.scale;
            d["translation"] = Eigen::VectorXd(a.transform.translation);
            d["angle"] = a.transform.angle;
            d["explained"] = a.explained;
            out.append(d);
        }
        return py::make_tuple(r.status, r.message, out);
    }

    // matchPoints: always disjoint; returns pairs arrays only
    py::tuple matchPointsPy(const DoubleMatrixR& left,
                            const DoubleMatrixR& right, int dim,
                            int labelCol, int angleCol, bool similarity,
                            double epsilon, double scaleRange,
                            double anchorTol, int minSupport,
                            double orientationTolDeg, int minMatched,
                            int maxMatched, size_t results,
                            double distinctMinFrac, size_t workers,
                            py::object progress)
    {
        const PointCall c = pointCall(
            dim, labelCol, angleCol, similarity, epsilon, scaleRange,
            anchorTol, minSupport, orientationTolDeg, minMatched,
            maxMatched, results, true, distinctMinFrac, workers);
        PyProgress p(std::move(progress));
        const MatchResult r =
            runReleased(p,
                        [&] {
                            return matchPoints(left, right, c.cols,
                                               c.options, c.clique, &p);
                        });
        py::list out;
        for (const PointMatch& m : r.matches)
            out.append(pairArray(m.pairs));
        return py::make_tuple(r.status, r.message, out);
    }

    py::tuple alignLines3DPy(const DoubleMatrixR& left,
                             const DoubleMatrixR& right, int labelCol,
                             double angleTol, double sepTol,
                             double anchorTol, double extentScale,
                             int minMatched, int maxMatched,
                             size_t results, bool disjoint,
                             double distinctMinFrac, size_t workers,
                             py::object progress)
    {
        LineCols cols;
        cols.label = labelCol;
        LineParams lp;
        lp.angleTol = angleTol;
        lp.sepTol = sepTol;
        lp.anchorTol = anchorTol;
        lp.extentScale = extentScale;
        const CliqueOptions c =
            cliqueOptions(minMatched, maxMatched, results, disjoint,
                          distinctMinFrac, workers);
        PyProgress p(std::move(progress));
        const LineResult r = runReleased(
            p,
            [&] { return alignLines3D(left, right, cols, lp, c, &p); });
        py::list out;
        for (const LineAlignment& a : r.alignments)
        {
            py::dict d;
            d["pairs"] = pairArray(a.pairs);
            d["rotation"] = Eigen::Matrix3d(a.transform.linear());
            d["translation"] =
                Eigen::Vector3d(a.transform.translation());
            d["axis"] = a.axis;
            d["rms_perp"] = a.rmsPerp;
            d["axial_weak"] = a.axialWeak;
            d["explained"] = a.explained;
            out.append(d);
        }
        return py::make_tuple(r.status, r.message, out);
    }
}  // namespace

PYBIND11_MODULE(_core, m)
{
    m.doc() = "aligncalc compiled core; use the aligncalc package.";
#ifdef ALIGNCALC_PY_VERSION
    m.attr("__version__") = ALIGNCALC_PY_VERSION;
#endif
    py::enum_<Status>(m, "Status")
        .value("Ok", Status::Ok)
        .value("NoMatch", Status::NoMatch)
        .value("Cancelled", Status::Cancelled)
        .value("Error", Status::Error);
    m.def("align_points", &alignPointsPy, py::arg("left"),
          py::arg("right"), py::arg("dim"), py::arg("label_col"),
          py::arg("angle_col"), py::arg("similarity"),
          py::arg("epsilon"), py::arg("scale_range"),
          py::arg("anchor_tol"), py::arg("min_support"),
          py::arg("orientation_tol_deg"), py::arg("min_matched"),
          py::arg("max_matched"), py::arg("results"),
          py::arg("disjoint"), py::arg("distinct_min_frac"),
          py::arg("workers"), py::arg("progress"));
    m.def("match_points", &matchPointsPy, py::arg("left"),
          py::arg("right"), py::arg("dim"), py::arg("label_col"),
          py::arg("angle_col"), py::arg("similarity"),
          py::arg("epsilon"), py::arg("scale_range"),
          py::arg("anchor_tol"), py::arg("min_support"),
          py::arg("orientation_tol_deg"), py::arg("min_matched"),
          py::arg("max_matched"), py::arg("results"),
          py::arg("distinct_min_frac"), py::arg("workers"),
          py::arg("progress"));
    m.def("align_lines_3d", &alignLines3DPy, py::arg("left"),
          py::arg("right"), py::arg("label_col"), py::arg("angle_tol"),
          py::arg("sep_tol"), py::arg("anchor_tol"),
          py::arg("extent_scale"), py::arg("min_matched"),
          py::arg("max_matched"), py::arg("results"),
          py::arg("disjoint"), py::arg("distinct_min_frac"),
          py::arg("workers"), py::arg("progress"));
}
