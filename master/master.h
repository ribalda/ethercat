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
#include "frame.h"
#include "domain.h"

/*****************************************************************************/

/**
   EtherCAT-Master

   Verwaltet die EtherCAT-Slaves und kommuniziert mit
   dem zugewiesenen EtherCAT-Gerät.
*/

struct ec_master
{
    ec_slave_t *slaves; /**< Array von Slaves auf dem Bus */
    unsigned int slave_count; /**< Anzahl Slaves auf dem Bus */
    ec_device_t device; /**< EtherCAT-Gerät */
    unsigned int device_registered; /**< Ein Geraet hat sich registriert. */
    uint8_t command_index; /**< Aktueller Kommando-Index */
    ec_domain_t *domains[EC_MASTER_MAX_DOMAINS]; /** Prozessdatendomänen */
    unsigned int domain_count;
    int debug_level; /**< Debug-Level im Master-Code */
    unsigned int bus_time; /**< Letzte Bus-Zeit in Mikrosekunden */
    unsigned int frames_lost; /**< Anzahl verlorene Frames */
    unsigned long t_lost_output; /*<< Timer-Ticks bei der letzten Ausgabe von
                                   verlorenen Frames */
};

/*****************************************************************************/

// Master creation and deletion
void ec_master_init(ec_master_t *);
void ec_master_clear(ec_master_t *);
void ec_master_reset(ec_master_t *);

// Registration of devices
int ec_master_open(ec_master_t *);
void ec_master_close(ec_master_t *);

// Slave management
int ec_scan_for_slaves(ec_master_t *);
ec_slave_t *ec_address(const ec_master_t *, const char *);

// Misc
void ec_output_debug_data(const ec_master_t *);
void ec_output_lost_frames(ec_master_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
