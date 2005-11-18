/****************************************************************
 *
 *  e c _ d e v i c e . h
 *
 *  Struktur für ein EtherCAT-Gerät.
 *
 *  $Date$
 *  $Author$
 *
 ***************************************************************/

#ifndef _EC_DEVICE_H_
#define _EC_DEVICE_H_

#include "ec_globals.h"

/**
   Zustand eines EtherCAT-Gerätes.

   Eine Für EtherCAT reservierte Netzwerkkarte kann bestimmte Zustände haben.
*/

typedef enum
{
  ECAT_DS_READY,    /**< Das Gerät ist bereit zum Senden */
  ECAT_DS_SENT,     /**< Das Gerät hat einen Rahmen abgesendet,
                       aber noch keine Antwort enpfangen */
  ECAT_DS_RECEIVED, /**< Das Gerät hat eine Antwort auf einen
                       zuvor gesendeten Rahmen empfangen */
  ECAT_DS_TIMEOUT,  /**< Nach dem Senden eines Rahmens meldete
                       das Gerät einen Timeout */
  ECAT_DS_ERROR     /**< Nach dem Senden eines frames hat das
                       Gerät einen Fehler festgestellt. */
}
EtherCAT_device_state_t;

#define ECAT_BUS_TIME(ecd_ptr) ((((ecd_ptr)->rx_time - \
                                  (ecd_ptr)->tx_time) * 1000) / cpu_khz)

/***************************************************************/

/**
   EtherCAT-Gerät.

   Ein EtherCAT-Gerät ist eine Netzwerkkarte, die vom
   EtherCAT-Master dazu verwendet wird, um Frames zu senden
   und zu empfangen.
*/

typedef struct
{
  struct net_device *dev;    /**< Zeiger auf das reservierte net_device */
  struct sk_buff *tx_skb;    /**< Zeiger auf Transmit-Socketbuffer */
  struct sk_buff *rx_skb;    /**< Zeiger auf Receive-Socketbuffer */
  unsigned long tx_time;     /**< Zeit des letzten Sendens */
  unsigned long rx_time;     /**< Zeit des letzten Empfangs */
  unsigned long tx_intr_cnt; /**< Anzahl Tx-Interrupts */
  unsigned long rx_intr_cnt; /**< Anzahl Rx-Interrupts */
  unsigned long intr_cnt;    /**< Anzahl Interrupts */
  volatile EtherCAT_device_state_t state; /**< Gesendet, Empfangen,
                                             Timeout, etc. */
  unsigned char rx_data[ECAT_FRAME_BUFFER_SIZE]; /**< Puffer für
                                                    empfangene Rahmen */
  volatile unsigned int rx_data_length; /**< Länge des zuletzt
                                           empfangenen Rahmens */
  spinlock_t *lock; /**< Zeiger auf das Spinlock des net_devices */
}
EtherCAT_device_t;

/***************************************************************/

void EtherCAT_device_init(EtherCAT_device_t *);
int EtherCAT_device_assign(EtherCAT_device_t *, struct net_device *);
void EtherCAT_device_clear(EtherCAT_device_t *);

int EtherCAT_device_open(EtherCAT_device_t *);
int EtherCAT_device_close(EtherCAT_device_t *);

int EtherCAT_device_send(EtherCAT_device_t *, unsigned char *, unsigned int);
int EtherCAT_device_receive(EtherCAT_device_t *, unsigned char *);
void EtherCAT_device_call_isr(EtherCAT_device_t *);

void EtherCAT_device_debug(EtherCAT_device_t *);

/***************************************************************/

#endif
