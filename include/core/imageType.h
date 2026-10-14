#ifndef SHOECOMP_CORE_IMAGE_TYPE_H
#define SHOECOMP_CORE_IMAGE_TYPE_H

#include <string>
#include <vector>

#include "core/pointType.h"

namespace shoecomp
{
    // The comparable input types, independent of any canvas. The GUI's
    // Shoe/HW/EBTS/X3PBulletLand canvases and the CLI's --type map onto
    // these.
    enum class ImageType
    {
        Shoe,
        HW,
        EBTS,
        X3P
    };

    // Describes how a type's YOLO-style bbox model is run and how its
    // outputs map onto annotation points (calc/onnxRuntime).
    struct DetectionSpec
    {
        int inputWidth = 640;
        int inputHeight = 640;
        bool nchw = true;       // NCHW layout (vs NHWC)
        bool normalize = true;  // divide pixel values by 255
        float confThreshold = 0.25f;
        float iouThreshold = 0.45f;
        // Model class index -> annotation PointType. A detection whose
        // class index is out of range is skipped.
        std::vector<PointType> classToPointType;
    };

    // CLI name: "shoe", "hw", "ebts", "x3p".
    const char* imageTypeName(ImageType t);
    bool imageTypeFromName(const std::string& name, ImageType& out);
    const std::vector<ImageType>& allImageTypes();

    bool is3DType(ImageType t);
    const std::vector<std::string>& extensionsFor(ImageType t);
    std::vector<PointType> pointTypesFor(ImageType t);

    // Point detection exists for the 2-D types only.
    bool detectionSupported(ImageType t);
    DetectionSpec detectionSpecFor(ImageType t);
}  // namespace shoecomp

#endif
