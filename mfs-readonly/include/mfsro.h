#pragma once
#include <cstdint>
#include <mfs.h>

struct mfs_driver_state {
    void (*read_disk)(void *buf, size_t count, size_t offset);
    size_t disk_part_start;

    uint32_t allocation_block_map_start;
    uint32_t directory_start;

    struct mfs_mdb mdb;
};

struct mfs_file_handle {
    uint32_t seekpos;
    bool resource_fork;
    bool open;
    struct mfs_dirent dirent;
};

// returns nonzero value on error
int init_mfs_driver(struct mfs_driver_state *ctx, void (*read_disk)(void *buf, size_t count, size_t offset), size_t disk_part_start);

// returns false on error
bool mfs_open_file(struct mfs_driver_state *ctx, struct mfs_file_handle *file, const char *path, bool resource_fork = false);

#define MFS_SEEK_CURRENT (0x01)
#define MFS_SEEK_BEGIN   (0x02)
#define MFS_SEEK_END     (0x04)
uint32_t mfs_seek(struct mfs_driver_state *ctx, struct mfs_file_handle *file, int32_t pos, uint8_t flags = MFS_SEEK_CURRENT);

uint32_t mfs_read(struct mfs_driver_state *ctx, struct mfs_file_handle *file, void *buf, uint32_t count);
