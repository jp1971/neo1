#include "neo1_msc.h"
#include "ff.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAKE_FILE_CAPACITY 4096u
#define FAKE_FILE_COUNT 8u

typedef struct {
    bool present;
    bool read_only;
    uint8_t attributes;
    char name[128];
    uint8_t data[FAKE_FILE_CAPACITY];
    size_t size;
} fake_file_t;

static fake_file_t g_files[FAKE_FILE_COUNT];
static FRESULT g_mount_result;
static FRESULT g_open_result;
static FRESULT g_opendir_result;
static FRESULT g_readdir_result;
static FRESULT g_lseek_result;
static FRESULT g_read_result;
static FRESULT g_write_result;
static FRESULT g_truncate_result;
static FRESULT g_sync_result;
static FRESULT g_unlink_result;
static bool g_short_write;
static int g_failures;
static neo1_msc_t g_msc;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            g_failures++; \
        } \
    } while (0)

static uint8_t neo1_msc_io_read(uint16_t addr) {
    return neo1_msc_read(&g_msc, addr);
}

static void neo1_msc_io_write(uint16_t addr, uint8_t data) {
    neo1_msc_write(&g_msc, addr, data);
}

static void test_protocol_init(void) {
    CHECK(neo1_msc_init(&g_msc, neo1_msc_fatfs_backend()));
}

static void fake_reset(void) {
    memset(g_files, 0, sizeof(g_files));
    g_mount_result = FR_OK;
    g_open_result = FR_OK;
    g_opendir_result = FR_OK;
    g_readdir_result = FR_OK;
    g_lseek_result = FR_OK;
    g_read_result = FR_OK;
    g_write_result = FR_OK;
    g_truncate_result = FR_OK;
    g_sync_result = FR_OK;
    g_unlink_result = FR_OK;
    g_short_write = false;
    test_protocol_init();
}

static fake_file_t* fake_add_file(const char* name, size_t size, uint8_t attributes) {
    for (size_t i = 0; i < FAKE_FILE_COUNT; i++) {
        if (!g_files[i].present) {
            fake_file_t* file = &g_files[i];
            file->present = true;
            file->attributes = attributes;
            file->read_only = (attributes & AM_RDO) != 0;
            snprintf(file->name, sizeof(file->name), "%s", name);
            file->size = size;
            for (size_t offset = 0; offset < size && offset < sizeof(file->data); offset++) {
                file->data[offset] = (uint8_t)(offset ^ 0x5Au);
            }
            return file;
        }
    }
    abort();
}

static int fake_find_file(const char* name) {
    for (size_t i = 0; i < FAKE_FILE_COUNT; i++) {
        if (g_files[i].present && strcmp(g_files[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)fs;
    (void)path;
    (void)opt;
    return g_mount_result;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    if (g_open_result != FR_OK) {
        return g_open_result;
    }

    int index = fake_find_file(path);
    if (index < 0) {
        if ((mode & FA_OPEN_ALWAYS) == 0) {
            return FR_NO_FILE;
        }
        fake_file_t* file = fake_add_file(path, 0, 0);
        index = (int)(file - g_files);
    }
    if (g_files[index].read_only && (mode & FA_WRITE) != 0) {
        return FR_WRITE_PROTECTED;
    }

    fp->file_index = index;
    fp->fptr = 0;
    fp->obj.objsize = g_files[index].size;
    return FR_OK;
}

FRESULT f_close(FIL* fp) {
    fp->file_index = -1;
    return FR_OK;
}

FRESULT f_lseek(FIL* fp, FSIZE_t offset) {
    if (g_lseek_result != FR_OK) {
        FRESULT result = g_lseek_result;
        g_lseek_result = FR_OK;
        return result;
    }
    if (offset > FAKE_FILE_CAPACITY) {
        return FR_INVALID_PARAMETER;
    }
    fp->fptr = offset;
    return FR_OK;
}

FRESULT f_read(FIL* fp, void* buffer, UINT requested, UINT* read_count) {
    if (g_read_result != FR_OK) {
        FRESULT result = g_read_result;
        g_read_result = FR_OK;
        return result;
    }
    fake_file_t* file = &g_files[fp->file_index];
    const size_t available = fp->fptr < file->size ? file->size - fp->fptr : 0;
    const size_t count = available < requested ? available : requested;
    memcpy(buffer, &file->data[fp->fptr], count);
    fp->fptr += count;
    *read_count = (UINT)count;
    return FR_OK;
}

FRESULT f_write(FIL* fp, const void* buffer, UINT requested, UINT* write_count) {
    if (g_write_result != FR_OK) {
        FRESULT result = g_write_result;
        g_write_result = FR_OK;
        return result;
    }
    if (fp->fptr + requested > FAKE_FILE_CAPACITY) {
        return FR_INVALID_PARAMETER;
    }

    const size_t count = g_short_write && requested > 0 ? requested - 1u : requested;
    g_short_write = false;
    fake_file_t* file = &g_files[fp->file_index];
    memcpy(&file->data[fp->fptr], buffer, count);
    fp->fptr += count;
    if (fp->fptr > file->size) {
        file->size = fp->fptr;
    }
    fp->obj.objsize = file->size;
    *write_count = (UINT)count;
    return FR_OK;
}

FRESULT f_truncate(FIL* fp) {
    if (g_truncate_result != FR_OK) {
        FRESULT result = g_truncate_result;
        g_truncate_result = FR_OK;
        return result;
    }
    fake_file_t* file = &g_files[fp->file_index];
    file->size = fp->fptr;
    fp->obj.objsize = file->size;
    return FR_OK;
}

FRESULT f_sync(FIL* fp) {
    (void)fp;
    if (g_sync_result != FR_OK) {
        FRESULT result = g_sync_result;
        g_sync_result = FR_OK;
        return result;
    }
    return FR_OK;
}

FRESULT f_opendir(DIR* dir, const TCHAR* path) {
    (void)path;
    if (g_opendir_result != FR_OK) {
        return g_opendir_result;
    }
    dir->position = 0;
    return FR_OK;
}

FRESULT f_closedir(DIR* dir) {
    (void)dir;
    return FR_OK;
}

FRESULT f_readdir(DIR* dir, FILINFO* info) {
    if (g_readdir_result != FR_OK) {
        FRESULT result = g_readdir_result;
        g_readdir_result = FR_OK;
        return result;
    }
    while (dir->position < FAKE_FILE_COUNT) {
        const fake_file_t* file = &g_files[dir->position++];
        if (!file->present) {
            continue;
        }
        snprintf(info->fname, sizeof(info->fname), "%s", file->name);
        info->fattrib = file->attributes;
        info->fsize = file->size;
        return FR_OK;
    }
    info->fname[0] = '\0';
    info->fattrib = 0;
    info->fsize = 0;
    return FR_OK;
}

FRESULT f_unlink(const TCHAR* path) {
    if (g_unlink_result != FR_OK) {
        FRESULT result = g_unlink_result;
        g_unlink_result = FR_OK;
        return result;
    }
    const int index = fake_find_file(path);
    if (index < 0) {
        return FR_NO_FILE;
    }
    if (g_files[index].read_only) {
        return FR_WRITE_PROTECTED;
    }
    g_files[index].present = false;
    return FR_OK;
}

static uint8_t status(void) {
    return neo1_msc_io_read(NEO1_IO_MSC_STATUS);
}

static uint16_t reported_size(void) {
    const uint16_t low = neo1_msc_io_read(NEO1_IO_MSC_SIZE_LO);
    const uint16_t high = neo1_msc_io_read(NEO1_IO_MSC_SIZE_HI);
    return (uint16_t)(low | (high << 8));
}

static void set_sector(uint16_t sector) {
    neo1_msc_io_write(NEO1_IO_MSC_SECTOR_LO, (uint8_t)sector);
    neo1_msc_io_write(NEO1_IO_MSC_SECTOR_HI, (uint8_t)(sector >> 8));
}

static void set_write_size(uint16_t size) {
    neo1_msc_io_write(NEO1_IO_MSC_SIZE_LO, (uint8_t)size);
    neo1_msc_io_write(NEO1_IO_MSC_SIZE_HI, (uint8_t)(size >> 8));
}

static void open_name(const char* name) {
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_OPEN);
    CHECK(status() == NEO1_MSC_STATUS_BUSY);
    for (const char* cursor = name; *cursor != '\0'; cursor++) {
        neo1_msc_io_write(NEO1_IO_MSC_DATA, (uint8_t)*cursor);
    }
    neo1_msc_io_write(NEO1_IO_MSC_DATA, 0);
}

static void stream_pattern(size_t count, uint8_t seed) {
    for (size_t i = 0; i < count; i++) {
        neo1_msc_io_write(NEO1_IO_MSC_DATA, (uint8_t)(seed + i));
    }
}

static void test_reset_and_command_errors(void) {
    fake_reset();
    CHECK(status() == NEO1_MSC_STATUS_READY);
    CHECK(neo1_msc_io_read(0xFFFF) == 0);
    CHECK(neo1_msc_io_read(NEO1_IO_MSC_INDEX) == 0);
    CHECK(reported_size() == 0);

    neo1_msc_io_write(NEO1_IO_MSC_CMD, 0xFF);
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | 1u));

    test_protocol_init();
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_READ);
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | 1u));

    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_DIR_NEXT);
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | FR_INVALID_OBJECT));
}

static void test_independent_protocol_instances(void) {
    neo1_msc_t first;
    neo1_msc_t second;
    CHECK(neo1_msc_init(&first, neo1_msc_fatfs_backend()));
    CHECK(neo1_msc_init(&second, neo1_msc_fatfs_backend()));

    neo1_msc_write(&first, NEO1_IO_MSC_SECTOR_LO, 0x34);
    neo1_msc_write(&first, NEO1_IO_MSC_SECTOR_HI, 0x12);
    neo1_msc_write(&first, NEO1_IO_MSC_INDEX, 7);
    neo1_msc_write(&first, NEO1_IO_MSC_CMD, NEO1_MSC_CMD_OPEN);

    CHECK(first.sector == 0x1234);
    CHECK(neo1_msc_read(&first, NEO1_IO_MSC_INDEX) == 7);
    CHECK(neo1_msc_read(&first, NEO1_IO_MSC_STATUS) == NEO1_MSC_STATUS_BUSY);
    CHECK(second.sector == 0);
    CHECK(neo1_msc_read(&second, NEO1_IO_MSC_INDEX) == 0);
    CHECK(neo1_msc_read(&second, NEO1_IO_MSC_STATUS) == NEO1_MSC_STATUS_READY);
}

static void test_media_and_open_errors(void) {
    fake_reset();
    g_mount_result = FR_NOT_READY;
    open_name("NOUSB.BIN");
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | FR_NOT_READY));

    fake_reset();
    fake_add_file("LOCKED.BIN", 16, AM_RDO);
    open_name("LOCKED.BIN");
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | FR_WRITE_PROTECTED));
}

static void test_directory_index_and_short_read(void) {
    fake_reset();
    fake_add_file(".hidden", 10, 0);
    fake_add_file("FOLDER", 0, AM_DIR);
    fake_file_t* payload = fake_add_file("PAYLOAD.BIN", 700, 0);

    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_DIR_OPEN);
    CHECK(status() == NEO1_MSC_STATUS_READY);
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_DIR_NEXT);
    CHECK(status() == NEO1_MSC_STATUS_READY);
    CHECK(neo1_msc_io_read(NEO1_IO_MSC_INFO) == NEO1_MSC_INFO_VALID);
    CHECK(reported_size() == 700);

    char name[32];
    size_t pos = 0;
    uint8_t ch;
    while ((ch = neo1_msc_io_read(NEO1_IO_MSC_DATA)) != 0 && pos + 1 < sizeof(name)) {
        name[pos++] = (char)ch;
    }
    name[pos] = '\0';
    CHECK(strcmp(name, "PAYLOAD.BIN") == 0);

    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_DIR_NEXT);
    CHECK(neo1_msc_io_read(NEO1_IO_MSC_INFO) == 0);

    neo1_msc_io_write(NEO1_IO_MSC_INDEX, 0);
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_OPEN_INDEX);
    CHECK(status() == NEO1_MSC_STATUS_READY);
    CHECK(reported_size() == 700);

    set_sector(1);
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_READ);
    CHECK(status() == NEO1_MSC_STATUS_READY);
    for (size_t i = 0; i < 188; i++) {
        CHECK(neo1_msc_io_read(NEO1_IO_MSC_DATA) == payload->data[512 + i]);
    }
    for (size_t i = 188; i < 512; i++) {
        CHECK(neo1_msc_io_read(NEO1_IO_MSC_DATA) == 0);
    }
    CHECK(neo1_msc_io_read(NEO1_IO_MSC_DATA) == 0);
}

static void test_multi_sector_write_and_truncating_overwrite(void) {
    fake_reset();
    open_name("OUTPUT.BIN");
    CHECK(status() == NEO1_MSC_STATUS_READY);

    set_sector(0);
    set_write_size(512);
    stream_pattern(520, 0x10);
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_WRITE);
    CHECK(status() == NEO1_MSC_STATUS_READY);

    set_sector(1);
    set_write_size(188);
    stream_pattern(512, 0x40);
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_WRITE);
    CHECK(status() == NEO1_MSC_STATUS_READY);

    int index = fake_find_file("OUTPUT.BIN");
    CHECK(index >= 0);
    CHECK(g_files[index].size == 700);
    CHECK(g_files[index].data[0] == 0x10);
    CHECK(g_files[index].data[511] == (uint8_t)(0x10 + 511));
    CHECK(g_files[index].data[512] == 0x40);
    CHECK(g_files[index].data[699] == (uint8_t)(0x40 + 187));

    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_CLOSE);
    CHECK(status() == NEO1_MSC_STATUS_READY);
    CHECK(reported_size() == 0);

    open_name("OUTPUT.BIN");
    CHECK(reported_size() == 700);
    set_sector(0);
    set_write_size(16);
    stream_pattern(512, 0xA0);
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_WRITE);
    CHECK(status() == NEO1_MSC_STATUS_READY);
    CHECK(g_files[index].size == 16);
    for (size_t i = 0; i < 16; i++) {
        CHECK(g_files[index].data[i] == (uint8_t)(0xA0 + i));
    }
}

static void test_io_failures_and_delete(void) {
    fake_reset();
    fake_add_file("FAIL.BIN", 32, 0);
    open_name("FAIL.BIN");
    CHECK(status() == NEO1_MSC_STATUS_READY);

    set_sector(0);
    set_write_size(16);
    stream_pattern(16, 0x20);
    g_short_write = true;
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_WRITE);
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | 1u));

    set_sector(9);
    g_lseek_result = FR_INVALID_PARAMETER;
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_READ);
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | FR_INVALID_PARAMETER));

    g_read_result = FR_DISK_ERR;
    set_sector(0);
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_READ);
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | FR_DISK_ERR));

    neo1_msc_io_write(NEO1_IO_MSC_INDEX, 0);
    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_DELETE_INDEX);
    CHECK(status() == NEO1_MSC_STATUS_READY);
    CHECK(fake_find_file("FAIL.BIN") < 0);

    neo1_msc_io_write(NEO1_IO_MSC_CMD, NEO1_MSC_CMD_DELETE_INDEX);
    CHECK(status() == (NEO1_MSC_STATUS_ERROR | FR_NO_FILE));
}

int main(void) {
    test_reset_and_command_errors();
    test_independent_protocol_instances();
    test_media_and_open_errors();
    test_directory_index_and_short_read();
    test_multi_sector_write_and_truncating_overwrite();
    test_io_failures_and_delete();

    if (g_failures != 0) {
        fprintf(stderr, "neo1_msc_tests: %d failure(s)\n", g_failures);
        return 1;
    }
    puts("neo1_msc_tests: all checks passed");
    return 0;
}
