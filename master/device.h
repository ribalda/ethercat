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

#include "globals.h"
#include "../include/EtherCAT_dev.h"

/*****************************************************************************/

/**
   EtherCAT-Gerät.

   Ein EtherCAT-Gerät ist eine Netzwerkkarte, die vom
   EtherCAT-Master dazu verwendet wird, um Frames zu senden
   und zu empfangen.
*/

struct ec_device
{
  struct net_device *dev;    /**< Zeiger auf das reservierte net_device */
  unsigned int open;         /**< Das net_device ist geoeffnet. */
  struct sk_buff *tx_skb;    /**< Zeiger auf Transmit-Socketbuffer */
  struct sk_buff *rx_skb;    /**< Zeiger auf Receive-Socketbuffer */
  unsigned long tx_time;     /**< Zeit des letzten Sendens */
  unsigned long rx_time;     /**< Zeit des letzten Empfangs */
  unsigned long tx_intr_cnt; /**< Anzahl Tx-Interrupts */
  unsigned long rx_intr_cnt; /**< Anzahl Rx-Interrupts */
  unsigned long intr_cnt;    /**< Anzahl Interrupts */
  volatile ec_device_state_t state; /**< Gesendet, Empfangen,
                                       Timeout, etc. */
  unsigned char rx_data[EC_FRAME_SIZE]; /**< Puffer für
                                           empfangene Rahmen */
  volatile unsigned int rx_data_length; /**< Länge des zuletzt
                                           empfangenen Rahmens */
  irqreturn_t (*isr)(int, void *, struct pt_regs *); /**< Adresse der ISR */
  struct module *module; /**< Zeiger auf das Modul, das das Gerät zur
                            Verfügung stellt. */
  int error_reported; /**< Zeigt an, ob ein Fehler im zyklischen Code
                         bereits gemeldet wurde. */
};

/*****************************************************************************/

int ec_device_init(ec_device_t *);
void ec_device_clear(ec_device_t *);
int ec_device_open(ec_device_t *);
int ec_device_close(ec_device_t *);
void ec_device_call_isr(ec_device_t *);
int ec_device_send(ec_device_t *, unsigned char *, unsigned int);
int ec_device_receive(ec_device_t *, unsigned char *);

/*****************************************************************************/

#endif
