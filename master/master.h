/******************************************************************************
 *
 *  m a s t e r . h
 *
 *  Struktur für einen EtherCAT-Master.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_MASTER_H_
#define _EC_MASTER_H_

#include "device.h"
#include "slave.h"
#include "command.h"
#include "domain.h"

/*****************************************************************************/

/**
   EtherCAT-Master

   Verwaltet die EtherCAT-Slaves und kommuniziert mit
   dem zugewiesenen EtherCAT-Gerät.
*/

typedef struct ec_master
{
  ec_slave_t *bus_slaves; /**< Array von Slaves auf dem Bus */
  unsigned int bus_slaves_count; /**< Anzahl Slaves auf dem Bus */
  ec_device_t device; /**< EtherCAT-Gerät */
  unsigned int device_registered; /**< Ein Geraet hat sich registriert. */
  unsigned char command_index; /**< Aktueller Kommando-Index */
  unsigned char tx_data[EC_FRAME_SIZE]; /**< Statischer Speicher
                                           für zu sendende Daten */
  unsigned int tx_data_length; /**< Länge der Daten im Sendespeicher */
  unsigned char rx_data[EC_FRAME_SIZE]; /**< Statische Speicher für
                                           eine Kopie des Rx-Buffers
                                           im EtherCAT-Gerät */
  unsigned int rx_data_length; /**< Länge der Daten im Empfangsspeicher */
  ec_domain_t domains[EC_MAX_DOMAINS]; /** Prozessdatendomänen */
  unsigned int domain_count;
  int debug_level; /**< Debug-Level im Master-Code */
  unsigned int bus_time; /**< Letzte Bus-Zeit in Mikrosekunden */
  unsigned int frames_lost; /**< Anzahl verlorene Frames */
  unsigned long t_lost_output; /*<< Timer-Ticks bei der letzten Ausgabe von
                                 verlorenen Frames */
}
ec_master_t;

/*****************************************************************************/

// Private Methods

// Master creation and deletion
void ec_master_init(ec_master_t *);
void ec_master_clear(ec_master_t *);

// Registration of devices
int ec_master_open(ec_master_t *);
void ec_master_close(ec_master_t *);

// Slave management
int ec_scan_for_slaves(ec_master_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
