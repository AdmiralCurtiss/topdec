#include "decompress.h"

#include <cassert>
#include <cstdint>

size_t compress_83_bound(size_t uncompressedLength) {
    return uncompressedLength + (uncompressedLength / 8) + 1;
}

size_t compress_83(const char* uncompressed, size_t uncompressedLength, char* compressed) {
    size_t compressedLength = 0;
    size_t compressedPosition = 0;
    size_t uncompressedPosition = 0;

    int bitsWritten = 0;
    size_t commandBitPosition = 0;

    const auto write_command_bit = [&](int isLiteral) -> void {
        if (bitsWritten == 0) {
            compressed[compressedPosition] = 0;
            commandBitPosition = compressedPosition;
            ++compressedPosition;
        }

        assert(isLiteral == 0 || isLiteral == 1);
        compressed[commandBitPosition] |= static_cast<char>(isLiteral << bitsWritten);

        // once 8 bits have been written the next bit must start a new byte
        bitsWritten = (bitsWritten + 1) & 7;
    };

    const auto write_literal = [&]() -> void {
        write_command_bit(1);
        compressed[compressedPosition] = uncompressed[uncompressedPosition];
        ++uncompressedPosition;
        ++compressedPosition;
    };

    const auto write_same_byte = [&](size_t count) -> void {
        write_command_bit(0);

        assert(count >= 4 && count <= 274);

        if (count <= 18) {
            compressed[compressedPosition] = uncompressed[uncompressedPosition];
            ++compressedPosition;
            compressed[compressedPosition] = static_cast<char>(0xf0 | (count - 3));
            ++compressedPosition;
        } else {
            compressed[compressedPosition] = static_cast<char>(count - 19);
            ++compressedPosition;
            compressed[compressedPosition] = static_cast<char>(0xf0);
            ++compressedPosition;
            compressed[compressedPosition] = uncompressed[uncompressedPosition];
            ++compressedPosition;
        }

        uncompressedPosition += count;
    };

    const auto count_same_byte = [&]() -> size_t {
        size_t count = 0;
        const char c = uncompressed[uncompressedPosition];
        for (size_t i = uncompressedPosition; i < uncompressedLength; ++i) {
            if (uncompressed[i] == c) {
                ++count;
            } else {
                break;
            }
        }
        return count;
    };

    while (uncompressedPosition < uncompressedLength) {
        size_t sameByteCount = count_same_byte();
        if (sameByteCount >= 4) {
            write_same_byte(sameByteCount > 274 ? 274 : sameByteCount);
        } else {
            write_literal();
        }
    }

    return compressedPosition;
}
