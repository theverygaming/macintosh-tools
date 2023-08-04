#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dc42.h>
#include <endianness.h>
#include <fstream>

// docs: https://www.discferret.com/wiki/Apple_DiskCopy_4.2

static void read_file(std::fstream &file, size_t bytes, void *buf) {
    file.read((char *)buf, bytes);
    if (!file.good()) {
        fprintf(stderr, "error reading file (%s)\n", std::strerror(errno));
        exit(1);
    }
}

void streamcopy(std::fstream &in, std::fstream &out, size_t bytes, size_t bufsize) {
    uint8_t *buffer = new uint8_t[bufsize];
    while (bytes != 0) {
        size_t chunksize = bytes > bufsize ? bufsize : bytes;
        in.read((char *)buffer, chunksize);
        if (!in.good()) {
            fprintf(stderr, "error reading file (%s)\n", std::strerror(errno));
            exit(1);
        }
        out.write((char *)buffer, chunksize);
        if (!out.good()) {
            fprintf(stderr, "error writing file (%s)\n", std::strerror(errno));
            exit(1);
        }
        bytes -= chunksize;
    }
    delete[] buffer;
}

static bool verify_chksum(std::fstream &file, struct dc42_header *header) {
    file.seekg(sizeof(struct dc42_header), std::ios_base::beg);
    uint32_t data_chksum = 0;
    for (uint32_t i = 0; i < header->data_size / 2; i++) {
        uint16_t word;
        read_file(file, sizeof(word), &word);
        word = swap_be(word);
        data_chksum += word;
        data_chksum = (data_chksum >> 1) | (data_chksum << 31);
    }
    if (data_chksum != header->data_chksum) {
        fprintf(stderr, "Data checksum invalid!\n");
        return false;
    }
    // the first 12 bytes of the tag section are skipped due to a bug in an old Apple DiskCopy version
    if (header->tag_size > 12) {
        uint32_t tag_chksum = 0;
        file.seekg(sizeof(struct dc42_header) + header->data_size + 12, std::ios_base::beg);
        for (uint32_t i = 0; i < (header->tag_size - 12) / 2; i++) {
            uint16_t word;
            read_file(file, sizeof(word), &word);
            word = swap_be(word);
            tag_chksum += word;
            tag_chksum = (tag_chksum >> 1) | (tag_chksum << 31);
        }
        if (tag_chksum != header->tag_chksum) {
            fprintf(stderr, "Tag checksum invalid!\n");
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s [input file] [output file]\n", argv[0]);
        exit(1);
    }
    std::fstream infile = std::fstream(argv[1], std::ios::in | std::ios::binary);
    if (!infile.is_open()) {
        fprintf(stderr, "failed to open input file (%s)\n", std::strerror(errno));
        exit(1);
    }
    struct dc42_header header;
    infile.seekg(0, std::ios_base::beg);
    read_file(infile, sizeof(header), &header);
    SWAP_DC42_HEADER(header);
    if ((header.magic != DC42_HEADER_MAGIC) ||
        ((header.data_size + header.tag_size + sizeof(header)) != infile.seekg(0, std::ios_base::end).tellg())) {
        fprintf(stderr, "File was not recognized as a valid DiskCopy 4.2 image!\n");
        exit(1);
    }
    if (!verify_chksum(infile, &header)) {
        fprintf(stderr, "Checksum invalid!\n");
        exit(1);
    }

    std::fstream outfile = std::fstream(argv[2], std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
        fprintf(stderr, "failed to open output file (%s)\n", std::strerror(errno));
        exit(1);
    }

    infile.seekg(sizeof(struct dc42_header), std::ios_base::beg);
    streamcopy(infile, outfile, header.data_size, 512 * 20);
    fprintf(stderr, "Successfully extracted data from DiskCopy image!\n");

    return 0;
}
