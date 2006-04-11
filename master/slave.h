/******************************************************************************
 *
 *  s l a v e . h
 *
 *  Struktur für einen EtherCAT-Slave.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_SLAVE_H_
#define _EC_SLAVE_H_

#include <linux/list.h>
#include <linux/kobject.h>

#include "globals.h"
#include "command.h"
#include "types.h"

/*****************************************************************************/

/**
   Zustand eines EtherCAT-Slaves.
*/

typedef enum
{
    EC_SLAVE_STATE_UNKNOWN = 0x00,
    /**< Status unbekannt */
    EC_SLAVE_STATE_INIT = 0x01,
    /**< Init-Zustand (Keine Mailbox-Kommunikation, Kein I/O) */
    EC_SLAVE_STATE_PREOP = 0x02,
    /**< Pre-Operational (Mailbox-Kommunikation, Kein I/O) */
    EC_SLAVE_STATE_SAVEOP = 0x04,
    /**< Save-Operational (Mailbox-Kommunikation und Input Update) */
    EC_SLAVE_STATE_OP = 0x08,
    /**< Operational, (Mailbox-Kommunikation und Input/Output Update) */
    EC_ACK = 0x10
    /**< Acknoledge-Bit beim Zustandswechsel (kein eigener Zustand) */
}
ec_slave_state_t;

/*****************************************************************************/

/**
   Unterstützte Mailbox-Protokolle.
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
   FMMU-Konfiguration.
*/

typedef struct
{
    const ec_domain_t *domain;
    const ec_sync_t *sync;
    uint32_t logical_start_address;
}
ec_fmmu_t;

/*****************************************************************************/

/**
   String im EEPROM eines EtherCAT-Slaves.
*/

typedef struct
{
    struct list_head list;
    size_t size;
    char *data;
}
ec_eeprom_string_t;

/*****************************************************************************/

/**
   Sync-Manager-Konfiguration laut EEPROM.
*/

typedef struct
{
    struct list_head list;
    unsigned int index;
    uint16_t physical_start_address;
    uint16_t length;
    uint8_t control_register;
    uint8_t enable;
}
ec_eeprom_sync_t;

/*****************************************************************************/

/**
   PDO-Typ.
*/

typedef enum
{
    EC_RX_PDO,
    EC_TX_PDO
}
ec_pdo_type_t;

/*****************************************************************************/

/**
   PDO-Beschreibung im EEPROM.
*/

typedef struct
{
    struct list_head list;
    ec_pdo_type_t type;
    uint16_t index;
    uint8_t sync_manager;
    char *name;
    struct list_head entries;
}
ec_eeprom_pdo_t;

/*****************************************************************************/

/**
   PDO-Entry-Beschreibung im EEPROM.
*/

typedef struct
{
    struct list_head list;
    uint16_t index;
    uint8_t subindex;
    char *name;
    uint8_t bit_length;
}
ec_eeprom_pdo_entry_t;

/*****************************************************************************/

/**
   CANopen-SDO.
*/

typedef struct
{
    struct list_head list;
    uint16_t index;
    //uint16_t type;
    uint8_t object_code;
    char *name;
    struct list_head entries;
}
ec_sdo_t;

/*****************************************************************************/

/**
   CANopen-SDO-Entry.
*/

typedef struct
{
    struct list_head list;
    uint8_t subindex;
    uint16_t data_type;
    uint16_t bit_length;
    char *name;
}
ec_sdo_entry_t;

/*****************************************************************************/

/**
   EtherCAT-Slave
*/

struct ec_slave
{
    struct list_head list; /**< Noetig fuer Slave-Liste im Master */
    struct kobject kobj; /**< Kernel-Object */
    ec_master_t *master; /**< EtherCAT-Master, zu dem der Slave gehört. */

    // Addresses
    uint16_t ring_position; /**< Position des Slaves im Bus */
    uint16_t station_address; /**< Konfigurierte Slave-Adresse */
    uint16_t buscoupler_index; /**< Letzter Buskoppler */
    uint16_t index_after_buscoupler; /**< Index hinter letztem Buskoppler */

    // Base data
    uint8_t base_type; /**< Slave-Typ */
    uint8_t base_revision; /**< Revision */
    uint16_t base_build; /**< Build-Nummer */
    uint16_t base_fmmu_count; /**< Anzahl unterstützter FMMUs */
    uint16_t base_sync_count; /**< Anzahl unterstützter Sync-Manager */

    // Data link status
    uint8_t dl_link[4]; /**< Verbindung erkannt */
    uint8_t dl_loop[4]; /**< Loop geschlossen */
    uint8_t dl_signal[4]; /**< Signal an RX-Seite erkannt */

    // Slave information interface
    uint16_t sii_alias; /**< Configured station alias */
    uint32_t sii_vendor_id; /**< Identifikationsnummer des Herstellers */
    uint32_t sii_product_code; /**< Herstellerspezifischer Produktcode */
    uint32_t sii_revision_number; /**< Revisionsnummer */
    uint32_t sii_serial_number; /**< Seriennummer der Klemme */
    uint16_t sii_rx_mailbox_offset; /**< Adresse der Mailbox (Master->Slave) */
    uint16_t sii_rx_mailbox_size; /**< Adresse der Mailbox (Master->Slave) */
    uint16_t sii_tx_mailbox_offset; /**< Adresse der Mailbox (Slave->Master) */
    uint16_t sii_tx_mailbox_size; /**< Adresse der Mailbox (Slave->Master) */
    uint16_t sii_mailbox_protocols; /**< Unterstützte Mailbox-Protokolle */

    const ec_slave_type_t *type; /**< Zeiger auf die Beschreibung
                                    des Slave-Typs */

    uint8_t registered; /**< Der Slave wurde registriert */

    ec_fmmu_t fmmus[EC_MAX_FMMUS]; /**< FMMU-Konfigurationen */
    uint8_t fmmu_count; /**< Wieviele FMMUs schon benutzt sind. */

    struct list_head eeprom_strings; /**< Strings im EEPROM */
    struct list_head eeprom_syncs; /**< Syncmanager-Konfigurationen (EEPROM) */
    struct list_head eeprom_pdos; /**< PDO-Beschreibungen im EEPROM */

    char *eeprom_name; /**< Slave-Name laut Hersteller */
    char *eeprom_group; /**< Slave-Beschreibung laut Hersteller */
    char *eeprom_desc; /**< Slave-Beschreibung laut Hersteller */

    struct list_head sdo_dictionary; /**< SDO-Verzeichnis des Slaves */

    ec_command_t mbox_command; /**< Kommando für Mailbox-Kommunikation */
};

/*****************************************************************************/

// Slave construction/destruction
int ec_slave_init(ec_slave_t *, ec_master_t *, uint16_t, uint16_t);
void ec_slave_clear(struct kobject *);

// Slave control
int ec_slave_fetch(ec_slave_t *);
int ec_slave_sii_read16(ec_slave_t *, uint16_t, uint16_t *);
int ec_slave_sii_read32(ec_slave_t *, uint16_t, uint32_t *);
int ec_slave_sii_write16(ec_slave_t *, uint16_t, uint16_t);
int ec_slave_state_change(ec_slave_t *, uint8_t);
int ec_slave_prepare_fmmu(ec_slave_t *, const ec_domain_t *,
                          const ec_sync_t *);

// CANopen over EtherCAT
int ec_slave_fetch_sdo_list(ec_slave_t *);

// Misc
void ec_slave_print(const ec_slave_t *, unsigned int);
int ec_slave_check_crc(ec_slave_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
