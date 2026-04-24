#include "ui/licenseData.h"
#include "ui/embeddedAssets.h"

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

}  // namespace shoecomp
