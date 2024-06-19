#include "decompress.h"

#include <cstdint>
#include <cstdio>

size_t decompress_reserve_extra_bytes() {
    return 273;
}

template<bool HasMultiByte, bool DoLogging>
static int64_t decompress_81_83(const char* compressed,
                                size_t compressedLength,
                                char* uncompressed,
                                size_t uncompressedLength) {
    size_t in = 0;
    size_t out = 0;

    int literalBits = 0;
    while (true) {
        if (out >= uncompressedLength) {
            return out;
        }
        if (in >= compressedLength) {
            return -1;
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
            const char c = compressed[in];
            if constexpr (DoLogging) {
                printf("literal byte 0x%02x\n", static_cast<uint8_t>(c));
            }
            uncompressed[out] = c;
            ++in;
            ++out;
            continue;
        }

        if ((in + 1) >= compressedLength) {
            return -1;
        }

        uint8_t b = static_cast<uint8_t>(compressed[in + 1]);
        if (HasMultiByte && (b >= 0xf0)) {
            // multiple copies of the same byte

            b &= 0x0f;
            if (b == 0) {
                if ((in + 2) >= compressedLength) {
                    return -1;
                }

                // 19 to 274 bytes
                const size_t count = static_cast<size_t>(static_cast<uint8_t>(compressed[in])) + 19;
                const char c = compressed[in + 2];
                if constexpr (DoLogging) {
                    printf("multi byte 0x%02x x%d\n",
                           static_cast<uint8_t>(c),
                           static_cast<int>(count));
                }
                for (size_t i = 0; i < count; ++i) {
                    uncompressed[out] = c;
                    ++out;
                }
                in += 3;
            } else {
                // 4 to 18 bytes
                const size_t count = static_cast<size_t>(b) + 3;
                const char c = compressed[in];
                if constexpr (DoLogging) {
                    printf("multi byte 0x%02x x%d\n",
                           static_cast<uint8_t>(c),
                           static_cast<int>(count));
                }
                for (size_t i = 0; i < count; ++i) {
                    uncompressed[out] = c;
                    ++out;
                }
                in += 2;
            }
        } else {
            // backref into decompressed data

            const uint16_t offset = static_cast<uint16_t>(static_cast<uint8_t>(compressed[in]))
                                    | (static_cast<uint16_t>(b & 0xf) << 8);
            if (offset == 0) {
                // the game just reads the unwritten output buffer and copies it over itself in this
                // case... while I suppose one *could* use this behavior in a really creative way by
                // pre-initializing the output buffer to something known, I doubt it actually does
                // that. so consider this a corrupted data stream.
                return -1;
            }
            if (out < offset) {
                // backref to before start of uncompressed data. this is invalid.
                return -1;
            }

            const size_t count = (static_cast<uint16_t>(b & 0xf0) >> 4) + 3;
            if constexpr (DoLogging) {
                printf("backref @%d for %d\n",
                       static_cast<int>(out - offset),
                       static_cast<int>(count));
            }
            for (size_t i = 0; i < count; ++i) {
                uncompressed[out] = uncompressed[out - offset];
                ++out;
            }
            in += 2;
        }
    }
}

static constexpr bool EnableLogging = false;

int64_t decompress_81(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength) {
    return decompress_81_83<false, EnableLogging>(
        compressed, compressedLength, uncompressed, uncompressedLength);
}

int64_t decompress_83(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength) {
    return decompress_81_83<true, EnableLogging>(
        compressed, compressedLength, uncompressed, uncompressedLength);
}
