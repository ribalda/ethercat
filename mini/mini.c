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

#define ABTASTFREQUENZ 100

struct timer_list timer;

/*****************************************************************************/

// EtherCAT
ec_master_t *master = NULL;
ec_domain_t *domain1 = NULL;

// Datenfelder
void *r_ssi;
void *r_inc;

// Kanäle
uint32_t k_angle, k_pos;

ec_field_init_t domain1_fields[] = {
    {&r_ssi,   "1", "Beckhoff", "EL5001", ec_ipvalue, 0, 1},
    {&r_inc, "0:3", "Beckhoff", "EL5101", ec_ipvalue, 0, 1},
    {}
};

/*****************************************************************************/

void run(unsigned long data)
{
    static unsigned int counter = 0;

    // Prozessdaten lesen und schreiben
    EtherCAT_rt_domain_queue(domain1);
    EtherCAT_rt_master_xio(master);
    EtherCAT_rt_domain_process(domain1);

    k_angle = EC_READ_U16(r_inc);
    k_pos   = EC_READ_U32(r_ssi);

    if (counter) {
        counter--;
    }
    else {
        counter = ABTASTFREQUENZ;
        printk(KERN_INFO "k_angle = %i\n", k_angle);
        printk(KERN_INFO "k_pos   = %i\n", k_pos);
    }

    // Timer neu starten
    timer.expires += HZ / ABTASTFREQUENZ;
    add_timer(&timer);
}

/*****************************************************************************/

int __init init_mini_module(void)
{
    const ec_field_init_t *field;

    printk(KERN_INFO "=== Starting Minimal EtherCAT environment... ===\n");

    if ((master = EtherCAT_rt_request_master(0)) == NULL) {
        printk(KERN_ERR "Error requesting master 0!\n");
        goto out_return;
    }

    EtherCAT_rt_master_print(master);

    printk(KERN_INFO "Registering domain...\n");

    if (!(domain1 = EtherCAT_rt_master_register_domain(master, ec_sync, 100)))
    {
        printk(KERN_ERR "EtherCAT: Could not register domain!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Registering domain fields...\n");

    for (field = domain1_fields; field->data; field++)
    {
        if (!EtherCAT_rt_register_slave_field(domain1,
                                              field->address,
                                              field->vendor,
                                              field->product,
                                              field->data,
                                              field->field_type,
                                              field->field_index,
                                              field->field_count)) {
            printk(KERN_ERR "EtherCAT: Could not register field!\n");
            goto out_release_master;
        }
    }

    printk(KERN_INFO "Activating master...\n");

    if (EtherCAT_rt_master_activate(master)) {
        printk(KERN_ERR "EtherCAT: Could not activate master!\n");
        goto out_release_master;
    }

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

        printk(KERN_INFO "Deactivating master...\n");

        EtherCAT_rt_master_deactivate(master);
        EtherCAT_rt_release_master(master);
    }

    printk(KERN_INFO "=== Minimal EtherCAT environment stopped. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT minimal test environment");

module_init(init_mini_module);
module_exit(cleanup_mini_module);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
