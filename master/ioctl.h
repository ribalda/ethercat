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

#include <linux/ioctl.h>

/*****************************************************************************/

#define EC_IOCTL_TYPE    0xa4

#define EC_IO(nr)          _IO(EC_IOCTL_TYPE,nr)
#define EC_IOR(nr,type)   _IOR(EC_IOCTL_TYPE,nr,type)
#define EC_IOW(nr,type)   _IOW(EC_IOCTL_TYPE,nr,type)
#define EC_IOWR(nr,type) _IOWR(EC_IOCTL_TYPE,nr,type)

#define EC_IOCTL_MASTER        EC_IOR(0x00, ec_ioctl_master_t)
#define EC_IOCTL_SLAVE        EC_IOWR(0x01, ec_ioctl_slave_t)
#define EC_IOCTL_SYNC         EC_IOWR(0x02, ec_ioctl_sync_t)
#define EC_IOCTL_PDO          EC_IOWR(0x03, ec_ioctl_pdo_t)
#define EC_IOCTL_PDO_ENTRY    EC_IOWR(0x04, ec_ioctl_pdo_entry_t)
#define EC_IOCTL_DOMAIN_COUNT   EC_IO(0x05)
#define EC_IOCTL_DOMAIN       EC_IOWR(0x06, ec_ioctl_domain_t)
#define EC_IOCTL_DOMAIN_FMMU  EC_IOWR(0x07, ec_ioctl_domain_fmmu_t)
#define EC_IOCTL_DATA         EC_IOWR(0x08, ec_ioctl_data_t)
#define EC_IOCTL_SET_DEBUG      EC_IO(0x09)
#define EC_IOCTL_SLAVE_STATE   EC_IOW(0x0a, ec_ioctl_slave_state_t)
#define EC_IOCTL_SDO          EC_IOWR(0x0b, ec_ioctl_sdo_t)
#define EC_IOCTL_SDO_ENTRY    EC_IOWR(0x0c, ec_ioctl_sdo_entry_t)
#define EC_IOCTL_SDO_UPLOAD   EC_IOWR(0x0d, ec_ioctl_sdo_upload_t)

/*****************************************************************************/

typedef struct {
    unsigned int slave_count;
    uint8_t mode;
    struct {
        uint8_t address[6];
        uint8_t attached;
        unsigned int tx_count;
        unsigned int rx_count;
    } devices[2];
} ec_ioctl_master_t;

/*****************************************************************************/

#define EC_IOCTL_SLAVE_NAME_SIZE 104

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
    uint16_t sdo_count;
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

typedef struct {
    // inputs
	uint16_t slave_position;
    uint8_t requested_state;
} ec_ioctl_slave_state_t;

/*****************************************************************************/

#define EC_IOCTL_SDO_NAME_SIZE 121

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_position;

    // outputs
    uint16_t sdo_index;
    uint8_t max_subindex;
    char name[EC_IOCTL_SDO_NAME_SIZE];
} ec_ioctl_sdo_t;

/*****************************************************************************/

#define EC_IOCTL_SDO_ENTRY_DESCRIPTION_SIZE 120

typedef struct {
    // inputs
    uint16_t slave_position;
    int sdo_spec; // positive: index, negative: list position
    uint8_t sdo_entry_subindex;

    // outputs
    uint16_t data_type;
    uint16_t bit_length;
    char description[EC_IOCTL_SDO_ENTRY_DESCRIPTION_SIZE];
} ec_ioctl_sdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_index;
    uint8_t sdo_entry_subindex;
    unsigned int target_size;
    uint8_t *target;

    // outputs
    unsigned int data_size;
} ec_ioctl_sdo_upload_t;

/*****************************************************************************/

#endif
