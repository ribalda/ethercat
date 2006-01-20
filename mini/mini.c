/******************************************************************************
 *
 * m i n i . c
 *
 * Minimalmodul für EtherCAT
 *
 * $Id$
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include "../include/EtherCAT_rt.h"

/*****************************************************************************/

// Auskommentieren, wenn keine zyklischen Daten erwuenscht
#define ECAT_CYCLIC_DATA

/*****************************************************************************/

ec_master_t *master = NULL;

#ifdef ECAT_CYCLIC_DATA

int value;
int dig1;

struct timer_list timer;
unsigned long last_start_jiffies;

#endif // ECAT_CYCLIC_DATA

/******************************************************************************
 *
 * Function: run
 *
 * Beschreibung: Zyklischer Prozess
 *
 *****************************************************************************/

#ifdef ECAT_CYCLIC_DATA

static void run(unsigned long data)
{
  static int ms = 0;
    static unsigned long int k = 0;
    static int firstrun = 1;

    ms++;
    ms %= 1000;

#if 0
    if (klemme >= 0)
        EtherCAT_write_value(&ecat_slaves[klemme], kanal, up_down);
#endif

    // Prozessdaten lesen und schreiben
    rdtscl(k);
    EtherCAT_rt_exchange_io(master, 1, 100);
    firstrun = 0;

    timer.expires += HZ / 1000;
    add_timer(&timer);
}

#endif // ECAT_CYCLIC_DATA

/******************************************************************************
 *
 * Function: init
 *
 *****************************************************************************/

int __init init_mini_module(void)
{
    printk(KERN_INFO "=== Starting Minimal EtherCAT environment... ===\n");

    if ((master = EtherCAT_rt_request_master(0)) == NULL) {
        printk(KERN_ERR "EtherCAT master 0 not available!\n");
        goto out_return;
    }

    //check_slaves();

    printk("Activating all EtherCAT slaves.\n");

    if (EtherCAT_rt_activate_slaves(master) != 0) {
        printk(KERN_ERR "EtherCAT: Could not activate slaves!\n");
        goto out_release_master;
    }

#ifdef ECAT_CYCLIC_DATA
    printk("Starting cyclic sample thread.\n");

    init_timer(&timer);

    timer.function = run;
    timer.data = 0;
    timer.expires = jiffies+10; // Das erste Mal sofort feuern
    last_start_jiffies = timer.expires;
    add_timer(&timer);

    printk("Initialised sample thread.\n");
#endif

    printk(KERN_INFO "=== Minimal EtherCAT environment started. ===\n");

    return 0;

 out_release_master:
  EtherCAT_rt_release_master(master);

 out_return:
  return -1;
}

/******************************************************************************
 *
 * Function: cleanup
 *
 *****************************************************************************/

void __exit cleanup_mini_module(void)
{
    printk(KERN_INFO "=== Stopping Minimal EtherCAT environment... ===\n");

    if (master)
    {
#ifdef ECAT_CYCLIC_DATA
        del_timer_sync(&timer);
#endif // ECAT_CYCLIC_DATA

        printk(KERN_INFO "Deactivating slaves.\n");

        EtherCAT_rt_deactivate_slaves(master);
        EtherCAT_rt_release_master(master);
    }

    printk(KERN_INFO "=== Minimal EtherCAT environment stopped. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("Minimal EtherCAT environment");

module_init(init_mini_module);
module_exit(cleanup_mini_module);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
