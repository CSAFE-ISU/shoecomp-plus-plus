#ifndef SHOECOMP_FORMATS_X3P_H
#define SHOECOMP_FORMATS_X3P_H

#include <string>
#include "jtjson/json.h"
#include "aligncalc/eigen/Eigen/Core"

// x3p (ISO 25178-72 / ISO5436_2) surface-topography loader.
//
// An x3p file is a ZIP container holding main.xml (metadata + axis
// definitions), bindata/data.bin (the binary height matrix), an
// optional bindata/mask.png, and a checksum file. This loader supports
// the common SUR (surface) case: incremental X/Y axes plus an absolute
// Z axis, so data.bin is a dense matrix of Z values only, stored
// row-major with X as the fastest index. Physical coordinates (metres)
// are recovered from the per-axis increment/offset.
namespace shoecomp
{
    // Dense height grid. RowMajor, sized (sizeY, sizeX), so the memory
    // layout matches data.bin (X fastest) and element access is
    // z(row, col). Invalid samples are stored as NaN.
    using HeightMatrix = Eigen::Matrix<float, Eigen::Dynamic,
                                       Eigen::Dynamic, Eigen::RowMajor>;

    struct Surface3D
    {
        int sizeX = 0;  // columns (fastest axis)
        int sizeY = 0;  // rows

        // Metres per grid step and coordinate origin, per axis.
        double incX = 1.0, incY = 1.0, incZ = 1.0;
        double offX = 0.0, offY = 0.0, offZ = 0.0;

        HeightMatrix z;     // (sizeY, sizeX); NaN = invalid
        float zMin = 0.0f;  // over finite values only
        float zMax = 0.0f;

        std::string name;      // basename of the source file
        std::string path;      // full source path
        jt::Json annotations;  // { "points": [...], "bounds": [...] }

        bool valid() const
        {
            return sizeX > 0 && sizeY > 0 && z.size() > 0;
        }

        // Physical coordinates in metres.
        double px(int col) const { return offX + col * incX; }
        double py(int row) const { return offY + row * incY; }
        double pz(float value) const
        {
            return offZ + (double)value * incZ;
        }
    };

    // Parse the x3p file at |path| into |out|. Only the SUR matrix case
    // (incremental X/Y, absolute Z, single Z layer) is supported.
    // Returns false and sets |err| on any I/O error, malformed
    // container/XML, or unsupported layout. Annotations are left empty
    // (loaded separately). Height values are converted to float; NaN
    // marks invalid samples in float source formats.
    bool loadX3P(const std::string& path, Surface3D& out,
                 std::string& err);
}  // namespace shoecomp

#endif
