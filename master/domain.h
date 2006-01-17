/******************************************************************************
 *
 *  d o m a i n . h
 *
 *  Struktur für eine Gruppe von EtherCAT-Slaves.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_DOMAIN_H_
#define _EC_DOMAIN_H_

#include "globals.h"
#include "slave.h"
#include "command.h"

/*****************************************************************************/

/**
   EtherCAT-Domäne

   Verwaltet die Prozessdaten und das hierfür nötige Kommando einer bestimmten
   Menge von Slaves.
*/

typedef struct ec_domain
{
  unsigned int number; /*<< Domänen-Identifikation */
  ec_command_t command; /**< Kommando zum Senden und Empfangen der
                           Prozessdaten */
  unsigned char data[EC_FRAME_SIZE]; /**< Prozessdaten-Array */
  unsigned int data_size; /**< Größe der Prozessdaten */
  unsigned int logical_offset; /**< Logische Basisaddresse */
  unsigned int response_count; /**< Anzahl antwortender Slaves */
}
ec_domain_t;

/*****************************************************************************/

void ec_domain_init(ec_domain_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
