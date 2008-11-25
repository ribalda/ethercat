/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
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
 * Canopen-over-EtherCAT Sdo request functions.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/jiffies.h>

#include "sdo_request.h"

/*****************************************************************************/

/** Default timeout in ms to wait for Sdo transfer responses.
 */
#define EC_SDO_REQUEST_RESPONSE_TIMEOUT 3000

/*****************************************************************************/

void ec_sdo_request_clear_data(ec_sdo_request_t *);

/*****************************************************************************/

/** Sdo request constructor.
 */
void ec_sdo_request_init(
        ec_sdo_request_t *req /**< Sdo request. */
        )
{
    req->data = NULL;
    req->mem_size = 0;
    req->data_size = 0;
    req->dir = EC_DIR_INVALID;
    req->issue_timeout = 0; // no timeout
    req->response_timeout = EC_SDO_REQUEST_RESPONSE_TIMEOUT;
    req->state = EC_INT_REQUEST_INIT;
    req->abort_code = 0x00000000;
}

/*****************************************************************************/

/** Sdo request destructor.
 */
void ec_sdo_request_clear(
        ec_sdo_request_t *req /**< Sdo request. */
        )
{
    ec_sdo_request_clear_data(req);
}

/*****************************************************************************/

/** Copy another Sdo request.
 *
 * \attention Only the index subindex and data are copied.
 */
int ec_sdo_request_copy(
        ec_sdo_request_t *req,
        const ec_sdo_request_t *other
        )
{
    req->index = other->index;
    req->subindex = other->subindex;
    return ec_sdo_request_copy_data(req, other->data, other->data_size);
}

/*****************************************************************************/

/** Sdo request destructor.
 */
void ec_sdo_request_clear_data(
        ec_sdo_request_t *req /**< Sdo request. */
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

/** Set the Sdo address.
 */
void ec_sdo_request_address(
        ec_sdo_request_t *req, /**< Sdo request. */
        uint16_t index, /**< Sdo index. */
        uint8_t subindex /**< Sdo subindex. */
        )
{
    req->index = index;
    req->subindex = subindex;
}

/*****************************************************************************/

/** Pre-allocates the data memory.
 *
 * If the \a mem_size is already bigger than \a size, nothing is done.
 *
 * \return 0 on success, otherwise -ENOMEM.
 */
int ec_sdo_request_alloc(
        ec_sdo_request_t *req, /**< Sdo request. */
        size_t size /**< Data size to allocate. */
        )
{
    if (size <= req->mem_size)
        return 0;

    ec_sdo_request_clear_data(req);

    if (!(req->data = (uint8_t *) kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes of Sdo memory.\n", size);
        return -ENOMEM;
    }

    req->mem_size = size;
    req->data_size = 0;
    return 0;
}

/*****************************************************************************/

/** Copies Sdo data from an external source.
 *
 * If the \a mem_size is to small, new memory is allocated.
 *
 * \retval  0 Success.
 * \retval <0 Error code.
 */
int ec_sdo_request_copy_data(
        ec_sdo_request_t *req, /**< Sdo request. */
        const uint8_t *source, /**< Source data. */
        size_t size /**< Number of bytes in \a source. */
        )
{
    int ret = ec_sdo_request_alloc(req, size);
    if (ret < 0)
        return ret;

    memcpy(req->data, source, size);
    req->data_size = size;
    return 0;
}

/*****************************************************************************/

/** Checks, if the timeout was exceeded.
 *
 * \return non-zero if the timeout was exceeded, else zero.
 */
int ec_sdo_request_timed_out(const ec_sdo_request_t *req /**< Sdo request. */)
{
    return req->issue_timeout
        && jiffies - req->jiffies_start > HZ * req->issue_timeout / 1000;
}

/*****************************************************************************
 * Realtime interface.
 ****************************************************************************/

void ecrt_sdo_request_timeout(ec_sdo_request_t *req, uint32_t timeout)
{
    req->issue_timeout = timeout;
}

/*****************************************************************************/

uint8_t *ecrt_sdo_request_data(ec_sdo_request_t *req)
{
    return req->data;
}

/*****************************************************************************/

size_t ecrt_sdo_request_data_size(const ec_sdo_request_t *req)
{
    return req->data_size;
}

/*****************************************************************************/

ec_request_state_t ecrt_sdo_request_state(const ec_sdo_request_t *req)
{
   return ec_request_state_translation_table[req->state];
}

/*****************************************************************************/

void ecrt_sdo_request_read(ec_sdo_request_t *req)
{
    req->dir = EC_DIR_INPUT;
    req->state = EC_INT_REQUEST_QUEUED;
    req->abort_code = 0x00000000;
    req->jiffies_start = jiffies;
}

/*****************************************************************************/

void ecrt_sdo_request_write(ec_sdo_request_t *req)
{
    req->dir = EC_DIR_OUTPUT;
    req->state = EC_INT_REQUEST_QUEUED;
    req->abort_code = 0x00000000;
    req->jiffies_start = jiffies;
}

/*****************************************************************************/

/** \cond */

EXPORT_SYMBOL(ecrt_sdo_request_timeout);
EXPORT_SYMBOL(ecrt_sdo_request_data);
EXPORT_SYMBOL(ecrt_sdo_request_data_size);
EXPORT_SYMBOL(ecrt_sdo_request_state);
EXPORT_SYMBOL(ecrt_sdo_request_read);
EXPORT_SYMBOL(ecrt_sdo_request_write);

/** \endcond */

/*****************************************************************************/
