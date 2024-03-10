#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "decompress.h"
#include "file.h"

static void PrintUsage() {
    printf(
        "Usage for decompression:\n"
        "  topdec d (path to compressed input) [path to uncompressed output]\n"
        "Output will be input file + '.dec' if not given.\n");
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


        SenPatcher::IO::File infile(source, SenPatcher::IO::OpenMode::Read);
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

        uint8_t compressionType = compressed[0];
        uint16_t compressedLength =
            static_cast<uint8_t>(compressed[1]) | (static_cast<uint8_t>(compressed[2]) << 8);
        uint16_t uncompressedLength =
            static_cast<uint8_t>(compressed[5]) | (static_cast<uint8_t>(compressed[6]) << 8);

        std::vector<char> uncompressed;
        uncompressed.resize(uncompressedLength);

        if (compressionType != 0x83) {
            printf("unsupported compression format\n");
            return -1;
        }

        if (!decompress_83(
                compressed.data() + 9, compressedLength, uncompressed.data(), uncompressedLength)) {
            printf("decompression failure\n");
            return -1;
        }

        SenPatcher::IO::File outfile(target, SenPatcher::IO::OpenMode::Write);
        if (!outfile.IsOpen()) {
            printf("failed to open output file\n");
            return -1;
        }
        if (outfile.Write(uncompressed.data(), uncompressed.size()) != uncompressed.size()) {
            printf("failed to write output file\n");
            return -1;
        }

        return 0;
    }

    PrintUsage();
    return -1;
}
