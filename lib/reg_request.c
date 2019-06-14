/******************************************************************************
 *
 *  Copyright (C) 2012-2019  Florian Pose, Ingenieurgemeinschaft IgH
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
 * EtherCAT register request functions.
 */

/*****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "ioctl.h"
#include "reg_request.h"
#include "slave_config.h"
#include "master.h"

/*****************************************************************************/

void ec_reg_request_clear(ec_reg_request_t *reg)
{
    if (reg->data) {
        free(reg->data);
        reg->data = NULL;
    }
}

/*****************************************************************************
 * Application interface.
 ****************************************************************************/

uint8_t *ecrt_reg_request_data(ec_reg_request_t *reg)
{
    return reg->data;
}

/*****************************************************************************/

ec_request_state_t ecrt_reg_request_state(ec_reg_request_t *reg)
{
    ec_ioctl_reg_request_t io;
    int ret;

    io.config_index = reg->config->index;
    io.request_index = reg->index;

    ret = ioctl(reg->config->master->fd, EC_IOCTL_REG_REQUEST_STATE, &io);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get register request state: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return EC_REQUEST_ERROR;
    }

    if (io.new_data) { // new data waiting to be copied

        io.data = reg->data;
        io.mem_size = reg->mem_size;

        ret = ioctl(reg->config->master->fd,
                EC_IOCTL_REG_REQUEST_DATA, &io);
        if (EC_IOCTL_IS_ERROR(ret)) {
            fprintf(stderr, "Failed to get register data: %s\n",
                    strerror(EC_IOCTL_ERRNO(ret)));
            return EC_REQUEST_ERROR;
        }
    }

    return io.state;
}

/*****************************************************************************/

void ecrt_reg_request_write(ec_reg_request_t *reg, uint16_t address,
        size_t size)
{
    ec_ioctl_reg_request_t io;
    int ret;

    io.config_index = reg->config->index;
    io.request_index = reg->index;
    io.data = reg->data;
    io.address = address;
    io.transfer_size = size;

    ret = ioctl(reg->config->master->fd, EC_IOCTL_REG_REQUEST_WRITE, &io);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to command an register write operation: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

void ecrt_reg_request_read(ec_reg_request_t *reg, uint16_t address,
        size_t size)
{
    ec_ioctl_reg_request_t io;
    int ret;

    io.config_index = reg->config->index;
    io.request_index = reg->index;
    io.address = address;
    io.transfer_size = size;

    ret = ioctl(reg->config->master->fd, EC_IOCTL_REG_REQUEST_READ, &io);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to command an register read operation: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/
