#ifndef SHOECOMP_UI_LICENSE_DATA
#define SHOECOMP_UI_LICENSE_DATA

#include <string>
#include <vector>

namespace shoecomp
{
    struct LicenseEntry
    {
        const char* name;
        const char* version;
        const char* licenseType;
        const char* url;
        const unsigned char* data;
        unsigned int size;
    };

    const std::vector<LicenseEntry>& getLicenses();

}  // namespace shoecomp

#endif
