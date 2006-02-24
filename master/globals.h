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

// EtherCAT-Protokoll
#define EC_MAX_FRAME_SIZE 1500 /**< Maximale Größe eines EtherCAT-Frames ohne
                                  Ethernet-II-Header und -Prüfsumme*/
#define EC_MIN_FRAME_SIZE 46 /** Minimale Größe, s. o. */
#define EC_FRAME_HEADER_SIZE 2 /**< Größe des EtherCAT-Frame-Headers */
#define EC_COMMAND_HEADER_SIZE 10 /**< Größe eines EtherCAT-Kommando-Headers */
#define EC_COMMAND_FOOTER_SIZE 2 /**< Größe eines EtherCAT-Kommando-Footers */
#define EC_SYNC_SIZE 8 /**< Größe einer Sync-Manager-Konfigurationsseite */
#define EC_FMMU_SIZE 16 /**< Größe einer FMMU-Konfigurationsseite */
#define EC_MAX_FMMUS 16 /**< Maximale Anzahl FMMUs pro Slave */

#define EC_MASTER_MAX_DOMAINS 10 /**< Maximale Anzahl Domänen eines Masters */

#ifndef NULL
#define NULL ((void *) 0) /**< NULL-Define, falls noch nicht definiert. */
#endif

#define EC_INFO(fmt, args...) \
    printk(KERN_INFO "EtherCAT: " fmt, ##args)
#define EC_ERR(fmt, args...) \
    printk(KERN_ERR "EtherCAT ERROR: " fmt, ##args)
#define EC_WARN(fmt, args...) \
    printk(KERN_WARNING "EtherCAT WARNING: " fmt, ##args)
#define EC_DBG(fmt, args...) \
    printk(KERN_DEBUG "EtherCAT DEBUG: " fmt, ##args)

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
    EC_ACK = 0x10                  /**< Acknoledge-Bit beim Zustandswechsel
                                      (dies ist kein eigener Zustand) */
}
ec_slave_state_t;

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
