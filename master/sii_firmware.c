/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2016  Gavin Lambert
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/**
   \file
   Methods of the SII "firmware" loader.
*/

/*****************************************************************************/

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
#include <linux/export.h>
#endif
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <asm/processor.h>

#include "master.h"
#include "slave.h"

#include "sii_firmware.h"

/*****************************************************************************/
#ifdef EC_SII_OVERRIDE

/*****************************************************************************/
#ifdef EC_SII_DIR

/**
   Request firmware direct from file.
 */

static int request_firmware_direct(
       ec_slave_t *slave,
       const char *filename,
       struct firmware **out_firmware
       )
{
    // firmware files must be readable but not
    // executable, a directory, or sticky
    const int permreqd   = S_IROTH;
    const int permforbid = S_IFDIR | S_ISVTX | S_IXOTH | S_IXGRP | S_IXUSR;

    int              retval = 0;
    struct file     *filp;
    struct firmware *firmware;
    umode_t          permission;
    mm_segment_t     old_fs;
    loff_t           pos;
    char             pathname[strlen(filename) + sizeof(EC_SII_DIR)];

    if (filename == NULL)
        return -EFAULT;
    if (strlen(filename) + 14 >= 256)   // Sanity check.
        return -EFAULT;

    EC_SLAVE_DBG(slave, 1, "request_firmware_direct: %s.\n", filename);
    sprintf(pathname, EC_SII_DIR "/%s", filename);

    // does the file exist?
    filp = filp_open(pathname, 0, O_RDONLY);
    if ((IS_ERR(filp)) || (filp == NULL) || (filp->f_dentry == NULL)) {
        retval = -ENOENT;
        goto out;
    }

    // must have correct permissions
    permission = filp->f_dentry->d_inode->i_mode;
    if ((permission & permreqd) != permreqd) {
        EC_SLAVE_WARN(slave, "Firmware %s not readable.\n", filename);
        retval = -EPERM;
        goto error_file;
    }
    if ((permission & permforbid) != 0) {
        EC_SLAVE_WARN(slave, "Firmware %s incorrect perms.\n", filename);
        retval = -EPERM;
        goto error_file;
    }

    *out_firmware = firmware = kzalloc(sizeof(*firmware), GFP_KERNEL);
    if (!firmware) {
        EC_SLAVE_ERR(slave, "Failed to allocate memory (struct firmware).\n");
        retval = -ENOMEM;
        goto error_file;
    }
    firmware->size = filp->f_dentry->d_inode->i_size;

    if (!(firmware->data = kmalloc(firmware->size, GFP_KERNEL))) {
        EC_SLAVE_ERR(slave, "Failed to allocate memory (firmware data).\n");
        retval = -ENOMEM;
        goto error_firmware;
    }

    // read the firmware (need to temporarily allow access to kernel mem)
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    pos = 0;
    while (pos < firmware->size) {
        retval = vfs_read(filp, (char __user *) firmware->data + pos,
                          firmware->size - pos, &pos);
        if (retval < 0) {
            set_fs(old_fs);
            EC_SLAVE_ERR(slave, "Failed to read firmware (%d).\n", retval);
            goto error_firmware_data;
        }
    }

    set_fs(old_fs);

    EC_SLAVE_INFO(slave, "SII firmware loaded from file %s.\n", filename);
    filp_close(filp, NULL);
    return 0;

error_firmware_data:
    kfree(firmware->data);
error_firmware:
    kfree(firmware);
    *out_firmware = NULL;
error_file:
    filp_close(filp, NULL);
out:
    return retval;
}
#endif

/*****************************************************************************/

struct firmware_request_context
{
#ifdef EC_SII_DIR
    struct work_struct work;
    const char *filename;
#else
    struct device fw_device;
    bool device_inited;
#endif
    ec_slave_t *slave;
    void (*cont)(const struct firmware *, void *);
    void *context;
    char filename_vendor_product[48];
    char filename_vendor_product_revision[48];
    bool fallback;
};

static void firmware_request_complete(const struct firmware *, void *);

/*****************************************************************************/
#ifdef EC_SII_DIR

static int request_firmware_direct_work_func(void *arg)
{
    struct firmware_request_context *ctx = arg;
    struct firmware *fw = NULL;

    if (request_firmware_direct(ctx->slave, ctx->filename, &fw) == 0) {
        firmware_request_complete(fw, ctx);
    } else {
        firmware_request_complete(NULL, ctx);
    }
    return 0;
}

/*****************************************************************************/

static int start_request(
        struct firmware_request_context *ctx, /**< request context. */
        const char *filename /**< firmware filename. */
        )
{
    struct task_struct *task;

    ctx->filename = filename;
    task = kthread_run(request_firmware_direct_work_func, ctx,
                       "firmware/%s", filename);
    if (IS_ERR(task)) {
        firmware_request_complete(NULL, ctx);
        return PTR_ERR(task);
    }
    return 0;
}

static void clear_request(
        struct firmware_request_context *ctx /**< request context. */
        )
{
    kfree(ctx);
}

/*****************************************************************************/
#else

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#error "SII override requires kernel 2.6.33 or later; use --disable-sii-override instead."
#endif

static void fw_device_release(struct device *dev)
{
    struct firmware_request_context *ctx =
            container_of(dev, struct firmware_request_context, fw_device);
    kfree(ctx);
}

static int start_request(
        struct firmware_request_context *ctx, /**< request context. */
        const char *filename /**< firmware filename. */
        )
{
    if (!ctx->device_inited) {
        int error;
        
        // register a child device for the slave, because only one
        // firmware request can be in flight per device.
        device_initialize(&ctx->fw_device);
        dev_set_name(&ctx->fw_device, "addr%u", ctx->slave->station_address);
        dev_set_uevent_suppress(&ctx->fw_device, true);
        ctx->fw_device.parent = ctx->slave->master->class_device;
        ctx->fw_device.release = fw_device_release;

        error = device_add(&ctx->fw_device);
        if (error) {
            EC_SLAVE_ERR(ctx->slave, "Unable to register child device for firmware.\n");
            put_device(&ctx->fw_device);
            return error;
        }
        ctx->device_inited = true;
    }
    
    return request_firmware_nowait(THIS_MODULE, 1, filename,
            &ctx->fw_device, GFP_KERNEL, ctx, firmware_request_complete);
}

static void clear_request(
        struct firmware_request_context *ctx /**< request context. */
        )
{
    if (ctx->device_inited) {
        device_del(&ctx->fw_device);
        put_device(&ctx->fw_device);
        // ctx freed via fw_device_release
    } else {
        kfree(ctx);
    }
}

#endif

/*****************************************************************************/

static void firmware_request_complete(
        const struct firmware *firmware,
        void *context
        )
{
    struct firmware_request_context *ctx = context;

    if (!firmware && !ctx->fallback) {
        ctx->fallback = true;
        if (start_request(ctx, ctx->filename_vendor_product) == 0) {
            return;
        }
    }

    ctx->cont(firmware, ctx->context);
    clear_request(ctx);
}

/*****************************************************************************/

/**
   Request overridden SII image "firmware" from filesystem.
 */

void ec_request_sii_firmware(
        ec_slave_t *slave, /**< slave. */
        void *context, /**< continuation context. */
        void (*cont)(const struct firmware *, void *) /**< continuation. */
        )
{
    struct firmware_request_context *ctx;

    if (!(ctx = kzalloc(sizeof(*ctx), GFP_KERNEL))) {
        EC_SLAVE_ERR(slave, "Unable to allocate firmware request context.\n");
        goto out_error;
    }

    sprintf(ctx->filename_vendor_product, "ethercat/ec_%08x_%08x.bin",
            slave->sii_image->sii.vendor_id,
            slave->sii_image->sii.product_code);
    sprintf(ctx->filename_vendor_product_revision, "ethercat/ec_%08x_%08x_%08x.bin",
            slave->sii_image->sii.vendor_id,
            slave->sii_image->sii.product_code,
            slave->sii_image->sii.revision_number);
    ctx->slave = slave;
    ctx->cont = cont;
    ctx->context = context;

    // request by vendor_product_revision first,
    // then try more generic vendor_product
    EC_SLAVE_DBG(slave, 1, "Trying to load SII firmware: %s\n", ctx->filename_vendor_product_revision);
    EC_SLAVE_DBG(slave, 1, "                       then: %s\n", ctx->filename_vendor_product);

    if (start_request(ctx, ctx->filename_vendor_product_revision) == 0) {
        return;
    }

out_error:
    cont(NULL, context);
}

/*****************************************************************************/

/**
   Releases firmware memory after a successful request.
 */

void ec_release_sii_firmware(const struct firmware *firmware /**< firmware to release. */)
{
#ifdef EC_SII_DIR
    if (firmware) {
        kfree(firmware->data);
        kfree(firmware);
    }
#else
    release_firmware(firmware);
#endif
}

/*****************************************************************************/
#endif //EC_SII_OVERRIDE

/*****************************************************************************/
