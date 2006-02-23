/******************************************************************************
 *
 *  t y p e s . h
 *
 *  EtherCAT-Slave-Typen.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_TYPES_H_
#define _EC_TYPES_H_

#include <linux/types.h>

#include "../include/EtherCAT_rt.h"

/*****************************************************************************/

#define EC_MAX_FIELDS 10
#define EC_MAX_SYNC   16

/*****************************************************************************/

/**
   Features eines EtherCAT-Slaves.

   Diese Angabe muss für die Konfiguration bekannt sein. Der
   Master entscheidet danach, ober bspw. Mailboxes konfigurieren,
   oder Sync-Manager setzen soll.
*/

typedef enum
{
    EC_SIMPLE_SLAVE, EC_MAILBOX_SLAVE, EC_NOSYNC_SLAVE
}
ec_slave_features_t;

/*****************************************************************************/

/**
   Prozessdatenfeld.
*/

typedef struct
{
    ec_field_type_t type;
    unsigned int size;
}
ec_field_t;

/*****************************************************************************/

/**
   Sync-Manager.
*/

typedef struct
{
    uint16_t physical_start_address;
    uint16_t size;
    uint8_t control_byte;
    const ec_field_t *fields[EC_MAX_FIELDS];
}
ec_sync_t;

/*****************************************************************************/

/**
   Beschreibung eines EtherCAT-Slave-Typs.

   Diese Beschreibung dient zur Konfiguration einer bestimmten
   Slave-Art. Sie enthält die Konfigurationsdaten für die
   Slave-internen Sync-Manager und FMMU's.
*/

typedef struct ec_slave_type
{
    const char *vendor_name; /**< Name des Herstellers */
    const char *product_name; /**< Name des Slaves-Typs */
    const char *description; /**< Genauere Beschreibung des Slave-Typs */
    ec_slave_features_t features; /**< Features des Slave-Typs */
    const ec_sync_t *sync_managers[EC_MAX_SYNC]; /**< Sync-Manager
                                                    Konfigurationen */
}
ec_slave_type_t;

/*****************************************************************************/

/**
   Identifikation eines Slave-Typs.

   Diese Struktur wird zur 1:n-Zuordnung von Hersteller- und
   Produktcodes zu den einzelnen Slave-Typen verwendet.
*/

typedef struct slave_ident
{
    unsigned int vendor_id; /**< Hersteller-Code */
    unsigned int product_code; /**< Herstellerspezifischer Produktcode */
    const ec_slave_type_t *type; /**< Zeiger auf den entsprechenden Typ */
}
ec_slave_ident_t;

extern ec_slave_ident_t slave_idents[]; /**< Statisches Array der
                                           Slave-Identifikationen */

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
