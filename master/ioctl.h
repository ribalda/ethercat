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

#include "globals.h"

/*****************************************************************************/

#define EC_IOCTL_TYPE    0xa4

#define EC_IO(nr)          _IO(EC_IOCTL_TYPE,nr)
#define EC_IOR(nr,type)   _IOR(EC_IOCTL_TYPE,nr,type)
#define EC_IOW(nr,type)   _IOW(EC_IOCTL_TYPE,nr,type)
#define EC_IOWR(nr,type) _IOWR(EC_IOCTL_TYPE,nr,type)

#define EC_IOCTL_MASTER            EC_IOR(0x00, ec_ioctl_master_t)
#define EC_IOCTL_SLAVE            EC_IOWR(0x01, ec_ioctl_slave_t)
#define EC_IOCTL_SYNC             EC_IOWR(0x02, ec_ioctl_sync_t)
#define EC_IOCTL_PDO              EC_IOWR(0x03, ec_ioctl_pdo_t)
#define EC_IOCTL_PDO_ENTRY        EC_IOWR(0x04, ec_ioctl_pdo_entry_t)
#define EC_IOCTL_DOMAIN           EC_IOWR(0x05, ec_ioctl_domain_t)
#define EC_IOCTL_DOMAIN_FMMU      EC_IOWR(0x06, ec_ioctl_domain_fmmu_t)
#define EC_IOCTL_DATA             EC_IOWR(0x07, ec_ioctl_data_t)
#define EC_IOCTL_SET_DEBUG          EC_IO(0x08)
#define EC_IOCTL_SLAVE_STATE       EC_IOW(0x09, ec_ioctl_slave_state_t)
#define EC_IOCTL_SDO              EC_IOWR(0x0a, ec_ioctl_sdo_t)
#define EC_IOCTL_SDO_ENTRY        EC_IOWR(0x0b, ec_ioctl_sdo_entry_t)
#define EC_IOCTL_SDO_UPLOAD       EC_IOWR(0x0c, ec_ioctl_sdo_upload_t)
#define EC_IOCTL_SDO_DOWNLOAD     EC_IOWR(0x0d, ec_ioctl_sdo_download_t)
#define EC_IOCTL_SII_READ         EC_IOWR(0x0e, ec_ioctl_sii_t)
#define EC_IOCTL_SII_WRITE         EC_IOW(0x0f, ec_ioctl_sii_t)
#define EC_IOCTL_CONFIG           EC_IOWR(0x10, ec_ioctl_config_t)
#define EC_IOCTL_CONFIG_PDO       EC_IOWR(0x11, ec_ioctl_config_pdo_t)
#define EC_IOCTL_CONFIG_PDO_ENTRY EC_IOWR(0x12, ec_ioctl_config_pdo_entry_t)
#define EC_IOCTL_CONFIG_SDO       EC_IOWR(0x13, ec_ioctl_config_sdo_t)

#define EC_IOCTL_STRING_SIZE 64

/*****************************************************************************/

typedef struct {
    uint32_t slave_count;
    uint32_t config_count;
    uint32_t domain_count;
    uint8_t phase;
    struct {
        uint8_t address[6];
        uint8_t attached;
        uint32_t tx_count;
        uint32_t rx_count;
    } devices[2];
} ec_ioctl_master_t;

/*****************************************************************************/

typedef struct {
    // input
    uint16_t position;

    // outputs
    uint32_t vendor_id;
    uint32_t product_code;
    uint32_t revision_number;
    uint32_t serial_number;
    uint16_t alias;
    uint16_t rx_mailbox_offset;
    uint16_t rx_mailbox_size;
    uint16_t tx_mailbox_offset;
    uint16_t tx_mailbox_size;
    uint16_t mailbox_protocols;
    uint8_t has_general_category;
    ec_sii_coe_details_t coe_details;
    ec_sii_general_flags_t general_flags;
    int16_t current_on_ebus;
    uint8_t state;
    uint8_t error_flag;
    uint8_t sync_count;
    uint16_t sdo_count;
    uint32_t sii_nwords;
    char name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_slave_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint32_t sync_index;

    // outputs
    uint16_t physical_start_address;
    uint16_t default_size;
    uint8_t control_register;
    uint8_t enable;
    uint8_t assign_source;
    uint8_t pdo_count;
} ec_ioctl_sync_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint32_t sync_index;
    uint32_t pdo_pos;

    // outputs
    uint16_t index;
    uint8_t entry_count;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_pdo_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint32_t sync_index;
    uint32_t pdo_pos;
    uint32_t entry_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_pdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
	uint32_t index;

    // outputs
	uint32_t data_size;
	uint32_t logical_base_address;
	uint16_t working_counter;
	uint16_t expected_working_counter;
    uint32_t fmmu_count;
} ec_ioctl_domain_t;

/*****************************************************************************/

typedef struct {
    // inputs
	uint32_t domain_index;
	uint32_t fmmu_index;

    // outputs
    uint16_t slave_config_alias;
    uint16_t slave_config_position;
    uint8_t sync_index;
    ec_direction_t dir;
	uint32_t logical_address;
    uint32_t data_size;
} ec_ioctl_domain_fmmu_t;

/*****************************************************************************/

typedef struct {
    // inputs
	uint32_t domain_index;
    uint32_t data_size;
    uint8_t *target;
} ec_ioctl_data_t;

/*****************************************************************************/

typedef struct {
    // inputs
	uint16_t slave_position;
    uint8_t requested_state;
} ec_ioctl_slave_state_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_position;

    // outputs
    uint16_t sdo_index;
    uint8_t max_subindex;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_sdo_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    int sdo_spec; // positive: index, negative: list position
    uint8_t sdo_entry_subindex;

    // outputs
    uint16_t data_type;
    uint16_t bit_length;
    int8_t description[EC_IOCTL_STRING_SIZE];
} ec_ioctl_sdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_index;
    uint8_t sdo_entry_subindex;
    uint32_t target_size;
    uint8_t *target;

    // outputs
    uint32_t data_size;
    uint32_t abort_code;
} ec_ioctl_sdo_upload_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t sdo_index;
    uint8_t sdo_entry_subindex;
    uint32_t data_size;
    uint8_t *data;

    // outputs
    uint32_t abort_code;
} ec_ioctl_sdo_download_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint16_t slave_position;
    uint16_t offset;
    uint32_t nwords;
    uint16_t *words;
} ec_ioctl_sii_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;

    // outputs
    uint16_t alias;
    uint16_t position;
    uint32_t vendor_id;
    uint32_t product_code;
    struct {
        ec_direction_t dir;
        uint32_t pdo_count;
    } syncs[16];
    uint32_t sdo_count;
    uint8_t attached;
} ec_ioctl_config_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint8_t sync_index;
    uint16_t pdo_pos;

    // outputs
    uint16_t index;
    uint8_t entry_count;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_config_pdo_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint8_t sync_index;
    uint16_t pdo_pos;
    uint8_t entry_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint8_t bit_length;
    int8_t name[EC_IOCTL_STRING_SIZE];
} ec_ioctl_config_pdo_entry_t;

/*****************************************************************************/

typedef struct {
    // inputs
    uint32_t config_index;
    uint32_t sdo_pos;

    // outputs
    uint16_t index;
    uint8_t subindex;
    uint32_t size;
    uint8_t data[4];
} ec_ioctl_config_sdo_t;

/*****************************************************************************/

#endif
