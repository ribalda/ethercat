/******************************************************************************
 *
 *  Copyright (C) 2006-2019  Florian Pose, Ingenieurgemeinschaft IgH
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
 * Vendor specific over EtherCAT protocol handler functions.
 */

/*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ioctl.h"
#include "voe_handler.h"
#include "slave_config.h"
#include "master.h"

/*****************************************************************************/

void ec_voe_handler_clear(ec_voe_handler_t *voe)
{
    if (voe->data) {
        free(voe->data);
        voe->data = NULL;
    }
}

/*****************************************************************************/

void ecrt_voe_handler_send_header(ec_voe_handler_t *voe, uint32_t vendor_id,
        uint16_t vendor_type)
{
    ec_ioctl_voe_t data;
    int ret;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;
    data.vendor_id = &vendor_id;
    data.vendor_type = &vendor_type;

    ret = ioctl(voe->config->master->fd, EC_IOCTL_VOE_SEND_HEADER, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to set VoE send header: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

void ecrt_voe_handler_received_header(const ec_voe_handler_t *voe,
        uint32_t *vendor_id, uint16_t *vendor_type)
{
    ec_ioctl_voe_t data;
    int ret;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;
    data.vendor_id = vendor_id;
    data.vendor_type = vendor_type;

    ret = ioctl(voe->config->master->fd, EC_IOCTL_VOE_REC_HEADER, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get received VoE header: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

uint8_t *ecrt_voe_handler_data(ec_voe_handler_t *voe)
{
    return voe->data;
}

/*****************************************************************************/

size_t ecrt_voe_handler_data_size(const ec_voe_handler_t *voe)
{
    return voe->data_size;
}

/*****************************************************************************/

void ecrt_voe_handler_read(ec_voe_handler_t *voe)
{
    ec_ioctl_voe_t data;
    int ret;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;

    ret = ioctl(voe->config->master->fd, EC_IOCTL_VOE_READ, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to initiate VoE reading: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

void ecrt_voe_handler_read_nosync(ec_voe_handler_t *voe)
{
    ec_ioctl_voe_t data;
    int ret;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;

    ret = ioctl(voe->config->master->fd, EC_IOCTL_VOE_READ_NOSYNC, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to initiate VoE reading: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

void ecrt_voe_handler_write(ec_voe_handler_t *voe, size_t size)
{
    ec_ioctl_voe_t data;
    int ret;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;
    data.size = size;
    data.data = voe->data;

    ret = ioctl(voe->config->master->fd, EC_IOCTL_VOE_WRITE, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to initiate VoE writing: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

ec_request_state_t ecrt_voe_handler_execute(ec_voe_handler_t *voe)
{
    ec_ioctl_voe_t data;
    int ret;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;

    ret = ioctl(voe->config->master->fd, EC_IOCTL_VOE_EXEC, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to execute VoE handler: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return EC_REQUEST_ERROR;
    }

    if (data.size) { // new data waiting to be copied
        if (voe->mem_size < data.size) {
            fprintf(stderr, "Received %zu bytes do not fit info VoE data"
                    " memory (%zu bytes)!\n", data.size, voe->mem_size);
            return EC_REQUEST_ERROR;
        }

        data.data = voe->data;

        ret = ioctl(voe->config->master->fd, EC_IOCTL_VOE_DATA, &data);
        if (EC_IOCTL_IS_ERROR(ret)) {
            fprintf(stderr, "Failed to get VoE data: %s\n",
                    strerror(EC_IOCTL_ERRNO(ret)));
            return EC_REQUEST_ERROR;
        }
        voe->data_size = data.size;
    }

    return data.state;
}

/*****************************************************************************/
