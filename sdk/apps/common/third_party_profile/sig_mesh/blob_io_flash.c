/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adaptation.h"
#include "blob.h"
#include "net.h"
#include "transport.h"
#if CONFIG_BT_MESH_DFU_TARGET
#include "mesh_target_node_ota.h"
#endif
#if CONFIG_BT_MESH_DFU_DIST
#include "mesh_distributor_loader.h"
#endif

#define LOG_TAG             "[MESH-blob_io_flash]"
#define LOG_INFO_ENABLE
// #define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"

#if (CONFIG_BT_MESH_BLOB_IO_FLASH)

#define FLASH_IO(_io) CONTAINER_OF(_io, struct bt_mesh_blob_io_flash, io)

static int test_flash_area(u8_t area_id)
{
    // test flash area can used or not.
    return 0;
}

static int io_open(const struct bt_mesh_blob_io *io,
                   const struct bt_mesh_blob_xfer *xfer,
                   enum bt_mesh_blob_io_mode mode)
{
    struct bt_mesh_blob_io_flash *flash = FLASH_IO(io);

    flash->mode = mode;
#if CONFIG_BT_MESH_DFU_DIST
    mesh_dist_loader_init(R_TYPE);
#endif
    return 0;//flash_area_open(flash->area_id, &flash->area);
}

static void io_close(const struct bt_mesh_blob_io *io,
                     const struct bt_mesh_blob_xfer *xfer)
{
    struct bt_mesh_blob_io_flash *flash = FLASH_IO(io);

    // flash_area_close(flash->area);
}

static int block_start(const struct bt_mesh_blob_io *io,
                       const struct bt_mesh_blob_xfer *xfer,
                       const struct bt_mesh_blob_block *block)
{
    struct bt_mesh_blob_io_flash *flash = FLASH_IO(io);
    size_t erase_size;

    if (flash->mode == BT_MESH_BLOB_READ) {
        return 0;
    }

    erase_size = block->size;

    return 0;//flash_area_flatten(flash->area, flash->offset + block->offset, erase_size);
}

static int rd_chunk(const struct bt_mesh_blob_io *io,
                    const struct bt_mesh_blob_xfer *xfer,
                    const struct bt_mesh_blob_block *block,
                    const struct bt_mesh_blob_chunk *chunk)
{
    struct bt_mesh_blob_io_flash *flash = FLASH_IO(io);

    LOG_DBG(">>> flash->offset %d block->offset %d chunk->offset %d", flash->offset,
            block->offset, chunk->offset);

#if CONFIG_BT_MESH_DFU_DIST
    mesh_dist_read_data(chunk->data, chunk->size, block->size, block->offset, chunk->offset);
#endif
    return 0;
}

static int wr_chunk(const struct bt_mesh_blob_io *io,
                    const struct bt_mesh_blob_xfer *xfer,
                    const struct bt_mesh_blob_block *block,
                    const struct bt_mesh_blob_chunk *chunk)
{
    struct bt_mesh_blob_io_flash *flash = FLASH_IO(io);

    LOG_DBG(">>> flash->offset %d block->offset %d chunk->offset %d", flash->offset,
            block->offset, chunk->offset);

#if CONFIG_BT_MESH_DFU_TARGET
    mesh_targe_ota_process(DFU_NODE_OTA_DATA, chunk->data, chunk->size, block->size, chunk->offset);
#endif

    return 0;
}

int bt_mesh_blob_io_flash_init(struct bt_mesh_blob_io_flash *flash,
                               u8_t area_id, off_t offset)
{
    int err;

    err = test_flash_area(area_id);
    if (err) {
        return err;
    }

    flash->area_id = area_id;
    flash->offset = offset;
    flash->io.open = io_open;
    flash->io.close = io_close;
    flash->io.block_start = block_start;
    flash->io.block_end = NULL;
    flash->io.rd = rd_chunk;
    flash->io.wr = wr_chunk;

    return 0;
}
#endif
