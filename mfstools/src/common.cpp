#include <algorithm>
#include <common.h>
#include <endianness.h>
#include <stdexcept>
#include <utility>

#define SECTOR_SIZE (512)

static int64_t mactime2unix(uint32_t mactime) {
    int64_t diff = (int64_t)(60 * 60 * 24) * ((365 * (1970 - 1904)) + (((1970 - 1904) / 4) + 1));
    return (int64_t)mactime - diff;
}

static uint32_t unixtime2mac(int64_t unixtime) {
    int64_t diff = (int64_t)(60 * 60 * 24) * ((365 * (1970 - 1904)) + (((1970 - 1904) / 4) + 1));
    int64_t mtime = unixtime + diff;
    if (mtime < 0) {
        mtime = 0;
    }
    if (mtime > UINT32_MAX) {
        mtime = UINT32_MAX;
    }
    return (uint32_t)mtime;
}

static void read_stream(std::iostream &stream, size_t bytes, void *buf) {
    stream.read((char *)buf, bytes);
    if (!stream.good()) {
        throw std::runtime_error("failed to read from input stream");
    }
}

bool maybe_diskcopy42(std::iostream &stream) {
    // test checksum
    stream.seekg(0x52, std::ios_base::beg);
    uint16_t checksum;
    read_stream(stream, sizeof(checksum), &checksum);
    if (swap_be(checksum) != 0x0100) {
        return false;
    }

    // test size
    uint32_t dsize, tsize;
    stream.seekg(0x40, std::ios_base::beg);
    read_stream(stream, sizeof(dsize), &dsize);
    read_stream(stream, sizeof(tsize), &tsize);
    // 0x54 is the size of the Apple Disk Copy 4.2 header
    if ((swap_be(dsize) + swap_be(tsize) + 0x54) != stream.seekg(0, std::ios_base::end).tellg()) {
        return false;
    }

    return true;
}

static bool mfs_namecmp(const char *mfs_name, const char *c_str, size_t mfs_name_len) {
    size_t i;
    for (i = 0; (i < mfs_name_len) && (*c_str != '\0'); i++) {
        if (mfs_name[i] != *c_str++) {
            return false;
        }
    }
    return *c_str == '\0' && i == mfs_name_len;
}

void mfs::read_stream(void *buf, size_t bytes) {
    _stream.get()->read((char *)buf, bytes);
    if (!_stream.get()->good()) {
        throw std::runtime_error("failed to read from input stream");
    }
}

void mfs::read_stream(void *buf, size_t bytes, size_t offset) {
    _stream.get()->seekg(offset, std::ios_base::beg);
    _stream.get()->read((char *)buf, bytes);
    if (!_stream.get()->good()) {
        throw std::runtime_error("failed to read from input stream");
    }
}

uint16_t mfs::get_alloc_block_map_value(uint16_t index) {
    size_t allocation_block_map_start = (SECTOR_SIZE * 2) + sizeof(struct mfs_mdb) + 27;

    index &= 0xFFF;
    index -= 2;
    size_t allocmap_byte_offset = index + (index / 2); // * 1.5
    uint16_t value;
    read_stream(&value, sizeof(value), allocation_block_map_start + allocmap_byte_offset);
    value = swap_be(value);
    // value = (index & 0x01) != 0 ? value >> 4 : value & 0xFFF;
    value = (index & 0x01) != 0 ? value & 0xFFF : value >> 4;
    return value;
}

std::vector<std::pair<struct mfs_dirent, std::string>> mfs::readdir_int() {
    std::vector<std::pair<struct mfs_dirent, std::string>> ret;
    size_t directory_start = (size_t)_mdb.drDirSt * SECTOR_SIZE;
    char *fname_buf = new char[256];
    uint32_t offset = 0;
    struct mfs_dirent dirent;
    for (uint16_t i = 0; i < _mdb.drNmFls; i++) {
        read_stream(&dirent, sizeof(dirent), directory_start + offset);
        SWAP_MFS_DIRENT(dirent);

        if (((dirent.flFlags & MFS_DIRENT_FLAGS_USED) != 0) && (dirent.flLgLen <= dirent.flPyLen) && (dirent.flRLgLen <= dirent.flRPyLen) &&
            (dirent.flNam > 0) && (dirent.flType == 0)) {
            ret.push_back({dirent, ""});
            read_stream(fname_buf, dirent.flNam, directory_start + offset + sizeof(struct mfs_dirent));
            ret.back().second.assign(&fname_buf[0], &fname_buf[dirent.flNam]);
        }

        // dirents are always aligned to 2-byte boundary
        if ((directory_start + offset + sizeof(struct mfs_dirent) + dirent.flNam) % 2 != 0) {
            offset++;
        }

        // donno any other solution rn
        uint8_t term;
        do {
            read_stream(&term, 1, directory_start + offset + sizeof(struct mfs_dirent) + dirent.flNam);
            if (term == 0) {
                offset++;
            }
        } while (term == 0);

        offset += sizeof(struct mfs_dirent) + (size_t)dirent.flNam;
    }
    delete[] fname_buf;
    return ret;
}

mfs::mfs(std::shared_ptr<std::iostream> stream) {
    static_assert(sizeof(size_t) >= sizeof(uint32_t), "size_t must be at least 32-bits wide");
    _stream = stream;
}

bool mfs::init_readonly() {
    read_stream(&_mdb, sizeof(_mdb), SECTOR_SIZE * 2);
    SWAP_MFS_MDB(_mdb);
    if (_mdb.drSigWord != MFS_MDB_SIGNATURE) {
        return false;
    }

    // sanity checks
    if ((_mdb.drAlBlkSiz == 0) || (_mdb.drAlBlkSiz % SECTOR_SIZE != 0) || (_mdb.drClpSiz == 0) || (_mdb.drClpSiz % _mdb.drAlBlkSiz != 0) ||
        (_mdb.drFreeBks > _mdb.drNmAlBlks) || ((_mdb.drBlLen * SECTOR_SIZE) < (_mdb.drNmFls * sizeof(struct mfs_dirent)))) {
        return false;
    }

    uint16_t dirent_block_count = 0;
    int state = 0;
    for (uint16_t i = 2; i < (_mdb.drNmAlBlks + 2); i++) {
        if (get_alloc_block_map_value(i) == MFS_ALLOC_BLOCK_MAP_DIRENTS) {
            dirent_block_count++;
            if (state == 0) {
                state = 1;
            }
        } else {
            if (state == 1) {
                return false;
            }
        }
    }
    // TODO: more sanity checks on dirent_block_count

    return true;
}

std::vector<struct mfs::mfs_dirent_abs> mfs::readdir() {
    std::vector<struct mfs::mfs_dirent_abs> ret;
    for (auto e : readdir_int()) {
        ret.push_back({e.second, e.first.flLgLen, e.first.flRLgLen, mactime2unix(e.first.flCrDat), mactime2unix(e.first.flMdDat)});
    }
    return ret;
}
