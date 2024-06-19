#include "decompress.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

static void InitializeDictionary(char* dict) {
    size_t offset = 0;
    for (size_t i = 0; i < 0x100; ++i) {
        dict[offset++] = static_cast<char>(i);
        dict[offset++] = static_cast<char>(0);
        dict[offset++] = static_cast<char>(i);
        dict[offset++] = static_cast<char>(0);
        dict[offset++] = static_cast<char>(i);
        dict[offset++] = static_cast<char>(0);
        dict[offset++] = static_cast<char>(i);
        dict[offset++] = static_cast<char>(0);
    }
    for (size_t i = 0; i < 0x100; ++i) {
        dict[offset++] = static_cast<char>(i);
        dict[offset++] = static_cast<char>(0xff);
        dict[offset++] = static_cast<char>(i);
        dict[offset++] = static_cast<char>(0xff);
        dict[offset++] = static_cast<char>(i);
        dict[offset++] = static_cast<char>(0xff);
        dict[offset++] = static_cast<char>(i);
    }
    for (size_t i = 0; i < 0x100; ++i) {
        dict[offset++] = 0;
    }
    assert(offset == 0x1000);
}

size_t decompress_reserve_extra_bytes() {
    return 273;
}

template<bool HasDict, bool HasMultiByte, bool DoLogging>
static int64_t decompress_internal(const char* compressed,
                                   size_t compressedLength,
                                   char* uncompressed,
                                   size_t uncompressedLength) {
    std::array<char, HasDict ? 0x1000 : 0> dict;
    size_t in = 0;
    size_t out = 0;
    size_t dictpos = 0;

    if constexpr (HasDict) {
        InitializeDictionary(dict.data());
        dictpos = HasMultiByte ? 0xfef : 0xfee;
    }

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
            if constexpr (HasDict) {
                dict[dictpos] = c;
                dictpos = (dictpos + 1u) & 0xfffu;
            }
            ++in;
            ++out;
            continue;
        }

        if ((in + 1) >= compressedLength) {
            return -1;
        }

        const uint8_t b = static_cast<uint8_t>(compressed[in + 1]);
        const uint8_t blow = static_cast<uint8_t>(b & 0xf);
        const uint8_t bhigh = static_cast<uint8_t>((b & 0xf0) >> 4);
        const uint8_t nibble1 = HasDict ? blow : bhigh;
        const uint8_t nibble2 = HasDict ? bhigh : blow;
        if (HasMultiByte && (nibble1 == 0xf)) {
            // multiple copies of the same byte

            if (nibble2 == 0) {
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
                    if constexpr (HasDict) {
                        dict[dictpos] = c;
                        dictpos = (dictpos + 1u) & 0xfffu;
                    }
                    ++out;
                }
                in += 3;
            } else {
                // 4 to 18 bytes
                const size_t count = static_cast<size_t>(nibble2) + 3;
                const char c = compressed[in];
                if constexpr (DoLogging) {
                    printf("multi byte 0x%02x x%d\n",
                           static_cast<uint8_t>(c),
                           static_cast<int>(count));
                }
                for (size_t i = 0; i < count; ++i) {
                    uncompressed[out] = c;
                    if constexpr (HasDict) {
                        dict[dictpos] = c;
                        dictpos = (dictpos + 1u) & 0xfffu;
                    }
                    ++out;
                }
                in += 2;
            }
        } else {
            const uint16_t offset = static_cast<uint16_t>(static_cast<uint8_t>(compressed[in]))
                                    | (static_cast<uint16_t>(nibble2) << 8);
            const size_t count = static_cast<uint16_t>(nibble1) + 3;

            if constexpr (HasDict) {
                // reference into dictionary
                if constexpr (DoLogging) {
                    printf("dictref @0x%03x for %d\n",
                           static_cast<int>(offset),
                           static_cast<int>(count));
                }
                for (size_t i = 0; i < count; ++i) {
                    const char c = dict[(offset + i) & 0xfffu];
                    uncompressed[out] = c;
                    dict[dictpos] = c;
                    dictpos = (dictpos + 1u) & 0xfffu;
                    ++out;
                }
            } else {
                // backref into decompressed data
                if (offset == 0) {
                    // the game just reads the unwritten output buffer and copies it over itself in
                    // this case... while I suppose one *could* use this behavior in a really
                    // creative way by pre-initializing the output buffer to something known, I
                    // doubt it actually does that. so consider this a corrupted data stream.
                    return -1;
                }
                if (out < offset) {
                    // backref to before start of uncompressed data. this is invalid.
                    return -1;
                }

                if constexpr (DoLogging) {
                    printf("backref @%d for %d\n",
                           static_cast<int>(out - offset),
                           static_cast<int>(count));
                }
                for (size_t i = 0; i < count; ++i) {
                    uncompressed[out] = uncompressed[out - offset];
                    ++out;
                }
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
    return decompress_internal<false, false, EnableLogging>(
        compressed, compressedLength, uncompressed, uncompressedLength);
}

int64_t decompress_83(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength) {
    return decompress_internal<false, true, EnableLogging>(
        compressed, compressedLength, uncompressed, uncompressedLength);
}

int64_t decompress_01(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength) {
    return decompress_internal<true, false, EnableLogging>(
        compressed, compressedLength, uncompressed, uncompressedLength);
}

int64_t decompress_03(const char* compressed,
                      size_t compressedLength,
                      char* uncompressed,
                      size_t uncompressedLength) {
    return decompress_internal<true, true, EnableLogging>(
        compressed, compressedLength, uncompressed, uncompressedLength);
}
