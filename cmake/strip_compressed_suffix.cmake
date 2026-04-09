# Runs binary_to_compressed_c against INPUT and rewrites the resulting
# .c file so that:
#   - each generated `const` global is declared `extern const`, matching
#     the declarations in include/ui/embeddedAssets.h and keeping the
#     symbols linkable from C++ translation units;
#   - for raw (uncompressed) assets, the misleading `_compressed_`
#     infix is stripped from symbol names (ShoeCompIcon_compressed_data
#     -> ShoeCompIcon_data).
#
# Expected -D arguments:
#   TOOL    path to the binary_to_compressed_c executable
#   INPUT   absolute path to the asset being embedded
#   SYMBOL  C symbol base name to use
#   OUTPUT  absolute path of the .c file to generate
#   MODE    "compress" (default) or "raw"

if(NOT DEFINED TOOL OR NOT DEFINED INPUT OR
   NOT DEFINED SYMBOL OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR
        "strip_compressed_suffix.cmake: missing required -D arguments")
endif()
if(NOT DEFINED MODE)
    set(MODE "compress")
endif()

set(_flags "-nostatic")
if(MODE STREQUAL "raw")
    list(APPEND _flags "-nocompress")
endif()

execute_process(
    COMMAND "${TOOL}" ${_flags} "${INPUT}" "${SYMBOL}"
    OUTPUT_FILE "${OUTPUT}"
    RESULT_VARIABLE _rc
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR
        "binary_to_compressed_c failed (${_rc}) on ${INPUT}")
endif()

file(READ "${OUTPUT}" _content)

# Every top-level `const` definition the tool emits should be
# declared `extern const` so the header's extern declarations match
# (and so C++ translation units can reference them without worrying
# about C-vs-C++ linkage rules).
string(REGEX REPLACE "(^|\n)const " "\\1extern const " _content "${_content}")

if(MODE STREQUAL "raw")
    string(REPLACE "_compressed_data" "_data" _content "${_content}")
    string(REPLACE "_compressed_size" "_size" _content "${_content}")
endif()

file(WRITE "${OUTPUT}" "${_content}")
