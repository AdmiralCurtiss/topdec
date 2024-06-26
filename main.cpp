#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "compress.h"
#include "decompress.h"
#include "file.h"

static void PrintUsage() {
    printf(
        "Usage for decompression:\n"
        "  topdec d (path to compressed input) [path to uncompressed output]\n"
        "Output will be input file + '.dec' if not given.\n"
        "\n"
        "Usage for compression:\n"
        "  topdec c [options] (path to decompressed input) [path to compressed output]\n"
        "  Options are:\n"
        "    --type 81/83 (defaults to 83)\n"
        "Output will be input file + '.comp' if not given.\n");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        PrintUsage();
        return -1;
    }

    if (strcmp("d", argv[1]) == 0) {
        std::string_view source(argv[2]);
        std::string_view target;
        std::string tmp;
        if (argc < 4) {
            tmp = std::string(source);
            tmp += ".dec";
            target = tmp;
        } else {
            target = std::string_view(argv[3]);
        }


        HyoutaUtils::IO::File infile(std::filesystem::path(source),
                                     HyoutaUtils::IO::OpenMode::Read);
        if (!infile.IsOpen()) {
            printf("failed to open input file\n");
            return -1;
        }
        std::vector<char> compressed;
        const auto infileLength = infile.GetLength();
        if (!infileLength) {
            printf("failed to get size of input file\n");
            return -1;
        }
        compressed.resize(*infileLength);
        if (infile.Read(compressed.data(), compressed.size()) != compressed.size()) {
            printf("failed to read input file\n");
            return -1;
        }

        if (compressed.size() < 9) {
            printf("input file too small\n");
            return -1;
        }

        uint8_t compressionType = static_cast<uint8_t>(compressed[0]);
        uint32_t compressedLength =
            static_cast<uint32_t>(static_cast<uint8_t>(compressed[1]))
            | (static_cast<uint32_t>(static_cast<uint8_t>(compressed[2])) << 8)
            | (static_cast<uint32_t>(static_cast<uint8_t>(compressed[3])) << 16)
            | (static_cast<uint32_t>(static_cast<uint8_t>(compressed[4])) << 24);
        uint32_t uncompressedLength =
            static_cast<uint32_t>(static_cast<uint8_t>(compressed[5]))
            | (static_cast<uint32_t>(static_cast<uint8_t>(compressed[6])) << 8)
            | (static_cast<uint32_t>(static_cast<uint8_t>(compressed[7])) << 16)
            | (static_cast<uint32_t>(static_cast<uint8_t>(compressed[8])) << 24);

        std::vector<char> uncompressed;
        uncompressed.resize(uncompressedLength + decompress_reserve_extra_bytes());

        int64_t decompressResult = -2;
        if (compressionType == 0x00 && compressedLength == uncompressedLength) {
            std::memcpy(uncompressed.data(), compressed.data() + 9, compressedLength);
            decompressResult = compressedLength;
        } else if (compressionType == 0x01) {
            decompressResult = decompress_01(
                compressed.data() + 9, compressedLength, uncompressed.data(), uncompressedLength);
        } else if (compressionType == 0x03) {
            decompressResult = decompress_03(
                compressed.data() + 9, compressedLength, uncompressed.data(), uncompressedLength);
        } else if (compressionType == 0x81) {
            decompressResult = decompress_81(
                compressed.data() + 9, compressedLength, uncompressed.data(), uncompressedLength);
        } else if (compressionType == 0x83) {
            decompressResult = decompress_83(
                compressed.data() + 9, compressedLength, uncompressed.data(), uncompressedLength);
        } else {
            printf("unsupported compression format\n");
            return -1;
        }

        if (decompressResult < 0) {
            printf("decompression failure\n");
            return -1;
        }

        if (static_cast<size_t>(decompressResult) != uncompressedLength) {
            printf("WARNING: Header specified 0x%zx bytes but decompression produced 0x%zx bytes\n",
                   static_cast<size_t>(uncompressedLength),
                   static_cast<size_t>(decompressResult));
        }

        HyoutaUtils::IO::File outfile(std::filesystem::path(target),
                                      HyoutaUtils::IO::OpenMode::Write);
        if (!outfile.IsOpen()) {
            printf("failed to open output file\n");
            return -1;
        }
        if (outfile.Write(uncompressed.data(), static_cast<size_t>(decompressResult))
            != static_cast<size_t>(decompressResult)) {
            printf("failed to write output file\n");
            return -1;
        }

        return 0;
    }

    if (strcmp("c", argv[1]) == 0) {
        int compressionType = 0x83;
        int idx = 2;
        while (idx < argc) {
            if (strcmp("--type", argv[idx]) == 0) {
                ++idx;
                if (idx < argc) {
                    if (strcmp("81", argv[idx]) == 0) {
                        compressionType = 0x81;
                    } else if (strcmp("83", argv[idx]) == 0) {
                        compressionType = 0x83;
                    } else {
                        printf("Invalid compression type.\n");
                        return -1;
                    }
                } else {
                    PrintUsage();
                    return -1;
                }
                ++idx;
                continue;
            }

            break;
        }

        std::string_view source(argv[idx]);
        std::string_view target;
        std::string tmp;
        if (argc - 2 < idx) {
            tmp = std::string(source);
            tmp += ".comp";
            target = tmp;
        } else {
            target = std::string_view(argv[idx + 1]);
        }


        HyoutaUtils::IO::File infile(std::filesystem::path(source),
                                     HyoutaUtils::IO::OpenMode::Read);
        if (!infile.IsOpen()) {
            printf("failed to open input file\n");
            return -1;
        }
        std::vector<char> uncompressed;
        const auto infileLength = infile.GetLength();
        if (!infileLength) {
            printf("failed to get size of input file\n");
            return -1;
        }
        if (*infileLength >= 0x10000) {
            printf("input too large\n");
            return -1;
        }
        uncompressed.resize(*infileLength);
        if (infile.Read(uncompressed.data(), uncompressed.size()) != uncompressed.size()) {
            printf("failed to read input file\n");
            return -1;
        }

        size_t headerSize = 9;
        std::vector<char> compressed;
        compressed.resize(compress_81_83_bound(uncompressed.size()) + headerSize);

        size_t compressedSize;
        if (compressionType == 0x81) {
            compressedSize = compress_81(
                uncompressed.data(), uncompressed.size(), compressed.data() + headerSize);
        } else if (compressionType == 0x83) {
            compressedSize = compress_83(
                uncompressed.data(), uncompressed.size(), compressed.data() + headerSize);
        } else {
            printf("invalid compression type\n");
            return -1;
        }

        if (compressedSize >= 0x10000) {
            printf("output too large\n");
            return -1;
        }

        compressed[0] = static_cast<char>(static_cast<uint8_t>(compressionType));
        compressed[1] = static_cast<char>(compressedSize & 0xff);
        compressed[2] = static_cast<char>((compressedSize >> 8) & 0xff);
        compressed[3] = 0;
        compressed[4] = 0;
        compressed[5] = static_cast<char>(uncompressed.size() & 0xff);
        compressed[6] = static_cast<char>((uncompressed.size() >> 8) & 0xff);
        compressed[7] = 0;
        compressed[8] = 0;

        HyoutaUtils::IO::File outfile(std::filesystem::path(target),
                                      HyoutaUtils::IO::OpenMode::Write);
        if (!outfile.IsOpen()) {
            printf("failed to open output file\n");
            return -1;
        }
        if (outfile.Write(compressed.data(), compressedSize + headerSize)
            != (compressedSize + headerSize)) {
            printf("failed to write output file\n");
            return -1;
        }

        return 0;
    }

    PrintUsage();
    return -1;
}
