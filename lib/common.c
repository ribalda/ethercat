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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/socket.h>

#include "ioctl.h"
#include "master.h"

/*****************************************************************************/

unsigned int ecrt_version_magic(void)
{
    return ECRT_VERSION_MAGIC;
}

/*****************************************************************************/

ec_master_t *ecrt_request_master(unsigned int master_index)
{
    ec_master_t *master = ecrt_open_master(master_index);
    if (master) {
        if (ecrt_master_reserve(master) < 0) {
            ec_master_clear(master);
            free(master);
            master = NULL;
        }
    }

    return master;
}

/*****************************************************************************/

#define MAX_PATH_LEN 64

ec_master_t *ecrt_open_master(unsigned int master_index)
{
    char path[MAX_PATH_LEN];
    ec_master_t *master = NULL;
    ec_ioctl_module_t module_data;
    int ret;

    master = malloc(sizeof(ec_master_t));
    if (!master) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }

    master->process_data = NULL;
    master->process_data_size = 0;
    master->first_domain = NULL;
    master->first_config = NULL;

    snprintf(path, MAX_PATH_LEN - 1,
#ifdef USE_RTDM
            "EtherCAT%u",
#else
            "/dev/EtherCAT%u",
#endif
            master_index);

#ifdef USE_RTDM
    master->fd = rt_dev_open(path, O_RDWR);
#else
    master->fd = open(path, O_RDWR);
#endif
    if (EC_IOCTL_IS_ERROR(master->fd)) {
        fprintf(stderr, "Failed to open %s: %s\n", path,
                strerror(EC_IOCTL_ERRNO(master->fd)));
        goto out_clear;
    }

    ret = ioctl(master->fd, EC_IOCTL_MODULE, &module_data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get module information from %s: %s\n",
                path, strerror(EC_IOCTL_ERRNO(ret)));
        goto out_clear;
    }

    if (module_data.ioctl_version_magic != EC_IOCTL_VERSION_MAGIC) {
        fprintf(stderr, "ioctl() version magic is differing:"
                " %s: %u, libethercat: %u.\n",
                path, module_data.ioctl_version_magic,
                EC_IOCTL_VERSION_MAGIC);
        goto out_clear;
    }

    return master;

out_clear:
    ec_master_clear(master);
    free(master);
    return 0;
}

/*****************************************************************************/

void ecrt_release_master(ec_master_t *master)
{
    ec_master_clear(master);
    free(master);
}

/*****************************************************************************/

float ecrt_read_real(const void *data)
{
    uint32_t raw = EC_READ_U32(data);
    return *(float *) (const void *) &raw;
}

/*****************************************************************************/

double ecrt_read_lreal(const void *data)
{
    uint64_t raw = EC_READ_U64(data);
    return *(double *) (const void *) &raw;
}

/*****************************************************************************/

void ecrt_write_real(void *data, float value)
{
    *(uint32_t *) data = cpu_to_le32(*(uint32_t *) (void *) &value);
}

/*****************************************************************************/

void ecrt_write_lreal(void *data, double value)
{
    *(uint64_t *) data = cpu_to_le64(*(uint64_t *) (void *) &value);
}

/*****************************************************************************/
