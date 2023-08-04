#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <endianness.h>
#include <fstream>
#include <mfs.h>
#include <mfsro.h>

#define SECTOR_SIZE (512)

static void read_file(std::fstream &file, size_t bytes, void *buf) {
    file.read((char *)buf, bytes);
    if (!file.good()) {
        fprintf(stderr, "error reading file (%s)\n", std::strerror(errno));
        exit(1);
    }
}

static bool is_diskcopy42(std::fstream &file) {
    // test checksum
    file.seekg(0x52, std::ios_base::beg);
    uint16_t checksum;
    read_file(file, sizeof(checksum), &checksum);
    if (swap_be(checksum) != 0x0100) {
        return false;
    }

    // test size
    uint32_t dsize, tsize;
    file.seekg(0x40, std::ios_base::beg);
    read_file(file, sizeof(dsize), &dsize);
    read_file(file, sizeof(tsize), &tsize);
    // 0x54 is the size of the Apple Disk Copy 4.2 header
    if ((swap_be(dsize) + swap_be(tsize) + 0x54) != file.seekg(0, std::ios_base::end).tellg()) {
        return false;
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s [MFS image filename] [file in MFS image] [output file]\n", argv[0]);
        exit(1);
    }
    static std::fstream infile = std::fstream(argv[1], std::ios::in | std::ios::binary);
    if (!infile.is_open()) {
        fprintf(stderr, "Failed to open input file (%s)\n", std::strerror(errno));
        exit(1);
    }
    if (is_diskcopy42(infile)) {
        fprintf(stderr, "This may be a Apple DiskCopy 4.2 image! Extract it before using it with this tool.\n");
    }

    infile.seekg(SECTOR_SIZE * 2, std::ios_base::beg); // skip boot blocks
    struct mfs_mdb mdb;
    read_file(infile, sizeof(mdb), &mdb);
    SWAP_MFS_MDB(mdb);
    if (mdb.drSigWord != MFS_MDB_SIGNATURE) {
        fprintf(stderr, "Master Directory Block signature mismatch\n");
    } else {
        char *name = new char[mdb.drVN + 1];
        read_file(infile, mdb.drVN, name);
        name[mdb.drVN] = 0;
        printf("Volume name: \"%s\"\n", name);
        delete[] name;
    }

    struct mfs_driver_state state;
    if (init_mfs_driver(
            &state,
            [](void *buf, size_t count, size_t offset) {
                infile.seekg(offset, std::ios_base::beg);
                read_file(infile, count, buf);
            },
            0x0) != 0) {
        fprintf(stderr, "Error initializing MFS driver\n");
        exit(1);
    }

    struct mfs_file_handle file;
    if (!mfs_open_file(&state, &file, argv[2], false)) {
        fprintf(stderr, "Error opening file on MFS image\n");
        exit(1);
    }
    uint32_t size = mfs_seek(&state, &file, 0, MFS_SEEK_END);
    printf("File size: %u\n", size);
    mfs_seek(&state, &file, 0, MFS_SEEK_BEGIN);
    uint8_t *buf = new uint8_t[size];
    uint32_t read = mfs_read(&state, &file, buf, size);
    if (read != size) {
        fprintf(stderr, "Error reading file\n");
        exit(1);
    }
    std::fstream outfile = std::fstream(argv[3], std::ios::out | std::ios::binary);
    if (!outfile.is_open()) {
        fprintf(stderr, "Failed to open output file (%s)\n", std::strerror(errno));
        exit(1);
    }
    outfile.write((char *)buf, read);
    outfile.close();
    delete[] buf;

    return 0;
}
