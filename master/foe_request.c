/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2008  Olav Zarges, imc Messsysteme GmbH
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

/** \file
 * File-over-EtherCAT request functions.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

#include "foe_request.h"

/*****************************************************************************/

/** Default timeout in ms to wait for FoE transfer responses.
 */
#define EC_FOE_REQUEST_RESPONSE_TIMEOUT 3000

/*****************************************************************************/

void ec_foe_request_clear_data(ec_foe_request_t *);

/*****************************************************************************/

/** FoE request constructor.
 */
void ec_foe_request_init(
        ec_foe_request_t *req /**< FoE request. */
        )
{
    INIT_LIST_HEAD(&req->list);
    req->buffer = NULL;
    req->file_name[0] = 0;
    req->password = 0;
    req->buffer_size = 0;
    req->data_size = 0;
    req->progress = 0;
    req->dir = EC_DIR_INVALID;
    req->issue_timeout = 0; // no timeout
    req->response_timeout = EC_FOE_REQUEST_RESPONSE_TIMEOUT;
    req->state = EC_INT_REQUEST_INIT;
    req->result = FOE_BUSY;
    req->error_code = 0x00000000;
}

/*****************************************************************************/

/** FoE request destructor.
 */
void ec_foe_request_clear(
        ec_foe_request_t *req /**< FoE request. */
        )
{
    ec_foe_request_clear_data(req);
}

/*****************************************************************************/

/** FoE request destructor.
 */
void ec_foe_request_clear_data(
        ec_foe_request_t *req /**< FoE request. */
        )
{
    if (req->buffer) {
        kfree(req->buffer);
        req->buffer = NULL;
    }

    req->buffer_size = 0;
    req->data_size = 0;
}

/*****************************************************************************/

/** Pre-allocates the data memory.
 *
 * If the internal \a buffer_size is already bigger than \a size, nothing is
 * done.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_foe_request_alloc(
        ec_foe_request_t *req, /**< FoE request. */
        size_t size /**< Data size to allocate. */
        )
{
    if (size <= req->buffer_size) {
        return 0;
    }

    ec_foe_request_clear_data(req);

    if (!(req->buffer = (uint8_t *) kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %zu bytes of FoE memory.\n", size);
        return -ENOMEM;
    }

    req->buffer_size = size;
    req->data_size = 0;
    return 0;
}

/*****************************************************************************/

/** Copies FoE data from an external source.
 *
 * If the \a buffer_size is to small, new memory is allocated.
 *
 * \return Zero on success, otherwise a negative error code.
 */
int ec_foe_request_copy_data(
        ec_foe_request_t *req, /**< FoE request. */
        const uint8_t *source, /**< Source data. */
        size_t size /**< Number of bytes in \a source. */
        )
{
    int ret;

    ret = ec_foe_request_alloc(req, size);
    if (ret) {
        return ret;
    }

    memcpy(req->buffer, source, size);
    req->data_size = size;
    return 0;
}

/*****************************************************************************/

/** Checks, if the timeout was exceeded.
 *
 * \return non-zero if the timeout was exceeded, else zero.
 */
int ec_foe_request_timed_out(
        const ec_foe_request_t *req /**< FoE request. */
        )
{
    return req->issue_timeout
        && jiffies - req->jiffies_start > HZ * req->issue_timeout / 1000;
}

/*****************************************************************************
 * Application interface.
 ****************************************************************************/

/** Set the request timeout.
 */
void ecrt_foe_request_timeout(
        ec_foe_request_t *req, /**< FoE request. */
        uint32_t timeout /**< Timeout in ms. */
        )
{
    req->issue_timeout = timeout;
}

/*****************************************************************************/

/** Selects a new file for the request.
 */
void ecrt_foe_request_file(
        ec_foe_request_t *req, /**< FoE request. */
        const char* file_name, /** filename */
        uint32_t password /** password */
        )
{
    strlcpy((char*) req->file_name, file_name, sizeof(req->file_name));
    req->password = password;
}

/*****************************************************************************/

/** Returns a pointer to the request's data.
 *
 * \return Data pointer.
 */
uint8_t *ecrt_foe_request_data(
        ec_foe_request_t *req /**< FoE request. */
        )
{
    return req->buffer;
}

/*****************************************************************************/

/** Returns the data size.
 *
 * \return Data size.
 */
size_t ecrt_foe_request_data_size(
        const ec_foe_request_t *req /**< FoE request. */
        )
{
    return req->data_size;
}

/*****************************************************************************/

ec_request_state_t ecrt_foe_request_state(const ec_foe_request_t *req)
{
    return ec_request_state_translation_table[req->state];
}

/*****************************************************************************/

ec_foe_error_t ecrt_foe_request_result(const ec_foe_request_t *req)
{
    return req->result;
}

/*****************************************************************************/

uint32_t ecrt_foe_request_error_code(const ec_foe_request_t *req)
{
    return req->error_code;
}

/*****************************************************************************/

/** Returns the progress of the current @EC_REQUEST_BUSY transfer.
 *
 * \return Progress in bytes.
 */
size_t ecrt_foe_request_progress(
        const ec_foe_request_t *req /**< FoE request. */
        )
{
    return req->progress;
}

/*****************************************************************************/

/** Prepares a read request (slave to master).
 */
void ecrt_foe_request_read(
        ec_foe_request_t *req /**< FoE request. */
        )
{
    req->data_size = 0;
    req->progress = 0;
    req->dir = EC_DIR_INPUT;
    req->state = EC_INT_REQUEST_QUEUED;
    req->result = FOE_BUSY;
    req->jiffies_start = jiffies;
}

/*****************************************************************************/

/** Prepares a write request (master to slave).
 */
void ecrt_foe_request_write(
        ec_foe_request_t *req, /**< FoE request. */
        size_t data_size /**< Data size. */
        )
{
    if (data_size > req->buffer_size) {
        EC_ERR("Request to write %zu bytes to FoE buffer of size %zu.\n",
               data_size, req->buffer_size);
        req->state = EC_INT_REQUEST_FAILURE;
        return;
    }
    req->data_size = data_size;
    req->progress = 0;
    req->dir = EC_DIR_OUTPUT;
    req->state = EC_INT_REQUEST_QUEUED;
    req->result = FOE_BUSY;
    req->jiffies_start = jiffies;
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_foe_request_file);
EXPORT_SYMBOL(ecrt_foe_request_timeout);
EXPORT_SYMBOL(ecrt_foe_request_data);
EXPORT_SYMBOL(ecrt_foe_request_data_size);
EXPORT_SYMBOL(ecrt_foe_request_state);
EXPORT_SYMBOL(ecrt_foe_request_result);
EXPORT_SYMBOL(ecrt_foe_request_error_code);
EXPORT_SYMBOL(ecrt_foe_request_progress);
EXPORT_SYMBOL(ecrt_foe_request_read);
EXPORT_SYMBOL(ecrt_foe_request_write);

/** \endcond */

/*****************************************************************************/
