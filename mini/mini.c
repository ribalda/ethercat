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

#include "../include/ecrt.h" // Echtzeitschnittstelle

#define ASYNC

/*****************************************************************************/

#define ABTASTFREQUENZ 100

struct timer_list timer;

/*****************************************************************************/

// EtherCAT
ec_master_t *master = NULL;
ec_domain_t *domain1 = NULL;

// Datenfelder
//void *r_ssi_input, *r_ssi_status, *r_4102[3];

// Kanäle
uint32_t k_pos;
uint8_t k_stat;

ec_field_init_t domain1_fields[] = {
    {NULL, "1", "Beckhoff", "EL1014", "InputValue",  0},
    {NULL, "2", "Beckhoff", "EL2004", "OutputValue", 0},
    //{NULL, "3", "Beckhoff", "EL3162", "InputValue",  0},
    {NULL, "4", "Beckhoff", "EL4132", "OutputValue",  0},
    {NULL, "6", "Beckhoff", "EL5001", "InputValue", 0},
    {}
};

/*****************************************************************************/

void run(unsigned long data)
{
    static unsigned int counter = 0;

#ifdef ASYNC
    // Prozessdaten empfangen
    ecrt_master_async_receive(master);
    ecrt_domain_process(domain1);

    // Prozessdaten verarbeiten
    //k_pos   = EC_READ_U32(r_ssi_input);
    //k_stat  = EC_READ_U8(r_ssi_status);

    // Prozessdaten senden
    ecrt_domain_queue(domain1);
    ecrt_master_async_send(master);
#else
    // Prozessdaten senden und empfangen
    ecrt_domain_queue(domain1);
    ecrt_master_sync_io(master);
    ecrt_domain_process(domain1);

    // Prozessdaten verarbeiten
    //k_pos   = EC_READ_U32(r_ssi_input);
    //k_stat  = EC_READ_U8(r_ssi_status);
#endif

    if (counter) {
        counter--;
    }
    else {
        counter = ABTASTFREQUENZ;
        //printk(KERN_INFO "k_pos    = %i\n", k_pos);
        //printk(KERN_INFO "k_stat   = 0x%02X\n", k_stat);
    }

    // Timer neu starten
    timer.expires += HZ / ABTASTFREQUENZ;
    add_timer(&timer);
}

/*****************************************************************************/

int __init init_mini_module(void)
{
    printk(KERN_INFO "=== Starting Minimal EtherCAT environment... ===\n");

    if ((master = ecrt_request_master(0)) == NULL) {
        printk(KERN_ERR "Error requesting master 0!\n");
        goto out_return;
    }

    ecrt_master_print(master);

    printk(KERN_INFO "Registering domain...\n");

    if (!(domain1 = ecrt_master_create_domain(master)))
    {
        printk(KERN_ERR "EtherCAT: Could not register domain!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Registering domain fields...\n");

    if (ecrt_domain_register_field_list(domain1, domain1_fields)) {
        printk(KERN_ERR "EtherCAT: Could not register field!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Activating master...\n");

    if (ecrt_master_activate(master)) {
        printk(KERN_ERR "EtherCAT: Could not activate master!\n");
        goto out_release_master;
    }

    //ecrt_master_debug(master, 2);

#if 0
    if (ecrt_master_sdo_write(master, "1", 0x4061, 1,  0, 1) ||
        ecrt_master_sdo_write(master, "1", 0x4061, 2,  1, 1) ||
        ecrt_master_sdo_write(master, "1", 0x4061, 3,  1, 1) ||
        ecrt_master_sdo_write(master, "1", 0x4066, 0,  0, 1) ||
        ecrt_master_sdo_write(master, "1", 0x4067, 0,  4, 1) ||
        ecrt_master_sdo_write(master, "1", 0x4068, 0,  0, 1) ||
        ecrt_master_sdo_write(master, "1", 0x4069, 0, 25, 1) ||
        ecrt_master_sdo_write(master, "1", 0x406A, 0, 25, 1) ||
        ecrt_master_sdo_write(master, "1", 0x406B, 0, 50, 1)) {
        printk(KERN_ERR "EtherCAT: Failed to configure SSI!\n");
        goto out_deactivate;
    }
#endif

    //ecrt_master_debug(master, 0);

#if 0
    printk(KERN_INFO "Writing alias...\n");
    if (ecrt_master_write_slave_alias(master, "0", 0xBEEF)) {
        printk(KERN_ERR "EtherCAT: Failed to write alias!\n");
        goto out_deactivate;
    }
#endif

#ifdef ASYNC
    // Einmal senden und warten...
    ecrt_master_prepare_async_io(master);
#endif

    printk("Starting cyclic sample thread.\n");

    init_timer(&timer);
    timer.function = run;
    timer.expires = jiffies + 10; // Das erste Mal sofort feuern
    add_timer(&timer);

    printk(KERN_INFO "=== Minimal EtherCAT environment started. ===\n");

    return 0;

#if 0
 out_deactivate:
    ecrt_master_deactivate(master);
#endif

 out_release_master:
    ecrt_release_master(master);

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

        ecrt_master_deactivate(master);
        ecrt_release_master(master);
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
