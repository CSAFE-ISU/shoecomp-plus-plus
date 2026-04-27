#include "formats/ebts.h"
#include "formats/png.h"
#include "stb_image.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace shoecomp
{
    namespace
    {
        constexpr uint8_t kFS = 0x1C;
        constexpr uint8_t kGS = 0x1D;
        constexpr uint8_t kRS = 0x1E;
        constexpr uint8_t kUS = 0x1F;

        struct EBTSField
        {
            int recordType = 0;
            int fieldNumber = 0;
            std::string rawValue;
        };

        struct EBTSRecord
        {
            int recordType = 0;
            int idc = 0;
            std::vector<EBTSField> fields;

            const EBTSField* findField(int num) const
            {
                for (auto& f : fields)
                {
                    if (f.fieldNumber == num) return &f;
                }
                return nullptr;
            }

            std::string fieldValue(int num) const
            {
                auto* f = findField(num);
                return f ? f->rawValue : std::string();
            }

            int fieldInt(int num) const
            {
                auto* f = findField(num);
                if (!f || f->rawValue.empty()) return 0;
                return std::atoi(f->rawValue.c_str());
            }
        };

        struct EBTSTransaction
        {
            std::vector<EBTSRecord> records;
        };

        // Parse a tag prefix "TT.NNN:" and return the
        // position after the colon. Sets recordType and
        // fieldNumber. Returns -1 on failure.
        static int parseTag(const uint8_t* buf, size_t len,
                            int& recordType, int& fieldNumber)
        {
            // Find the colon
            size_t colonPos = 0;
            for (; colonPos < len && colonPos < 20; ++colonPos)
            {
                if (buf[colonPos] == ':') break;
            }
            if (colonPos >= len || buf[colonPos] != ':') return -1;

            // Find the dot
            size_t dotPos = 0;
            for (; dotPos < colonPos; ++dotPos)
            {
                if (buf[dotPos] == '.') break;
            }
            if (dotPos >= colonPos) return -1;

            std::string rtStr(reinterpret_cast<const char*>(buf),
                              dotPos);
            std::string fnStr(
                reinterpret_cast<const char*>(buf + dotPos + 1),
                colonPos - dotPos - 1);

            recordType = std::atoi(rtStr.c_str());
            fieldNumber = std::atoi(fnStr.c_str());
            return (int)(colonPos + 1);
        }

        // Parse a single ASCII field starting at buf.
        // Returns bytes consumed (including the trailing GS or
        // FS), or -1 on error. The field value does NOT include
        // the separator.
        static int parseAsciiField(const uint8_t* buf, size_t maxLen,
                                   EBTSField& field)
        {
            int rt = 0, fn = 0;
            int tagLen = parseTag(buf, maxLen, rt, fn);
            if (tagLen < 0) return -1;

            field.recordType = rt;
            field.fieldNumber = fn;

            // Scan for GS or FS (field/record separator)
            size_t valStart = (size_t)tagLen;
            size_t pos = valStart;
            for (; pos < maxLen; ++pos)
            {
                if (buf[pos] == kGS || buf[pos] == kFS) break;
            }

            field.rawValue.assign(
                reinterpret_cast<const char*>(buf + valStart),
                pos - valStart);

            // Include the separator byte in consumed count
            if (pos < maxLen) ++pos;
            return (int)pos;
        }

        // Parse the LEN field (x.001) from the start of a
        // record. Returns the record length, or -1 on failure.
        static int parseRecordLen(const uint8_t* buf, size_t maxLen)
        {
            EBTSField f;
            int consumed = parseAsciiField(buf, maxLen, f);
            if (consumed < 0 || f.fieldNumber != 1) return -1;
            return std::atoi(f.rawValue.c_str());
        }

        // Parse a fully ASCII record (Type-1, Type-2, Type-9).
        static bool parseAsciiRecord(const uint8_t* buf, size_t recLen,
                                     EBTSRecord& rec)
        {
            size_t offset = 0;
            while (offset < recLen)
            {
                // Skip if we're sitting on a separator
                if (buf[offset] == kFS || buf[offset] == kGS)
                {
                    ++offset;
                    continue;
                }

                EBTSField field;
                int consumed = parseAsciiField(buf + offset,
                                               recLen - offset, field);
                if (consumed < 0) break;
                rec.fields.push_back(std::move(field));
                offset += (size_t)consumed;
            }

            if (!rec.fields.empty())
            {
                rec.recordType = rec.fields[0].recordType;
                rec.idc = rec.fieldInt(2);
            }
            return !rec.fields.empty();
        }

        // Parse an image record (Type-13/14/15) which has
        // ASCII fields followed by binary x.999 DATA.
        static bool parseImageRecord(const uint8_t* buf, size_t recLen,
                                     EBTSRecord& rec,
                                     const uint8_t*& dataOut,
                                     size_t& dataSizeOut)
        {
            dataOut = nullptr;
            dataSizeOut = 0;

            size_t offset = 0;
            while (offset < recLen)
            {
                if (buf[offset] == kFS || buf[offset] == kGS)
                {
                    ++offset;
                    continue;
                }

                // Peek at the tag to check if it's x.999
                int rt = 0, fn = 0;
                int tagLen =
                    parseTag(buf + offset, recLen - offset, rt, fn);
                if (tagLen < 0) break;

                if (fn == 999)
                {
                    // Binary DATA field: everything from
                    // after the colon to end of record
                    // (minus trailing FS)
                    size_t dataStart = offset + (size_t)tagLen;
                    size_t dataEnd = recLen;
                    // Strip trailing FS if present
                    if (dataEnd > dataStart && buf[dataEnd - 1] == kFS)
                    {
                        --dataEnd;
                    }
                    dataOut = buf + dataStart;
                    dataSizeOut = dataEnd - dataStart;

                    EBTSField field;
                    field.recordType = rt;
                    field.fieldNumber = 999;
                    rec.fields.push_back(std::move(field));
                    break;
                }

                // Normal ASCII field
                EBTSField field;
                int consumed = parseAsciiField(buf + offset,
                                               recLen - offset, field);
                if (consumed < 0) break;
                rec.fields.push_back(std::move(field));
                offset += (size_t)consumed;
            }

            if (!rec.fields.empty())
            {
                rec.recordType = rec.fields[0].recordType;
                rec.idc = rec.fieldInt(2);
            }
            return !rec.fields.empty();
        }

        // Split a string by a single-byte separator.
        static std::vector<std::string> splitBySep(const std::string& s,
                                                   char sep)
        {
            std::vector<std::string> parts;
            std::string cur;
            for (char c : s)
            {
                if (c == sep)
                {
                    parts.push_back(cur);
                    cur.clear();
                }
                else
                {
                    cur += c;
                }
            }
            parts.push_back(cur);
            return parts;
        }

        // Parse the 1.003 CNT field to get record type + IDC
        // pairs. Format: "1<US>count<RS>type<US>idc<RS>..."
        struct CntEntry
        {
            int recordType;
            int idc;
        };

        static std::vector<CntEntry> parseCnt(const std::string& cntStr)
        {
            std::vector<CntEntry> entries;
            auto subfields = splitBySep(cntStr, (char)kRS);
            for (auto& sf : subfields)
            {
                auto items = splitBySep(sf, (char)kUS);
                if (items.size() >= 2)
                {
                    CntEntry e;
                    e.recordType = std::atoi(items[0].c_str());
                    e.idc = std::atoi(items[1].c_str());
                    entries.push_back(e);
                }
            }
            return entries;
        }

        // Compute PPI from SLC/THPS/TVPS fields.
        static double computePpi(int slc, int thps, int tvps)
        {
            if (thps <= 0 && tvps <= 0) return 500.0;
            int res = thps > 0 ? thps : tvps;
            if (slc == 2)
            {
                // pixels per cm -> convert to ppi
                return res * 2.54;
            }
            // slc == 1 means ppi, slc == 0 means no scale
            return (double)res;
        }

        // Convert a coordinate from 0.01mm units to pixels.
        static double convertToPixels(double coord, double ppi)
        {
            // 1 inch = 25.4 mm = 2540 * 0.01mm
            return coord * ppi / 2540.0;
        }

        // Parse EFS minutiae from field 9.331 (MIN).
        // Subfields separated by RS, items by US.
        // Items: MXC, MYC, MTD, MTY, MRU, MDU, MINX
        static void parseMinutiae(const std::string& val,
                                  jt::Json& points, bool pixelUnits,
                                  double ppi)
        {
            auto subfields = splitBySep(val, (char)kRS);
            for (auto& sf : subfields)
            {
                if (sf.empty()) continue;
                auto items = splitBySep(sf, (char)kUS);
                if (items.size() < 4) continue;

                double x = std::atof(items[0].c_str());
                double y = std::atof(items[1].c_str());
                double theta = std::atof(items[2].c_str());
                std::string mtype = items.size() > 3 ? items[3] : "X";

                if (!pixelUnits)
                {
                    x = convertToPixels(x, ppi);
                    y = convertToPixels(y, ppi);
                }

                jt::Json pt;
                pt.setObject();
                pt["x"] = jt::Json(x);
                pt["y"] = jt::Json(y);
                pt["type"] = jt::Json(std::string("Corner"));
                pt["EBTSType"] = jt::Json(std::string("minutia"));
                pt["minutiaType"] = jt::Json(mtype);
                pt["theta"] = jt::Json(theta);
                points.getArray().push_back(std::move(pt));
            }
        }

        // Parse EFS cores from field 9.320 (COR).
        // Subfields separated by RS, items by US.
        // Items: CXC, CYC, CDI, ...
        static void parseCores(const std::string& val, jt::Json& points,
                               bool pixelUnits, double ppi)
        {
            auto subfields = splitBySep(val, (char)kRS);
            for (auto& sf : subfields)
            {
                if (sf.empty()) continue;
                auto items = splitBySep(sf, (char)kUS);
                if (items.size() < 2) continue;

                double x = std::atof(items[0].c_str());
                double y = std::atof(items[1].c_str());
                double dir = items.size() > 2
                                 ? std::atof(items[2].c_str())
                                 : 0.0;

                if (!pixelUnits)
                {
                    x = convertToPixels(x, ppi);
                    y = convertToPixels(y, ppi);
                }

                jt::Json pt;
                pt.setObject();
                pt["x"] = jt::Json(x);
                pt["y"] = jt::Json(y);
                pt["type"] = jt::Json(std::string("Corner"));
                pt["EBTSType"] = jt::Json(std::string("core"));
                pt["direction"] = jt::Json(dir);
                points.getArray().push_back(std::move(pt));
            }
        }

        // Parse EFS deltas from field 9.321 (DEL).
        // Subfields separated by RS, items by US.
        // Items: DXC, DYC, DDI, DTC, ...
        static void parseDeltas(const std::string& val,
                                jt::Json& points, bool pixelUnits,
                                double ppi)
        {
            auto subfields = splitBySep(val, (char)kRS);
            for (auto& sf : subfields)
            {
                if (sf.empty()) continue;
                auto items = splitBySep(sf, (char)kUS);
                if (items.size() < 2) continue;

                double x = std::atof(items[0].c_str());
                double y = std::atof(items[1].c_str());
                std::string deltaCode =
                    items.size() > 3 ? items[3] : "";

                if (!pixelUnits)
                {
                    x = convertToPixels(x, ppi);
                    y = convertToPixels(y, ppi);
                }

                jt::Json pt;
                pt.setObject();
                pt["x"] = jt::Json(x);
                pt["y"] = jt::Json(y);
                pt["type"] = jt::Json(std::string("Corner"));
                pt["EBTSType"] = jt::Json(std::string("delta"));
                pt["deltaCode"] = jt::Json(deltaCode);
                points.getArray().push_back(std::move(pt));
            }
        }

        struct ImageInfo
        {
            int idc = 0;
            int recordType = 0;
            int width = 0;
            int height = 0;
            double ppi = 500.0;
            int fgp = 0;
            const uint8_t* pngData = nullptr;
            size_t pngSize = 0;
        };

    }  // anonymous namespace

    int loadNistFromDisk(const std::string& filePath,
                         std::vector<ImageCanvas>& outCanvases)
    {
        // Read entire file
        std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) return -1;
        size_t fileSize = (size_t)ifs.tellg();
        if (fileSize < 10) return -1;
        ifs.seekg(0);
        std::vector<uint8_t> buf(fileSize);
        ifs.read(reinterpret_cast<char*>(buf.data()),
                 (std::streamsize)fileSize);
        if (!ifs) return -1;
        ifs.close();

        // Parse Type-1 record (always first)
        int type1Len = parseRecordLen(buf.data(), fileSize);
        if (type1Len <= 0 || (size_t)type1Len > fileSize) return -1;

        EBTSRecord type1Rec;
        if (!parseAsciiRecord(buf.data(), (size_t)type1Len, type1Rec))
            return -1;
        if (type1Rec.recordType != 1) return -1;

        // Parse 1.003 CNT to know what records follow
        std::string cntStr = type1Rec.fieldValue(3);
        if (cntStr.empty()) return -1;
        auto cntEntries = parseCnt(cntStr);
        if (cntEntries.empty()) return -1;

        // Walk through all records after Type-1
        EBTSTransaction txn;
        txn.records.push_back(type1Rec);

        std::vector<ImageInfo> images;
        size_t offset = (size_t)type1Len;

        // cntEntries[0] is the Type-1 record itself, skip it
        for (size_t ci = 1; ci < cntEntries.size() && offset < fileSize;
             ++ci)
        {
            int expectedType = cntEntries[ci].recordType;

            // Get record length from first field
            int recLen =
                parseRecordLen(buf.data() + offset, fileSize - offset);
            if (recLen <= 0 || offset + (size_t)recLen > fileSize)
                break;

            if (expectedType == 13 || expectedType == 14 ||
                expectedType == 15)
            {
                // Image record
                EBTSRecord rec;
                const uint8_t* imgData = nullptr;
                size_t imgSize = 0;
                if (parseImageRecord(buf.data() + offset,
                                     (size_t)recLen, rec, imgData,
                                     imgSize))
                {
                    txn.records.push_back(rec);

                    // Check compression - only PNG
                    std::string cga = rec.fieldValue(11);
                    bool isPng = (cga == "PNG");

                    if (isPng && imgData && imgSize > 0)
                    {
                        ImageInfo info;
                        info.idc = rec.idc;
                        info.recordType = expectedType;
                        info.width = rec.fieldInt(6);
                        info.height = rec.fieldInt(7);
                        int slc = rec.fieldInt(8);
                        int thps = rec.fieldInt(9);
                        int tvps = rec.fieldInt(10);
                        info.ppi = computePpi(slc, thps, tvps);
                        info.fgp = rec.fieldInt(13);
                        info.pngData = imgData;
                        info.pngSize = imgSize;
                        images.push_back(info);
                    }
                }
            }
            else if (expectedType == 9)
            {
                // Type-9 metadata record (ASCII)
                EBTSRecord rec;
                if (parseAsciiRecord(buf.data() + offset,
                                     (size_t)recLen, rec))
                {
                    txn.records.push_back(rec);
                }
            }
            else
            {
                // Other record types - skip
                EBTSRecord rec;
                parseAsciiRecord(buf.data() + offset, (size_t)recLen,
                                 rec);
                txn.records.push_back(rec);
            }

            offset += (size_t)recLen;
        }

        if (images.empty()) return -1;

        // Extract filename for naming
        namespace fs = std::filesystem;
        std::string basename = fs::path(filePath).filename().string();

        int loaded = 0;

        for (auto& img : images)
        {
            // Decode PNG from memory
            int w = 0, h = 0, channels = 0;
            unsigned char* rgba = stbi_load_from_memory(
                img.pngData, (int)img.pngSize, &w, &h, &channels, 4);
            if (!rgba) continue;

            ImTextureID texId = createTextureRGBA(rgba, w, h);
            stbi_image_free(rgba);

            ImageCanvas canvas;
            char nameBuf[256];
            snprintf(nameBuf, sizeof(nameBuf),
                     "%s [Type-%d IDC:%02d FGP:%d]", basename.c_str(),
                     img.recordType, img.idc, img.fgp);
            canvas.image->name = nameBuf;
            canvas.image->path = filePath;
            canvas.image->textureId = texId;
            canvas.image->width = w;
            canvas.image->height = h;

            // Build annotations
            canvas.image->annotations.setObject();
            canvas.image->annotations["bounds"].setArray();
            canvas.image->annotations["points"].setArray();

            // Find matching Type-9 record by IDC
            for (auto& rec : txn.records)
            {
                if (rec.recordType != 9) continue;
                if (rec.idc != img.idc) continue;

                // Determine coordinate units
                std::string unt = rec.fieldValue(304);
                bool pixelUnits = (unt == "P");

                // Parse minutiae (9.331)
                auto* minField = rec.findField(331);
                if (minField)
                {
                    parseMinutiae(minField->rawValue,
                                  canvas.image->annotations["points"],
                                  pixelUnits, img.ppi);
                }

                // Parse cores (9.320)
                auto* corField = rec.findField(320);
                if (corField)
                {
                    parseCores(corField->rawValue,
                               canvas.image->annotations["points"],
                               pixelUnits, img.ppi);
                }

                // Parse deltas (9.321)
                auto* delField = rec.findField(321);
                if (delField)
                {
                    parseDeltas(delField->rawValue,
                                canvas.image->annotations["points"],
                                pixelUnits, img.ppi);
                }

                break;
            }

            // Store EBTS metadata in annotations
            jt::Json ebts;
            ebts.setObject();
            ebts["fingerPosition"] = jt::Json((double)img.fgp);
            ebts["ppi"] = jt::Json(img.ppi);
            canvas.image->annotations["EBTS"] = std::move(ebts);

            outCanvases.push_back(std::move(canvas));
            ++loaded;
        }

        return loaded > 0 ? loaded : -1;
    }

}  // namespace shoecomp
