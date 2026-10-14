#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "CLI/CLI.hpp"
#include "cli/cli.h"
#include "core/licenseData.h"
#include "shoecompVersion.h"

using namespace shoecomp;

namespace
{
    const char* sectionTitle(const std::string& section)
    {
        if (section == "detect2d") return "2-D point detection (ONNX)";
        if (section == "align2d") return "2-D alignment";
        if (section == "striation") return "Striation detection (x3p)";
        if (section == "align3d") return "3-D alignment (x3p)";
        return "Parameters";
    }

    // Values given on the command line, by paramTable() index.
    struct ParamOverrides
    {
        std::vector<std::string> values;
        std::vector<bool> given;
    };

    struct State
    {
        std::string type;
        std::string paramsFile;
        int threads = 0;
        bool verbose = false;
        bool quiet = false;
        bool redetect = false;
        std::string saveAnnotationsDir;
        int matchedResults = 1;
        ParamOverrides overrides;
    };

    void addParamOptions(CLI::App* app, State& st)
    {
        app->add_option("-p,--params", st.paramsFile,
                        "INI file of tuning parameters (see `scpp "
                        "params`); flags below override it")
            ->check(CLI::ExistingFile)
            ->group("Parameters");
        Params defaults;
        const auto& table = paramTable();
        for (size_t i = 0; i < table.size(); ++i)
        {
            const ParamDesc& d = table[i];
            std::string def = paramValueString(d, defaults);
            const char* typeName = "";
            switch (d.kind)
            {
                case ParamKind::Bool:
                    typeName = "BOOL";
                    break;
                case ParamKind::Int:
                case ParamKind::Size:
                    typeName = "INT";
                    break;
                case ParamKind::Float:
                case ParamKind::Double:
                    typeName = "NUM";
                    break;
                case ParamKind::String:
                    typeName = "TEXT";
                    break;
            }
            app->add_option_function<std::string>(
                   paramFlag(d),
                   [&st, i](const std::string& v)
                   {
                       st.overrides.values[i] = v;
                       st.overrides.given[i] = true;
                   },
                   std::string(d.help) + " (INI: [" + d.section + "] " +
                       d.key + ")")
                ->type_name(typeName)
                ->default_str(def.empty() ? "\"\"" : def)
                ->group(sectionTitle(d.section));
        }
    }

    void addTypeOption(CLI::App* app, State& st)
    {
        app->add_option("-t,--type", st.type,
                        "Input type: shoe, hw, ebts or x3p")
            ->check(CLI::IsMember({"shoe", "hw", "ebts", "x3p"}))
            ->group("Input/Output");
    }

    void addRunOptions(CLI::App* app, State& st)
    {
        const unsigned hw =
            std::max(1u, std::thread::hardware_concurrency());
        app->add_option("-j,--threads", st.threads,
                        "Threads for detection and alignment; 0 = all "
                        "cores (" +
                            std::to_string(hw) + ")")
            ->check(CLI::NonNegativeNumber)
            ->capture_default_str()
            ->group("Run");
        app->add_flag("--redetect", st.redetect,
                      "Ignore annotation sidecars and always detect "
                      "(needs --detect2d-model for 2-D types)")
            ->group("Input/Output");
        app->add_option("--matched-results", st.matchedResults,
                        "Alignments per pair written by --save-matched "
                        "(best first)")
            ->check(CLI::PositiveNumber)
            ->capture_default_str()
            ->group("Input/Output");
        app->add_flag("-v,--progress", st.verbose,
                      "Print progress to stderr")
            ->group("Run");
        app->add_flag("-q,--quiet", st.quiet,
                      "Only print results and errors")
            ->group("Run");
        addParamOptions(app, st);
    }

    void printVersion(bool full)
    {
        const LicenseEntry& self = projectLicense();
        const std::string selfText = licenseText(self);
        std::printf("scpp %s (ShoeComp)\n", self.version);
        size_t c = selfText.find("Copyright");
        if (c != std::string::npos)
            std::printf(
                "%s\n",
                selfText.substr(c, selfText.find('\n', c) - c).c_str());
        std::printf("License: %s  %s\n\nThird-party components:\n",
                    self.licenseType, self.url);
        for (const auto& e : getLicenses())
        {
            if (!(e.binaries & kInCli)) continue;
            std::printf("  %-18s %-8s %-22s %s\n", e.name, e.version,
                        e.licenseType, e.url);
        }
        if (!full)
        {
            std::printf(
                "\n`scpp version --full` prints the license "
                "texts.\n");
            return;
        }
        auto dump = [](const LicenseEntry& e)
        {
            std::printf("\n==== %s%s%s (%s) ====\n%s\n", e.name,
                        e.version[0] ? " " : "", e.version,
                        e.licenseType, licenseText(e).c_str());
        };
        dump(self);
        for (const auto& e : getLicenses())
            if (e.binaries & kInCli) dump(e);
    }

    // Last resort: report an escaped exception instead of aborting.
    template <typename F>
    int guarded(F&& f)
    {
        try
        {
            return f();
        }
        catch (const std::exception& e)
        {
            std::fprintf(stderr, "scpp: unexpected error: %s\n",
                         e.what());
        }
        catch (...)
        {
            std::fprintf(stderr, "scpp: unexpected error\n");
        }
        return kExitError;
    }

    // Defaults <- params file <- flag overrides, then validated.
    bool resolveParams(const State& st, Params& p, std::string& err)
    {
        if (!st.paramsFile.empty() &&
            !loadParamsFile(st.paramsFile, p, err))
            return false;
        const auto& table = paramTable();
        for (size_t i = 0; i < table.size(); ++i)
        {
            if (!st.overrides.given[i]) continue;
            if (!setParamValue(table[i], p, st.overrides.values[i],
                               err))
            {
                err = paramFlag(table[i]) + ": " + err;
                return false;
            }
        }
        return validateParams(p, err);
    }

    bool makeRun(const State& st, CliRun& c, std::string& err)
    {
        if (st.type.empty())
        {
            err = "--type is required (shoe, hw, ebts or x3p)";
            return false;
        }
        imageTypeFromName(st.type, c.type);
        if (!resolveParams(st, c.params, err)) return false;
        c.run.threads = st.threads;
        c.run.redetect = st.redetect;
        c.run.saveAnnotationsDir = st.saveAnnotationsDir;
        c.quiet = st.quiet;
        c.matchedResults = st.matchedResults;
        if (st.verbose && !st.quiet)
        {
            c.run.log = [](const std::string& m)
            { std::fprintf(stderr, "  %s\n", m.c_str()); };
        }
        return true;
    }
}  // namespace

int main(int argc, char** argv)
{
    State st;
    st.overrides.values.resize(paramTable().size());
    st.overrides.given.resize(paramTable().size(), false);

    CLI::App app{
        "Compare two annotated images or x3p surfaces (default), or "
        "every pair in a folder (batch).\n"
        "2-D inputs use <stem>.json annotation sidecars, EBTS Type-9 "
        "points, or ONNX detection; x3p inputs use sidecar striations "
        "or detect them.",
        "scpp"};
    app.set_help_all_flag("--help-all", "Help for every subcommand");
    app.set_version_flag("--version",
                         std::string("scpp ") + SHOECOMP_VERSION,
                         "Print the version and exit");
    app.require_subcommand(0, 1);

    CompareArgs cmp;
    addTypeOption(&app, st);
    app.add_option("LEFT", cmp.left, "Left input (EBTS: file@N)")
        ->group("Input/Output");
    app.add_option("RIGHT", cmp.right, "Right input (EBTS: file@N)")
        ->group("Input/Output");
    app.add_option("--left-ann", cmp.leftAnn,
                   "Annotation JSON for LEFT (instead of its sidecar)")
        ->check(CLI::ExistingFile)
        ->group("Input/Output");
    app.add_option("--right-ann", cmp.rightAnn,
                   "Annotation JSON for RIGHT (instead of its sidecar)")
        ->check(CLI::ExistingFile)
        ->group("Input/Output");
    app.add_flag("--json", cmp.json,
                 "Print the full result JSON instead of a summary")
        ->group("Input/Output");
    app.add_option("-o,--output", cmp.outFile,
                   "Write the full result JSON to this file")
        ->group("Input/Output");
    app.add_option("--save-annotations", st.saveAnnotationsDir,
                   "Write detected annotations to DIR/<stem>.json "
                   "(not next to the inputs)")
        ->type_name("DIR")
        ->group("Input/Output");
    app.add_option(
           "--save-matched", cmp.matchedDir,
           "Write the matched points/striations only, per input, "
           "to DIR (JSON; 2-D also a numbered PNG)")
        ->type_name("DIR")
        ->group("Input/Output");
    addRunOptions(&app, st);

    BatchArgs bat;
    CLI::App* batch = app.add_subcommand(
        "batch",
        "Compare every pair of inputs in a folder, or the pairs listed "
        "in a file; writes summary.csv, pairs/*.json and params.ini");
    addTypeOption(batch, st);
    batch->add_option("DIR", bat.dir, "Folder of inputs")
        ->required()
        ->check(CLI::ExistingDirectory)
        ->group("Input/Output");
    auto* all = batch
                    ->add_flag("--all-pairs", bat.allPairs,
                               "Compare every unordered pair")
                    ->group("Input/Output");
    auto* pairs =
        batch
            ->add_option("--pairs", bat.pairsFile,
                         "File of pairs, one `left,right` per line "
                         "(# comments; paths relative to DIR)")
            ->check(CLI::ExistingFile)
            ->group("Input/Output");
    all->excludes(pairs);
    batch->add_option("--out-dir", bat.outDir, "Output folder")
        ->required()
        ->group("Input/Output");
    batch
        ->add_flag("-r,--recursive", bat.recursive,
                   "Scan DIR recursively")
        ->group("Input/Output");
    batch
        ->add_flag("--save-annotations", bat.saveAnnotations,
                   "Write detected annotations to OUT/annotations/")
        ->group("Input/Output");
    batch
        ->add_flag("--save-matched", bat.saveMatched,
                   "Write matched-only annotations (and 2-D PNGs) to "
                   "OUT/matched/")
        ->group("Input/Output");
    addRunOptions(batch, st);

    CLI::App* params = app.add_subcommand(
        "params",
        "Print the tuning parameters as INI (defaults, or the result "
        "of -p and any parameter flags)");
    addParamOptions(params, st);

    bool fullLicenses = false;
    CLI::App* version = app.add_subcommand(
        "version",
        "Print the version, ShoeComp's license and the third-party "
        "components with their licenses");
    version->add_flag("--full", fullLicenses,
                      "Also print every license text");

    std::string helpTarget;
    CLI::App* help = app.add_subcommand(
        "help", "Show help for scpp or one of its subcommands");
    help->add_option("SUBCOMMAND", helpTarget)
        ->check(CLI::IsMember({"batch", "params", "version", "help"}));

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e)
    {
        int rc = app.exit(e);
        return rc == 0 ? kExitOk : kExitUsage;
    }

    if (help->parsed())
    {
        if (helpTarget.empty())
            std::fputs(app.help().c_str(), stdout);
        else
            std::fputs(app.get_subcommand(helpTarget)->help().c_str(),
                       stdout);
        return kExitOk;
    }

    if (version->parsed())
    {
        printVersion(fullLicenses);
        return kExitOk;
    }

    std::string err;
    if (params->parsed())
    {
        Params p;
        if (!resolveParams(st, p, err))
        {
            std::fprintf(stderr, "scpp: %s\n", err.c_str());
            return kExitUsage;
        }
        std::fputs(paramsToIni(p).c_str(), stdout);
        return kExitOk;
    }

    CliRun run;
    if (!makeRun(st, run, err))
    {
        std::fprintf(stderr, "scpp: %s\n", err.c_str());
        return kExitUsage;
    }
    if (batch->parsed())
    {
        if (!bat.allPairs && bat.pairsFile.empty())
        {
            std::fprintf(
                stderr,
                "scpp batch: give --all-pairs or --pairs FILE\n");
            return kExitUsage;
        }
        if (!st.saveAnnotationsDir.empty() || !cmp.matchedDir.empty())
        {
            std::fprintf(
                stderr,
                "scpp batch: --save-annotations/--save-matched "
                "are flags here (OUT/annotations/, "
                "OUT/matched/)\n");
            return kExitUsage;
        }
        return guarded([&] { return runBatch(run, bat); });
    }
    if (cmp.left.empty() || cmp.right.empty())
    {
        std::fprintf(stderr,
                     "scpp: give LEFT and RIGHT inputs (see scpp "
                     "--help)\n");
        return kExitUsage;
    }
    return guarded([&] { return runCompare(run, cmp); });
}
