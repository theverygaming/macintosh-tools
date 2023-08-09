#pragma once
#include <endianness.h>
#include <stdint.h>

// Data structures from Inside Macintosh II pages 119-123

struct __attribute__((packed)) mfs_mdb {
    uint16_t drSigWord;  // should be MFS_MDB_SIGNATURE
    uint32_t drCrDate;   // initialisation timestamp
    uint32_t drLsBkUp;   // last backup timestamp
    uint16_t drAtrb;     // attributes
    uint16_t drNmFls;    // number of files in directory
    uint16_t drDirSt;    // first block of directory
    uint16_t drBlLen;    // size of directory in blocks
    uint16_t drNmAlBlks; // number of allocation blocks on volume
    uint32_t drAlBlkSiz; // size of allocation blocks
    uint32_t drClpSiz;   // clump size in bytes
    uint16_t drAlBiSt;   // first allocation block sector
    uint32_t drNxtFNum;  // next unused file number
    uint16_t drFreeBks;  // number of unused allocation blocks
    uint8_t drVN;        // length of volume name
    // volume name string follows - 27 bytes long
};
#define MFS_MDB_SIGNATURE (0xD2D7)

#define MFS_ALLOC_BLOCK_MAP_FREE    (0)
#define MFS_ALLOC_BLOCK_MAP_LAST    (1)
#define MFS_ALLOC_BLOCK_MAP_DIRENTS (0xFFF)

struct __attribute__((packed)) mfs_dirent {
    uint8_t flFlags;      // MFS_DIRENT_FLAGS_...
    uint8_t flType;       // version number
    uint8_t flUsrWds[16]; // finder stuff
    uint32_t flFlNum;     // file number
    uint16_t flStBlk;     // first allocation block of data fork
    uint32_t flLgLen;     // logical file size (bytes)
    uint32_t flPyLen;     // physical file size (bytes)
    uint16_t flRStBlk;    // first allocation block of resource fork
    uint32_t flRLgLen;    // logical resource size (bytes)
    uint32_t flRPyLen;    // physical resource size (bytes)
    uint32_t flCrDat;     // creation timestamp
    uint32_t flMdDat;     // modification timestamp
    uint8_t flNam;        // length of file name
    // file name string follows - flNam + 1 bytes long
};

#define MFS_DIRENT_FLAGS_USED   (1 << 7)
#define MFS_DIRENT_FLAGS_LOCKED (1 << 0)

#define _SWAPFIELD(s, f) (s).f = swap_be((s).f)

#define SWAP_MFS_MDB(x)        \
    _SWAPFIELD(x, drSigWord);  \
    _SWAPFIELD(x, drCrDate);   \
    _SWAPFIELD(x, drLsBkUp);   \
    _SWAPFIELD(x, drAtrb);     \
    _SWAPFIELD(x, drNmFls);    \
    _SWAPFIELD(x, drDirSt);    \
    _SWAPFIELD(x, drBlLen);    \
    _SWAPFIELD(x, drNmAlBlks); \
    _SWAPFIELD(x, drAlBlkSiz); \
    _SWAPFIELD(x, drClpSiz);   \
    _SWAPFIELD(x, drAlBiSt);   \
    _SWAPFIELD(x, drNxtFNum);  \
    _SWAPFIELD(x, drFreeBks)

#define SWAP_MFS_DIRENT(x)   \
    _SWAPFIELD(x, flFlNum);  \
    _SWAPFIELD(x, flStBlk);  \
    _SWAPFIELD(x, flLgLen);  \
    _SWAPFIELD(x, flPyLen);  \
    _SWAPFIELD(x, flRStBlk); \
    _SWAPFIELD(x, flRLgLen); \
    _SWAPFIELD(x, flRPyLen); \
    _SWAPFIELD(x, flCrDat);  \
    _SWAPFIELD(x, flMdDat)
