#pragma once
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mfs.h>
#include <string>
#include <utility>
#include <vector>

bool maybe_diskcopy42(std::iostream &stream);

class mfs {
public:
    mfs(std::shared_ptr<std::iostream> stream);
    bool init_readonly();

    struct mfs_dirent_abs {
        std::string name;
        size_t fsize; // file size
        size_t rsize; // resource size
        // unix timestamps
        int64_t ctime;
        int64_t mtime;
    };

    std::vector<struct mfs_dirent_abs> readdir();

private:
    void read_stream(void *buf, size_t bytes);
    void read_stream(void *buf, size_t bytes, size_t offset);

    uint16_t get_alloc_block_map_value(uint16_t index);

    std::vector<std::pair<struct mfs_dirent, std::string>> readdir_int();

    std::shared_ptr<std::iostream> _stream;
    struct mfs_mdb _mdb;
};
