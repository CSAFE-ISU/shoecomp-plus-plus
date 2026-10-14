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
            try
            {
                if (a.recursive)
                {
                    for (const auto& e :
                         fs::recursive_directory_iterator(
                             a.dir, fs::directory_options::
                                        skip_permission_denied))
                        consider(e);
                }
                else
                {
                    for (const auto& e : fs::directory_iterator(a.dir))
                        consider(e);
                }
            }
            catch (const std::exception& e)
            {
                err = "cannot scan " + a.dir + ": " + e.what();
                return false;
            }
            std::sort(files.begin(), files.end());
            for (const auto& f : files)
            {
                std::vector<Item> got;
                bool ok = false;
                try
                {
                    ok = itemsForFile(c.type, f.string(), got, err);
                }
                catch (const std::exception& e)
                {
                    err = e.what();
                }
                if (!ok)
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

    namespace
    {
        // Fields of one CSV line as written by csvRow (RFC 4180
        // quoting).
        std::vector<std::string> splitCsv(const std::string& line)
        {
            std::vector<std::string> f(1);
            bool quoted = false;
            for (size_t i = 0; i < line.size(); ++i)
            {
                const char ch = line[i];
                if (quoted)
                {
                    if (ch == '"' && i + 1 < line.size() &&
                        line[i + 1] == '"')
                    {
                        f.back() += '"';
                        ++i;
                    }
                    else if (ch == '"')
                        quoted = false;
                    else
                        f.back() += ch;
                }
                else if (ch == '"')
                    quoted = true;
                else if (ch == ',')
                    f.emplace_back();
                else
                    f.back() += ch;
            }
            return f;
        }

        std::string pairFileName(const Item& left, const Item& right)
        {
            return fileSafe(left.label) + "__" + fileSafe(right.label) +
                   ".json";
        }

        // Rows of a previous run's summary.csv that need no rerun
        // (status ok or none), keyed by "left\nright". False with
        // |err| when the folder holds a run with other parameters or
        // another input type.
        bool loadFinished(const CliRun& c, const fs::path& out,
                          std::map<std::string, std::string>& rows,
                          std::string& err)
        {
            std::ifstream prev(out / "params.ini", std::ios::binary);
            if (prev)
            {
                std::ostringstream ss;
                ss << prev.rdbuf();
                if (ss.str() != paramsToIni(c.params))
                {
                    err = "--resume: " + (out / "params.ini").string() +
                          " differs from the current parameters (use "
                          "-p with that file, or a new --out-dir)";
                    return false;
                }
            }
            std::ifstream csv(out / "summary.csv");
            std::string line;
            if (!csv || !std::getline(csv, line)) return true;
            if (line != csvHeader(c.type))
            {
                err = "--resume: " + (out / "summary.csv").string() +
                      " is from a different input type";
                return false;
            }
            while (std::getline(csv, line))
            {
                const auto f = splitCsv(line);
                if (f.size() < 3 || (f[2] != "ok" && f[2] != "none"))
                    continue;
                rows[f[0] + "\n" + f[1]] = line;
            }
            return true;
        }

        // Prepare (once) + compare + write one pair's files. File
        // write failures are counted in |errors| and reported; the
        // returned result is the comparison's.
        PairResult runPair(CliRun& c, Item& left, Item& right,
                           const fs::path& out,
                           const std::string& matchedDir, int& errors)
        {
            std::string err;
            for (Item* it : {&left, &right})
            {
                if (it->prepared || !it->error.empty()) continue;
                if (!prepareItem(*it, c.params, c.run) && !c.quiet)
                    std::fprintf(stderr, "  %s: %s\n",
                                 it->label.c_str(), it->error.c_str());
            }
            PairResult r = comparePair(left, right, c.params, c.run);

            bool written = false;
            try
            {
                const std::string name = pairFileName(left, right);
                const jt::Json doc =
                    pairDocument(c.type, left, right, r);
                written =
                    writeTextFile((out / "pairs" / name).string(),
                                  doc.toStringPretty() + "\n", err) &&
                    (matchedDir.empty() ||
                     saveMatched(matchedDir, left, right, r,
                                 c.matchedResults, err));
            }
            catch (const std::exception& e)
            {
                err = std::string("writing results: ") + e.what();
            }
            if (!written)
            {
                std::fprintf(
                    stderr, "scpp batch: %s: %s\n",
                    (left.label + " vs " + right.label).c_str(),
                    err.c_str());
                ++errors;
            }
            return r;
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
        std::map<std::string, std::string> finished;
        if (a.resume && !loadFinished(c, out, finished, err))
        {
            std::fprintf(stderr, "scpp batch: %s\n", err.c_str());
            return kExitUsage;
        }
        std::vector<char> done(pairs.size(), 0);
        size_t numDone = 0;
        for (size_t k = 0; k < pairs.size(); ++k)
        {
            const Item& l = items[pairs[k].first];
            const Item& r = items[pairs[k].second];
            if (finished.count(l.label + "\n" + r.label) &&
                fs::exists(out / "pairs" / pairFileName(l, r)))
            {
                done[k] = 1;
                ++numDone;
            }
        }

        if (a.saveAnnotations)
            c.run.saveAnnotationsDir = (out / "annotations").string();
        const std::string matchedDir =
            a.saveMatched ? (out / "matched").string() : std::string();
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
        for (size_t k = 0; k < pairs.size(); ++k)
        {
            if (!done[k]) continue;
            const Item& l = items[pairs[k].first];
            const Item& r = items[pairs[k].second];
            csv << finished[l.label + "\n" + r.label] << "\n";
        }
        csv.flush();

        if (!c.quiet)
        {
            std::fprintf(stderr,
                         "scpp batch: %zu input(s), %zu pair(s)\n",
                         items.size(), pairs.size());
            if (a.resume)
                std::fprintf(
                    stderr,
                    "scpp batch: resuming, %zu pair(s) already "
                    "done\n",
                    numDone);
        }

        int errors = 0;  // failed result writes
        int failed = 0;  // error / timeout pairs
        for (size_t k = 0; k < pairs.size(); ++k)
        {
            if (done[k]) continue;
            Item& left = items[pairs[k].first];
            Item& right = items[pairs[k].second];
            PairResult r;
            try
            {
                r = runPair(c, left, right, out, matchedDir, errors);
            }
            catch (const std::exception& e)
            {
                r = PairResult{};
                r.alignments.setArray();
                r.message =
                    std::string("unexpected error: ") + e.what();
            }
            catch (...)
            {
                r = PairResult{};
                r.alignments.setArray();
                r.message = "unexpected error";
            }
            if (r.status == PairStatus::Error ||
                r.status == PairStatus::Timeout)
                ++failed;
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
                "scpp batch: done, %d failed pair(s), %d write "
                "error(s); results in %s\n",
                failed, errors, a.outDir.c_str());
        return failed || errors ? kExitError : kExitOk;
    }
}  // namespace shoecomp
