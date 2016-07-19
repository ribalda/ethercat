/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2016       Gavin Lambert
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
 * File access over EtherCAT FoE request functions.
 */

/*****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "ioctl.h"
#include "foe_request.h"
#include "slave_config.h"
#include "master.h"

/*****************************************************************************/

void ec_foe_request_clear(ec_foe_request_t *req)
{
    if (req->data) {
        free(req->data);
    }
}

/*****************************************************************************
 * Application interface.
 ****************************************************************************/

void ecrt_foe_request_file(ec_foe_request_t *req,
        const char *file_name, uint32_t password)
{
    ec_ioctl_foe_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;
    strncpy(data.file_name, file_name, sizeof(data.file_name));
    data.file_name[sizeof(data.file_name)-1] = 0;
    data.password = password;

    ret = ioctl(req->config->master->fd, EC_IOCTL_FOE_REQUEST_FILE, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to set FoE request filename: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

void ecrt_foe_request_timeout(ec_foe_request_t *req, uint32_t timeout)
{
    ec_ioctl_foe_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;
    data.timeout = timeout;

    ret = ioctl(req->config->master->fd, EC_IOCTL_FOE_REQUEST_TIMEOUT, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to set FoE request timeout: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

uint8_t *ecrt_foe_request_data(ec_foe_request_t *req)
{
    return req->data;
}

/*****************************************************************************/

size_t ecrt_foe_request_data_size(const ec_foe_request_t *req)
{
    return req->data_size;
}

/*****************************************************************************/

ec_request_state_t ecrt_foe_request_state(ec_foe_request_t *req)
{
    ec_ioctl_foe_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;

    ret = ioctl(req->config->master->fd, EC_IOCTL_FOE_REQUEST_STATE, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get SDO request state: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return EC_REQUEST_ERROR;
    }
    req->result = data.result;
    req->error_code = data.error_code;

    if (data.size) { // new data waiting to be copied
        if (req->mem_size < data.size) {
            fprintf(stderr, "Received %zu bytes do not fit into FoE data"
                    " memory (%zu bytes)!\n", data.size, req->mem_size);
            return EC_REQUEST_ERROR;
        }

        data.data = req->data;

        ret = ioctl(req->config->master->fd,
                EC_IOCTL_FOE_REQUEST_DATA, &data);
        if (EC_IOCTL_IS_ERROR(ret)) {
            fprintf(stderr, "Failed to get FoE data: %s\n",
                    strerror(EC_IOCTL_ERRNO(ret)));
            return EC_REQUEST_ERROR;
        }
        req->data_size = data.size;
    }

    return data.state;
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

void ecrt_foe_request_read(ec_foe_request_t *req)
{
    ec_ioctl_foe_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;

    ret = ioctl(req->config->master->fd, EC_IOCTL_FOE_REQUEST_READ, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to command an FoE read operation : %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

void ecrt_foe_request_write(ec_foe_request_t *req, size_t size)
{
    ec_ioctl_foe_request_t data;
    int ret;

    data.config_index = req->config->index;
    data.request_index = req->index;
    data.data = req->data;
    data.size = size;

    ret = ioctl(req->config->master->fd, EC_IOCTL_FOE_REQUEST_WRITE, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to command an FoE write operation : %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/
