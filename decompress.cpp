#include "decompress.h"

#include <cstdint>
#include <cstdio>

size_t decompress_reserve_extra_bytes() {
    return 273;
}

int64_t decompress_81_83(const char* compressed,
                         size_t compressedLength,
                         char* uncompressed,
                         size_t uncompressedLength,
                         bool is83) {
    size_t in = 0;
    size_t out = 0;

    int literalBits = 0;
    while (true) {
        if (out >= uncompressedLength) {
            return out;
        }

        int isLiteralByte = (literalBits & 1);
        literalBits = (literalBits >> 1);
        if (literalBits == 0) {
            literalBits = static_cast<uint8_t>(compressed[in]);
            ++in;
            isLiteralByte = (literalBits & 1);
            literalBits = (0x80 | (literalBits >> 1));
        }
        if (isLiteralByte) {
            // printf("literal byte 0x%02x\n", (uint8_t)compressed[in]);
            uncompressed[out] = compressed[in];
            ++in;
            ++out;
            continue;
        }

        uint8_t b = static_cast<uint8_t>(compressed[in + 1]);
        if (is83 && (b >= 0xf0)) {
            // multiple copies of the same byte

            b &= 0x0f;
            if (b == 0) {
                // 19 to 274 bytes
                size_t count = static_cast<size_t>(static_cast<uint8_t>(compressed[in])) + 19;
                // printf("multi byte 0x%02x x%d\n", (uint8_t)compressed[in + 2], (int)count);
                for (size_t i = 0; i < count; ++i) {
                    uncompressed[out] = compressed[in + 2];
                    ++out;
                }
                in += 3;
            } else {
                // 4 to 18 bytes
                size_t count = static_cast<size_t>(b) + 3;
                // printf("multi byte 0x%02x x%d\n", (uint8_t)compressed[in], (int)count);
                for (size_t i = 0; i < count; ++i) {
                    uncompressed[out] = compressed[in];
                    ++out;
                }
                in += 2;
            }
        } else {
            // backref into decompressed data

            uint16_t offset = static_cast<uint16_t>(static_cast<uint8_t>(compressed[in]))
                              | (static_cast<uint16_t>(b & 0xf) << 8);
            if (offset == 0) {
                // the game just reads the unwritten output buffer and copies it over itself in this
                // case... while I suppose one *could* use this behavior in a really creative way by
                // pre-initializing the output buffer to something known, I doubt it actually does
                // that. so consider this a corrupted data stream.
                return -1;
            }

            size_t count = (static_cast<uint16_t>(b & 0xf0) >> 4) + 3;
            // printf("backref @%d for %d\n", (int)(out - offset), (int)count);
            for (size_t i = 0; i < count; ++i) {
                uncompressed[out] = uncompressed[out - offset];
                ++out;
            }
            in += 2;
        }
    }
}

int64_t decompress_81(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength) {
    return decompress_81_83(compressed, compressedLength, uncompressed, uncompressedLength, false);
}

int64_t decompress_83(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength) {
    return decompress_81_83(compressed, compressedLength, uncompressed, uncompressedLength, true);
}
