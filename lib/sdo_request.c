/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
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
#include <errno.h>
#include <string.h>

#include "sdo_request.h"
#include "master/ioctl.h"
#include "slave_config.h"
#include "master.h"

/*****************************************************************************
 * Realtime interface.
 ****************************************************************************/

void ecrt_sdo_request_timeout(ec_sdo_request_t *req, uint32_t timeout)
{
    ec_ioctl_sdo_request_t data;

    data.config_index = req->config->index;
    data.request_index = req->index;
    data.timeout = timeout;

    if (ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_TIMEOUT,
                &data) == -1)
        fprintf(stderr, "Failed to set SDO request timeout: %s\n",
                strerror(errno));
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

    data.config_index = req->config->index;
    data.request_index = req->index;

    if (ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_STATE,
                &data) == -1)
        fprintf(stderr, "Failed to get SDO request state: %s\n",
                strerror(errno));

    if (data.size) { // new data waiting to be copied
        if (req->mem_size < data.size) {
            if (req->data)
                free(req->data);
            req->data = malloc(data.size);
            if (!req->data) {
                req->mem_size = 0;
                fprintf(stderr, "Failed to allocate %u bytes of SDO data"
                        " memory!\n", data.size);
                return EC_REQUEST_ERROR;
            }
            req->mem_size = data.size;
        }

        data.data = req->data;

        if (ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_DATA,
                    &data) == -1) {
            fprintf(stderr, "Failed to get SDO data: %s\n", strerror(errno));
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

    data.config_index = req->config->index;
    data.request_index = req->index;

    if (ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_READ,
                &data) == -1)
        fprintf(stderr, "Failed to command an SDO read operation : %s\n",
                strerror(errno));
}

/*****************************************************************************/

void ecrt_sdo_request_write(ec_sdo_request_t *req)
{
    ec_ioctl_sdo_request_t data;

    data.config_index = req->config->index;
    data.request_index = req->index;
    data.data = req->data;
    data.size = req->data_size;

    if (ioctl(req->config->master->fd, EC_IOCTL_SDO_REQUEST_WRITE,
                &data) == -1)
        fprintf(stderr, "Failed to command an SDO write operation : %s\n",
                strerror(errno));
}

/*****************************************************************************/
