/******************************************************************************
 *
 *  c o m m a n d . h
 *
 *  Struktur für ein EtherCAT-Kommando.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_COMMAND_H_
#define _EC_COMMAND_H_

#include "globals.h"

/*****************************************************************************/

/**
   Status eines EtherCAT-Kommandos.
*/

typedef enum
{
  EC_COMMAND_STATE_READY, EC_COMMAND_STATE_SENT, EC_COMMAND_STATE_RECEIVED
}
ec_command_state_t;

/*****************************************************************************/

/**
   EtherCAT-Adresse.

   Im EtherCAT-Rahmen sind 4 Bytes für die Adresse reserviert, die
   ja nach Kommandoty eine andere bedeutung haben: Bei Autoinkrement-
   befehlen sind die ersten zwei Bytes die (negative)
   Autoinkrement-Adresse, bei Knoten-adressierten Befehlen entsprechen
   sie der Knotenadresse. Das dritte und vierte Byte entspricht in
   diesen Fällen der physikalischen Speicheradresse auf dem Slave.
   Bei einer logischen Adressierung entsprechen alle vier Bytes
   der logischen Adresse.
*/

typedef union
{
  struct
  {
    union
    {
      short pos; /**< (Negative) Ring-Position des Slaves */
      unsigned short node; /**< Konfigurierte Knotenadresse */
    }
    dev;

    unsigned short mem; /**< Physikalische Speicheradresse im Slave */
  }
  phy;

  unsigned long logical; /**< Logische Adresse */
  unsigned char raw[4]; /**< Rohdaten für die Generierung des Frames */
}
ec_address_t;

/*****************************************************************************/

/**
   EtherCAT-Kommando.
*/

typedef struct ec_command
{
  ec_command_type_t type; /**< Typ des Kommandos (APRD, NPWR, etc...) */
  ec_address_t address; /**< Adresse des/der Empfänger */
  unsigned int data_length; /**< Länge der zu sendenden und/oder
                               empfangenen Daten */
  ec_command_state_t state; /**< Zustand des Kommandos
                           (bereit, gesendet, etc...) */
  unsigned char index; /**< Kommando-Index, mit der das Kommando gesendet
                          wurde (wird vom Master beim Senden gesetzt. */
  unsigned int working_counter; /**< Working-Counter bei Empfang (wird
                                   vom Master gesetzt) */
  unsigned char data[EC_FRAME_SIZE]; /**< Kommandodaten */
}
ec_command_t;

/*****************************************************************************/

void ec_command_read(ec_command_t *, unsigned short, unsigned short,
                     unsigned int);
void ec_command_write(ec_command_t *, unsigned short, unsigned short,
                      unsigned int, const unsigned char *);
void ec_command_position_read(ec_command_t *, short, unsigned short,
                              unsigned int);
void ec_command_position_write(ec_command_t *, short, unsigned short,
                               unsigned int, const unsigned char *);
void ec_command_broadcast_read(ec_command_t *, unsigned short, unsigned int);
void ec_command_broadcast_write(ec_command_t *, unsigned short, unsigned int,
                                const unsigned char *);
void ec_command_logical_read_write(ec_command_t *, unsigned int, unsigned int,
                                   unsigned char *);

/*****************************************************************************/

#endif
