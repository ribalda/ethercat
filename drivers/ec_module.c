/******************************************************************************
 *
 *  ec_module.c
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

#include "ec_module.h"

/*****************************************************************************/

#define LIT(X) #X
#define STR(X) LIT(X)

#define COMPILE_INFO "Revision " STR(EC_REV) \
                     ", compiled by " STR(EC_USER) \
                     " at " STR(EC_DATE)

/*****************************************************************************/

int ecat_master_count = 1;
EtherCAT_master_t *ecat_masters = NULL;
int *ecat_masters_reserved = NULL;

/*****************************************************************************/

MODULE_AUTHOR ("Wilhelm Hagemeister <hm@igh-essen.com>,"
               "Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(COMPILE_INFO);

module_param(ecat_master_count, int, 1);
MODULE_PARM_DESC(ecat_master_count,
                 "Number of EtherCAT master to initialize.");

module_init(ecat_init_module);
module_exit(ecat_cleanup_module);

EXPORT_SYMBOL(EtherCAT_register_device);
EXPORT_SYMBOL(EtherCAT_unregister_device);
EXPORT_SYMBOL(EtherCAT_request);
EXPORT_SYMBOL(EtherCAT_release);

/*****************************************************************************/

/**
   Init-Funktion des EtherCAT-Master-Treibermodules

   Initialisiert soviele Master, wie im Parameter ecat_master_count
   angegeben wurde (Default ist 1).

   @returns 0, wenn alles o.k., -1 bei ungueltiger Anzahl Master
            oder zu wenig Speicher.
*/

int __init ecat_init_module(void)
{
  unsigned int i;

  printk(KERN_ERR "EtherCAT: Master driver, %s\n", COMPILE_INFO);

  if (ecat_master_count < 1) {
    printk(KERN_ERR "EtherCAT: Error - Illegal"
           " ecat_master_count: %i\n", ecat_master_count);
    return -1;
  }

  printk(KERN_ERR "EtherCAT: Initializing %i EtherCAT master(s)...\n",
         ecat_master_count);

  if ((ecat_masters =
       (EtherCAT_master_t *) kmalloc(sizeof(EtherCAT_master_t)
                                     * ecat_master_count,
                                     GFP_KERNEL)) == NULL) {
    printk(KERN_ERR "EtherCAT: Could not allocate"
           " memory for EtherCAT master(s)!\n");
    return -1;
  }

  if ((ecat_masters_reserved =
       (int *) kmalloc(sizeof(int) * ecat_master_count,
                       GFP_KERNEL)) == NULL) {
    printk(KERN_ERR "EtherCAT: Could not allocate"
           " memory for reservation flags!\n");
    kfree(ecat_masters);
    return -1;
  }

  for (i = 0; i < ecat_master_count; i++)
  {
    EtherCAT_master_init(&ecat_masters[i]);
    ecat_masters_reserved[i] = 0;
  }

  printk(KERN_ERR "EtherCAT: Master driver initialized.\n");

  return 0;
}

/*****************************************************************************/

/**
   Cleanup-Funktion des EtherCAT-Master-Treibermoduls

   Entfernt alle Master-Instanzen.
*/

void __exit ecat_cleanup_module(void)
{
  unsigned int i;

  printk(KERN_ERR "EtherCAT: Cleaning up master driver...\n");

  if (ecat_masters)
  {
    for (i = 0; i < ecat_master_count; i++)
    {
      if (ecat_masters_reserved[i]) {
        printk(KERN_WARNING "EtherCAT: Warning -"
               " Master %i is still in use!\n", i);
      }

      EtherCAT_master_clear(&ecat_masters[i]);
    }

    kfree(ecat_masters);
  }

  printk(KERN_ERR "EtherCAT: Master driver cleaned up.\n");
}

/*****************************************************************************/

/**
   Setzt das EtherCAT-Geraet, auf dem der Master arbeitet.

   Registriert das Geraet beim Master, der es daraufhin oeffnet.

   @param master Der EtherCAT-Master
   @param device Das EtherCAT-Geraet
   @return 0, wenn alles o.k.,
           < 0, wenn bereits ein Geraet registriert
           oder das Geraet nicht geoeffnet werden konnte.
*/

int EtherCAT_register_device(int index, EtherCAT_device_t *device)
{
  if (index < 0 || index >= ecat_master_count) {
    printk(KERN_ERR "EtherCAT: Master %i does not exist!\n", index);
    return -1;
  }

  return EtherCAT_master_open(&ecat_masters[index], device);
}

/*****************************************************************************/

/**
   Loescht das EtherCAT-Geraet, auf dem der Master arbeitet.

   @param master Der EtherCAT-Master
   @param device Das EtherCAT-Geraet
*/

void EtherCAT_unregister_device(int index, EtherCAT_device_t *device)
{
  if (index < 0 || index >= ecat_master_count) {
    printk(KERN_WARNING "EtherCAT: Master %i does not exist!\n", index);
    return;
  }

  EtherCAT_master_close(&ecat_masters[index], device);
}

/*****************************************************************************/

/**
   Reserviert einen bestimmten EtherCAT-Master und das zugehörige Gerät.

   Gibt einen Zeiger auf den reservierten EtherCAT-Master zurueck.

   @param index Index des gewuenschten Masters
   @returns Zeiger auf EtherCAT-Master oder NULL, wenn Parameter ungueltig.
*/

EtherCAT_master_t *EtherCAT_request(int index)
{
  if (index < 0 || index >= ecat_master_count) {
    printk(KERN_ERR "EtherCAT: Master %i does not exist!\n", index);
    return NULL;
  }

  if (ecat_masters_reserved[index]) {
    printk(KERN_ERR "EtherCAT: Master %i already in use!\n", index);
    return NULL;
  }

  if (!ecat_masters[index].dev) {
    printk(KERN_ERR "EtherCAT: Master %i has no device assigned yet!\n",
           index);
    return NULL;
  }

  if (!ecat_masters[index].dev->module) {
    printk(KERN_ERR "EtherCAT: Master %i device module is not set!\n", index);
    return NULL;
  }

  if (!try_module_get(ecat_masters[index].dev->module)) {
    printk(KERN_ERR "EtherCAT: Could not reserve device module!\n");
    return NULL;
  }

  ecat_masters_reserved[index] = 1;

  printk(KERN_INFO "EtherCAT: Reserved master %i.\n", index);

  return &ecat_masters[index];
}

/*****************************************************************************/

/**
   Gibt einen zuvor reservierten EtherCAT-Master wieder frei.

   @param master Zeiger auf den freizugebenden EtherCAT-Master.
*/

void EtherCAT_release(EtherCAT_master_t *master)
{
  unsigned int i;

  for (i = 0; i < ecat_master_count; i++)
  {
    if (&ecat_masters[i] == master)
    {
      if (!ecat_masters[i].dev) {
        printk(KERN_WARNING "EtherCAT: Could not release device"
               "module because of no device!\n");
        return;
      }

      module_put(ecat_masters[i].dev->module);
      ecat_masters_reserved[i] = 0;

      printk(KERN_INFO "EtherCAT: Released master %i.\n", i);

      return;
    }
  }

  printk(KERN_WARNING "EtherCAT: Master %X was never reserved!\n",
         (unsigned int) master);
}

/*****************************************************************************/
