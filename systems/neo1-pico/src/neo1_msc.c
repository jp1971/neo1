// Pico FatFs backend for the shared Neo1 MSC register protocol.
//
// This module owns only FatFs volume/file/directory handles and synchronous
// filesystem operations. Command sequencing, register state, buffering,
// filtering, and status presentation belong to devices/neo1_msc.c.

#include "neo1_msc.h"

#include "ff.h"

#include <stdio.h>
#include <string.h>

static FATFS g_fatfs;
static FIL g_file;
static DIR g_dir;

static FRESULT neo1_msc_fatfs_mount(void) {
    return f_mount(&g_fatfs, "0:", 1);
}

static uint8_t neo1_msc_fatfs_open(void* context,
                                    const char* name,
                                    uint32_t* out_size) {
    (void)context;
    FRESULT result = neo1_msc_fatfs_mount();
    if (result != FR_OK) {
        return (uint8_t)result;
    }

    result = f_open(&g_file, name, FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
    if (result != FR_OK) {
#if NEO1_MSC_DEBUG
        printf("[msc] open '%s' failed: err=%d\n", name, result);
#endif
        return (uint8_t)result;
    }

    *out_size = (uint32_t)f_size(&g_file);
#if NEO1_MSC_DEBUG
    printf("[msc] opened '%s'\n", name);
#endif
    return 0;
}

static void neo1_msc_fatfs_close(void* context) {
    (void)context;
    (void)f_close(&g_file);
}

static uint8_t neo1_msc_fatfs_read(void* context,
                                    uint16_t sector,
                                    uint8_t* buffer,
                                    size_t* out_size) {
    (void)context;
    FRESULT result = f_lseek(&g_file, (DWORD)sector * NEO1_MSC_SECTOR_SIZE);
    if (result != FR_OK) {
#if NEO1_MSC_DEBUG
        printf("[msc] read sector %u failed: err=%d\n",
               (unsigned)sector, result);
#endif
        return (uint8_t)result;
    }

    UINT bytes_read = 0;
    result = f_read(&g_file, buffer, NEO1_MSC_SECTOR_SIZE, &bytes_read);
    if (result != FR_OK) {
#if NEO1_MSC_DEBUG
        printf("[msc] read sector %u: f_read failed: err=%d\n",
               (unsigned)sector, result);
#endif
        return (uint8_t)result;
    }

    *out_size = bytes_read;
#if NEO1_MSC_DEBUG
    printf("[msc] read sector %u: %u bytes\n",
           (unsigned)sector, (unsigned)bytes_read);
#endif
    return 0;
}

static uint8_t neo1_msc_fatfs_write(void* context,
                                     uint16_t sector,
                                     const uint8_t* buffer,
                                     uint16_t size,
                                     uint32_t* out_file_size) {
    (void)context;
    FRESULT result = f_lseek(&g_file, (DWORD)sector * NEO1_MSC_SECTOR_SIZE);
    if (result != FR_OK) {
        return (uint8_t)result;
    }

    UINT bytes_written = 0;
    result = f_write(&g_file, buffer, size, &bytes_written);
    if (result != FR_OK) {
        return (uint8_t)result;
    }
    if (bytes_written != size) {
        return 1;
    }

    const DWORD target_size =
        ((DWORD)sector * NEO1_MSC_SECTOR_SIZE) + (DWORD)size;
    result = f_lseek(&g_file, target_size);
    if (result != FR_OK) {
        return (uint8_t)result;
    }
    result = f_truncate(&g_file);
    if (result != FR_OK) {
        return (uint8_t)result;
    }
    result = f_sync(&g_file);
    if (result != FR_OK) {
        return (uint8_t)result;
    }

    *out_file_size = (uint32_t)f_size(&g_file);
    return 0;
}

static uint8_t neo1_msc_fatfs_dir_open(void* context) {
    (void)context;
    FRESULT result = neo1_msc_fatfs_mount();
    if (result != FR_OK) {
        return (uint8_t)result;
    }
    result = f_opendir(&g_dir, "0:");
#if NEO1_MSC_DEBUG
    if (result == FR_OK) {
        printf("[msc] dir open ok\n");
    } else {
        printf("[msc] dir open failed: err=%d\n", result);
    }
#endif
    return (uint8_t)result;
}

static uint8_t neo1_msc_fatfs_dir_next(void* context,
                                        neo1_msc_dir_entry_t* out_entry) {
    (void)context;
    FILINFO info;
    const FRESULT result = f_readdir(&g_dir, &info);
    if (result != FR_OK) {
        return (uint8_t)result;
    }
    if (info.fname[0] == '\0') {
        out_entry->valid = false;
        return 0;
    }

    const size_t name_size = strlen(info.fname);
    const size_t copy_size = name_size < (sizeof(out_entry->name) - 1u)
                                 ? name_size
                                 : (sizeof(out_entry->name) - 1u);
    memcpy(out_entry->name, info.fname, copy_size);
    out_entry->name[copy_size] = '\0';
    out_entry->size = (uint32_t)info.fsize;
    out_entry->directory = (info.fattrib & AM_DIR) != 0;
    out_entry->valid = true;
    return 0;
}

static void neo1_msc_fatfs_dir_close(void* context) {
    (void)context;
    (void)f_closedir(&g_dir);
}

static uint8_t neo1_msc_fatfs_delete(void* context, const char* name) {
    (void)context;
#if NEO1_MSC_DEBUG
    printf("[msc] delete '%s'\n", name);
#endif
    return (uint8_t)f_unlink(name);
}

static const neo1_msc_backend_ops_t neo1_msc_fatfs_ops = {
    .open = neo1_msc_fatfs_open,
    .close = neo1_msc_fatfs_close,
    .read_sector = neo1_msc_fatfs_read,
    .write_sector = neo1_msc_fatfs_write,
    .dir_open = neo1_msc_fatfs_dir_open,
    .dir_next = neo1_msc_fatfs_dir_next,
    .dir_close = neo1_msc_fatfs_dir_close,
    .delete_file = neo1_msc_fatfs_delete,
};

const neo1_msc_backend_t* neo1_msc_fatfs_backend(void) {
    static const neo1_msc_backend_t backend = {
        .ops = &neo1_msc_fatfs_ops,
        .context = NULL,
        .flags = 0,
        .not_found_error = FR_NO_FILE,
        .invalid_state_error = FR_INVALID_OBJECT,
    };
    return &backend;
}
