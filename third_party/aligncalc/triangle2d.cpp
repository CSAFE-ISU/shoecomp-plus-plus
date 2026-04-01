#include "aligncalc.internal.h"
#include <cmath>

namespace AlignCalc
{
    static constexpr double MIN_RATIO = 0.10f;
    static constexpr double MAX_RATIO = 10.0f;
    static constexpr double MIN_DIST = 1e-2;
    static constexpr double MAX_DIST = 1e8;
    static constexpr double PI = 3.14159265358979;

#define TAXICAB_METRIC(a1, a2, b1, b2, c1, c2)         \
    (std::fabs((a1) - (a2)) + std::fabs((b1) - (b2)) + \
     std::fabs((c1) - (c2)))

#define EUCDIST_METRIC(a1, a2, b1, b2, c1, c2) \
    (std::hypot((a1) - (a2), std::hypot((b1) - (b2), (c1) - (c2))))

#define ANGLE_COMPARE(a1, a2, b1, b2, c1, c2) \
    EUCDIST_METRIC((a1), (a2), (b1), (b2), (c1), (c2));

#define SRAT_COMPARE(a1, a2, b1, b2, c1, c2) \
    EUCDIST_METRIC((a1), (a2), (b1), (b2), (c1), (c2));

#define SIDE_RATIO(a1, a2, b1, b2, c1, c2) \
    (((a1) / (a2) + (b1) / (b2) + (c1) / (c2)) / 3);

    static inline double stable_angle(double u1, double u2, double v1,
                                      double v2)
    {
        // https://people.eecs.berkeley.edu/~wkahan/MathH110/Cross.pdf
        // Section 13
        double mod_u = std::hypot(u1, u2);
        double mod_v = std::hypot(v1, v2);
        double numerator = std::hypot(u1 * mod_v - v1 * mod_u,
                                      u2 * mod_v - v2 * mod_u);
        double denominator = std::hypot(u1 * mod_v + v1 * mod_u,
                                        u2 * mod_v + v2 * mod_u);
        return std::atan2(numerator, denominator);
    }

    static double angle_compare0(const Triangle2D &self,
                                 const Triangle2D &other)
    {
        double x = ANGLE_COMPARE(self.at, other.at, self.bt, other.bt,
                                 self.ct, other.ct);
        return x;
    }
    static double angle_compare1(const Triangle2D &self,
                                 const Triangle2D &other)
    {
        double x = ANGLE_COMPARE(self.at, other.at, self.bt, other.ct,
                                 self.ct, other.bt);
        return x;
    }
    static double angle_compare2(const Triangle2D &self,
                                 const Triangle2D &other)
    {
        double x = ANGLE_COMPARE(self.at, other.bt, self.bt, other.at,
                                 self.ct, other.ct);
        return x;
    }
    static double angle_compare3(const Triangle2D &self,
                                 const Triangle2D &other)
    {
        double x = ANGLE_COMPARE(self.at, other.bt, self.bt, other.ct,
                                 self.ct, other.at);
        return x;
    }
    static double angle_compare4(const Triangle2D &self,
                                 const Triangle2D &other)
    {
        double x = ANGLE_COMPARE(self.at, other.ct, self.bt, other.bt,
                                 self.ct, other.at);
        return x;
    }
    static double angle_compare5(const Triangle2D &self,
                                 const Triangle2D &other)
    {
        double x = ANGLE_COMPARE(self.at, other.ct, self.bt, other.at,
                                 self.ct, other.bt);
        return x;
    }
    //
    //
    static double sr_compare0(const Triangle2D &self,
                              const Triangle2D &other)
    {
        double r1 = self.as / other.as;
        double r2 = self.bs / other.bs;
        double r3 = self.cs / other.cs;
        double x = SRAT_COMPARE(r1, r2, r2, r3, r3, r1);
        return x;
    }
    static double sr_compare1(const Triangle2D &self,
                              const Triangle2D &other)
    {
        double r1 = self.as / other.as;
        double r2 = self.bs / other.cs;
        double r3 = self.cs / other.bs;
        double x = SRAT_COMPARE(r1, r2, r2, r3, r3, r1);
        return x;
    }
    static double sr_compare2(const Triangle2D &self,
                              const Triangle2D &other)
    {
        double r1 = self.as / other.bs;
        double r2 = self.bs / other.as;
        double r3 = self.cs / other.cs;
        double x = SRAT_COMPARE(r1, r2, r2, r3, r3, r1);
        return x;
    }
    static double sr_compare3(const Triangle2D &self,
                              const Triangle2D &other)
    {
        double r1 = self.as / other.bs;
        double r2 = self.bs / other.cs;
        double r3 = self.cs / other.as;
        double x = SRAT_COMPARE(r1, r2, r2, r3, r3, r1);
        return x;
    }
    static double sr_compare4(const Triangle2D &self,
                              const Triangle2D &other)
    {
        double r1 = self.as / other.cs;
        double r2 = self.bs / other.bs;
        double r3 = self.cs / other.as;
        double x = SRAT_COMPARE(r1, r2, r2, r3, r3, r1);
        return x;
    }
    static double sr_compare5(const Triangle2D &self,
                              const Triangle2D &other)
    {
        double r1 = self.as / other.cs;
        double r2 = self.bs / other.as;
        double r3 = self.cs / other.bs;
        double x = SRAT_COMPARE(r1, r2, r2, r3, r3, r1);
        return x;
    }
    //
    //
    static double side_ratio0(const Triangle2D &self,
                              const Triangle2D &other)
    {
        return SIDE_RATIO(self.as, other.as, self.bs, other.bs, self.cs,
                          other.cs);
    }
    static double side_ratio1(const Triangle2D &self,
                              const Triangle2D &other)
    {
        return SIDE_RATIO(self.as, other.as, self.bs, other.cs, self.cs,
                          other.bs);
    }
    static double side_ratio2(const Triangle2D &self,
                              const Triangle2D &other)
    {
        return SIDE_RATIO(self.as, other.bs, self.bs, other.as, self.cs,
                          other.cs);
    }
    static double side_ratio3(const Triangle2D &self,
                              const Triangle2D &other)
    {
        return SIDE_RATIO(self.as, other.bs, self.bs, other.cs, self.cs,
                          other.as);
    }
    static double side_ratio4(const Triangle2D &self,
                              const Triangle2D &other)
    {
        return SIDE_RATIO(self.as, other.cs, self.bs, other.bs, self.cs,
                          other.as);
    }
    static double side_ratio5(const Triangle2D &self,
                              const Triangle2D &other)
    {
        return SIDE_RATIO(self.as, other.cs, self.bs, other.as, self.cs,
                          other.bs);
    }
    //
    //
    void Triangle2D::construct(const DoubleMatrixR &pts, const int32_t ii,
                             const int32_t jj, const int32_t kk)
    {
        this->as = std::hypot(pts(kk, 0) - pts(jj, 0),
                              pts(kk, 1) - pts(jj, 1));
        this->at = stable_angle(
            pts(ii, 0) - pts(kk, 0), pts(ii, 1) - pts(kk, 1),
            pts(jj, 0) - pts(ii, 0), pts(jj, 1) - pts(ii, 1));
        this->bs = std::hypot(pts(ii, 0) - pts(kk, 0),
                              pts(ii, 1) - pts(kk, 1));
        this->bt = stable_angle(
            pts(jj, 0) - pts(ii, 0), pts(jj, 1) - pts(ii, 1),
            pts(kk, 0) - pts(jj, 0), pts(kk, 1) - pts(jj, 1));
        this->cs = std::hypot(pts(jj, 0) - pts(ii, 0),
                              pts(jj, 1) - pts(ii, 1));
        this->ct = stable_angle(
            pts(kk, 0) - pts(jj, 0), pts(kk, 1) - pts(jj, 1),
            pts(ii, 0) - pts(kk, 0), pts(ii, 1) - pts(kk, 1));
    }

    bool Triangle2D::valid() const
    {
        return (as > MIN_DIST && bs > MIN_DIST && cs > MIN_DIST);
    };

#define BINARY_CMP(n, other, delta, epsilon)          \
    ((angle_compare##n(*this, (other)) <= (delta)) && \
     (sr_compare##n(*this, (other)) <= (epsilon)) &&  \
     (side_ratio##n(*this, (other)) <= MAX_RATIO) &&  \
     (side_ratio##n(*this, (other)) >= MIN_RATIO))

    bool Triangle2D::compare(const Triangle2D &other, TaskInfo &task,
                           double delta, double epsilon) const
    {
        for (int i = 0; i < 8; ++i) task.check[i] = 0;
        task.check[0] = BINARY_CMP(0, other, delta, epsilon);
        task.check[1] = BINARY_CMP(1, other, delta, epsilon);
        task.check[2] = BINARY_CMP(2, other, delta, epsilon);
        task.check[3] = BINARY_CMP(3, other, delta, epsilon);
        task.check[4] = BINARY_CMP(4, other, delta, epsilon);
        task.check[5] = BINARY_CMP(5, other, delta, epsilon);
        for (int i = 0; i < 8; ++i)
        {
            if (task.check[i]) return true;
        }
        return false;
    };

} /* namespace AlignCalc */
