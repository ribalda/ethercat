/******************************************************************************
 *
 *  m o d u l e . c
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

#include "master.h"
#include "device.h"

/*****************************************************************************/

int __init ec_init_module(void);
void __exit ec_cleanup_module(void);

/*****************************************************************************/

#define LIT(X) #X
#define STR(X) LIT(X)

#define COMPILE_INFO "Revision " STR(EC_REV) \
                     ", compiled by " STR(EC_USER) \
                     " at " STR(EC_DATE)

/*****************************************************************************/

int ec_master_count = 1;
ec_master_t *ec_masters = NULL;
int *ec_masters_reserved = NULL;

/*****************************************************************************/

MODULE_AUTHOR ("Wilhelm Hagemeister <hm@igh-essen.com>,"
               "Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(COMPILE_INFO);

module_param(ec_master_count, int, 1);
MODULE_PARM_DESC(ec_master_count, "Number of EtherCAT master to initialize.");

/*****************************************************************************/

/**
   Init-Funktion des EtherCAT-Master-Treibermodules

   Initialisiert soviele Master, wie im Parameter ec_master_count
   angegeben wurde (Default ist 1).

   @returns 0, wenn alles o.k., -1 bei ungueltiger Anzahl Master
   oder zu wenig Speicher.
*/

int __init ec_init_module(void)
{
  unsigned int i;

  printk(KERN_ERR "EtherCAT: Master driver, %s\n", COMPILE_INFO);

  if (ec_master_count < 1) {
    printk(KERN_ERR "EtherCAT: Error - Illegal"
           " ec_master_count: %i\n", ec_master_count);
    return -1;
  }

  printk(KERN_ERR "EtherCAT: Initializing %i EtherCAT master(s)...\n",
         ec_master_count);

  if ((ec_masters =
       (ec_master_t *) kmalloc(sizeof(ec_master_t)
                               * ec_master_count,
                               GFP_KERNEL)) == NULL) {
    printk(KERN_ERR "EtherCAT: Could not allocate"
           " memory for EtherCAT master(s)!\n");
    return -1;
  }

  if ((ec_masters_reserved =
       (int *) kmalloc(sizeof(int) * ec_master_count,
                       GFP_KERNEL)) == NULL) {
    printk(KERN_ERR "EtherCAT: Could not allocate"
           " memory for reservation flags!\n");
    kfree(ec_masters);
    return -1;
  }

  for (i = 0; i < ec_master_count; i++)
  {
    ec_master_init(&ec_masters[i]);
    ec_masters_reserved[i] = 0;
  }

  printk(KERN_ERR "EtherCAT: Master driver initialized.\n");

  return 0;
}

/*****************************************************************************/

/**
   Cleanup-Funktion des EtherCAT-Master-Treibermoduls

   Entfernt alle Master-Instanzen.
*/

void __exit ec_cleanup_module(void)
{
  unsigned int i;

  printk(KERN_ERR "EtherCAT: Cleaning up master driver...\n");

  if (ec_masters)
  {
    for (i = 0; i < ec_master_count; i++)
    {
      if (ec_masters_reserved[i]) {
        printk(KERN_WARNING "EtherCAT: Warning -"
               " Master %i is still in use!\n", i);
      }

      ec_master_clear(&ec_masters[i]);
    }

    kfree(ec_masters);
  }

  printk(KERN_ERR "EtherCAT: Master driver cleaned up.\n");
}

/******************************************************************************
 *
 * Treiberschnittstelle
 *
 *****************************************************************************/

/**
   Setzt das EtherCAT-Geraet, auf dem der Master arbeitet.


   @param master Der EtherCAT-Master
   @param device Das EtherCAT-Geraet
   @return 0, wenn alles o.k.,
   < 0, wenn bereits ein Geraet registriert
   oder das Geraet nicht geoeffnet werden konnte.
*/

ec_device_t *EtherCAT_dev_register(unsigned int master_index,
                                   struct net_device *dev,
                                   irqreturn_t (*isr)(int, void *,
                                                      struct pt_regs *),
                                   struct module *module)
{
  ec_device_t *ecd;
  ec_master_t *master;

  if (master_index >= ec_master_count) {
    printk(KERN_ERR "EtherCAT: Master %i does not exist!\n", master_index);
    return NULL;
  }

  if (!dev) {
    printk("EtherCAT: Device is NULL!\n");
    return NULL;
  }

  master = ec_masters + master_index;

  if (master->device_registered) {
    printk(KERN_ERR "EtherCAT: Master %i already has a device!\n",
           master_index);
    return NULL;
  }

  ecd = &master->device;

  if (ec_device_init(ecd) < 0) {
    return NULL;
  }

  ecd->dev = dev;
  ecd->tx_skb->dev = dev;
  ecd->rx_skb->dev = dev;
  ecd->isr = isr;
  ecd->module = module;

  master->device_registered = 1;

  return ecd;
}

/*****************************************************************************/

/**
   Loescht das EtherCAT-Geraet, auf dem der Master arbeitet.

   @param master Der EtherCAT-Master
   @param device Das EtherCAT-Geraet
*/

void EtherCAT_dev_unregister(unsigned int master_index, ec_device_t *ecd)
{
  ec_master_t *master;

  if (master_index >= ec_master_count) {
    printk(KERN_WARNING "EtherCAT: Master %i does not exist!\n", master_index);
    return;
  }

  master = ec_masters + master_index;

  if (!master->device_registered || &master->device != ecd) {
    printk(KERN_ERR "EtherCAT: Unable to unregister device!\n");
    return;
  }

  master->device_registered = 0;
  ec_device_clear(ecd);
}

/******************************************************************************
 *
 * Echtzeitschnittstelle
 *
 *****************************************************************************/

/**
   Reserviert einen bestimmten EtherCAT-Master und das zugehörige Gerät.

   Gibt einen Zeiger auf den reservierten EtherCAT-Master zurueck.

   @param index Index des gewuenschten Masters
   @returns Zeiger auf EtherCAT-Master oder NULL, wenn Parameter ungueltig.
*/

ec_master_t *EtherCAT_rt_request_master(unsigned int index)
{
  ec_master_t *master;

  if (index < 0 || index >= ec_master_count) {
    printk(KERN_ERR "EtherCAT: Master %i does not exist!\n", index);
    goto req_return;
  }

  if (ec_masters_reserved[index]) {
    printk(KERN_ERR "EtherCAT: Master %i already in use!\n", index);
    goto req_return;
  }

  master = &ec_masters[index];

  if (!master->device_registered) {
    printk(KERN_ERR "EtherCAT: Master %i has no device assigned yet!\n",
           index);
    goto req_return;
  }

  if (!try_module_get(master->device.module)) {
    printk(KERN_ERR "EtherCAT: Could not reserve device module!\n");
    goto req_return;
  }

  if (ec_master_open(master) < 0) {
    printk(KERN_ERR "EtherCAT: Could not open device!\n");
    goto req_module_put;
  }

  if (ec_scan_for_slaves(master) != 0) {
    printk(KERN_ERR "EtherCAT: Could not scan for slaves!\n");
    goto req_close;
  }

  ec_masters_reserved[index] = 1;
  printk(KERN_INFO "EtherCAT: Reserved master %i.\n", index);

  return master;

 req_close:
  ec_master_close(master);

 req_module_put:
  module_put(master->device.module);

 req_return:
  return NULL;
}

/*****************************************************************************/

/**
   Gibt einen zuvor angeforderten EtherCAT-Master wieder frei.

   @param master Zeiger auf den freizugebenden EtherCAT-Master.
*/

void EtherCAT_rt_release_master(ec_master_t *master)
{
  unsigned int i;

  for (i = 0; i < ec_master_count; i++)
  {
    if (&ec_masters[i] == master)
    {
      if (!master->device_registered) {
        printk(KERN_WARNING "EtherCAT: Could not release device"
               "module because of no device!\n");
        return;
      }

      ec_master_close(master);
      ec_master_reset(master);

      module_put(master->device.module);
      ec_masters_reserved[i] = 0;

      printk(KERN_INFO "EtherCAT: Released master %i.\n", i);

      return;
    }
  }

  printk(KERN_WARNING "EtherCAT: Master %X was never requested!\n",
         (unsigned int) master);
}

/*****************************************************************************/

module_init(ec_init_module);
module_exit(ec_cleanup_module);

EXPORT_SYMBOL(EtherCAT_dev_register);
EXPORT_SYMBOL(EtherCAT_dev_unregister);
EXPORT_SYMBOL(EtherCAT_rt_request_master);
EXPORT_SYMBOL(EtherCAT_rt_release_master);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
