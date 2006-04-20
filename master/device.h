/******************************************************************************
 *
 *  d e v i c e . h
 *
 *  EtherCAT device structure.
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
   EtherCAT device.

   An EtherCAT device is a network interface card, that is owned by an
   EtherCAT master to send and receive EtherCAT frames with.
*/

struct ec_device
{
    ec_master_t *master; /**< EtherCAT master */
    struct net_device *dev; /**< pointer to the assigned net_device */
    uint8_t open; /**< true, if the net_device has been opened */
    struct sk_buff *tx_skb; /**< transmit socket buffer */
    ec_isr_t isr; /**< pointer to the device's interrupt service routine */
    struct module *module; /**< pointer to the device's owning module */
    uint8_t link_state; /**< device link state */
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
