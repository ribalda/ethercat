/******************************************************************************
 *
 * Oeffentliche EtherCAT-Schnittstellen fuer EtherCAT-Geraetetreiber.
 *
 * $Id$
 *
 *****************************************************************************/

#ifndef _ETHERCAT_DEVICE_H_
#define _ETHERCAT_DEVICE_H_

#include <linux/netdevice.h>

/*****************************************************************************/

struct ec_device;
typedef struct ec_device ec_device_t;

/*****************************************************************************/

ec_device_t *EtherCAT_dev_register(unsigned int, struct net_device *,
                                   irqreturn_t (*)(int, void *,
                                                   struct pt_regs *),
                                   struct module *);
void EtherCAT_dev_unregister(unsigned int, ec_device_t *);

int EtherCAT_dev_is_ec(const ec_device_t *, const struct net_device *);
void EtherCAT_dev_receive(ec_device_t *, const void *, size_t);
void EtherCAT_dev_link_state(ec_device_t *, uint8_t);

/*****************************************************************************/

#endif
