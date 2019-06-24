/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2019  Florian Pose, Ingenieurgemeinschaft IgH
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
 * EtherCAT Mailbox Gateway request functions.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

#include "mbox_gateway_request.h"

/*****************************************************************************/

/** Default timeout in ms to wait for Mbox Gateway responses.
 */
#define EC_MBG_REQUEST_RESPONSE_TIMEOUT 1000

/*****************************************************************************/

void ec_mbg_request_clear_data(ec_mbg_request_t *);

/*****************************************************************************/

/** Mbox Gateway request constructor.
 */
void ec_mbg_request_init(
        ec_mbg_request_t *req /**< Mbox Gateway request. */
        )
{
    INIT_LIST_HEAD(&req->list);
    req->data = NULL;
    req->mem_size = 0;
    req->data_size = 0;
    req->response_timeout = EC_MBG_REQUEST_RESPONSE_TIMEOUT;
    req->state = EC_INT_REQUEST_INIT;
    req->jiffies_sent = 0U;
    req->error_code = 0x0000;
}

/*****************************************************************************/

/** Mbox Gateway request destructor.
 */
void ec_mbg_request_clear(
        ec_mbg_request_t *req /**< Mbox Gateway request. */
        )
{
    ec_mbg_request_clear_data(req);
}

/*****************************************************************************/

/** Free allocated memory.
 */
void ec_mbg_request_clear_data(
        ec_mbg_request_t *req /**< Mbox Gateway request. */
        )
{
    if (req->data) {
        kfree(req->data);
        req->data = NULL;
    }

    req->mem_size = 0;
    req->data_size = 0;
}

/*****************************************************************************/

/** Pre-allocates the data memory.
 *
 * If the \a mem_size is already bigger than \a size, nothing is done.
 *
 * \return 0 on success, otherwise -ENOMEM.
 */
int ec_mbg_request_alloc(
        ec_mbg_request_t *req, /**< Mbox Gateway request. */
        size_t size /**< Data size to allocate. */
        )
{
    if (size <= req->mem_size)
        return 0;

    ec_mbg_request_clear_data(req);

    if (!(req->data = (uint8_t *) kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %zu bytes of Mbox Gateway memory.\n", size);
        return -ENOMEM;
    }

    req->mem_size = size;
    req->data_size = 0;
    return 0;
}

/*****************************************************************************/

/** Copies Mbox Gateway data from an external source.
 *
 * If the \a mem_size is to small, new memory is allocated.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ec_mbg_request_copy_data(
        ec_mbg_request_t *req, /**< Mbox Gateway request. */
        const uint8_t *source, /**< Source data. */
        size_t size /**< Number of bytes in \a source. */
        )
{
    int ret = ec_mbg_request_alloc(req, size);
    if (ret < 0)
        return ret;

    memcpy(req->data, source, size);
    req->data_size = size;
    return 0;
}

/*****************************************************************************/

/** Request to run.
 */
void ec_mbg_request_run(
        ec_mbg_request_t *req /**< Mbox Gateway request. */
       )
{
    req->state = EC_INT_REQUEST_QUEUED;
    req->error_code = 0x0000;
}

/*****************************************************************************/
