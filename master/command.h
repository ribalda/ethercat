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

#include <linux/list.h>

#include "globals.h"

/*****************************************************************************/

/**
   EtherCAT-Kommando-Typ.
*/

typedef enum
{
    EC_CMD_NONE = 0x00, /**< Dummy */
    EC_CMD_APRD = 0x01, /**< Auto-increment physical read */
    EC_CMD_APWR = 0x02, /**< Auto-increment physical write */
    EC_CMD_NPRD = 0x04, /**< Node-addressed physical read */
    EC_CMD_NPWR = 0x05, /**< Node-addressed physical write */
    EC_CMD_BRD  = 0x07, /**< Broadcast read */
    EC_CMD_BWR  = 0x08, /**< Broadcast write */
    EC_CMD_LRW  = 0x0C  /**< Logical read/write */
}
ec_command_type_t;

/**
   EtherCAT-Kommando-Zustand.
*/

typedef enum
{
    EC_CMD_INIT, /**< Neues Kommando */
    EC_CMD_QUEUED, /**< Kommando in Warteschlange */
    EC_CMD_SENT, /**< Kommando gesendet */
    EC_CMD_RECEIVED, /**< Kommando empfangen */
    EC_CMD_TIMEOUT, /**< Zeitgrenze überschritten */
    EC_CMD_ERROR /**< Fehler beim Senden oder Empfangen */
}
ec_command_state_t;

/*****************************************************************************/

/**
   EtherCAT-Adresse.

   Im EtherCAT-Kommando sind 4 Bytes für die Adresse reserviert, die je nach
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
        uint16_t slave; /**< Adresse des Slaves (Ringposition oder Knoten) */
        uint16_t mem; /**< Physikalische Speicheradresse im Slave */
    }
    physical; /**< Physikalische Adresse */

    uint32_t logical; /**< Logische Adresse */
}
ec_address_t;

/*****************************************************************************/

/**
   EtherCAT-Kommando.
*/

typedef struct
{
    struct list_head list; /**< Kommando-Listeneintrag */
    struct list_head queue; /**< Master-Kommando-Queue */
    ec_command_type_t type; /**< Typ des Kommandos (APRD, NPWR, etc) */
    ec_address_t address; /**< Adresse des/der Empfänger */
    uint8_t *data; /**< Kommandodaten */
    size_t mem_size; /**< Größe des Speichers */
    size_t data_size; /**< Länge der zu sendenden und/oder empfangenen Daten */
    uint8_t index; /**< Kommando-Index, wird vom Master beim Senden gesetzt. */
    uint16_t working_counter; /**< Working-Counter */
    ec_command_state_t state; /**< Zustand */
}
ec_command_t;

/*****************************************************************************/

void ec_command_init(ec_command_t *);
void ec_command_clear(ec_command_t *);

int ec_command_nprd(ec_command_t *, uint16_t, uint16_t, size_t);
int ec_command_npwr(ec_command_t *, uint16_t, uint16_t, size_t);
int ec_command_aprd(ec_command_t *, uint16_t, uint16_t, size_t);
int ec_command_apwr(ec_command_t *, uint16_t, uint16_t, size_t);
int ec_command_brd(ec_command_t *, uint16_t, size_t);
int ec_command_bwr(ec_command_t *, uint16_t, size_t);
int ec_command_lrw(ec_command_t *, uint32_t, size_t);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
