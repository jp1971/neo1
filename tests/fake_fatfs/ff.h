#pragma once

// Minimal FatFs API surface used only to compile neo1_msc.c for host tests.

#include <stddef.h>
#include <stdint.h>

typedef uint8_t BYTE;
typedef unsigned int UINT;
typedef uint32_t DWORD;
typedef size_t FSIZE_t;
typedef char TCHAR;

typedef struct {
    int unused;
} FATFS;

typedef struct {
    struct {
        FSIZE_t objsize;
    } obj;
    FSIZE_t fptr;
    int file_index;
} FIL;

typedef struct {
    size_t position;
} DIR;

typedef struct {
    FSIZE_t fsize;
    BYTE fattrib;
    TCHAR fname[128];
} FILINFO;

typedef enum {
    FR_OK = 0,
    FR_DISK_ERR,
    FR_INT_ERR,
    FR_NOT_READY,
    FR_NO_FILE,
    FR_NO_PATH,
    FR_INVALID_NAME,
    FR_DENIED,
    FR_EXIST,
    FR_INVALID_OBJECT,
    FR_WRITE_PROTECTED,
    FR_INVALID_DRIVE,
    FR_NOT_ENABLED,
    FR_NO_FILESYSTEM,
    FR_MKFS_ABORTED,
    FR_TIMEOUT,
    FR_LOCKED,
    FR_NOT_ENOUGH_CORE,
    FR_TOO_MANY_OPEN_FILES,
    FR_INVALID_PARAMETER
} FRESULT;

#define FA_READ        0x01
#define FA_WRITE       0x02
#define FA_OPEN_ALWAYS 0x10

#define AM_RDO 0x01
#define AM_DIR 0x10

#define f_size(fp) ((fp)->obj.objsize)

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt);
FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode);
FRESULT f_close(FIL* fp);
FRESULT f_read(FIL* fp, void* buffer, UINT requested, UINT* read_count);
FRESULT f_write(FIL* fp, const void* buffer, UINT requested, UINT* write_count);
FRESULT f_lseek(FIL* fp, FSIZE_t offset);
FRESULT f_truncate(FIL* fp);
FRESULT f_sync(FIL* fp);
FRESULT f_opendir(DIR* dir, const TCHAR* path);
FRESULT f_closedir(DIR* dir);
FRESULT f_readdir(DIR* dir, FILINFO* info);
FRESULT f_unlink(const TCHAR* path);
