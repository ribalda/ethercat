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
 ******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "ec_module.h"

/******************************************************************************/

#define SUBVERSION_ID "$Id$"

int ecat_master_count = 1;
EtherCAT_master_t *ecat_masters = NULL;

/******************************************************************************/

MODULE_AUTHOR ("Wilhelm Hagemeister <hm@igh-essen.com>, Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(SUBVERSION_ID);

module_param(ecat_master_count, int, 1);
MODULE_PARM_DESC(ecat_master_count, "Number of EtherCAT master to initialize.");

module_init(ecat_init_module);
module_exit(ecat_cleanup_module);

EXPORT_SYMBOL(EtherCAT_master);

/******************************************************************************/

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

  printk(KERN_ERR "EtherCAT: Master driver %s\n", SUBVERSION_ID);

  if (ecat_master_count < 1)
  {
    printk(KERN_ERR "EtherCAT: Error - Illegal ecat_master_count: %i\n",
           ecat_master_count);
    return -1;
  }

  printk(KERN_ERR "EtherCAT: Initializing %i EtherCAT master(s)...\n",
         ecat_master_count);

  if ((ecat_masters = (EtherCAT_master_t *) kmalloc(sizeof(EtherCAT_master_t)
                                                    * ecat_master_count,
                                                    GFP_KERNEL)) == NULL)
  {
    printk(KERN_ERR "EtherCAT: Could not allocate memory for EtherCAT master(s)!\n");
    return -1;
  }

  for (i = 0; i < ecat_master_count; i++)
  {
    EtherCAT_master_init(&ecat_masters[i]);
  }

  printk(KERN_ERR "EtherCAT: Master driver initialized.\n");

  return 0;
}

/******************************************************************************/

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
      EtherCAT_master_clear(&ecat_masters[i]);
    }

    kfree(ecat_masters);
  }

  printk(KERN_ERR "EtherCAT: Master driver cleaned up.\n");
}

/******************************************************************************/

/**
   Gibt einen Zeiger auf einen bestimmten EtherCAT-Master zurueck.

   @param index Index des gewuenschten Masters
   @returns Zeiger auf EtherCAT-Master oder NULL, wenn Parameter ungueltig.
*/

EtherCAT_master_t *EtherCAT_master(int index)
{
  if (index < 0 || index >= ecat_master_count)
  {
    printk(KERN_ERR "EtherCAT: Master %i does not exist!\n", index);
    return NULL;
  }

  return &ecat_masters[index];
}

/******************************************************************************/
