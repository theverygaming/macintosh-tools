#pragma once
#include "endianness.h"
#include <stdint.h>

struct __attribute__((packed)) dc42_header {
    uint8_t name_len;
    uint8_t name[63];
    uint32_t data_size;
    uint32_t tag_size;
    uint32_t data_chksum;
    uint32_t tag_chksum;
    uint8_t disk_encoding;
    uint8_t format;
    uint16_t magic;
};
#define DC42_HEADER_MAGIC (0x0100)

#define _SWAPFIELD(s, f) (s).f = swap_be((s).f)

#define SWAP_DC42_HEADER(x)     \
    _SWAPFIELD(x, data_size);   \
    _SWAPFIELD(x, tag_size);    \
    _SWAPFIELD(x, data_chksum); \
    _SWAPFIELD(x, tag_chksum);  \
    _SWAPFIELD(x, magic)
