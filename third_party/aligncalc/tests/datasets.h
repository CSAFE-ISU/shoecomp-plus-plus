#ifndef ALIGNCALC_TESTS_DATASETS_H
#define ALIGNCALC_TESTS_DATASETS_H
// Seeded 2-D datasets: right = a rigid (sameScale) or similarity copy
// of a left subset, plus random points; rows are (x, y, label 0).
#include "aligncalc/alignTypes.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

struct Dataset
{
    int n;
    bool sameScale;
    double rot, scale, tx, ty;  // T(left) = right
    AlignCalc::DoubleMatrixR L, R;
    std::vector<int> planted;  // left * 1000 + right, sorted
};

inline std::vector<Dataset> datasets()
{
    std::vector<Dataset> out;
    std::mt19937 rng(77);
    std::uniform_real_distribution<double> U(0, 1000);
    std::normal_distribution<double> noise(0, 0.05);
    for (int rep = 0; rep < 40; ++rep)
        for (int ss = 0; ss < 2; ++ss)
        {
            Dataset d;
            d.n = 8 + rng() % 18;
            d.sameScale = ss;
            d.rot = U(rng) / 1000.0 * 6.0 - 3.0;
            d.scale = ss ? 1.0 : 0.5 + U(rng) / 1000.0 * 1.5;
            d.tx = U(rng) - 500;
            d.ty = U(rng) - 500;
            d.L.setZero(d.n, 3);
            d.R.setZero(d.n, 3);
            for (int i = 0; i < d.n; ++i)
            {
                d.L(i, 0) = U(rng);
                d.L(i, 1) = U(rng);
                d.R(i, 0) = U(rng);
                d.R(i, 1) = U(rng);
            }
            const int k = 4 + rng() % (d.n / 2);
            const double c = d.scale * std::cos(d.rot),
                         s = d.scale * std::sin(d.rot);
            for (int i = 0; i < k; ++i)
            {
                const int r = (i * 5 + 3) % d.n;
                const double x = d.L(i, 0), y = d.L(i, 1);
                d.R(r, 0) = c * x - s * y + d.tx + noise(rng);
                d.R(r, 1) = s * x + c * y + d.ty + noise(rng);
                d.planted.push_back(i * 1000 + r);
            }
            std::sort(d.planted.begin(), d.planted.end());
            out.push_back(std::move(d));
        }
    return out;
}

#endif
