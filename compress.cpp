#include "decompress.h"

#include <cstdint>

size_t compress_83_bound(size_t uncompressedLength) {
    return uncompressedLength + (uncompressedLength / 8) + 1;
}

size_t compress_83(const char* uncompressed, size_t uncompressedLength, char* compressed) {
    size_t compressedLength = 0;
    size_t compressedPosition = 0;

    size_t left = uncompressedLength;
    size_t uncompressedPosition = 0;

    do {
        compressed[compressedPosition] = static_cast<char>(0xff);
        ++compressedPosition;

        if (left < 8) {
            for (size_t i = 0; i < left; ++i) {
                compressed[compressedPosition] = uncompressed[uncompressedPosition];
                ++uncompressedPosition;
                ++compressedPosition;
            }
            break;
        } else {
            for (size_t i = 0; i < 8; ++i) {
                compressed[compressedPosition] = uncompressed[uncompressedPosition];
                ++uncompressedPosition;
                ++compressedPosition;
            }
            left -= 8;
        }
    } while (left > 0);

    return compressedPosition;
}
