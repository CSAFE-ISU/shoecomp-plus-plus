#ifndef SHOECOMP_FORMATS_EBTS
#define SHOECOMP_FORMATS_EBTS

#include "ui/imageCanvas.h"
#include <string>
#include <vector>

namespace shoecomp
{
    // Loads all image records (Type-13/14/15) from a NIST
    // ANSI/NIST-ITL transaction file. Each image record becomes
    // an ImageCanvas entry. Type-9 EFS annotation data is matched
    // to images by IDC and stored in the annotations JSON.
    // Returns the number of images successfully loaded, or -1 on
    // error.
    int loadNistFromDisk(const std::string& filePath,
                         std::vector<ImageCanvas>& outCanvases);
} /* namespace shoecomp */

#endif
