#pragma once

#include <cstddef>
#include <cstdint>

bool decompress_81(const char* compressed,
                   size_t compressedLength,
                   char* uncompressed,
                   size_t uncompressedLength);
bool decompress_83(const char* compressed,
                   size_t compressedLength,
                   char* uncompressed,
                   size_t uncompressedLength);
