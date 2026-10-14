#ifndef SHOECOMP_FORMATS_EBTS
#define SHOECOMP_FORMATS_EBTS

#include <cstdint>
#include <string>
#include <vector>
#include "jtjson/json.h"

namespace shoecomp
{
    // One PNG-compressed image record (Type-13/14/15) of a NIST
    // ANSI/NIST-ITL transaction, with its Type-9 EFS minutiae, cores
    // and deltas (matched by IDC) as annotation points.
    struct EBTSImage
    {
        std::string name;  // "<file> [Type-T IDC:NN FGP:F]"
        int recordType = 0;
        int idc = 0;
        int fgp = 0;
        double ppi = 500.0;
        std::vector<uint8_t> encoded;  // PNG bytes
        jt::Json annotations;          // { bounds, points, EBTS }
    };

    // Parses every PNG image record of the file at |filePath| into
    // |out| (appended, file order). Returns the count, or -1 with
    // |err| set.
    int parseEBTS(const std::string& filePath,
                  std::vector<EBTSImage>& out, std::string& err);
} /* namespace shoecomp */

#endif
