#include "devices/neo1_msc.h"

#include <string.h>

static uint8_t neo1_msc_error_code(uint8_t error) {
    return error ? error : 1u;
}

static void neo1_msc_set_error(neo1_msc_t* msc, uint8_t error) {
    msc->status = (uint8_t)(NEO1_MSC_STATUS_ERROR |
                            (neo1_msc_error_code(error) & 0x7Fu));
}

static void neo1_msc_set_ready(neo1_msc_t* msc) {
    msc->status = NEO1_MSC_STATUS_READY;
}

static void neo1_msc_close_dir(neo1_msc_t* msc) {
    if (msc->dir_open) {
        msc->backend.ops->dir_close(msc->backend.context);
        msc->dir_open = false;
    }
}

static uint16_t neo1_msc_saturated_size(uint32_t size) {
    return size > 0xFFFFu ? 0xFFFFu : (uint16_t)size;
}

static bool neo1_msc_loadable_entry(const neo1_msc_dir_entry_t* entry) {
    return entry->valid && entry->name[0] != '\0' &&
           entry->name[0] != '.' && !entry->directory;
}

static void neo1_msc_do_open(neo1_msc_t* msc) {
    uint32_t size = 0;
    const uint8_t error = msc->backend.ops->open(
        msc->backend.context, msc->open_filename, &size);
    if (error) {
        neo1_msc_set_error(msc, error);
        return;
    }

    msc->file_open = true;
    msc->file_size = neo1_msc_saturated_size(size);
    msc->data_offset = 0;
    neo1_msc_set_ready(msc);
}

static void neo1_msc_do_close(neo1_msc_t* msc) {
    if (msc->file_open) {
        msc->backend.ops->close(msc->backend.context);
        msc->file_open = false;
    }
    msc->file_size = 0;
    neo1_msc_close_dir(msc);
    neo1_msc_set_ready(msc);
}

static void neo1_msc_do_dir_open(neo1_msc_t* msc) {
    neo1_msc_close_dir(msc);
    const uint8_t error = msc->backend.ops->dir_open(msc->backend.context);
    if (error) {
        neo1_msc_set_error(msc, error);
        return;
    }

    msc->dir_open = true;
    msc->info = 0;
    msc->last_dir_index = 0xFF;
    msc->file_size = 0;
    neo1_msc_set_ready(msc);
}

static bool neo1_msc_next_loadable(neo1_msc_t* msc,
                                    neo1_msc_dir_entry_t* entry) {
    while (true) {
        memset(entry, 0, sizeof(*entry));
        const uint8_t error = msc->backend.ops->dir_next(
            msc->backend.context, entry);
        if (error) {
            neo1_msc_set_error(msc, error);
            return false;
        }
        entry->name[NEO1_MSC_FILENAME_MAX - 1] = '\0';
        if (!entry->valid) {
            return false;
        }
        if (neo1_msc_loadable_entry(entry)) {
            return true;
        }
    }
}

static void neo1_msc_publish_entry(neo1_msc_t* msc,
                                    const neo1_msc_dir_entry_t* entry) {
    const size_t name_len = strlen(entry->name);
    const size_t copy_len = name_len < (sizeof(msc->buffer) - 1u)
                                ? name_len
                                : (sizeof(msc->buffer) - 1u);
    memcpy(msc->buffer, entry->name, copy_len);
    msc->buffer[copy_len] = '\0';
    msc->data_offset = 0;
    msc->last_dir_index = (uint8_t)(msc->last_dir_index + 1u);
    msc->info = NEO1_MSC_INFO_VALID;
    msc->file_size = neo1_msc_saturated_size(entry->size);
}

static void neo1_msc_publish_dir_end(neo1_msc_t* msc) {
    msc->info = 0;
    msc->data_offset = 0;
    msc->file_size = 0;
    msc->buffer[0] = '\0';
}

static void neo1_msc_do_dir_next(neo1_msc_t* msc) {
    if (!msc->dir_open) {
        neo1_msc_set_error(msc, msc->backend.invalid_state_error);
        return;
    }

    neo1_msc_set_ready(msc);
    neo1_msc_dir_entry_t entry;
    if (neo1_msc_next_loadable(msc, &entry)) {
        neo1_msc_publish_entry(msc, &entry);
        neo1_msc_set_ready(msc);
        return;
    }
    if ((msc->status & NEO1_MSC_STATUS_ERROR) == 0) {
        neo1_msc_publish_dir_end(msc);
        neo1_msc_set_ready(msc);
    }
}

static bool neo1_msc_find_indexed(neo1_msc_t* msc,
                                   uint8_t index,
                                   char* out_name) {
    neo1_msc_close_dir(msc);
    neo1_msc_set_ready(msc);
    uint8_t error = msc->backend.ops->dir_open(msc->backend.context);
    if (error) {
        neo1_msc_set_error(msc, error);
        return false;
    }
    msc->dir_open = true;

    uint8_t current = 0;
    neo1_msc_dir_entry_t entry;
    while (neo1_msc_next_loadable(msc, &entry)) {
        if (current == index) {
            memcpy(out_name, entry.name, NEO1_MSC_FILENAME_MAX);
            out_name[NEO1_MSC_FILENAME_MAX - 1] = '\0';
            msc->last_dir_index = current;
            return true;
        }
        current = (uint8_t)(current + 1u);
    }

    if ((msc->status & NEO1_MSC_STATUS_ERROR) == 0) {
        neo1_msc_set_error(msc, msc->backend.not_found_error);
    }
    return false;
}

static void neo1_msc_do_open_index(neo1_msc_t* msc) {
    if (!neo1_msc_find_indexed(msc, msc->index, msc->open_filename)) {
        return;
    }
    neo1_msc_do_open(msc);
}

static void neo1_msc_do_delete_index(neo1_msc_t* msc) {
    if (!neo1_msc_find_indexed(msc, msc->index, msc->open_filename)) {
        return;
    }
    if (msc->file_open) {
        msc->backend.ops->close(msc->backend.context);
        msc->file_open = false;
    }
    neo1_msc_close_dir(msc);

    const uint8_t error = msc->backend.ops->delete_file(
        msc->backend.context, msc->open_filename);
    if (error) {
        neo1_msc_set_error(msc, error);
        return;
    }

    msc->info = 0;
    msc->file_size = 0;
    msc->data_offset = 0;
    neo1_msc_set_ready(msc);
}

static bool neo1_msc_sector_access_allowed(const neo1_msc_t* msc) {
    return msc->file_open ||
           ((msc->backend.flags & NEO1_MSC_BACKEND_OPEN_OPTIONAL) != 0);
}

static void neo1_msc_do_read(neo1_msc_t* msc) {
    if (!neo1_msc_sector_access_allowed(msc)) {
        neo1_msc_set_error(msc, 1);
        return;
    }

    msc->data_offset = 0;
    size_t size = 0;
    const uint8_t error = msc->backend.ops->read_sector(
        msc->backend.context, msc->sector, msc->buffer, &size);
    if (error || size > sizeof(msc->buffer)) {
        neo1_msc_set_error(msc, error);
        return;
    }
    if (size < sizeof(msc->buffer)) {
        memset(&msc->buffer[size], 0, sizeof(msc->buffer) - size);
    }
    neo1_msc_set_ready(msc);
}

static void neo1_msc_do_write(neo1_msc_t* msc) {
    if (!neo1_msc_sector_access_allowed(msc)) {
        neo1_msc_set_error(msc, 1);
        return;
    }

    msc->data_offset = 0;
    uint16_t write_size = msc->file_size;
    if (write_size == 0 || write_size > sizeof(msc->buffer)) {
        write_size = (uint16_t)sizeof(msc->buffer);
    }

    uint32_t file_size = 0;
    const uint8_t error = msc->backend.ops->write_sector(
        msc->backend.context,
        msc->sector,
        msc->buffer,
        write_size,
        &file_size);
    if (error) {
        neo1_msc_set_error(msc, error);
        return;
    }
    msc->file_size = neo1_msc_saturated_size(file_size);
    neo1_msc_set_ready(msc);
}

static bool neo1_msc_backend_valid(const neo1_msc_backend_t* backend) {
    return backend && backend->ops && backend->ops->open &&
           backend->ops->close && backend->ops->read_sector &&
           backend->ops->write_sector && backend->ops->dir_open &&
           backend->ops->dir_next && backend->ops->dir_close &&
           backend->ops->delete_file;
}

bool neo1_msc_init(neo1_msc_t* msc, const neo1_msc_backend_t* backend) {
    if (!msc || !neo1_msc_backend_valid(backend)) {
        return false;
    }
    memset(msc, 0, sizeof(*msc));
    msc->backend = *backend;
    msc->status = NEO1_MSC_STATUS_READY;
    msc->last_dir_index = 0xFF;
    return true;
}

uint8_t neo1_msc_read(neo1_msc_t* msc, uint16_t addr) {
    if (!msc) {
        return 0;
    }
    switch (addr) {
        case NEO1_IO_MSC_STATUS:
            return msc->status;
        case NEO1_IO_MSC_DATA:
            if (msc->data_offset >= sizeof(msc->buffer)) {
                return 0;
            }
            return msc->buffer[msc->data_offset++];
        case NEO1_IO_MSC_INDEX:
            return msc->index;
        case NEO1_IO_MSC_INFO:
            return msc->info;
        case NEO1_IO_MSC_SIZE_LO:
            return (uint8_t)msc->file_size;
        case NEO1_IO_MSC_SIZE_HI:
            return (uint8_t)(msc->file_size >> 8);
        default:
            return 0;
    }
}

void neo1_msc_write(neo1_msc_t* msc, uint16_t addr, uint8_t data) {
    if (!msc) {
        return;
    }
    switch (addr) {
        case NEO1_IO_MSC_CMD:
            switch (data) {
                case NEO1_MSC_CMD_OPEN:
                    msc->open_filename_pos = 0;
                    msc->open_filename[0] = '\0';
                    msc->open_pending = true;
                    msc->data_offset = 0;
                    msc->status = NEO1_MSC_STATUS_BUSY;
                    break;
                case NEO1_MSC_CMD_CLOSE:
                    neo1_msc_do_close(msc);
                    break;
                case NEO1_MSC_CMD_READ:
                    neo1_msc_do_read(msc);
                    break;
                case NEO1_MSC_CMD_WRITE:
                    neo1_msc_do_write(msc);
                    break;
                case NEO1_MSC_CMD_DIR_OPEN:
                    neo1_msc_do_dir_open(msc);
                    break;
                case NEO1_MSC_CMD_DIR_NEXT:
                    neo1_msc_do_dir_next(msc);
                    break;
                case NEO1_MSC_CMD_OPEN_INDEX:
                    neo1_msc_do_open_index(msc);
                    break;
                case NEO1_MSC_CMD_DELETE_INDEX:
                    neo1_msc_do_delete_index(msc);
                    break;
                default:
                    neo1_msc_set_error(msc, 1);
                    break;
            }
            break;

        case NEO1_IO_MSC_SECTOR_LO:
            msc->sector = (uint16_t)((msc->sector & 0xFF00u) | data);
            break;
        case NEO1_IO_MSC_SECTOR_HI:
            msc->sector = (uint16_t)((msc->sector & 0x00FFu) |
                                     ((uint16_t)data << 8));
            break;
        case NEO1_IO_MSC_DATA:
            if (msc->open_pending) {
                if (msc->open_filename_pos <
                    (sizeof(msc->open_filename) - 1u)) {
                    msc->open_filename[msc->open_filename_pos++] = (char)data;
                    msc->open_filename[msc->open_filename_pos] = '\0';
                }
                if (data == 0 || msc->open_filename_pos >=
                                     (sizeof(msc->open_filename) - 1u)) {
                    msc->open_pending = false;
                    neo1_msc_do_open(msc);
                }
                break;
            }
            if (msc->data_offset < sizeof(msc->buffer)) {
                msc->buffer[msc->data_offset++] = data;
            }
            break;
        case NEO1_IO_MSC_INDEX:
            msc->index = data;
            break;
        case NEO1_IO_MSC_SIZE_LO:
            msc->file_size = (uint16_t)((msc->file_size & 0xFF00u) | data);
            break;
        case NEO1_IO_MSC_SIZE_HI:
            msc->file_size = (uint16_t)((msc->file_size & 0x00FFu) |
                                        ((uint16_t)data << 8));
            break;
        default:
            break;
    }
}
