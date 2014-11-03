/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2012  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT master userspace library.
 *
 *  The IgH EtherCAT master userspace library is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation; version 2.1
 *  of the License.
 *
 *  The IgH EtherCAT master userspace library is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with the IgH EtherCAT master userspace library. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/** \file
 * Canopen over EtherCAT SDO request functions.
 */

/*****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "ioctl.h"
#include "sdo_request.h"
#include "slave_config.h"
#include "master.h"

/*****************************************************************************/

void ec_sdo_request_clear(ec_sdo_request_t *req)
{
    if (req->data) {
        free(req->data);
    }
}

/*****************************************************************************
 * Application interface.
 ****************************************************************************/

void ecrt_sdo_request_index(ec_sdo_request_t *req, uint16_t index,
        uint8_t subindex)
{
    ec_ioctl_sdo_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;
    data.sdo_index = index;
    data.sdo_subindex = subindex;

    ret = ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_INDEX, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to set SDO request index/subindex: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

void ecrt_sdo_request_timeout(ec_sdo_request_t *req, uint32_t timeout)
{
    ec_ioctl_sdo_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;
    data.timeout = timeout;

    ret = ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_TIMEOUT, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to set SDO request timeout: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
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

ec_request_state_t ecrt_sdo_request_state(ec_sdo_request_t *req)
{
    ec_ioctl_sdo_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;

    ret = ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_STATE, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get SDO request state: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return EC_REQUEST_ERROR;
    }

    if (data.size) { // new data waiting to be copied
        if (req->mem_size < data.size) {
            fprintf(stderr, "Received %zu bytes do not fit info SDO data"
                    " memory (%zu bytes)!\n", data.size, req->mem_size);
            return EC_REQUEST_ERROR;
        }

        data.data = req->data;

        ret = ioctl(req->config->master->fd,
                EC_IOCTL_SDO_REQUEST_DATA, &data);
        if (EC_IOCTL_IS_ERROR(ret)) {
            fprintf(stderr, "Failed to get SDO data: %s\n",
                    strerror(EC_IOCTL_ERRNO(ret)));
            return EC_REQUEST_ERROR;
        }
        req->data_size = data.size;
    }

    return data.state;
}

/*****************************************************************************/

void ecrt_sdo_request_read(ec_sdo_request_t *req)
{
    ec_ioctl_sdo_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;

    ret = ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_READ, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to command an SDO read operation : %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

void ecrt_sdo_request_write(ec_sdo_request_t *req)
{
    ec_ioctl_sdo_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;
    data.data = req->data;
    data.size = req->data_size;

    ret = ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_WRITE, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to command an SDO write operation : %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/
