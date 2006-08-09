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
   EtherCAT stave structure.
*/

/*****************************************************************************/

#ifndef _EC_SLAVE_H_
#define _EC_SLAVE_H_

#include <linux/list.h>
#include <linux/kobject.h>

#include "../include/ecrt.h"

#include "globals.h"
#include "datagram.h"

/*****************************************************************************/

/**
   State of an EtherCAT slave.
*/

typedef enum
{
    EC_SLAVE_STATE_UNKNOWN = 0x00,
    /**< unknown state */
    EC_SLAVE_STATE_INIT = 0x01,
    /**< INIT state (no mailbox communication, no IO) */
    EC_SLAVE_STATE_PREOP = 0x02,
    /**< PREOP state (mailbox communication, no IO) */
    EC_SLAVE_STATE_SAVEOP = 0x04,
    /**< SAVEOP (mailbox communication and input update) */
    EC_SLAVE_STATE_OP = 0x08,
    /**< OP (mailbox communication and input/output update) */
    EC_ACK = 0x10
    /**< Acknoledge bit (no state) */
}
ec_slave_state_t;

/*****************************************************************************/

/**
   Supported mailbox protocols.
*/

enum
{
    EC_MBOX_AOE = 0x01, /**< ADS-over-EtherCAT */
    EC_MBOX_EOE = 0x02, /**< Ethernet-over-EtherCAT */
    EC_MBOX_COE = 0x04, /**< CANopen-over-EtherCAT */
    EC_MBOX_FOE = 0x08, /**< File-Access-over-EtherCAT */
    EC_MBOX_SOE = 0x10, /**< Servo-Profile-over-EtherCAT */
    EC_MBOX_VOE = 0x20  /**< Vendor specific */
};

/*****************************************************************************/

/**
   String object (EEPROM).
*/

typedef struct
{
    struct list_head list; /**< list item */
    size_t size; /**< size in bytes */
    char *data; /**< string data */
}
ec_sii_string_t;

/*****************************************************************************/

/**
   Sync manager configuration (EEPROM).
*/

typedef struct
{
    struct list_head list; /**< list item */
    unsigned int index; /**< sync manager index */
    uint16_t physical_start_address; /**< physical start address */
    uint16_t length; /**< data length in bytes */
    uint8_t control_register; /**< control register value */
    uint8_t enable; /**< enable bit */
}
ec_sii_sync_t;

/*****************************************************************************/

/**
   PDO type.
*/

typedef enum
{
    EC_RX_PDO, /**< Reveive PDO */
    EC_TX_PDO /**< Transmit PDO */
}
ec_sii_pdo_type_t;

/*****************************************************************************/

/**
   PDO description (EEPROM).
*/

typedef struct
{
    struct list_head list; /**< list item */
    ec_sii_pdo_type_t type; /**< PDO type */
    uint16_t index; /**< PDO index */
    uint8_t sync_index; /**< assigned sync manager */
    char *name; /**< PDO name */
    struct list_head entries; /**< entry list */
}
ec_sii_pdo_t;

/*****************************************************************************/

/**
   PDO entry description (EEPROM).
*/

typedef struct
{
    struct list_head list; /**< list item */
    uint16_t index; /**< PDO index */
    uint8_t subindex; /**< entry subindex */
    char *name; /**< entry name */
    uint8_t bit_length; /**< entry length in bit */
}
ec_sii_pdo_entry_t;

/*****************************************************************************/

/**
   CANopen SDO.
*/

typedef struct
{
    struct list_head list; /**< list item */
    uint16_t index; /**< SDO index */
    uint8_t object_code; /**< object code */
    char *name; /**< SDO name */
    struct list_head entries; /**< entry list */
}
ec_sdo_t;

/*****************************************************************************/

/**
   CANopen SDO entry.
*/

typedef struct
{
    struct list_head list; /**< list item */
    uint8_t subindex; /**< entry subindex */
    uint16_t data_type; /**< entry data type */
    uint16_t bit_length; /**< entry length in bit */
    char *name; /**< entry name */
}
ec_sdo_entry_t;

/*****************************************************************************/

typedef struct
{
    struct list_head list; /**< list item */
    uint16_t index; /**< SDO index */
    uint8_t subindex; /**< SDO subindex */
    uint8_t *data; /**< pointer to SDO data */
    size_t size; /**< size of SDO data */
}
ec_sdo_data_t;

/*****************************************************************************/

/**
   FMMU configuration.
*/

typedef struct
{
    const ec_domain_t *domain; /**< domain */
    const ec_sii_sync_t *sync; /**< sync manager */
    uint32_t logical_start_address; /**< logical start address */
}
ec_fmmu_t;

/*****************************************************************************/

/**
   Variable-sized field information.
*/

typedef struct
{
    struct list_head list; /**< list item */
    const ec_sii_pdo_t *pdo; /**< PDO */
    size_t size; /**< field size */
}
ec_varsize_t;

/*****************************************************************************/

/**
   EtherCAT slave.
*/

struct ec_slave
{
    struct list_head list; /**< list item */
    struct kobject kobj; /**< kobject */
    ec_master_t *master; /**< master owning the slave */

    ec_slave_state_t requested_state; /**< requested slave state */
    ec_slave_state_t current_state; /**< current slave state */
    unsigned int error_flag; /**< stop processing after an error */
    unsigned int online; /**< non-zero, if the slave responds. */
    uint8_t registered; /**< true, if slave has been registered */

    // addresses
    uint16_t ring_position; /**< ring position */
    uint16_t station_address; /**< configured station address */
    uint16_t coupler_index; /**< index of the last bus coupler */
    uint16_t coupler_subindex; /**< index of this slave after last coupler */

    // base data
    uint8_t base_type; /**< slave type */
    uint8_t base_revision; /**< revision */
    uint16_t base_build; /**< build number */
    uint16_t base_fmmu_count; /**< number of supported FMMUs */
    uint16_t base_sync_count; /**< number of supported sync managers */

    // data link status
    uint8_t dl_link[4]; /**< link detected */
    uint8_t dl_loop[4]; /**< loop closed */
    uint8_t dl_signal[4]; /**< detected signal on RX port */

    // EEPROM
    uint8_t *eeprom_data; /**< Complete EEPROM image */
    uint16_t eeprom_size; /**< size of the EEPROM contents in byte */
    uint16_t *new_eeprom_data; /**< new EEPROM data to write */
    uint16_t new_eeprom_size; /**< size of new EEPROM data in words */

    // slave information interface
    uint16_t sii_alias; /**< configured station alias */
    uint32_t sii_vendor_id; /**< vendor id */
    uint32_t sii_product_code; /**< vendor's product code */
    uint32_t sii_revision_number; /**< revision number */
    uint32_t sii_serial_number; /**< serial number */
    uint16_t sii_rx_mailbox_offset; /**< mailbox address (master to slave) */
    uint16_t sii_rx_mailbox_size; /**< mailbox size (master to slave) */
    uint16_t sii_tx_mailbox_offset; /**< mailbox address (slave to master) */
    uint16_t sii_tx_mailbox_size; /**< mailbox size (slave to master) */
    uint16_t sii_mailbox_protocols; /**< supported mailbox protocols */
    uint8_t sii_physical_layer[4]; /**< port media */
    struct list_head sii_strings; /**< EEPROM STRING categories */
    struct list_head sii_syncs; /**< EEPROM SYNC MANAGER categories */
    struct list_head sii_pdos; /**< EEPROM [RT]XPDO categories */
    char *sii_group; /**< slave group acc. to EEPROM */
    char *sii_image; /**< slave image name acc. to EEPROM */
    char *sii_order; /**< slave order number acc. to EEPROM */
    char *sii_name; /**< slave name acc. to EEPROM */

    ec_fmmu_t fmmus[EC_MAX_FMMUS]; /**< FMMU configurations */
    uint8_t fmmu_count; /**< number of FMMUs used */

    struct list_head sdo_dictionary; /**< SDO directory list */
    struct list_head sdo_confs; /**< list of SDO configurations */

    struct list_head varsize_fields; /**< size information for variable-sized
                                        data fields. */
};

/*****************************************************************************/

// slave construction/destruction
int ec_slave_init(ec_slave_t *, ec_master_t *, uint16_t, uint16_t);
void ec_slave_clear(struct kobject *);

int ec_slave_prepare_fmmu(ec_slave_t *, const ec_domain_t *,
                          const ec_sii_sync_t *);

// CoE
//int ec_slave_fetch_sdo_list(ec_slave_t *);

// SII categories
int ec_slave_fetch_strings(ec_slave_t *, const uint8_t *);
void ec_slave_fetch_general(ec_slave_t *, const uint8_t *);
int ec_slave_fetch_sync(ec_slave_t *, const uint8_t *, size_t);
int ec_slave_fetch_pdo(ec_slave_t *, const uint8_t *, size_t,
                       ec_sii_pdo_type_t);
int ec_slave_locate_string(ec_slave_t *, unsigned int, char **);

// misc.
uint16_t ec_slave_calc_sync_size(const ec_slave_t *,
                                 const ec_sii_sync_t *);

int ec_slave_is_coupler(const ec_slave_t *);

/*****************************************************************************/

#endif
