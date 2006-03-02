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

#include "../include/EtherCAT_rt.h"
#include "../include/EtherCAT_dev.h"
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
    unsigned long tx_time;  /**< Zeit des letzten Sendens */
    unsigned long rx_time;  /**< Zeit des letzten Empfangs */
    ec_device_state_t state; /**< Zustand des Gerätes */
    uint8_t rx_data[EC_MAX_FRAME_SIZE]; /**< Speicher für empfangene Rahmen */
    size_t rx_data_size; /**< Länge des empfangenen Rahmens */
    irqreturn_t (*isr)(int, void *, struct pt_regs *); /**< Adresse der ISR */
    struct module *module; /**< Zeiger auf das Modul, das das Gerät zur
                              Verfügung stellt. */
    uint8_t error_reported; /**< Zeigt an, ob ein Fehler im zyklischen Code
                               bereits gemeldet wurde. */
    uint8_t link_state; /**< Verbindungszustand */
};

/*****************************************************************************/

int ec_device_init(ec_device_t *, ec_master_t *);
void ec_device_clear(ec_device_t *);

int ec_device_open(ec_device_t *);
int ec_device_close(ec_device_t *);

void ec_device_call_isr(ec_device_t *);
uint8_t *ec_device_prepare(ec_device_t *);
void ec_device_send(ec_device_t *, size_t);
unsigned int ec_device_received(const ec_device_t *);
uint8_t *ec_device_data(ec_device_t *);

void ec_device_debug(const ec_device_t *);
void ec_data_print(const uint8_t *, size_t);
void ec_data_print_diff(const uint8_t *, const uint8_t *, size_t);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
