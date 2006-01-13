/******************************************************************************
 *
 *  ec_module.h
 *
 *  EtherCAT-Master-Treiber
 *
 *  Autoren: Wilhelm Hagemeister, Florian Pose
 *
 *  $Id$
 *
 *  (C) Copyright IgH 2005
 *  Ingenieurgemeinschaft IgH
 *  Heinz-Bäcker Str. 34
 *  D-45356 Essen
 *  Tel.: +49 201/61 99 31
 *  Fax.: +49 201/61 98 36
 *  E-mail: sp@igh-essen.com
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "ec_master.h"

/*****************************************************************************/

// Registration of devices
int EtherCAT_register_device(int, EtherCAT_device_t *);
void EtherCAT_unregister_device(int, EtherCAT_device_t *);

EtherCAT_master_t *EtherCAT_request(int);
void EtherCAT_release(EtherCAT_master_t *);

/*****************************************************************************/
