#include <algorithm>
#include <alloca.h>
#include <mfsro.h>

#define SECTOR_SIZE (512)

static uint16_t get_alloc_block_map_value(struct mfs_driver_state *ctx, uint16_t index) {
    uint32_t allocation_block_map_start = (SECTOR_SIZE * 2) + sizeof(struct mfs_mdb) + 27;
    index &= 0xFFF;
    index -= 2;
    size_t allocmap_byte_offset = index + (index / 2); // * 1.5
    uint16_t value;
    ctx->read_disk(&value, sizeof(value), ctx->disk_part_start + allocation_block_map_start + allocmap_byte_offset);
    value = swap_be(value);
    // value = (index & 0x01) != 0 ? value >> 4 : value & 0xFFF;
    value = (index & 0x01) != 0 ? value & 0xFFF : value >> 4;
    return value;
}

static uint32_t mfs_alloc_block_to_sector(struct mfs_driver_state *ctx, uint16_t block) {
    return (ctx->mdb.drAlBiSt * SECTOR_SIZE) + (((uint32_t)block - 2) * ctx->mdb.drAlBlkSiz);
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

static bool mfs_find_file(struct mfs_driver_state *ctx, struct mfs_dirent *dirent, const char *filename) {
    uint32_t directory_start = (uint32_t)ctx->mdb.drDirSt * SECTOR_SIZE;
    char *fname_buf = (char *)alloca(256);
    uint32_t offset = 0;
    for (uint16_t i = 0; i < ctx->mdb.drNmFls; i++) {
        ctx->read_disk(dirent, sizeof(struct mfs_dirent), ctx->disk_part_start + directory_start + offset);
        SWAP_MFS_DIRENT(*dirent);

        if (((dirent->flFlags & MFS_DIRENT_FLAGS_USED) != 0) && (dirent->flLgLen <= dirent->flPyLen) && (dirent->flRLgLen <= dirent->flRPyLen) &&
            (dirent->flNam > 0) && (dirent->flType == 0)) {
            ctx->read_disk(fname_buf, dirent->flNam, ctx->disk_part_start + directory_start + offset + sizeof(struct mfs_dirent));
            if (mfs_namecmp(fname_buf, filename, dirent->flNam)) {
                return true;
            }
        }

        // dirents are always aligned to 2-byte boundary
        if ((directory_start + offset + sizeof(struct mfs_dirent) + dirent->flNam) % 2 != 0) {
            offset++;
        }

        // donno any other solution rn
        uint8_t term;
        do {
            ctx->read_disk(&term, 1, ctx->disk_part_start + directory_start + offset + sizeof(struct mfs_dirent) + dirent->flNam);
            if (term == 0) {
                offset++;
            }
        } while (term == 0);

        offset += sizeof(struct mfs_dirent) + (size_t)dirent->flNam;
    }
    return false;
}

int init_mfs_driver(struct mfs_driver_state *ctx, void (*read_disk)(void *buf, size_t count, size_t offset), size_t disk_part_start) {
    ctx->read_disk = read_disk;
    ctx->disk_part_start = disk_part_start;
    ctx->read_disk(&ctx->mdb, sizeof(ctx->mdb), ctx->disk_part_start + (SECTOR_SIZE * 2));
    SWAP_MFS_MDB(ctx->mdb);
    if (ctx->mdb.drSigWord != MFS_MDB_SIGNATURE) {
        return -1;
    }

    // sanity checks
    if ((ctx->mdb.drAlBlkSiz == 0) || (ctx->mdb.drAlBlkSiz % SECTOR_SIZE != 0) || (ctx->mdb.drClpSiz == 0) ||
        (ctx->mdb.drClpSiz % ctx->mdb.drAlBlkSiz != 0) || (ctx->mdb.drFreeBks > ctx->mdb.drNmAlBlks) ||
        ((ctx->mdb.drBlLen * SECTOR_SIZE) < (ctx->mdb.drNmFls * sizeof(struct mfs_dirent)))) {
        return -2;
    }

    uint16_t dirent_block_count = 0;
    int state = 0;
    for (uint16_t i = 2; i < (ctx->mdb.drNmAlBlks + 2); i++) {
        if (get_alloc_block_map_value(ctx, i) == MFS_ALLOC_BLOCK_MAP_DIRENTS) {
            dirent_block_count++;
            if (state == 0) {
                state = 1;
            }
        } else {
            if (state == 1) {
                return -3;
            }
        }
    }
    // TODO: more sanity checks on dirent_block_count

    return 0;
}

bool mfs_open_file(struct mfs_driver_state *ctx, struct mfs_file_handle *file, const char *path, bool resource_fork) {
    if (!mfs_find_file(ctx, &file->dirent, path)) {
        file->open = false;
        return false;
    }
    file->seekpos = 0;
    file->resource_fork = resource_fork;
    file->open = true;
    return true;
}

uint32_t mfs_seek(struct mfs_driver_state *ctx, struct mfs_file_handle *file, int32_t pos, uint8_t flags) {
    if (!file->open) {
        return 0;
    }
    if (flags & MFS_SEEK_CURRENT) {
        file->seekpos += pos;
    } else if (flags & MFS_SEEK_END) {
        file->seekpos = file->resource_fork ? file->dirent.flRLgLen : file->dirent.flLgLen;
        file->seekpos += pos;
    } else if (flags & MFS_SEEK_BEGIN) {
        file->seekpos = pos;
    }

    file->seekpos = std::min(file->resource_fork ? file->dirent.flRLgLen : file->dirent.flLgLen, file->seekpos);
    return file->seekpos;
}

uint32_t mfs_read(struct mfs_driver_state *ctx, struct mfs_file_handle *file, void *buf, uint32_t count) {
    if (!file->open) {
        return 0;
    }
    uint16_t start_block = file->resource_fork ? file->dirent.flRStBlk : file->dirent.flStBlk;
    uint32_t file_size =
        file->resource_fork ? std::min(file->dirent.flRLgLen, file->dirent.flRPyLen) : std::min(file->dirent.flLgLen, file->dirent.flPyLen);
    if ((start_block == 0) || (file_size == 0)) {
        return 0;
    }

    file->seekpos = std::min(file->seekpos, file_size);
    if ((file->seekpos + count) > file_size) {
        count = file_size - file->seekpos;
    }

    uint16_t current_block = start_block;
    for (uint16_t i = 0; i < (file->seekpos / ctx->mdb.drAlBlkSiz); i++) {
        if (current_block == MFS_ALLOC_BLOCK_MAP_LAST) {
            return 0;
        }
        current_block = get_alloc_block_map_value(ctx, current_block);
        if ((current_block == MFS_ALLOC_BLOCK_MAP_FREE) || (current_block == MFS_ALLOC_BLOCK_MAP_DIRENTS) ||
            (current_block >= (ctx->mdb.drNmAlBlks + 2))) {
            return 0;
        }
    }

    uint32_t current_block_offset = file->seekpos % ctx->mdb.drAlBlkSiz;

    uint32_t leftover_read_count = count;
    while (leftover_read_count != 0) {
        if (current_block_offset >= ctx->mdb.drAlBlkSiz) {
            if (current_block == MFS_ALLOC_BLOCK_MAP_LAST) {
                return count - leftover_read_count;
            }
            current_block = get_alloc_block_map_value(ctx, current_block);
            if ((current_block == MFS_ALLOC_BLOCK_MAP_FREE) || (current_block == MFS_ALLOC_BLOCK_MAP_DIRENTS) ||
                (current_block >= (ctx->mdb.drNmAlBlks + 2))) {
                return count - leftover_read_count;
            }
            current_block_offset = 0;
        }
        uint32_t read_amount = std::min(leftover_read_count, ctx->mdb.drAlBlkSiz - current_block_offset);

        ctx->read_disk((uint8_t *)buf + (count - leftover_read_count),
                       read_amount,
                       ctx->disk_part_start + mfs_alloc_block_to_sector(ctx, current_block) + current_block_offset);

        current_block_offset += read_amount;
        file->seekpos += read_amount;
        leftover_read_count -= read_amount;
    }

    return count;
}
