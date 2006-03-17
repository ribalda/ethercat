/******************************************************************************
 *
 *  d e v i c e . h
 *
 *  Struktur für ein EtherCAT-Gerät.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_DEVICE_H_
#define _EC_DEVICE_H_

#include <linux/interrupt.h>

#include "../include/ecrt.h"
#include "../devices/ecdev.h"
#include "globals.h"

/*****************************************************************************/

/**
   EtherCAT-Gerät.

   Ein EtherCAT-Gerät ist eine Netzwerkkarte, die vom
   EtherCAT-Master dazu verwendet wird, um Frames zu senden
   und zu empfangen.
*/

struct ec_device
{
    ec_master_t *master; /**< EtherCAT-Master */
    struct net_device *dev; /**< Zeiger auf das reservierte net_device */
    uint8_t open; /**< Das net_device ist geoeffnet. */
    struct sk_buff *tx_skb; /**< Zeiger auf Transmit-Socketbuffer */
    ec_isr_t isr; /**< Adresse der ISR */
    struct module *module; /**< Zeiger auf das Modul, das das Gerät zur
                              Verfügung stellt. */
    uint8_t link_state; /**< Verbindungszustand */
};

/*****************************************************************************/

int ec_device_init(ec_device_t *, ec_master_t *, struct net_device *,
                   ec_isr_t, struct module *);
void ec_device_clear(ec_device_t *);

int ec_device_open(ec_device_t *);
int ec_device_close(ec_device_t *);

void ec_device_call_isr(ec_device_t *);
uint8_t *ec_device_tx_data(ec_device_t *);
void ec_device_send(ec_device_t *, size_t);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
