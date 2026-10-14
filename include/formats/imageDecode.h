#ifndef SHOECOMP_FORMATS_IMAGE_DECODE_H
#define SHOECOMP_FORMATS_IMAGE_DECODE_H

#include <cstddef>
#include <string>
#include <vector>

namespace shoecomp
{
    // Decode a PNG/JPEG (anything stb_image reads) to 8-bit RGBA,
    // row-major, top-left origin. False with |err| set on failure.
    bool decodeImageFile(const std::string& path,
                         std::vector<unsigned char>& rgba, int& w,
                         int& h, std::string& err);
    bool decodeImageMemory(const unsigned char* data, size_t size,
                           std::vector<unsigned char>& rgba, int& w,
                           int& h, std::string& err);
}  // namespace shoecomp

#endif
