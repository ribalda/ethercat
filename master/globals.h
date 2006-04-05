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

#include <linux/types.h>

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
                          - EC_COMMAND_FOOTER_SIZE) /**< Maximale Datengröße
                                                       bei einem Kommando pro
                                                       Frame */

/*****************************************************************************/

#define EC_INFO(fmt, args...) \
    printk(KERN_INFO "EtherCAT: " fmt, ##args)
#define EC_ERR(fmt, args...) \
    printk(KERN_ERR "EtherCAT ERROR: " fmt, ##args)
#define EC_WARN(fmt, args...) \
    printk(KERN_WARNING "EtherCAT WARNING: " fmt, ##args)
#define EC_DBG(fmt, args...) \
    printk(KERN_DEBUG "EtherCAT DEBUG: " fmt, ##args)

/*****************************************************************************/

extern void ec_print_data(const uint8_t *, size_t);
extern void ec_print_data_diff(const uint8_t *, const uint8_t *, size_t);

/*****************************************************************************/

/**
   Code - Message Pair.

   Some EtherCAT commands support reading a status code to display a certain
   message. This type allows to map a code to a message string.
*/

typedef struct
{
    uint32_t code; /**< Code */
    const char *message; /**< Message belonging to \a code */
}
ec_code_msg_t;

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
