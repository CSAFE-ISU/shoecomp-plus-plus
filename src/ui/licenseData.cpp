#include "ui/licenseData.h"
#include "ui/embeddedAssets.h"
#include "aligncalc/workerChannel.h"
#include <cstdio>

namespace shoecomp
{
    const std::vector<LicenseEntry>& getLicenses()
    {
        // clang-format off
        static const std::vector<LicenseEntry> entries = {
            {"Dear ImGui", "1.92.6", "MIT",
             "https://github.com/ocornut/imgui",
             LicDearImGui_data, LicDearImGui_size},
            {"Hello ImGui", "1.92.6", "MIT",
             "https://github.com/pthom/hello_imgui",
             LicHelloImGui_data, LicHelloImGui_size},
            {"Eigen", "5.0.1", "MPL-2.0",
             "https://gitlab.com/libeigen/eigen",
             LicEigen_data, LicEigen_size},
            {"FreeType", "2.14.2", "FreeType License",
             "https://www.freetype.org/",
             LicFreetype_data, LicFreetype_size},
            {"json.cpp", "", "Apache-2.0",
             "https://github.com/jart/json.cpp",
             LicJsonCpp_data, LicJsonCpp_size},
            {"libpng", "1.6.55", "libpng/zlib License",
             "http://www.libpng.org/",
             LicLibpng_data, LicLibpng_size},
            {"zlib", "1.3.2", "zlib License",
             "https://zlib.net/",
             LicZlib_data, LicZlib_size},
            {"libjpeg-turbo", "3.1.2",
             "IJG License + BSD-3-Clause",
             "https://libjpeg-turbo.org/",
             LicLibjpegTurbo_data,
             LicLibjpegTurbo_size},
            {"double-conversion", "", "BSD-3-Clause",
             "https://github.com/google/double-conversion",
             LicDoubleConversion_data,
             LicDoubleConversion_size},
            {"Inconsolata", "",
             "SIL Open Font License",
             "https://fonts.google.com/specimen/Inconsolata",
             LicInconsolata_data, LicInconsolata_size},
            {"Montserrat", "",
             "SIL Open Font License",
             "https://fonts.google.com/specimen/Montserrat",
             LicMontserrat_data, LicMontserrat_size},
        };
        // clang-format on
        return entries;
    }

    std::vector<std::string> preloadLicenseTexts()
    {
        const auto& entries = getLicenses();
        std::vector<std::string> texts;
        texts.reserve(entries.size());
        for (const auto& e : entries)
            texts.emplace_back(reinterpret_cast<const char*>(e.data),
                               e.size);
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
            texts.emplace_back(reinterpret_cast<const char*>(e.data),
                               e.size);

            float p = (float)(i + 1) / (float)n;
            char msg[120];
            snprintf(msg, sizeof(msg), "Loading %s", e.name);
            channel.report(p, msg);
        }

        channel.done();
        return texts;
    }

}  // namespace shoecomp
