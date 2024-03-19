#pragma once

#include <cstddef>
#include <cstdint>

size_t compress_83_bound(size_t uncompressedLength);
size_t compress_83(const char* uncompressed, size_t uncompressedLength, char* compressed);
