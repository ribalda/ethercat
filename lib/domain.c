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

/**
   \file
   EtherCAT domain methods.
*/

/*****************************************************************************/

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "domain.h"
#include "master.h"
#include "master/ioctl.h"

/*****************************************************************************/

int ecrt_domain_reg_pdo_entry_list(ec_domain_t *domain,
        const ec_pdo_entry_reg_t *regs)
{
    const ec_pdo_entry_reg_t *reg;
    ec_slave_config_t *sc;
    int ret;
    
    for (reg = regs; reg->index; reg++) {
        if (!(sc = ecrt_master_slave_config(domain->master, reg->alias,
                        reg->position, reg->vendor_id, reg->product_code)))
            return -1;

        if ((ret = ecrt_slave_config_reg_pdo_entry(sc, reg->index,
                        reg->subindex, domain, reg->bit_position)) < 0)
            return -1;

        *reg->offset = ret;
    }

    return 0;
}

/*****************************************************************************/

uint8_t *ecrt_domain_data(ec_domain_t *domain)
{
    if (!domain->process_data) {
        int offset = 0;

        offset = ioctl(domain->master->fd, EC_IOCTL_DOMAIN_OFFSET,
                domain->index);
        if (offset == -1) {
            fprintf(stderr, "Failed to get domain offset: %s\n",
                    strerror(errno));
            return NULL; 
        }
    
        domain->process_data = domain->master->process_data + offset;
    }

    return domain->process_data;
}

/*****************************************************************************/

void ecrt_domain_process(ec_domain_t *domain)
{
    if (ioctl(domain->master->fd, EC_IOCTL_DOMAIN_PROCESS,
                domain->index) == -1) {
        fprintf(stderr, "Failed to process domain offset: %s\n",
                strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_domain_queue(ec_domain_t *domain)
{
    if (ioctl(domain->master->fd, EC_IOCTL_DOMAIN_QUEUE,
                domain->index) == -1) {
        fprintf(stderr, "Failed to queue domain offset: %s\n",
                strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_domain_state(const ec_domain_t *domain, ec_domain_state_t *state)
{
    ec_ioctl_domain_state_t data;

    data.domain_index = domain->index;
    data.state = state;
    
    if (ioctl(domain->master->fd, EC_IOCTL_DOMAIN_STATE, &data) == -1) {
        fprintf(stderr, "Failed to get domain state: %s\n",
                strerror(errno));
    }
}

/*****************************************************************************/
