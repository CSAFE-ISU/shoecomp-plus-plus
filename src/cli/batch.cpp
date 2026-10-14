#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "cli/cli.h"

namespace fs = std::filesystem;

namespace shoecomp
{
    namespace
    {
        std::string trim(const std::string& s)
        {
            size_t a = 0, b = s.size();
            while (a < b && std::isspace((unsigned char)s[a])) ++a;
            while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
            return s.substr(a, b - a);
        }

        // File-name-safe form of an item label.
        std::string safeName(const std::string& label)
        {
            std::string s = label;
            for (char& ch : s)
            {
                const unsigned char u = (unsigned char)ch;
                if (!std::isalnum(u) && ch != '.' && ch != '-' &&
                    ch != '_' && ch != '@')
                    ch = '_';
            }
            return s;
        }

        bool hasTypeExtension(ImageType type, const fs::path& p)
        {
            const std::string ext = p.extension().string();
            const auto& exts = extensionsFor(type);
            return std::find(exts.begin(), exts.end(), ext) !=
                   exts.end();
        }

        // Label = path relative to the batch folder, plus @N.
        void relabel(Item& it, const fs::path& dir)
        {
            std::error_code ec;
            fs::path rel = fs::relative(it.path, dir, ec);
            it.label =
                (ec || rel.empty() ? fs::path(it.path).filename() : rel)
                    .generic_string();
            if (it.record >= 0)
                it.label += "@" + std::to_string(it.record);
        }

        bool scanItems(const CliRun& c, const BatchArgs& a,
                       std::vector<Item>& items, std::string& err)
        {
            std::vector<fs::path> files;
            auto consider = [&](const fs::directory_entry& e)
            {
                if (e.is_regular_file() &&
                    hasTypeExtension(c.type, e.path()))
                    files.push_back(e.path());
            };
            std::error_code ec;
            if (a.recursive)
            {
                for (const auto& e :
                     fs::recursive_directory_iterator(a.dir, ec))
                    consider(e);
            }
            else
            {
                for (const auto& e : fs::directory_iterator(a.dir, ec))
                    consider(e);
            }
            if (ec)
            {
                err = "cannot scan " + a.dir + ": " + ec.message();
                return false;
            }
            std::sort(files.begin(), files.end());
            for (const auto& f : files)
            {
                std::vector<Item> got;
                if (!itemsForFile(c.type, f.string(), got, err))
                {
                    std::fprintf(stderr,
                                 "scpp batch: skipping %s: %s\n",
                                 f.string().c_str(), err.c_str());
                    continue;
                }
                for (auto& it : got)
                {
                    relabel(it, a.dir);
                    items.push_back(std::move(it));
                }
            }
            return true;
        }

        bool readPairs(const CliRun& c, const BatchArgs& a,
                       std::vector<Item>& items,
                       std::vector<std::pair<size_t, size_t>>& pairs,
                       std::string& err)
        {
            std::ifstream in(a.pairsFile);
            if (!in)
            {
                err = "cannot open " + a.pairsFile;
                return false;
            }
            std::map<std::string, size_t> index;
            auto itemFor = [&](const std::string& spec, size_t& out)
            {
                fs::path p(spec);
                const std::string full =
                    p.is_absolute() ? spec
                                    : (fs::path(a.dir) / p).string();
                auto found = index.find(full);
                if (found != index.end())
                {
                    out = found->second;
                    return true;
                }
                Item it;
                if (!parseItemSpec(c.type, full, it, err)) return false;
                relabel(it, a.dir);
                out = items.size();
                index[full] = out;
                items.push_back(std::move(it));
                return true;
            };
            std::string line;
            int lineNo = 0;
            while (std::getline(in, line))
            {
                ++lineNo;
                const std::string t = trim(line);
                if (t.empty() || t[0] == '#') continue;
                const size_t comma = t.find(',');
                const std::string where = a.pairsFile + ": line " +
                                          std::to_string(lineNo) + ": ";
                if (comma == std::string::npos)
                {
                    err = where + "expected `left,right`";
                    return false;
                }
                size_t li = 0, ri = 0;
                if (!itemFor(trim(t.substr(0, comma)), li) ||
                    !itemFor(trim(t.substr(comma + 1)), ri))
                {
                    err = where + err;
                    return false;
                }
                pairs.push_back({li, ri});
            }
            return true;
        }
    }  // namespace

    int runBatch(CliRun& c, const BatchArgs& a)
    {
        std::string err;
        std::vector<Item> items;
        std::vector<std::pair<size_t, size_t>> pairs;
        if (a.allPairs)
        {
            if (!scanItems(c, a, items, err))
            {
                std::fprintf(stderr, "scpp batch: %s\n", err.c_str());
                return kExitError;
            }
            for (size_t i = 0; i < items.size(); ++i)
                for (size_t j = i + 1; j < items.size(); ++j)
                    pairs.push_back({i, j});
        }
        else if (!readPairs(c, a, items, pairs, err))
        {
            std::fprintf(stderr, "scpp batch: %s\n", err.c_str());
            return kExitUsage;
        }
        if (pairs.empty())
        {
            std::fprintf(stderr, "scpp batch: no pairs to compare\n");
            return kExitError;
        }

        const fs::path out(a.outDir);
        std::error_code ec;
        fs::create_directories(out / "pairs", ec);
        if (ec)
        {
            std::fprintf(stderr, "scpp batch: cannot create %s: %s\n",
                         (out / "pairs").string().c_str(),
                         ec.message().c_str());
            return kExitError;
        }
        if (a.saveAnnotations)
            c.run.saveAnnotationsDir = (out / "annotations").string();
        if (!writeTextFile((out / "params.ini").string(),
                           paramsToIni(c.params), err))
        {
            std::fprintf(stderr, "scpp batch: %s\n", err.c_str());
            return kExitError;
        }
        std::ofstream csv(out / "summary.csv", std::ios::trunc);
        if (!csv)
        {
            std::fprintf(stderr, "scpp batch: cannot write %s\n",
                         (out / "summary.csv").string().c_str());
            return kExitError;
        }
        csv << csvHeader(c.type) << "\n";

        if (!c.quiet)
            std::fprintf(stderr,
                         "scpp batch: %zu input(s), %zu pair(s)\n",
                         items.size(), pairs.size());

        int errors = 0;
        for (size_t k = 0; k < pairs.size(); ++k)
        {
            Item& left = items[pairs[k].first];
            Item& right = items[pairs[k].second];
            for (Item* it : {&left, &right})
            {
                if (it->prepared || !it->error.empty()) continue;
                if (!prepareItem(*it, c.params, c.run) && !c.quiet)
                    std::fprintf(stderr, "  %s: %s\n",
                                 it->label.c_str(), it->error.c_str());
            }
            const PairResult r =
                comparePair(left, right, c.params, c.run);
            if (r.status == PairStatus::Error) ++errors;

            const std::string name = safeName(left.label) + "__" +
                                     safeName(right.label) + ".json";
            const jt::Json doc = pairDocument(c.type, left, right, r);
            if (!writeTextFile((out / "pairs" / name).string(),
                               doc.toStringPretty() + "\n", err))
            {
                std::fprintf(stderr, "scpp batch: %s\n", err.c_str());
                ++errors;
            }
            csv << csvRow(left, right, r) << "\n";
            csv.flush();
            if (!c.quiet)
            {
                std::fprintf(stderr, "[%zu/%zu] ", k + 1, pairs.size());
                printSummary(stderr, left, right, r);
            }
        }
        if (!c.quiet)
            std::fprintf(
                stderr,
                "scpp batch: done, %d error(s); results in %s\n",
                errors, a.outDir.c_str());
        return errors ? kExitError : kExitOk;
    }
}  // namespace shoecomp
