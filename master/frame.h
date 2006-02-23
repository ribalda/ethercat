/******************************************************************************
 *
 *  f r a m e . h
 *
 *  Struktur für einen EtherCAT-Rahmen.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_FRAME_H_
#define _EC_FRAME_H_

#include "globals.h"
#include "../include/EtherCAT_rt.h"

/*****************************************************************************/

#define EC_MAX_DATA_SIZE (EC_MAX_FRAME_SIZE - EC_FRAME_HEADER_SIZE \
                                            - EC_COMMAND_HEADER_SIZE \
                                            - EC_COMMAND_FOOTER_SIZE)

/*****************************************************************************/

/**
   Status eines EtherCAT-Rahmens.
*/

typedef enum {
  ec_frame_ready, ec_frame_sent, ec_frame_received
}
ec_frame_state_t;

/*****************************************************************************/

/**
   EtherCAT-Rahmen-Typ
*/

typedef enum
{
  ec_frame_type_none = 0x00, /**< Dummy */
  ec_frame_type_aprd = 0x01, /**< Auto-increment physical read */
  ec_frame_type_apwr = 0x02, /**< Auto-increment physical write */
  ec_frame_type_nprd = 0x04, /**< Node-addressed physical read */
  ec_frame_type_npwr = 0x05, /**< Node-addressed physical write */
  ec_frame_type_brd = 0x07,  /**< Broadcast read */
  ec_frame_type_bwr = 0x08,  /**< Broadcast write */
  ec_frame_type_lrw = 0x0C   /**< Logical read/write */
}
ec_frame_type_t;

/*****************************************************************************/

/**
   EtherCAT-Adresse.

   Im EtherCAT-Rahmen sind 4 Bytes für die Adresse reserviert, die je nach
   Kommandotyp, eine andere Bedeutung haben können: Bei Autoinkrementbefehlen
   sind die ersten zwei Bytes die (negative) Autoinkrement-Adresse, bei Knoten-
   adressierten Befehlen entsprechen sie der Knotenadresse. Das dritte und
   vierte Byte entspricht in diesen Fällen der physikalischen Speicheradresse
   auf dem Slave. Bei einer logischen Adressierung entsprechen alle vier Bytes
   der logischen Adresse.
*/

typedef union
{
  struct
  {
      uint16_t slave; /**< Adresse des Slaves */
      uint16_t mem; /**< Physikalische Speicheradresse im Slave */
  }
  physical; /**< Physikalische Adresse */

  uint32_t logical; /**< Logische Adresse */
  uint8_t raw[4]; /**< Rohdaten für die Generierung des Frames */
}
ec_address_t;

/*****************************************************************************/

/**
   EtherCAT-Frame.
*/

typedef struct
{
    ec_master_t *master; /**< EtherCAT-Master */
    ec_frame_type_t type; /**< Typ des Frames (APRD, NPWR, etc) */
    ec_address_t address; /**< Adresse des/der Empfänger */
    unsigned int data_length; /**< Länge der zu sendenden und/oder empfangenen
                                 Daten */
    ec_frame_state_t state; /**< Zustand des Kommandos */
    uint8_t index; /**< Kommando-Index, mit dem der Frame gesendet wurde
                            (wird vom Master beim Senden gesetzt). */
    uint16_t working_counter; /**< Working-Counter */
    uint8_t data[EC_MAX_FRAME_SIZE]; /**< Rahmendaten */
}
ec_frame_t;

/*****************************************************************************/

void ec_frame_init_nprd(ec_frame_t *, ec_master_t *, uint16_t, uint16_t,
                        unsigned int);
void ec_frame_init_npwr(ec_frame_t *, ec_master_t *, uint16_t, uint16_t,
                        unsigned int, const unsigned char *);
void ec_frame_init_aprd(ec_frame_t *, ec_master_t *, uint16_t, uint16_t,
                        unsigned int);
void ec_frame_init_apwr(ec_frame_t *, ec_master_t *, uint16_t, uint16_t,
                        unsigned int, const unsigned char *);
void ec_frame_init_brd(ec_frame_t *, ec_master_t *, uint16_t, unsigned int);
void ec_frame_init_bwr(ec_frame_t *, ec_master_t *, uint16_t, unsigned int,
                       const unsigned char *);
void ec_frame_init_lrw(ec_frame_t *, ec_master_t *, uint32_t, unsigned int,
                       unsigned char *);

int ec_frame_send(ec_frame_t *);
int ec_frame_receive(ec_frame_t *);
int ec_frame_send_receive(ec_frame_t *);

void ec_frame_print(const ec_frame_t *);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
