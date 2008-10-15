/******************************************************************************
 *  
 * $Id$
 * 
 * Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 * 
 * This file is part of the IgH EtherCAT master userspace library.
 * 
 * The IgH EtherCAT master userspace library is free software: you can
 * redistribute it and/or modify it under the terms of the GNU Lesser General
 * Public License as published by the Free Software Foundation, version 2 of
 * the License.
 * 
 * The IgH EtherCAT master userspace library is distributed in the hope that
 * it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the IgH EtherCAT master userspace library. If not, see
 * <http://www.gnu.org/licenses/>.
 * 
 * The right to use EtherCAT Technology is granted and comes free of charge
 * under condition of compatibility of product made by Licensee. People
 * intending to distribute/sell products based on the code, have to sign an
 * agreement to guarantee that products using software based on IgH EtherCAT
 * master stay compatible with the actual EtherCAT specification (which are
 * released themselves as an open standard) as the (only) precondition to have
 * the right to use EtherCAT Technology, IP and trade marks.
 *
 *****************************************************************************/

/** \file
 * Vendor-specific-over-EtherCAT protocol handler functions.
 */

/*****************************************************************************/

#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "voe_handler.h"
#include "slave_config.h"
#include "master.h"
#include "master/ioctl.h"

/*****************************************************************************/

void ecrt_voe_handler_send_header(ec_voe_handler_t *voe, uint32_t vendor_id,
        uint16_t vendor_type)
{
    ec_ioctl_voe_t data;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;
    data.vendor_id = &vendor_id;
    data.vendor_type = &vendor_type;

    if (ioctl(voe->config->master->fd, EC_IOCTL_VOE_SEND_HEADER, &data) == -1)
        fprintf(stderr, "Failed to set VoE send header: %s\n",
                strerror(errno));
}

/*****************************************************************************/

void ecrt_voe_handler_received_header(const ec_voe_handler_t *voe,
        uint32_t *vendor_id, uint16_t *vendor_type)
{
    ec_ioctl_voe_t data;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;
    data.vendor_id = vendor_id;
    data.vendor_type = vendor_type;

    if (ioctl(voe->config->master->fd, EC_IOCTL_VOE_REC_HEADER, &data) == -1)
        fprintf(stderr, "Failed to get received VoE header: %s\n",
                strerror(errno));
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

    data.config_index = voe->config->index;
    data.voe_index = voe->index;

    if (ioctl(voe->config->master->fd, EC_IOCTL_VOE_READ, &data) == -1)
        fprintf(stderr, "Failed to initiate VoE reading: %s\n",
                strerror(errno));
}

/*****************************************************************************/

void ecrt_voe_handler_write(ec_voe_handler_t *voe, size_t size)
{
    ec_ioctl_voe_t data;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;
    data.size = size;
    data.data = voe->data;

    if (ioctl(voe->config->master->fd, EC_IOCTL_VOE_WRITE, &data) == -1)
        fprintf(stderr, "Failed to initiate VoE writing: %s\n",
                strerror(errno));
}

/*****************************************************************************/

ec_request_state_t ecrt_voe_handler_execute(ec_voe_handler_t *voe)
{
    ec_ioctl_voe_t data;

    data.config_index = voe->config->index;
    data.voe_index = voe->index;
    data.size = 0;

    if (ioctl(voe->config->master->fd, EC_IOCTL_VOE_EXEC, &data) == -1) {
        fprintf(stderr, "Failed to execute VoE handler: %s\n",
                strerror(errno));
        return EC_REQUEST_ERROR;
    }

    if (data.size) { // new data waiting to be copied
        if (voe->mem_size < data.size) {
            if (voe->data)
                free(voe->data);
            voe->data = malloc(data.size);
            if (!voe->data) {
                voe->mem_size = 0;
                fprintf(stderr, "Failed to allocate VoE data memory!");
                return EC_REQUEST_ERROR;
            }
            voe->mem_size = data.size;
        }

        data.data = voe->data;

        if (ioctl(voe->config->master->fd, EC_IOCTL_VOE_DATA, &data) == -1) {
            fprintf(stderr, "Failed to get VoE data: %s\n", strerror(errno));
            return EC_REQUEST_ERROR;
        }
        voe->data_size = data.size;
    }

    return data.state;
}

/*****************************************************************************/
