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

/**
   \file
   EtherCAT master character device IOCTL commands.
*/

/*****************************************************************************/

#ifndef __EC_IOCTL_H__
#define __EC_IOCTL_H__

/*****************************************************************************/

enum {
    EC_IOCTL_SLAVE_COUNT,
    EC_IOCTL_SLAVE,
    EC_IOCTL_SYNC,
    EC_IOCTL_PDO,
    EC_IOCTL_PDO_ENTRY,
	EC_IOCTL_DOMAIN_COUNT,
	EC_IOCTL_DOMAIN,
	EC_IOCTL_DOMAIN_FMMU,
	EC_IOCTL_DATA,
    EC_IOCTL_DEBUG_LEVEL,
};

/*****************************************************************************/

#define EC_IOCTL_SLAVE_NAME_SIZE 114

typedef struct {
    // input
    uint16_t position;

    // outputs
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    uint32_t serial_number;
    uint16_t alias;
    uint8_t state;
    uint8_t sync_count;
    char name[EC_IOCTL_SLAVE_NAME_SIZE];
} ec_ioctl_slave_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    unsigned int sync_index;

    // outputs
    uint16_t physical_start_address;
    uint16_t default_size;
    uint8_t control_register;
    uint8_t enable;
    uint8_t assign_source;
    uint8_t pdo_count;
} ec_ioctl_sync_t;

/*****************************************************************************/

#define EC_IOCTL_PDO_NAME_SIZE 114

typedef struct {
    // inputs
    uint16_t slave_position;
    unsigned int sync_index;
    unsigned int pdo_pos;

    // outputs
    uint8_t dir;
    uint16_t index;
    uint8_t entry_count;
    char name[EC_IOCTL_PDO_NAME_SIZE];
} ec_ioctl_pdo_t;

/*****************************************************************************/

#define EC_IOCTL_PDO_ENTRY_NAME_SIZE 110

typedef struct {
    // inputs
    uint16_t slave_position;
    unsigned int sync_index;
    unsigned int pdo_pos;
    unsigned int entry_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    char name[EC_IOCTL_PDO_NAME_SIZE];
} ec_ioctl_pdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
	unsigned int index;

    // outputs
	unsigned int data_size;
	uint32_t logical_base_address;
	uint16_t working_counter;
	uint16_t expected_working_counter;
    unsigned int fmmu_count;
} ec_ioctl_domain_t;

/*****************************************************************************/

typedef struct {
    // inputs
	unsigned int domain_index;
	unsigned int fmmu_index;

    // outputs
    uint16_t slave_config_alias;
    uint16_t slave_config_position;
    uint8_t fmmu_dir;
	uint32_t logical_address;
    unsigned int data_size;
} ec_ioctl_domain_fmmu_t;

/*****************************************************************************/

typedef struct {
    // inputs
	unsigned int domain_index;
    unsigned int data_size;
    unsigned char *target;
} ec_ioctl_data_t;

/*****************************************************************************/

#endif
