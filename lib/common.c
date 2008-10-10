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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "master.h"
#include "master/ioctl.h"

/*****************************************************************************/

unsigned int ecrt_version_magic(void)
{
    return ECRT_VERSION_MAGIC;
}

/*****************************************************************************/

#define MAX_PATH_LEN 64

ec_master_t *ecrt_request_master(unsigned int master_index)
{
    char path[MAX_PATH_LEN];
    ec_master_t *master;

    master = malloc(sizeof(ec_master_t));
    if (!master) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }

    master->process_data = NULL;
    master->process_data_size = 0;

    snprintf(path, MAX_PATH_LEN - 1, "/dev/EtherCAT%u", master_index);

    master->fd = open(path, O_RDWR);
    if (master->fd == -1) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        free(master);
        return 0;
    }

    if (ioctl(master->fd, EC_IOCTL_REQUEST, NULL) == -1) {
        fprintf(stderr, "Failed to request master %u: %s\n",
                master_index, strerror(errno));
        close(master->fd);
        free(master);
        return 0; 
    }

    return master;
}

/*****************************************************************************/

void ecrt_release_master(ec_master_t *master)
{
    if (master->process_data)  {
        munmap(master->process_data, master->process_data_size);
    }

    close(master->fd);
    free(master);
}

/*****************************************************************************/
