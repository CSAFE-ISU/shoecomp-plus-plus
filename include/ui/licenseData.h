#ifndef SHOECOMP_UI_LICENSE_DATA
#define SHOECOMP_UI_LICENSE_DATA

#include <string>
#include <vector>

struct WorkerChannel;

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

    // Pre-convert all embedded license bytes to strings,
    // reporting progress through the channel.
    std::vector<std::string> preloadLicenseTexts(
        WorkerChannel& channel);

}  // namespace shoecomp

#endif
