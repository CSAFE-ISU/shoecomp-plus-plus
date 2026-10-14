#include <algorithm>
#include <cmath>
#include <string>

#include "cli/cli.h"

namespace shoecomp
{
    namespace
    {
        // 5x7 digits, one byte per row, bit 4 = leftmost column.
        const unsigned char kDigits[10][7] = {
            {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
            {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
            {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
            {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
            {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
            {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
            {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
            {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
            {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
            {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}};

        struct Canvas
        {
            std::vector<unsigned char>& px;
            int w, h;

            void blend(int x, int y, const PointColor& c)
            {
                if (x < 0 || y < 0 || x >= w || y >= h) return;
                unsigned char* p = &px[((size_t)y * w + x) * 4];
                const float rgb[3] = {c.r, c.g, c.b};
                for (int k = 0; k < 3; ++k)
                    p[k] = (unsigned char)std::lround(
                        p[k] * (1.0f - c.a) + 255.0f * rgb[k] * c.a);
                p[3] = 255;
            }

            void rect(int x0, int y0, int x1, int y1,
                      const PointColor& c)
            {
                for (int y = std::max(0, y0); y < std::min(h, y1); ++y)
                    for (int x = std::max(0, x0); x < std::min(w, x1);
                         ++x)
                        blend(x, y, c);
            }

            // Disc of radius r0..r1 (r0 = 0: filled).
            void ring(double cx, double cy, double r0, double r1,
                      const PointColor& c)
            {
                const int x0 = (int)std::floor(cx - r1);
                const int x1 = (int)std::ceil(cx + r1);
                const int y0 = (int)std::floor(cy - r1);
                const int y1 = (int)std::ceil(cy + r1);
                for (int y = y0; y <= y1; ++y)
                    for (int x = x0; x <= x1; ++x)
                    {
                        const double d =
                            std::hypot(x + 0.5 - cx, y + 0.5 - cy);
                        if (d <= r1 && d >= r0) blend(x, y, c);
                    }
            }
        };
    }  // namespace

    void drawMarkers(std::vector<unsigned char>& rgba, int w, int h,
                     const std::vector<Marker>& markers)
    {
        Canvas cv{rgba, w, h};
        const double r = std::max(4.0, std::min(w, h) / 150.0);
        const double edge = std::max(1.0, r / 4.0);
        const int s =
            std::max(1, (int)std::lround(r / 4.0));  // px/cell
        const PointColor dark{0.0f, 0.0f, 0.0f, 0.9f};
        const PointColor white{1.0f, 1.0f, 1.0f, 0.85f};

        for (const Marker& m : markers)
        {
            cv.ring(m.x, m.y, 0.0, r - edge, m.color);
            cv.ring(m.x, m.y, r - edge, r, dark);

            const std::string digits = std::to_string(m.number);
            const int gw = 6 * s;  // 5 columns + 1 gap
            const int x0 = (int)std::lround(m.x + r + 2.0 * s);
            const int y0 = (int)std::lround(m.y - 3.5 * s);
            cv.rect(x0 - s, y0 - s, x0 + (int)digits.size() * gw,
                    y0 + 8 * s, white);
            for (size_t i = 0; i < digits.size(); ++i)
            {
                const unsigned char* g = kDigits[digits[i] - '0'];
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                    {
                        if (!(g[row] & (0x10 >> col))) continue;
                        const int px = x0 + (int)i * gw + col * s;
                        const int py = y0 + row * s;
                        cv.rect(px, py, px + s, py + s,
                                {0.0f, 0.0f, 0.0f, 1.0f});
                    }
            }
        }
    }
}  // namespace shoecomp
