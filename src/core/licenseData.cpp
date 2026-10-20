#include "core/licenseData.h"
#include "core/embeddedLicenses.h"
#include "shoecompVersion.h"
#include "calc/workerChannel.h"
#include <cstdio>

namespace shoecomp
{
    const LicenseEntry& projectLicense()
    {
        static const LicenseEntry e = {
            "ShoeComp",
            SHOECOMP_VERSION,
            "MIT",
            "https://github.com/CSAFE-ISU/shoecomp",
            LicShoeComp_data,
            LicShoeComp_size,
            kInGui | kInCli};
        return e;
    }

    const std::vector<LicenseEntry>& getLicenses()
    {
        constexpr unsigned kBoth = kInGui | kInCli;
        // clang-format off
        static const std::vector<LicenseEntry> entries = {
            {"Dear ImGui", "1.92.6", "MIT",
             "https://github.com/ocornut/imgui",
             LicDearImGui_data, LicDearImGui_size, kInGui},
            {"Hello ImGui", "1.92.6", "MIT",
             "https://github.com/pthom/hello_imgui",
             LicHelloImGui_data, LicHelloImGui_size, kInGui},
            {"Eigen", "5.0.1", "MPL-2.0 + Apache/BSD",
             "https://gitlab.com/libeigen/eigen",
             LicEigen_data, LicEigen_size, kBoth},
            {"FreeType", "2.14.2", "FreeType License",
             "https://www.freetype.org/",
             LicFreetype_data, LicFreetype_size, kInGui},
            {"json.cpp", "", "Apache-2.0",
             "https://github.com/jart/json.cpp",
             LicJsonCpp_data, LicJsonCpp_size, kBoth},
            {"libpng", "1.6.55", "libpng/zlib License",
             "http://www.libpng.org/",
             LicLibpng_data, LicLibpng_size, kBoth},
            {"zlib", "1.3.2", "zlib License",
             "https://zlib.net/",
             LicZlib_data, LicZlib_size, kBoth},
            {"libjpeg-turbo", "3.1.2",
             "IJG License + BSD-3-Clause",
             "https://libjpeg-turbo.org/",
             LicLibjpegTurbo_data,
             LicLibjpegTurbo_size, kInGui},
            {"double-conversion", "", "BSD-3-Clause",
             "https://github.com/google/double-conversion",
             LicDoubleConversion_data,
             LicDoubleConversion_size, kBoth},
            {"stb_image", "2.29", "MIT",
             "https://github.com/nothings/stb",
             LicStbImage_data, LicStbImage_size, kBoth},
            {"CLI11", "2.7.2", "BSD-3-Clause",
             "https://github.com/CLIUtils/CLI11",
             LicCLI11_data, LicCLI11_size, kInCli},
            {"Inconsolata", "",
             "SIL Open Font License",
             "https://fonts.google.com/specimen/Inconsolata",
             LicInconsolata_data, LicInconsolata_size, kInGui},
            {"Montserrat", "",
             "SIL Open Font License",
             "https://fonts.google.com/specimen/Montserrat",
             LicMontserrat_data, LicMontserrat_size, kInGui},
            {"Material Icons", "", "Apache-2.0",
             "https://github.com/google/material-design-icons",
             LicMaterialIcons_data, LicMaterialIcons_size, kInGui},
        };
        // clang-format on
        return entries;
    }

    std::string licenseText(const LicenseEntry& e)
    {
        return std::string(reinterpret_cast<const char*>(e.data),
                           e.size);
    }

    std::vector<std::string> preloadLicenseTexts()
    {
        const auto& entries = getLicenses();
        std::vector<std::string> texts;
        texts.reserve(entries.size());
        for (const auto& e : entries) texts.push_back(licenseText(e));
        return texts;
    }

    std::vector<std::string> preloadLicenseTexts(WorkerChannel& channel)
    {
        const auto& entries = getLicenses();
        int n = (int)entries.size();
        std::vector<std::string> texts;
        texts.reserve(n);

        channel.is_running.store(true);

        for (int i = 0; i < n; ++i)
        {
            if (channel.should_cancel())
            {
                channel.cancelled();
                return texts;
            }
            const auto& e = entries[i];
            texts.push_back(licenseText(e));

            float p = (float)(i + 1) / (float)n;
            char msg[120];
            snprintf(msg, sizeof(msg), "Loading %s", e.name);
            channel.report(p, msg);
        }

        channel.done();
        return texts;
    }

}  // namespace shoecomp
