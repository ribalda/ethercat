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
                                  Ethernet-II-Header und -Prüfsumme */
#define EC_MIN_FRAME_SIZE 46 /** Minimale Größe, s. o. */
#define EC_FRAME_HEADER_SIZE 2 /**< Größe des EtherCAT-Frame-Headers */
#define EC_COMMAND_HEADER_SIZE 10 /**< Größe eines EtherCAT-Kommando-Headers */
#define EC_COMMAND_FOOTER_SIZE 2 /**< Größe eines EtherCAT-Kommando-Footers */
#define EC_SYNC_SIZE 8 /**< Größe einer Sync-Manager-Konfigurationsseite */
#define EC_FMMU_SIZE 16 /**< Größe einer FMMU-Konfigurationsseite */
#define EC_MAX_FMMUS 16 /**< Maximale Anzahl FMMUs pro Slave */
#define EC_MAX_DATA_SIZE (EC_MAX_FRAME_SIZE \
                          - EC_FRAME_HEADER_SIZE \
                          - EC_COMMAND_HEADER_SIZE \
                          - EC_COMMAND_FOOTER_SIZE) /**< Maximale Datengröße */

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

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
