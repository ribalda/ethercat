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

#include "types.h"

/*****************************************************************************/

/**
   EtherCAT-Slave

   Achtung: Bei Änderungen dieser Struktur immer das Define
   ECAT_INIT_SLAVE anpassen!
*/

typedef struct
{
  // Base data
  unsigned char type; /**< Slave-Typ */
  unsigned char revision; /**< Revision */
  unsigned short build; /**< Build-Nummer */

  // Addresses
  short ring_position; /**< (Negative) Position des Slaves im Bus */
  unsigned short station_address; /**< Konfigurierte Slave-Adresse */

  // Slave information interface
  unsigned int vendor_id; /**< Identifikationsnummer des Herstellers */
  unsigned int product_code; /**< Herstellerspezifischer Produktcode */
  unsigned int revision_number; /**< Revisionsnummer */
  unsigned int serial_number; /**< Seriennummer der Klemme */

  const ec_slave_desc_t *desc; /**< Zeiger auf die Beschreibung
                                        des Slave-Typs */

  unsigned int logical_address; /**< Konfigurierte, logische adresse */

  ec_slave_state_t current_state; /**< Aktueller Zustand */
  ec_slave_state_t requested_state; /**< Angeforderter Zustand */

  unsigned char *process_data; /**< Zeiger auf den Speicherbereich
                                  innerhalb eines Prozessdatenobjekts */
  unsigned int domain; /**< Prozessdatendomäne */
  int error_reported; /**< Ein Zugriffsfehler wurde bereits gemeldet */
}
ec_slave_t;

#define EC_INIT_SLAVE(TYPE, DOMAIN) {0, 0, 0, 0, 0, 0, 0, 0, 0, \
                                       TYPE, 0, ECAT_STATE_UNKNOWN, \
                                       EC_STATE_UNKNOWN, NULL, DOMAIN, 0}

/*****************************************************************************/

// Slave construction and deletion
void ec_slave_init(ec_slave_t *);

#if 0
int EtherCAT_read_value(EtherCAT_slave_t *, unsigned int);
void EtherCAT_write_value(EtherCAT_slave_t *, unsigned int, int);
#endif

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
