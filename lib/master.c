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

#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "master.h"
#include "domain.h"
#include "slave_config.h"
#include "master/ioctl.h"

/*****************************************************************************/

ec_domain_t *ecrt_master_create_domain(ec_master_t *master)
{
    ec_domain_t *domain;
    int index;

    domain = malloc(sizeof(ec_domain_t));
    if (!domain) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }
    
    index = ioctl(master->fd, EC_IOCTL_CREATE_DOMAIN, NULL);
    if (index == -1) {
        fprintf(stderr, "Failed to create domain: %s\n", strerror(errno));
        free(domain);
        return 0; 
    }

    domain->index = (unsigned int) index;
    return domain;
}

/*****************************************************************************/

ec_slave_config_t *ecrt_master_slave_config(ec_master_t *master,
        uint16_t alias, uint16_t position, uint32_t vendor_id,
        uint32_t product_code)
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc;
    int index;

    sc = malloc(sizeof(ec_slave_config_t));
    if (!sc) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }
    
    data.alias = alias;
    data.position = position;
    data.vendor_id = vendor_id;
    data.product_code = product_code;
    
    if (ioctl(master->fd, EC_IOCTL_CREATE_SLAVE_CONFIG, &data) == -1) {
        fprintf(stderr, "Failed to create slave config: %s\n",
                strerror(errno));
        free(sc);
        return 0; 
    }

    sc->index = data.config_index;
    return sc;
}

/*****************************************************************************/

int ecrt_master_activate(ec_master_t *master)
{
    return 0;
}

/*****************************************************************************/

void ecrt_master_send(ec_master_t *master)
{
}

/*****************************************************************************/

void ecrt_master_receive(ec_master_t *master)
{
}

/*****************************************************************************/

void ecrt_master_state(const ec_master_t *master, ec_master_state_t *state)
{
}


/*****************************************************************************/
