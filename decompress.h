#pragma once

#include <cstddef>
#include <cstdint>

// amount of bytes that should be reserved in addition to the uncompressedLength in case the
// compressed data produces more data than what is listed in the header
size_t decompress_reserve_extra_bytes();

int64_t decompress_81(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength);
int64_t decompress_83(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength);
