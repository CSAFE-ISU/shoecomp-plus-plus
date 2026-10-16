#ifndef SHOECOMP_CORE_PARAMS_H
#define SHOECOMP_CORE_PARAMS_H

#include <cstddef>
#include <string>
#include <vector>

#include "aligncalc/aligncalc.h"
#include "calc/align3d.h"
#include "calc/striationDetect.h"

// Tuning parameters shared by the GUI dialogs and the scpp CLI, in
// the units the dialogs show. One descriptor table drives the INI
// file, the CLI flags and validation.
namespace shoecomp
{
    struct Detect2DSettings
    {
        std::string model;  // .onnx path; empty = no detection
        float confThreshold = 0.25f;
        float iouThreshold = 0.45f;
    };

    struct StriationSettings
    {
        float minLengthMm = 0.1f;
        float minHeightUm = 0.2f;
        float maxSurfDevDeg = 20.0f;
        int maxCount = 200;
        bool includeValleys = true;
        bool excludeShoulders = true;
        float shoulderMarginUm = 20.0f;

        StriationParams toParams(int numThreads = 0) const;
    };

    // Clique search bounds and results, for 2-D and 3-D alike.
    struct CliqueSettings
    {
        int minMatched = 5;
        int maxMatched = 0;  // 0 = fewer of the two counts
        int numResults = 2;  // distinct alignments, best first
        // "explained" or "disjoint" (AlignCalc::DistinctMode)
        std::string distinct = "explained";

        AlignCalc::DistinctMode distinctMode() const;
    };

    struct Align3DSettings
    {
        float angleTolDeg = 0.5f;
        float sepTolUm = 4.0f;
        float maxLengthRatio = 0.0f;   // 0 = off
        float extentScale = 0.0f;      // 0 = off
        float maxRotationDeg = 30.0f;  // 0 = off

        StriationAlignParams toParams(size_t numWorkers,
                                      const CliqueSettings& c) const;
    };

    // EBTS: allowed mismatch of relative minutia directions.
    constexpr float kDefaultMinutiaAngleTolDeg = 20.0f;

    struct Params
    {
        Detect2DSettings detect2d;
        // Bounds, results and distinct come from |clique| (see
        // rtsParams()); numWorkers is a run option, not a parameter.
        AlignCalc::RTSParams align2d;
        float minutiaAngleTolDeg = kDefaultMinutiaAngleTolDeg;
        StriationSettings striation;
        Align3DSettings align3d;
        CliqueSettings clique;

        // align2d with |clique| applied
        AlignCalc::RTSParams rtsParams() const;
    };

    enum class ParamKind
    {
        Bool,
        Int,
        Size,
        Float,
        Double,
        String
    };

    struct ParamDesc
    {
        const char* section;
        const char* key;
        const char* help;
        ParamKind kind;
        void* (*ref)(Params&);
    };

    const std::vector<ParamDesc>& paramTable();

    // Value as written to INI / shown as a CLI default.
    std::string paramValueString(const ParamDesc& d, Params& p);
    // Parses |value| into the parameter; false with |err| on a bad
    // value.
    bool setParamValue(const ParamDesc& d, Params& p,
                       const std::string& value, std::string& err);
    // "--align3d-sep-tol-um" for [align3d] sep_tol_um.
    std::string paramFlag(const ParamDesc& d);

    std::string paramsToIni(const Params& p);
    // Applies the keys present in |text| on top of |p|. Unknown
    // sections/keys and bad values fail with a line number in |err|.
    bool paramsFromIni(const std::string& text, Params& p,
                       std::string& err);
    bool loadParamsFile(const std::string& path, Params& p,
                        std::string& err);

    // Range checks matching the GUI widgets' limits.
    bool validateParams(const Params& p, std::string& err);
}  // namespace shoecomp

#endif
