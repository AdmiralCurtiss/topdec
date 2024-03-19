#include "decompress.h"

#include <cstdint>

bool decompress_83(const char* compressed,
                   size_t compressedLength,
                   char* uncompressed,
                   size_t uncompressedLength) {
    size_t in = 0;
    size_t out = 0;

    int literalBits = 0;
    while (true) {
        if (out >= uncompressedLength) {
            return out == uncompressedLength;
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
            uncompressed[out] = compressed[in];
            ++in;
            ++out;
            continue;
        }

        uint8_t b = static_cast<uint8_t>(compressed[in + 1]);
        if (b >= 0xf0) {
            // multiple copies of the same byte

            b &= 0x0f;
            if (b == 0) {
                // 19 to 274 bytes
                size_t count = static_cast<size_t>(static_cast<uint8_t>(compressed[in])) + 19;
                for (size_t i = 0; i < count; ++i) {
                    uncompressed[out] = compressed[in + 2];
                    ++out;
                }
                in += 3;
            } else {
                // 4 to 18 bytes
                size_t count = static_cast<size_t>(b) + 3;
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
            size_t count = (static_cast<uint16_t>(b & 0xf0) >> 4) + 3;
            for (size_t i = 0; i < count; ++i) {
                uncompressed[out] = uncompressed[out - offset];
                ++out;
            }
            in += 2;
        }
    }
}
