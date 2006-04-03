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

#include "../include/ecrt.h"

/*****************************************************************************/

#define EC_MAX_FIELDS 10
#define EC_MAX_SYNC   16

/*****************************************************************************/

/**
   Besondere Slaves.
*/

typedef enum
{
    EC_TYPE_NORMAL,
    EC_TYPE_BUS_COUPLER,
    EC_TYPE_EOE
}
ec_special_type_t;

/*****************************************************************************/

/**
   Prozessdatenfeld.
*/

typedef struct
{
    const char *name;
    size_t size;
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
   Slave-internen Sync-Manager und FMMUs.
*/

typedef struct ec_slave_type
{
    const char *vendor_name; /**< Name des Herstellers */
    const char *product_name; /**< Name des Slaves-Typs */
    const char *description; /**< Genauere Beschreibung des Slave-Typs */
    ec_special_type_t special; /**< Spezieller Slave-Typ */
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
    uint32_t vendor_id; /**< Hersteller-Code */
    uint32_t product_code; /**< Herstellerspezifischer Produktcode */
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
