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

#include "../include/EtherCAT_rt.h" // Echtzeitschnittstelle
#include "../include/EtherCAT_si.h" // Slave-Interface-Makros

/*****************************************************************************/

ec_master_t *master = NULL;
ec_slave_t *s_in, *s_out, *s_ssi;

struct timer_list timer;

ec_slave_init_t slaves[] = {
    // Zeiger, Index, Herstellername, Produktname, Domäne
    {  &s_out, 2,     "Beckhoff",     "EL2004",    1      },
    {  &s_in,  1,     "Beckhoff",     "EL3102",    1      },
    {  &s_ssi, 7,     "Beckhoff",     "EL5001",    1      }
};

#define SLAVE_COUNT (sizeof(slaves) / sizeof(ec_slave_init_t))

/*****************************************************************************/

void run(unsigned long data)
{
    // Klemmen-IO
    EC_WRITE_EL20XX(s_out, 3, EC_READ_EL31XX(s_in, 0) < 0);

    // Prozessdaten lesen und schreiben
    EtherCAT_rt_domain_xio(master, 1, 100);

    // Timer neu starten
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

    if (EtherCAT_rt_register_slave_list(master, slaves, SLAVE_COUNT)) {
        printk(KERN_ERR "Could not register slaves!\n");
        goto out_release_master;
    }

    printk("Activating all EtherCAT slaves.\n");

    if (EtherCAT_rt_activate_slaves(master)) {
        printk(KERN_ERR "EtherCAT: Could not activate slaves!\n");
        goto out_release_master;
    }

    printk("Configuring EtherCAT slaves.\n");

    EtherCAT_rt_debug_level(master, 2);

    if (EtherCAT_rt_canopen_sdo_write(master, s_ssi, 0x4067, 2, 2)) {
        printk(KERN_ERR "EtherCAT: Could not set SSI baud rate!\n");
        goto out_release_master;
    }

    EtherCAT_rt_debug_level(master, 0);

    printk("Starting cyclic sample thread.\n");

    init_timer(&timer);

    timer.function = run;
    timer.expires = jiffies + 10; // Das erste Mal sofort feuern
    add_timer(&timer);

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
