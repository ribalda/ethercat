/******************************************************************************
 *
 *  e c _ c o m m a n d . h
 *
 *  Struktur für ein EtherCAT-Kommando.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_COMMAND_H_
#define _EC_COMMAND_H_

#include "ec_globals.h"

/*****************************************************************************/

/**
   Status eines EtherCAT-Kommandos.
*/

typedef enum
{
  ECAT_CS_READY, ECAT_CS_SENT, ECAT_CS_RECEIVED
}
EtherCAT_command_state_t;

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
EtherCAT_address_t;

/*****************************************************************************/

/**
   EtherCAT-Kommando.
*/

typedef struct EtherCAT_command
{
  EtherCAT_cmd_type_t type; /**< Typ des Kommandos (APRD, NPWR, etc...) */
  EtherCAT_address_t address; /**< Adresse des/der Empfänger */
  unsigned int data_length; /**< Länge der zu sendenden und/oder
                               empfangenen Daten */

  EtherCAT_command_state_t state; /**< Zustand des Kommandos
                                     (bereit, gesendet, etc...) */
  unsigned char index; /**< Kommando-Index, mit der das Kommando gesendet
                          wurde (wird vom Master beim Senden gesetzt. */
  unsigned int working_counter; /**< Working-Counter bei Empfang (wird
                                   vom Master gesetzt) */

  unsigned char data[ECAT_FRAME_BUFFER_SIZE]; /**< Kommandodaten */
}
EtherCAT_command_t;

/*****************************************************************************/

void EtherCAT_command_init(EtherCAT_command_t *);
void EtherCAT_command_clear(EtherCAT_command_t *);

void EtherCAT_command_read(EtherCAT_command_t *,
                           unsigned short,
                           unsigned short,
                           unsigned int);
void EtherCAT_command_write(EtherCAT_command_t *,
                            unsigned short,
                            unsigned short,
                            unsigned int,
                            const unsigned char *);
void EtherCAT_command_position_read(EtherCAT_command_t *,
                                    short,
                                    unsigned short,
                                    unsigned int);
void EtherCAT_command_position_write(EtherCAT_command_t *,
                                     short,
                                     unsigned short,
                                     unsigned int,
                                     const unsigned char *);
void EtherCAT_command_broadcast_read(EtherCAT_command_t *,
                                     unsigned short,
                                     unsigned int);
void EtherCAT_command_broadcast_write(EtherCAT_command_t *,
                                      unsigned short,
                                      unsigned int,
                                      const unsigned char *);
void EtherCAT_command_logical_read_write(EtherCAT_command_t *,
                                         unsigned int,
                                         unsigned int,
                                         unsigned char *);

/*****************************************************************************/

#endif
