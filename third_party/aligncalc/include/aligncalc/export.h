#ifndef ALIGNCALC_EXPORT_H
#define ALIGNCALC_EXPORT_H

// ALIGNCALC_SHARED is defined (by CMake, for the library and its users)
// when aligncalc is a shared library; ALIGNCALC_BUILDING only while
// building it. A static build needs neither.
#if defined(ALIGNCALC_SHARED)
#if defined(_WIN32)
#if defined(ALIGNCALC_BUILDING)
#define ALIGNCALC_API __declspec(dllexport)
#else
#define ALIGNCALC_API __declspec(dllimport)
#endif
#else
#define ALIGNCALC_API __attribute__((visibility("default")))
#endif
#else
#define ALIGNCALC_API
#endif

// Explicit instantiation declarations for the instantiations the
// library ships (LabelRules). MSVC rejects dllexport on an extern
// template, so the library's own build skips them there.
#if defined(ALIGNCALC_SHARED) && defined(_WIN32) && \
    defined(ALIGNCALC_BUILDING)
#define ALIGNCALC_EXTERN_TEMPLATE(...)
#else
#define ALIGNCALC_EXTERN_TEMPLATE(...) extern template __VA_ARGS__;
#endif

// For the library's explicit instantiation definitions: on Windows
// they carry the dllexport (their extern declarations are skipped);
// elsewhere the extern declarations already set the visibility.
#if defined(_WIN32)
#define ALIGNCALC_INSTANTIATION_API ALIGNCALC_API
#else
#define ALIGNCALC_INSTANTIATION_API
#endif

#endif
