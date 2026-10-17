#ifndef SHOECOMP_CORE_LICENSE_DATA
#define SHOECOMP_CORE_LICENSE_DATA

#include <string>
#include <vector>

namespace shoecomp
{
    struct WorkerChannel;

    // Which binaries contain a component.
    enum LicenseBinaries : unsigned
    {
        kInGui = 1u,
        kInCli = 2u
    };

    struct LicenseEntry
    {
        const char* name;
        const char* version;
        const char* licenseType;
        const char* url;
        const unsigned char* data;
        unsigned int size;
        unsigned binaries;  // LicenseBinaries mask
    };

    // ShoeComp itself (version = SHOECOMP_VERSION).
    const LicenseEntry& projectLicense();

    // Third-party components, in display order.
    const std::vector<LicenseEntry>& getLicenses();

    std::string licenseText(const LicenseEntry& e);

    // Pre-convert all embedded license bytes to strings.
    std::vector<std::string> preloadLicenseTexts();

    // Overload that reports progress through a channel.
    std::vector<std::string> preloadLicenseTexts(
        WorkerChannel& channel);

}  // namespace shoecomp

#endif
