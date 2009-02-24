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

#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

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
    domain->master = master;
    domain->process_data = NULL;
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

    sc->master = master;
    sc->index = data.config_index;
    sc->alias = alias;
    sc->position = position;
    return sc;
}

/*****************************************************************************/

int ecrt_master_slave(ec_master_t *master, uint16_t position,
        ec_slave_info_t *slave_info)
{
    ec_ioctl_slave_t data;
    int index;

    data.position = position;

    if (ioctl(master->fd, EC_IOCTL_SLAVE, &data) == -1) {
        fprintf(stderr, "Failed to get slave info: %s\n",
                strerror(errno));
        return -1;
    }

    slave_info->position = data.position;
    slave_info->vendor_id = data.vendor_id;
    slave_info->product_code = data.product_code;
    slave_info->revision_number = data.revision_number;
    slave_info->serial_number = data.serial_number;
    slave_info->alias = data.alias;
    slave_info->current_on_ebus = data.current_on_ebus;
    slave_info->al_state = data.al_state;
    slave_info->error_flag = data.error_flag;
    slave_info->sync_count = data.sync_count;
    slave_info->sdo_count = data.sdo_count;
    strncpy(slave_info->name, data.name, EC_IOCTL_STRING_SIZE);

    return 0;
}

/*****************************************************************************/

int ecrt_master_activate(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_ACTIVATE,
                &master->process_data_size) == -1) {
        fprintf(stderr, "Failed to activate master: %s\n",
                strerror(errno));
        return -1; // FIXME
    }

    if (master->process_data_size) {
        master->process_data = mmap(0, master->process_data_size,
                PROT_READ | PROT_WRITE, MAP_SHARED, master->fd, 0);
        if (master->process_data == MAP_FAILED) {
            fprintf(stderr, "Failed to map process data: %s", strerror(errno));
            master->process_data = NULL;
            master->process_data_size = 0;
            return -1; // FIXME
        }

        // Access the mapped region to cause the initial page fault
        printf("pd: %x\n", master->process_data[0]);
    }

    return 0;
}

/*****************************************************************************/

void ecrt_master_send(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_SEND, NULL) == -1) {
        fprintf(stderr, "Failed to send: %s\n", strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_receive(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_RECEIVE, NULL) == -1) {
        fprintf(stderr, "Failed to receive: %s\n", strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_state(const ec_master_t *master, ec_master_state_t *state)
{
    if (ioctl(master->fd, EC_IOCTL_MASTER_STATE, state) == -1) {
        fprintf(stderr, "Failed to get master state: %s\n", strerror(errno));
    }
}

/*****************************************************************************/
