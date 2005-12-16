/******************************************************************************
 *
 * ec_mini.c
 *
 * Minimalmodul für EtherCAT
 *
 * $Id$
 *
 ******************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include "../drivers/ec_master.h"
#include "../drivers/ec_device.h"
#include "../drivers/ec_types.h"
#include "../drivers/ec_module.h"

/******************************************************************************/

// Auskommentieren, wenn keine zyklischen Daten erwuenscht
#define ECAT_CYCLIC_DATA

/******************************************************************************/

static EtherCAT_master_t *ecat_master = NULL;

static EtherCAT_slave_t ecat_slaves[] =
{
#if 0
    // Block 1
    ECAT_INIT_SLAVE(Beckhoff_EK1100),
    ECAT_INIT_SLAVE(Beckhoff_EL4102),
    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),

    ECAT_INIT_SLAVE(Beckhoff_EL4102),
    ECAT_INIT_SLAVE(Beckhoff_EL4102),
    ECAT_INIT_SLAVE(Beckhoff_EL4102),

    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL3102),
    ECAT_INIT_SLAVE(Beckhoff_EL3102),
    ECAT_INIT_SLAVE(Beckhoff_EL3102),

#endif

#if 1
    // Block 2
    ECAT_INIT_SLAVE(Beckhoff_EK1100),
    ECAT_INIT_SLAVE(Beckhoff_EL4102),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),
    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL3102),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),

    // Block 3
    ECAT_INIT_SLAVE(Beckhoff_EK1100),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL2004),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),
    ECAT_INIT_SLAVE(Beckhoff_EL1014),
    ECAT_INIT_SLAVE(Beckhoff_EL1014)
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

#if 0
    ecat_tx_delay = ((unsigned int) (100000 / HZ) * (ecat_master->dev->tx_time-k))
        / (current_cpu_data.loops_per_jiffy / 10);
    ecat_rx_delay = ((unsigned int) (100000 / HZ) * (ecat_master->dev->rx_time-k))
        / (current_cpu_data.loops_per_jiffy / 10);

    rx_intr = ecat_master->dev->rx_intr_cnt;
    tx_intr = ecat_master->dev->tx_intr_cnt;
    total_intr = ecat_master->dev->intr_cnt;
#endif

    // Prozessdaten lesen
    if (!firstrun)
    {
        EtherCAT_read_process_data(ecat_master);

        // Daten lesen und skalieren
//        value = EtherCAT_read_value(&ecat_master->slaves[5], 0) / 3276; //.7; FIXME kein FP im Kernel ohne Schutz !!
//        dig1 = EtherCAT_read_value(&ecat_master->slaves[3], 0);
    }
    else
        klemme = next2004(&wrap);


#if 0
    // Daten schreiben
    EtherCAT_write_value(&ecat_master->slaves[4], 0, ms > 500 ? 1 : 0);
    EtherCAT_write_value(&ecat_master->slaves[4], 1, ms > 500 ? 0 : 1);
    EtherCAT_write_value(&ecat_master->slaves[4], 2, ms > 500 ? 0 : 1);
    EtherCAT_write_value(&ecat_master->slaves[4], 3, ms > 500 ? 1 : 0);
#endif

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

    if (klemme >= 0) {
        EtherCAT_write_value(&ecat_master->slaves[klemme], kanal,up_down);
	//printk("ECAT write: Klemme: %d, Kanal: %d, Wert: %d\n",klemme,kanal,up_down); 
    }

#if 0
    EtherCAT_write_value(&ecat_master->slaves[13], 1, ms > 500 ? 0 : 1);
    EtherCAT_write_value(&ecat_master->slaves[14], 2, ms > 500 ? 0 : 1);
    EtherCAT_write_value(&ecat_master->slaves[15], 3, ms > 500 ? 1 : 0);
#endif

    // Prozessdaten schreiben
    rdtscl(k);
    EtherCAT_write_process_data(ecat_master);
    firstrun = 0;

    timer.expires += HZ / 1000;
    add_timer(&timer);
}

#endif // ECAT_CYCLIC_DATA

/******************************************************************************
 *
 * Function: init
 *
 ******************************************************************************/

int __init init_module()
{
  printk(KERN_INFO "=== Starting Minimal EtherCAT environment... ===\n");

  if ((ecat_master = EtherCAT_master(0)) == NULL)
  {
    printk(KERN_ERR "No EtherCAT master available!\n");
    return -1;
  }

  printk("Checking EtherCAT slaves.\n");

  if (EtherCAT_check_slaves(ecat_master, ecat_slaves, ECAT_SLAVES_COUNT) != 0)
  {
    printk(KERN_ERR "EtherCAT: Could not init slaves!\n");
    return -1;
  }

  printk("Activating all EtherCAT slaves.\n");

  if (EtherCAT_activate_all_slaves(ecat_master) != 0)
  {
    printk(KERN_ERR "EtherCAT: Could not activate slaves!\n");
    return -1;
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
}

/******************************************************************************
 *
 * Function: cleanup
 *
 ******************************************************************************/

void __exit cleanup_module()
{
    printk(KERN_INFO "=== Stopping Minimal EtherCAT environment... ===\n");

    if (ecat_master)
    {
#ifdef ECAT_CYCLIC_DATA
      del_timer_sync(&timer);
      EtherCAT_clear_process_data(ecat_master);
#endif // ECAT_CYCLIC_DATA

      printk(KERN_INFO "Deactivating slaves.\n");
      EtherCAT_deactivate_all_slaves(ecat_master);
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
