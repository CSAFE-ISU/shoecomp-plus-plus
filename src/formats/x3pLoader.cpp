#include "formats/x3p.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <vector>

#include "zlib.h"

namespace shoecomp
{
    namespace
    {
        // --- Little-endian scalar reads from a byte buffer ---
        uint16_t rd16(const uint8_t* p)
        {
            return (uint16_t)(p[0] | (p[1] << 8));
        }
        uint32_t rd32(const uint8_t* p)
        {
            return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) |
                              ((uint32_t)p[3] << 24));
        }

        // --- Minimal ZIP reader (store + deflate; no zip64) ---
        struct ZipEntry
        {
            uint16_t method = 0;
            uint32_t compSize = 0;
            uint32_t rawSize = 0;
            uint32_t localOffset = 0;
        };

        // Raw-DEFLATE decompress |src| (rawSize known) into |out|.
        bool inflateRaw(const uint8_t* src, size_t srcLen,
                        size_t rawSize, std::vector<uint8_t>& out,
                        std::string& err)
        {
            out.resize(rawSize);
            if (rawSize == 0) return true;
            z_stream s;
            std::memset(&s, 0, sizeof(s));
            // Negative window bits => raw deflate (no zlib/gzip
            // header).
            if (inflateInit2(&s, -MAX_WBITS) != Z_OK)
            {
                err = "zlib inflateInit2 failed";
                return false;
            }
            s.next_in = const_cast<Bytef*>(src);
            s.avail_in = (uInt)srcLen;
            s.next_out = out.data();
            s.avail_out = (uInt)rawSize;
            int r = inflate(&s, Z_FINISH);
            inflateEnd(&s);
            if (r != Z_STREAM_END)
            {
                err = "zlib inflate failed on compressed entry";
                return false;
            }
            return true;
        }

        class ZipArchive
        {
           public:
            bool open(const std::string& path, std::string& err)
            {
                std::ifstream f(path, std::ios::binary);
                if (!f)
                {
                    err = "cannot open file: " + path;
                    return false;
                }
                f.seekg(0, std::ios::end);
                std::streamoff n = f.tellg();
                if (n <= 0)
                {
                    err = "empty or unreadable file: " + path;
                    return false;
                }
                f.seekg(0, std::ios::beg);
                buf_.resize((size_t)n);
                f.read((char*)buf_.data(), n);
                if (!f)
                {
                    err = "failed to read file: " + path;
                    return false;
                }
                return parseCentralDir(err);
            }

            // Extract entry |name| into |out|. Returns false if the
            // entry is missing or cannot be decompressed.
            bool extract(const std::string& name,
                         std::vector<uint8_t>& out, std::string& err)
            {
                auto it = entries_.find(name);
                if (it == entries_.end())
                {
                    err = "x3p entry not found: " + name;
                    return false;
                }
                const ZipEntry& e = it->second;
                // Local header: recompute data offset from its own name
                // + extra field lengths.
                if (e.localOffset + 30 > buf_.size())
                {
                    err = "corrupt local header for: " + name;
                    return false;
                }
                const uint8_t* lh = buf_.data() + e.localOffset;
                if (rd32(lh) != 0x04034b50)
                {
                    err = "bad local header signature for: " + name;
                    return false;
                }
                uint16_t nameLen = rd16(lh + 26);
                uint16_t extraLen = rd16(lh + 28);
                size_t dataOff =
                    e.localOffset + 30 + nameLen + extraLen;
                if (dataOff + e.compSize > buf_.size())
                {
                    err = "entry data out of bounds: " + name;
                    return false;
                }
                const uint8_t* data = buf_.data() + dataOff;
                if (e.method == 0)  // stored
                {
                    out.assign(data, data + e.compSize);
                    return true;
                }
                if (e.method == 8)  // deflate
                    return inflateRaw(data, e.compSize, e.rawSize, out,
                                      err);
                err = "unsupported ZIP compression method for: " + name;
                return false;
            }

           private:
            bool parseCentralDir(std::string& err)
            {
                // Scan backwards for the End Of Central Directory
                // record.
                if (buf_.size() < 22)
                {
                    err = "file too small to be a ZIP (x3p)";
                    return false;
                }
                size_t maxBack =
                    std::min<size_t>(buf_.size(), 22 + 65535);
                size_t start = buf_.size() - maxBack;
                long eocd = -1;
                for (size_t i = buf_.size() - 22 + 1; i-- > start;)
                {
                    if (rd32(buf_.data() + i) == 0x06054b50)
                    {
                        eocd = (long)i;
                        break;
                    }
                }
                if (eocd < 0)
                {
                    err = "not a valid ZIP archive (no EOCD); not x3p?";
                    return false;
                }
                const uint8_t* e = buf_.data() + eocd;
                uint16_t count = rd16(e + 10);
                uint32_t cdSize = rd32(e + 12);
                uint32_t cdOff = rd32(e + 16);
                if (cdOff == 0xFFFFFFFF || cdSize == 0xFFFFFFFF)
                {
                    err = "zip64 x3p archives are not supported";
                    return false;
                }
                if ((size_t)cdOff + cdSize > buf_.size())
                {
                    err = "corrupt ZIP central directory";
                    return false;
                }
                size_t p = cdOff;
                for (uint16_t i = 0; i < count; ++i)
                {
                    if (p + 46 > buf_.size() ||
                        rd32(buf_.data() + p) != 0x02014b50)
                    {
                        err = "corrupt ZIP central directory entry";
                        return false;
                    }
                    const uint8_t* c = buf_.data() + p;
                    ZipEntry ent;
                    ent.method = rd16(c + 10);
                    ent.compSize = rd32(c + 20);
                    ent.rawSize = rd32(c + 24);
                    uint16_t nameLen = rd16(c + 28);
                    uint16_t extraLen = rd16(c + 30);
                    uint16_t commentLen = rd16(c + 32);
                    ent.localOffset = rd32(c + 42);
                    if (ent.compSize == 0xFFFFFFFF ||
                        ent.rawSize == 0xFFFFFFFF ||
                        ent.localOffset == 0xFFFFFFFF)
                    {
                        err = "zip64 x3p archives are not supported";
                        return false;
                    }
                    if (p + 46 + nameLen > buf_.size())
                    {
                        err = "corrupt ZIP entry name";
                        return false;
                    }
                    std::string name((const char*)(c + 46), nameLen);
                    entries_[name] = ent;
                    p += 46 + nameLen + extraLen + commentLen;
                }
                return true;
            }

            std::vector<uint8_t> buf_;
            std::map<std::string, ZipEntry> entries_;
        };

        // --- Minimal XML value extraction (well-formed x3p only) ---
        std::string trim(const std::string& s)
        {
            size_t a = s.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) return "";
            size_t b = s.find_last_not_of(" \t\r\n");
            return s.substr(a, b - a + 1);
        }

        // Inner substring of the first <tag>...</tag> in |s| (searching
        // from |from|). Returns "" if not found or self-closed.
        std::string tagText(const std::string& s,
                            const std::string& tag, size_t from = 0)
        {
            std::string open = "<" + tag + ">";
            std::string close = "</" + tag + ">";
            size_t a = s.find(open, from);
            if (a == std::string::npos) return "";
            a += open.size();
            size_t b = s.find(close, a);
            if (b == std::string::npos) return "";
            return trim(s.substr(a, b - a));
        }

        // Whole <tag>...</tag> block (inner content) for a container
        // element (e.g. CX / MatrixDimension).
        std::string tagBlock(const std::string& s,
                             const std::string& tag)
        {
            return tagText(s, tag);
        }

        double toDouble(const std::string& s, double dflt)
        {
            if (s.empty()) return dflt;
            try
            {
                return std::stod(s);
            }
            catch (...)
            {
                return dflt;
            }
        }

        int toInt(const std::string& s, int dflt)
        {
            if (s.empty()) return dflt;
            try
            {
                return std::stoi(s);
            }
            catch (...)
            {
                return dflt;
            }
        }

        // Decode |bytes| as N values of DataType |dt| (F/D/I/L) into
        // the row-major matrix storage |dst| (length N floats).
        // Little-endian.
        bool decodeHeights(const std::vector<uint8_t>& bytes, char dt,
                           size_t n, float* dst, std::string& err)
        {
            size_t elem = (dt == 'F')   ? 4
                          : (dt == 'D') ? 8
                          : (dt == 'I') ? 2
                          : (dt == 'L') ? 4
                                        : 0;
            if (elem == 0)
            {
                err = "unsupported x3p DataType (expected F/D/I/L)";
                return false;
            }
            if (bytes.size() < n * elem)
            {
                err = "x3p data.bin smaller than declared matrix";
                return false;
            }
            const uint8_t* p = bytes.data();
            for (size_t i = 0; i < n; ++i, p += elem)
            {
                switch (dt)
                {
                    case 'F':
                    {
                        float v;
                        std::memcpy(&v, p, 4);
                        dst[i] = v;
                        break;
                    }
                    case 'D':
                    {
                        double v;
                        std::memcpy(&v, p, 8);
                        dst[i] = (float)v;
                        break;
                    }
                    case 'I':
                    {
                        int16_t v;
                        std::memcpy(&v, p, 2);
                        dst[i] = (float)v;
                        break;
                    }
                    case 'L':
                    {
                        int32_t v;
                        std::memcpy(&v, p, 4);
                        dst[i] = (float)v;
                        break;
                    }
                }
            }
            return true;
        }
    }  // namespace

    bool loadX3P(const std::string& path, Surface3D& out,
                 std::string& err)
    {
        ZipArchive zip;
        if (!zip.open(path, err)) return false;

        std::vector<uint8_t> xmlBytes;
        if (!zip.extract("main.xml", xmlBytes, err)) return false;
        std::string xml((const char*)xmlBytes.data(), xmlBytes.size());

        // Matrix dimensions.
        std::string md = tagBlock(xml, "MatrixDimension");
        if (md.empty())
        {
            err = "x3p main.xml missing MatrixDimension";
            return false;
        }
        int sizeX = toInt(tagText(md, "SizeX"), 0);
        int sizeY = toInt(tagText(md, "SizeY"), 0);
        int sizeZ = toInt(tagText(md, "SizeZ"), 1);
        if (sizeX <= 0 || sizeY <= 0)
        {
            err = "x3p has invalid MatrixDimension";
            return false;
        }
        if (sizeZ != 1)
        {
            err = "x3p with multiple Z layers is not supported";
            return false;
        }

        // Axes.
        std::string axes = tagBlock(xml, "Axes");
        std::string cx = tagBlock(axes, "CX");
        std::string cy = tagBlock(axes, "CY");
        std::string cz = tagBlock(axes, "CZ");
        if (cx.empty() || cy.empty() || cz.empty())
        {
            err = "x3p main.xml missing axis definitions";
            return false;
        }
        std::string axX = tagText(cx, "AxisType");
        std::string axY = tagText(cy, "AxisType");
        if (axX != "I" || axY != "I")
        {
            err =
                "only incremental X/Y x3p surfaces are supported "
                "(absolute-coordinate point lists are not)";
            return false;
        }

        // The stored matrix holds Z values; their binary type is CZ's
        // DataType (falls back to CX's if absent).
        std::string dtStr = tagText(cz, "DataType");
        if (dtStr.empty()) dtStr = tagText(cx, "DataType");
        if (dtStr.empty())
        {
            err = "x3p main.xml missing DataType";
            return false;
        }
        char dt = dtStr[0];

        // Data payload location (honour PointDataLink, default path).
        std::string dl = tagBlock(xml, "DataLink");
        std::string binName = tagText(dl, "PointDataLink");
        if (binName.empty()) binName = "bindata/data.bin";

        std::vector<uint8_t> bin;
        if (!zip.extract(binName, bin, err)) return false;

        out.sizeX = sizeX;
        out.sizeY = sizeY;
        out.incX = toDouble(tagText(cx, "Increment"), 1.0);
        out.incY = toDouble(tagText(cy, "Increment"), 1.0);
        out.incZ = toDouble(tagText(cz, "Increment"), 1.0);
        out.offX = toDouble(tagText(cx, "Offset"), 0.0);
        out.offY = toDouble(tagText(cy, "Offset"), 0.0);
        out.offZ = toDouble(tagText(cz, "Offset"), 0.0);

        out.z.resize(sizeY, sizeX);
        size_t count = (size_t)sizeX * (size_t)sizeY;
        if (!decodeHeights(bin, dt, count, out.z.data(), err))
            return false;

        // Finite Z range for colormap scaling.
        float lo = std::numeric_limits<float>::infinity();
        float hi = -std::numeric_limits<float>::infinity();
        const float* z = out.z.data();
        for (size_t i = 0; i < count; ++i)
        {
            float v = z[i];
            if (std::isfinite(v))
            {
                if (v < lo) lo = v;
                if (v > hi) hi = v;
            }
        }
        if (!std::isfinite(lo))  // all invalid
        {
            lo = 0.0f;
            hi = 0.0f;
        }
        out.zMin = lo;
        out.zMax = hi;

        // Name/path.
        size_t slash = path.find_last_of("/\\");
        out.name =
            slash == std::string::npos ? path : path.substr(slash + 1);
        out.path = path;
        out.annotations = jt::Json();
        return true;
    }
}  // namespace shoecomp
