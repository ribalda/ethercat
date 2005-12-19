/******************************************************************************
 *
 *  e c _ g l o b a l s . h
 *
 *  Globale Definitionen und Makros für das EtherCAT-Protokoll.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_GLOBALS_
#define _EC_GLOBALS_

/**
   Maximale Größe eines EtherCAT-Frames
*/
#define ECAT_FRAME_BUFFER_SIZE 1500

/**
   NULL-Define, falls noch nicht definiert.
*/

#ifndef NULL
#define NULL ((void *) 0)
#endif

/**
   EtherCAT-Kommando-Typ
*/

typedef enum
{
  ECAT_CMD_NONE = 0x00, /**< Dummy */
  ECAT_CMD_APRD = 0x01, /**< Auto-increment physical read */
  ECAT_CMD_APWR = 0x02, /**< Auto-increment physical write */
  ECAT_CMD_NPRD = 0x04, /**< Node-addressed physical read */
  ECAT_CMD_NPWR = 0x05, /**< Node-addressed physical write */
  ECAT_CMD_BRD = 0x07,  /**< Broadcast read */
  ECAT_CMD_BWR = 0x08,  /**< Broadcast write */
  ECAT_CMD_LRW = 0x0C   /**< Logical read/write */
}
EtherCAT_cmd_type_t;

/**
   Zustand eines EtherCAT-Slaves
*/

typedef enum
{
  ECAT_STATE_UNKNOWN = 0x00, /**< Status unbekannt */
  ECAT_STATE_INIT = 0x01,    /**< Init-Zustand (Keine Mailbox-
                                Kommunikation, Kein I/O) */
  ECAT_STATE_PREOP = 0x02,   /**< Pre-Operational (Mailbox-
                                Kommunikation, Kein I/O) */
  ECAT_STATE_SAVEOP = 0x04,  /**< Save-Operational (Mailbox-
                                Kommunikation und Input Update) */
  ECAT_STATE_OP = 0x08,      /**< Operational, (Mailbox-
                                Kommunikation und Input/Output Update) */
  ECAT_ACK_STATE = 0x10      /**< Acknoledge-Bit beim Zustandswechsel
                                (dies ist kein eigener Zustand) */
}
EtherCAT_state_t;

/*****************************************************************************/

#endif
