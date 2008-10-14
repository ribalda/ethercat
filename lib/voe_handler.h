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

#include "include/ecrt.h"

/*****************************************************************************/

struct ec_voe_handler {
    ec_slave_config_t *config;
    unsigned int index;
    size_t data_size;
    size_t mem_size;
    uint8_t *data;
};

/*****************************************************************************/
