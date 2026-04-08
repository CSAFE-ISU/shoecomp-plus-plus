# Rewrites a generated binary_to_compressed_c .c file so that symbols
# named <sym>_compressed_data / <sym>_compressed_size become
# <sym>_data / <sym>_size. Used for raw (uncompressed) embedded
# assets where the "_compressed_" suffix is misleading.
file(READ "${FILE}" _content)
string(REPLACE "_compressed_data" "_data" _content "${_content}")
string(REPLACE "_compressed_size" "_size" _content "${_content}")
file(WRITE "${FILE}" "${_content}")
