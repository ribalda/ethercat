/******************************************************************************
 *
 *  g l o b a l s . h
 *
 *  Globale Definitionen und Makros für das EtherCAT-Protokoll.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_GLOBALS_
#define _EC_GLOBALS_

/*****************************************************************************/

/**
   Maximale Größe eines EtherCAT-Frames
*/
#define EC_FRAME_SIZE 1500

/**
   Maximale Anzahl der Prozessdatendomänen in einem Master
*/
#define EC_MAX_DOMAINS 10

/**
   NULL-Define, falls noch nicht definiert.
*/

#ifndef NULL
#define NULL ((void *) 0)
#endif

/*****************************************************************************/

/**
   EtherCAT-Kommando-Typ
*/

typedef enum
{
  EC_COMMAND_NONE = 0x00, /**< Dummy */
  EC_COMMAND_APRD = 0x01, /**< Auto-increment physical read */
  EC_COMMAND_APWR = 0x02, /**< Auto-increment physical write */
  EC_COMMAND_NPRD = 0x04, /**< Node-addressed physical read */
  EC_COMMAND_NPWR = 0x05, /**< Node-addressed physical write */
  EC_COMMAND_BRD = 0x07,  /**< Broadcast read */
  EC_COMMAND_BWR = 0x08,  /**< Broadcast write */
  EC_COMMAND_LRW = 0x0C   /**< Logical read/write */
}
ec_command_type_t;

/*****************************************************************************/

/**
   Zustand eines EtherCAT-Slaves
*/

typedef enum
{
  EC_SLAVE_STATE_UNKNOWN = 0x00, /**< Status unbekannt */
  EC_SLAVE_STATE_INIT = 0x01,    /**< Init-Zustand (Keine Mailbox-
                                    Kommunikation, Kein I/O) */
  EC_SLAVE_STATE_PREOP = 0x02,   /**< Pre-Operational (Mailbox-
                                    Kommunikation, Kein I/O) */
  EC_SLAVE_STATE_SAVEOP = 0x04,  /**< Save-Operational (Mailbox-
                                    Kommunikation und Input Update) */
  EC_SLAVE_STATE_OP = 0x08,      /**< Operational, (Mailbox-
                                    Kommunikation und Input/Output Update) */
  EC_ACK = 0x10      /**< Acknoledge-Bit beim Zustandswechsel
                        (dies ist kein eigener Zustand) */
}
ec_slave_state_t;

/*****************************************************************************/

#endif
