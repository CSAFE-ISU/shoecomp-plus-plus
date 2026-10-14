#include "core/imageType.h"

namespace shoecomp
{
    const char* imageTypeName(ImageType t)
    {
        switch (t)
        {
            case ImageType::Shoe:
                return "shoe";
            case ImageType::HW:
                return "hw";
            case ImageType::EBTS:
                return "ebts";
            case ImageType::X3P:
            default:
                return "x3p";
        }
    }

    const std::vector<ImageType>& allImageTypes()
    {
        static const std::vector<ImageType> types = {
            ImageType::Shoe, ImageType::HW, ImageType::EBTS,
            ImageType::X3P};
        return types;
    }

    bool imageTypeFromName(const std::string& name, ImageType& out)
    {
        for (ImageType t : allImageTypes())
        {
            if (name == imageTypeName(t))
            {
                out = t;
                return true;
            }
        }
        return false;
    }

    bool is3DType(ImageType t) { return t == ImageType::X3P; }

    const std::vector<std::string>& extensionsFor(ImageType t)
    {
        static const std::vector<std::string> raster = {
            ".png", ".PNG", ".jpg", ".JPG", ".jpeg", ".JPEG"};
        static const std::vector<std::string> ebts = {
            ".png", ".PNG", ".an2", ".AN2", ".irr", ".lffs", ".ebts"};
        static const std::vector<std::string> x3p = {".x3p", ".X3P"};
        switch (t)
        {
            case ImageType::EBTS:
                return ebts;
            case ImageType::X3P:
                return x3p;
            case ImageType::Shoe:
            case ImageType::HW:
            default:
                return raster;
        }
    }

    std::vector<PointType> pointTypesFor(ImageType t)
    {
        switch (t)
        {
            case ImageType::Shoe:
                return {PointType::Corner, PointType::Center};
            case ImageType::HW:
                return {PointType::WordStart, PointType::WordEnd,
                        PointType::Intersection, PointType::CurveTurn};
            case ImageType::EBTS:
                return {PointType::RidgeEnding, PointType::Bifurcation,
                        PointType::Core, PointType::Delta,
                        PointType::Other};
            case ImageType::X3P:
            default:
                return {PointType::Peak};
        }
    }

    bool detectionSupported(ImageType t) { return !is3DType(t); }

    DetectionSpec detectionSpecFor(ImageType t)
    {
        DetectionSpec spec;
        switch (t)
        {
            case ImageType::Shoe:
                spec.classToPointType = {PointType::Corner,
                                         PointType::Center};
                break;
            case ImageType::HW:
                spec.classToPointType = {
                    PointType::WordStart, PointType::WordEnd,
                    PointType::Intersection, PointType::CurveTurn};
                break;
            case ImageType::EBTS:
                spec.classToPointType = {
                    PointType::RidgeEnding, PointType::Bifurcation,
                    PointType::Core, PointType::Delta,
                    PointType::Other};
                break;
            case ImageType::X3P:
            default:
                break;
        }
        return spec;
    }
}  // namespace shoecomp
