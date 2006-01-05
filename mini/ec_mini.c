/******************************************************************************
 *
 * ec_mini.c
 *
 * Minimalmodul für EtherCAT
 *
 * $Id$
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include "../drivers/ec_master.h"
#include "../drivers/ec_device.h"
#include "../drivers/ec_types.h"
#include "../drivers/ec_module.h"

/*****************************************************************************/

// Auskommentieren, wenn keine zyklischen Daten erwuenscht
#define ECAT_CYCLIC_DATA

/*****************************************************************************/

static EtherCAT_master_t *ecat_master = NULL;

static EtherCAT_slave_t ecat_slaves[] =
{
#if 0
    // Block 1
    ECAT_INIT_SLAVE(Beckhoff_EK1100, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL4102, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL3162, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),

    ECAT_INIT_SLAVE(Beckhoff_EL4102, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL4102, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL4102, 1),

    ECAT_INIT_SLAVE(Beckhoff_EL3162, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL3162, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL3162, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL3102, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL3102, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL3102, 1),

#endif

#if 1
    // Block 2
    ECAT_INIT_SLAVE(Beckhoff_EK1100, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL4102, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL3162, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL3102, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),

    // Block 3
    ECAT_INIT_SLAVE(Beckhoff_EK1100, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 1)
#endif
};

#define ECAT_SLAVES_COUNT (sizeof(ecat_slaves) / sizeof(EtherCAT_slave_t))

#ifdef ECAT_CYCLIC_DATA

int value;
int dig1;

struct timer_list timer;
unsigned long last_start_jiffies;

#endif // ECAT_CYCLIC_DATA

/******************************************************************************
 *
 * Function: next2004
 *
 *****************************************************************************/

#ifdef ECAT_CYCLIC_DATA

static int next2004(int *wrap)
{
    static int i = 0;
    unsigned int j = 0;

    *wrap = 0;

    for (j = 0; j < ECAT_SLAVES_COUNT; j++)
    {
        i++;

        i %= ECAT_SLAVES_COUNT;

        if (i == 0) *wrap = 1;

        if (ecat_slaves[i].desc == Beckhoff_EL2004)
        {
            return i;
        }
    }

    return -1;
}
#endif

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
    static int cnt = 0;
    static unsigned long int k = 0;
    static int firstrun = 1;

    static int klemme = 0;
    static int kanal = 0;
    static int up_down = 0;
    int wrap = 0;

    ms++;
    ms %= 1000;

    if (firstrun) klemme = next2004(&wrap);

    if (cnt++ > 20)
    {
        cnt = 0;

        if (++kanal > 3)
        {
            kanal = 0;
            klemme = next2004(&wrap);

            if (wrap == 1)
            {
                if (up_down == 1) up_down = 0;
                else up_down = 1;
            }
        }
    }

    if (klemme >= 0)
        EtherCAT_write_value(&ecat_slaves[klemme], kanal, up_down);

    // Prozessdaten lesen und schreiben
    rdtscl(k);
    EtherCAT_process_data_cycle(ecat_master, 1);
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

int __init init_module()
{
    unsigned int i;

    printk(KERN_INFO "=== Starting Minimal EtherCAT environment... ===\n");

    if ((ecat_master = EtherCAT_request(0)) == NULL) {
        printk(KERN_ERR "EtherCAT master 0 not available!\n");
        goto out_return;
    }

    printk("Checking EtherCAT slaves.\n");

    if (EtherCAT_check_slaves(ecat_master, ecat_slaves,
                              ECAT_SLAVES_COUNT) != 0) {
        printk(KERN_ERR "EtherCAT: Could not init slaves!\n");
        goto out_release_master;
    }

    printk("Activating all EtherCAT slaves.\n");

    for (i = 0; i < ECAT_SLAVES_COUNT; i++) {
        if (EtherCAT_activate_slave(ecat_master, &ecat_slaves[i]) != 0) {
            printk(KERN_ERR "EtherCAT: Could not activate slave %i!\n", i);
            goto out_release_master;
        }
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
  EtherCAT_release(ecat_master);

 out_return:
  return -1;
}

/******************************************************************************
 *
 * Function: cleanup
 *
 *****************************************************************************/

void __exit cleanup_module()
{
    unsigned int i;

    printk(KERN_INFO "=== Stopping Minimal EtherCAT environment... ===\n");

    if (ecat_master)
    {
#ifdef ECAT_CYCLIC_DATA
        del_timer_sync(&timer);
#endif // ECAT_CYCLIC_DATA

        printk(KERN_INFO "Deactivating slaves.\n");

        for (i = 0; i < ECAT_SLAVES_COUNT; i++) {
            EtherCAT_deactivate_slave(ecat_master, &ecat_slaves[i]);
        }

        EtherCAT_release(ecat_master);
    }

    printk(KERN_INFO "=== Minimal EtherCAT environment stopped. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("Minimal EtherCAT environment");

module_init(init_module);
module_exit(cleanup_module);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
