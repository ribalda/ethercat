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
#include "../include/EtherCAT_si.h"

/*****************************************************************************/

ec_master_t *master = NULL;
ec_slave_t *s_in, *s_out;

int value;
int dig1;

struct timer_list timer;
unsigned long last_start_jiffies;

/*****************************************************************************/

int __init check_slaves(void)
{
    s_in = EtherCAT_rt_register_slave(master, 1, "Beckhoff", "EL3102", 1);
    s_out = EtherCAT_rt_register_slave(master, 9, "Beckhoff", "EL2004", 1);

    return s_in && s_out;
}

/*****************************************************************************/

void run(unsigned long data)
{
    static int ms = 0;
    static unsigned long int k = 0;
    static int firstrun = 1;

    ms++;
    ms %= 1000;

    EC_WRITE_EL20XX(s_out, 3, EC_READ_EL31XX(s_in, 0) < 0);

    // Prozessdaten lesen und schreiben
    rdtscl(k);
    EtherCAT_rt_domain_xio(master, 1, 100);
    firstrun = 0;

    timer.expires += HZ / 1000;
    add_timer(&timer);
}

/*****************************************************************************/

int __init init_mini_module(void)
{
    printk(KERN_INFO "=== Starting Minimal EtherCAT environment... ===\n");

    if ((master = EtherCAT_rt_request_master(0)) == NULL) {
        printk(KERN_ERR "EtherCAT master 0 not available!\n");
        goto out_return;
    }

    if (!check_slaves()) {
        printk(KERN_ERR "Could not register slaves!\n");
        goto out_release_master;
    }

    printk("Activating all EtherCAT slaves.\n");

    if (EtherCAT_rt_activate_slaves(master) != 0) {
        printk(KERN_ERR "EtherCAT: Could not activate slaves!\n");
        goto out_release_master;
    }

    printk("Starting cyclic sample thread.\n");

    init_timer(&timer);

    timer.function = run;
    timer.data = 0;
    timer.expires = jiffies + 10; // Das erste Mal sofort feuern
    last_start_jiffies = timer.expires;
    add_timer(&timer);

    printk("Initialised sample thread.\n");

    printk(KERN_INFO "=== Minimal EtherCAT environment started. ===\n");

    return 0;

 out_release_master:
  EtherCAT_rt_release_master(master);

 out_return:
  return -1;
}

/*****************************************************************************/

void __exit cleanup_mini_module(void)
{
    printk(KERN_INFO "=== Stopping Minimal EtherCAT environment... ===\n");

    if (master)
    {
        del_timer_sync(&timer);

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
