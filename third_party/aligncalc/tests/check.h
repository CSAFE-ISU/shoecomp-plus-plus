#ifndef ALIGNCALC_TESTS_CHECK_H
#define ALIGNCALC_TESTS_CHECK_H
// Minimal checks for the aligncalc tests: CHECK(cond) or
// CHECKF(cond, fmt, ...) records a failure with file:line and goes
// on; main returns checkResult() (nonzero when anything failed).
#include <cstdio>

namespace check
{
    inline int& failures()
    {
        static int n = 0;
        return n;
    }
    inline int& total()
    {
        static int n = 0;
        return n;
    }
    inline bool record(bool ok, const char* file, int line,
                       const char* expr)
    {
        ++total();
        if (!ok)
        {
            ++failures();
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", file, line,
                    expr);
        }
        return ok;
    }
}  // namespace check

#define CHECK(cond) check::record(bool(cond), __FILE__, __LINE__, #cond)
#define CHECKF(cond, ...)                                          \
    (CHECK(cond)                                                   \
         ? true                                                    \
         : (fprintf(stderr, "    "), fprintf(stderr, __VA_ARGS__), \
            fprintf(stderr, "\n"), false))

inline int checkResult()
{
    printf("%d checks, %d failed\n", check::total(), check::failures());
    return check::failures() != 0;
}

#endif
