/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2008  Olav Zarges, imc Meﬂsysteme GmbH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  The right to use EtherCAT Technology is granted and comes free of
 *  charge under condition of compatibility of product made by
 *  Licensee. People intending to distribute/sell products based on the
 *  code, have to sign an agreement to guarantee that products using
 *  software based on IgH EtherCAT master stay compatible with the actual
 *  EtherCAT specification (which are released themselves as an open
 *  standard) as the (only) precondition to have the right to use EtherCAT
 *  Technology, IP and trade marks.
 *
 *****************************************************************************/

/** \file
 * File-over-EtherCAT request functions.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/jiffies.h>

#include "foe_request.h"
#include "foe.h"

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
        ec_foe_request_t *req, /**< FoE request. */
        uint8_t* file_name /** filename */)
{
    req->buffer = NULL;
    req->file_name = file_name;
    req->buffer_size = 0;
    req->data_size = 0;
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
 * If the \a buffer_size is already bigger than \a size, nothing is done.
 */
int ec_foe_request_alloc(
        ec_foe_request_t *req, /**< FoE request. */
        size_t size /**< Data size to allocate. */
        )
{
    if (size <= req->buffer_size)
        return 0;

    ec_foe_request_clear_data(req);

    if (!(req->buffer = (uint8_t *) kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes of FoE memory.\n", size);
        return -1;
    }

    req->buffer_size = size;
    req->data_size = 0;
    return 0;
}

/*****************************************************************************/

/** Copies FoE data from an external source.
 *
 * If the \a buffer_size is to small, new memory is allocated.
 */
int ec_foe_request_copy_data(
        ec_foe_request_t *req, /**< FoE request. */
        const uint8_t *source, /**< Source data. */
        size_t size /**< Number of bytes in \a source. */
        )
{
    if (ec_foe_request_alloc(req, size))
        return -1;

    memcpy(req->buffer, source, size);
    req->data_size = size;
    return 0;
}

/*****************************************************************************/

/** Checks, if the timeout was exceeded.
 *
 * \return non-zero if the timeout was exceeded, else zero.
 */
int ec_foe_request_timed_out(const ec_foe_request_t *req /**< FoE request. */)
{
    return req->issue_timeout
        && jiffies - req->jiffies_start > HZ * req->issue_timeout / 1000;
}

/*****************************************************************************/

void ec_foe_request_timeout(ec_foe_request_t *req, uint32_t timeout)
{
    req->issue_timeout = timeout;
}

/*****************************************************************************/

uint8_t *ec_foe_request_data(ec_foe_request_t *req)
{
    return req->buffer;
}

/*****************************************************************************/

size_t ec_foe_request_data_size(const ec_foe_request_t *req)
{
    return req->data_size;
}

/*****************************************************************************/

void ec_foe_request_read(ec_foe_request_t *req)
{
    req->dir = EC_DIR_INPUT;
    req->state = EC_INT_REQUEST_QUEUED;
    req->result = FOE_BUSY;
    req->jiffies_start = jiffies;
}

/*****************************************************************************/

void ec_foe_request_write(ec_foe_request_t *req)
{
    req->dir = EC_DIR_OUTPUT;
    req->state = EC_INT_REQUEST_QUEUED;
    req->result = FOE_BUSY;
    req->jiffies_start = jiffies;
}

/*****************************************************************************/
