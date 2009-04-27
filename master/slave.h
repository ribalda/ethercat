/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/**
   \file
   EtherCAT slave structure.
*/

/*****************************************************************************/

#ifndef __EC_SLAVE_H__
#define __EC_SLAVE_H__

#include <linux/list.h>
#include <linux/kobject.h>

#include "globals.h"
#include "datagram.h"
#include "pdo.h"
#include "sync.h"
#include "sdo.h"

/*****************************************************************************/

/** Slave information interface data.
 */
typedef struct {
    // Non-category data 
    uint16_t alias; /**< Configured station alias. */
    uint32_t vendor_id; /**< Vendor ID. */
    uint32_t product_code; /**< Vendor-specific product code. */
    uint32_t revision_number; /**< Revision number. */
    uint32_t serial_number; /**< Serial number. */
    uint16_t boot_rx_mailbox_offset; /**< Bootstrap receive mailbox address. */
    uint16_t boot_rx_mailbox_size; /**< Bootstrap receive mailbox size. */
    uint16_t boot_tx_mailbox_offset; /**< Bootstrap transmit mailbox address. */
    uint16_t boot_tx_mailbox_size; /**< Bootstrap transmit mailbox size. */
    uint16_t std_rx_mailbox_offset; /**< Standard receive mailbox address. */
    uint16_t std_rx_mailbox_size; /**< Standard receive mailbox size. */
    uint16_t std_tx_mailbox_offset; /**< Standard transmit mailbox address. */
    uint16_t std_tx_mailbox_size; /**< Standard transmit mailbox size. */
    uint16_t mailbox_protocols; /**< Supported mailbox protocols. */

    // Strings
    char **strings; /**< Strings in SII categories. */
    unsigned int string_count; /**< Number of SII strings. */

    // General
    unsigned int has_general; /**< General category present. */
    char *group; /**< Group name. */
    char *image; /**< Image name. */
    char *order; /**< Order number. */
    char *name; /**< Slave name. */
    uint8_t physical_layer[EC_MAX_PORTS]; /**< Port media. */
    ec_sii_coe_details_t coe_details; /**< CoE detail flags. */
    ec_sii_general_flags_t general_flags; /**< General flags. */
    int16_t current_on_ebus; /**< Power consumption in mA. */

    // SyncM
    ec_sync_t *syncs; /**< SYNC MANAGER categories. */
    unsigned int sync_count; /**< Number of sync managers. */

    // [RT]XPDO
    struct list_head pdos; /**< SII [RT]XPDO categories. */
} ec_sii_t;

/*****************************************************************************/

/** EtherCAT slave.
 */
struct ec_slave
{
    ec_master_t *master; /**< Master owning the slave. */

    // addresses
    uint16_t ring_position; /**< Ring position. */
    uint16_t station_address; /**< Configured station address. */

    // configuration
    ec_slave_config_t *config; /**< Current configuration. */
    ec_slave_state_t requested_state; /**< Requested application state. */
    ec_slave_state_t current_state; /**< Current application state. */
    unsigned int error_flag; /**< Stop processing after an error. */
    unsigned int force_config; /**< Force (re-)configuration. */
    uint16_t configured_rx_mailbox_offset;
    uint16_t configured_rx_mailbox_size;
    uint16_t configured_tx_mailbox_offset;
    uint16_t configured_tx_mailbox_size;

    // base data
    uint8_t base_type; /**< Slave type. */
    uint8_t base_revision; /**< Revision. */
    uint16_t base_build; /**< Build number. */
    uint8_t base_fmmu_count; /**< Number of supported FMMUs. */
    uint8_t base_sync_count; /**< Number of supported sync managers. */
    ec_slave_port_desc_t base_ports[EC_MAX_PORTS]; /**< Port descriptors. */
    uint8_t base_fmmu_bit_operation; /**< FMMU bit operation is supported. */
    uint8_t base_dc_supported; /**< Distributed clocks are supported. */
    ec_slave_dc_range_t base_dc_range; /**< DC range. */
    uint8_t has_dc_system_time; /**< The slave supports the DC system time
                                  register. Otherwise it can only be used for
                                  delay measurement. */
    uint32_t dc_receive_times[EC_MAX_PORTS]; /**< Port receive times for delay
                                               measurement. */

    // data link status
    ec_slave_port_t ports[EC_MAX_PORTS]; /**< Port link status. */

    // SII
    uint16_t *sii_words; /**< Complete SII image. */
    size_t sii_nwords; /**< Size of the SII contents in words. */

    // Slave information interface
    ec_sii_t sii; /**< Extracted SII data. */

    struct list_head sdo_dictionary; /**< SDO dictionary list */
    uint8_t sdo_dictionary_fetched; /**< Dictionary has been fetched. */
    unsigned long jiffies_preop; /**< Time, the slave went to PREOP. */
};

/*****************************************************************************/

// slave construction/destruction
void ec_slave_init(ec_slave_t *, ec_master_t *, uint16_t, uint16_t);
void ec_slave_clear(ec_slave_t *);

void ec_slave_clear_sync_managers(ec_slave_t *);

void ec_slave_request_state(ec_slave_t *, ec_slave_state_t);
void ec_slave_set_state(ec_slave_t *, ec_slave_state_t);

// SII categories
int ec_slave_fetch_sii_strings(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_general(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_syncs(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_sii_pdos(ec_slave_t *, const uint8_t *, size_t,
        ec_direction_t);

// misc.
ec_sync_t *ec_slave_get_sync(ec_slave_t *, uint8_t); 

void ec_slave_sdo_dict_info(const ec_slave_t *,
        unsigned int *, unsigned int *);
ec_sdo_t *ec_slave_get_sdo(ec_slave_t *, uint16_t);
const ec_sdo_t *ec_slave_get_sdo_const(const ec_slave_t *, uint16_t);
const ec_sdo_t *ec_slave_get_sdo_by_pos_const(const ec_slave_t *, uint16_t);
uint16_t ec_slave_sdo_count(const ec_slave_t *);
const ec_pdo_t *ec_slave_find_pdo(const ec_slave_t *, uint16_t);
void ec_slave_attach_pdo_names(ec_slave_t *);

/*****************************************************************************/

#endif
