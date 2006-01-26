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

#include "../include/EtherCAT_rt.h"

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
   Beschreibung eines EtherCAT-Slave-Typs.

   Diese Beschreibung dient zur Konfiguration einer bestimmten
   Slave-Art. Sie enthält die Konfigurationsdaten für die
   Slave-internen Sync-Manager und FMMU's.
*/

struct ec_slave_type
{
    const char *vendor_name; /**< Name des Herstellers */
    const char *product_name; /**< Name des Slaves-Typs */
    const char *product_desc; /**< Genauere Beschreibung des Slave-Typs */

    ec_slave_features_t features; /**< Features des Slave-Typs */

    const unsigned char *sm0; /**< Konfigurationsdaten des
                                 ersten Sync-Managers */
    const unsigned char *sm1; /**< Konfigurationsdaten des
                                 zweiten Sync-Managers */
    const unsigned char *sm2; /**< Konfigurationsdaten des
                                 dritten Sync-Managers */
    const unsigned char *sm3; /**< Konfigurationsdaten des
                                 vierten Sync-Managers */

    const unsigned char *fmmu0; /**< Konfigurationsdaten
                                   der ersten FMMU */

    unsigned int process_data_size; /**< Länge der Prozessdaten in Bytes */
};

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
extern unsigned int slave_ident_count; /**< Anzahl der vorhandenen
                                          Slave-Identifikationen */

/*****************************************************************************/

#endif
