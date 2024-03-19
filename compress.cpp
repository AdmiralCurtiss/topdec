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

    struct Backref {
        size_t Length;
        size_t Position;
    };

    constexpr static size_t minBackrefLength = 3;
    constexpr static size_t maxBackrefLength = 17;

    // TODO: what happens if the offset is 0? does that count as 4096 does it do nonsense?
    constexpr static size_t minBackrefOffset = 1;
    constexpr static size_t maxBackrefOffset = 4095;

    const auto write_backref = [&](size_t length, size_t offset) -> void {
        write_command_bit(0);

        assert(length >= minBackrefLength && length <= maxBackrefLength);
        assert(offset >= minBackrefOffset && offset <= maxBackrefOffset);

        compressed[compressedPosition] = static_cast<char>(offset & 0xff);
        ++compressedPosition;
        compressed[compressedPosition] =
            static_cast<char>(((offset >> 8) & 0xf) | ((length - minBackrefLength) << 4));
        ++compressedPosition;

        uncompressedPosition += length;
    };

    const auto find_best_backref = [&]() -> Backref {
        Backref bestBackref{0, 0};

        if (uncompressedPosition < minBackrefOffset) {
            return bestBackref; // no backref possible
        }

        const size_t firstPossibleBackrefPosition =
            uncompressedPosition < maxBackrefOffset ? 0 : (uncompressedPosition - maxBackrefOffset);
        const size_t lastPossibleBackrefPosition = uncompressedPosition - 1;
        const size_t allowedBackrefLength =
            (uncompressedLength - uncompressedPosition) >= maxBackrefLength
                ? maxBackrefLength
                : (uncompressedLength - uncompressedPosition);

        if (allowedBackrefLength < minBackrefLength) {
            return bestBackref; // no backref possible
        }

        size_t currentBackrefTest = lastPossibleBackrefPosition;
        const auto count_backref_from_here = [&]() -> size_t {
            size_t count = 0;
            for (size_t i = 0; i < allowedBackrefLength; ++i) {
                if (uncompressed[currentBackrefTest + i]
                    == uncompressed[uncompressedPosition + i]) {
                    ++count;
                } else {
                    break;
                }
            }
            return count;
        };
        while (true) {
            size_t length = count_backref_from_here();
            if (length > bestBackref.Length) {
                bestBackref.Length = length;
                bestBackref.Position = currentBackrefTest;
            }
            if (length == maxBackrefLength) {
                break;
            }

            if (currentBackrefTest == firstPossibleBackrefPosition) {
                break;
            }
            --currentBackrefTest;
        }

        return bestBackref;
    };

    while (uncompressedPosition < uncompressedLength) {
        const size_t sameByteCount = count_same_byte();
        const auto bestBackref = find_best_backref();
        const bool sameByteCountValid = sameByteCount >= 4;
        const bool backrefValid = bestBackref.Length >= minBackrefLength;
        if (sameByteCountValid && backrefValid) {
            if (bestBackref.Length >= sameByteCount) {
                write_backref(bestBackref.Length, uncompressedPosition - bestBackref.Position);
            } else {
                write_same_byte(sameByteCount > 274 ? 274 : sameByteCount);
            }
        } else if (sameByteCountValid) {
            write_same_byte(sameByteCount > 274 ? 274 : sameByteCount);
        } else if (backrefValid) {
            write_backref(bestBackref.Length, uncompressedPosition - bestBackref.Position);
        } else {
            write_literal();
        }
    }

    return compressedPosition;
}
