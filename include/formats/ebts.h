#ifndef SHOECOMP_FORMATS_EBTS
#define SHOECOMP_FORMATS_EBTS

#include "ui/imageCanvas.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace shoecomp
{
    struct ImageCanvas2D;

    // Loads all image records (Type-13/14/15) from a NIST
    // ANSI/NIST-ITL transaction file. Each image record becomes a
    // canvas built by `makeCanvas` (so callers control the concrete
    // 2D kind). Type-9 EFS annotation data is matched to images by
    // IDC and stored in the annotations JSON. Returns the number of
    // images successfully loaded, or -1 on error.
    int loadNistFromDisk(
        const std::string& filePath,
        std::vector<std::unique_ptr<ImageCanvas>>& outCanvases,
        const std::function<std::unique_ptr<ImageCanvas2D>()>&
            makeCanvas);
} /* namespace shoecomp */

#endif
