/******************************************************************************
 *
 *  e c _ d o m a i n . h
 *
 *  Struktur für eine Gruppe von EtherCAT-Slaves.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_DOMAIN_H_
#define _EC_DOMAIN_H_

#include "ec_globals.h"
#include "ec_slave.h"
#include "ec_command.h"

/*****************************************************************************/

/**
   EtherCAT-Domäne

   Verwaltet die Prozessdaten und das hierfür nötige Kommando einer bestimmten
   Menge von Slaves.
*/

typedef struct EtherCAT_domain
{
  unsigned int number; /*<< Domänen-Identifikation */
  EtherCAT_command_t command; /**< Kommando zum Senden und Empfangen der
                                 Prozessdaten */
  unsigned char data[ECAT_FRAME_BUFFER_SIZE]; /**< Prozessdaten-Array */
  unsigned int data_size; /**< Größe der Prozessdaten */
  unsigned int logical_offset; /**< Logische Basisaddresse */
  unsigned int response_count; /**< Anzahl antwortender Slaves */
}
EtherCAT_domain_t;

/*****************************************************************************/

void EtherCAT_domain_init(EtherCAT_domain_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
