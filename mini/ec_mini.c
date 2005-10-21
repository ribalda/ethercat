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
#include <linux/tqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "../drivers/ec_master.h"
#include "../drivers/ec_device.h"
#include "../drivers/ec_types.h"
#include "../drivers/ec_dbg.h"

extern EtherCAT_device_t rtl_ecat_dev;

//static EtherCAT_master_t *ecat_master = NULL;

#if 0
static EtherCAT_slave_t ecat_slaves[] =
{
  // Block 1
  ECAT_INIT_SLAVE(Beckhoff_EK1100),
  ECAT_INIT_SLAVE(Beckhoff_EL4102),
  ECAT_INIT_SLAVE(Beckhoff_EL3162),
  ECAT_INIT_SLAVE(Beckhoff_EL1014),
  ECAT_INIT_SLAVE(Beckhoff_EL2004),
  ECAT_INIT_SLAVE(Beckhoff_EL3102),
  ECAT_INIT_SLAVE(Beckhoff_EL4102),
  ECAT_INIT_SLAVE(Beckhoff_EL4102),
  ECAT_INIT_SLAVE(Beckhoff_EL4102),
  ECAT_INIT_SLAVE(Beckhoff_EL3162),
  ECAT_INIT_SLAVE(Beckhoff_EL3162),
  ECAT_INIT_SLAVE(Beckhoff_EL3162),
  ECAT_INIT_SLAVE(Beckhoff_EL3102),
  ECAT_INIT_SLAVE(Beckhoff_EL3102),
  ECAT_INIT_SLAVE(Beckhoff_EL2004),
  ECAT_INIT_SLAVE(Beckhoff_EL2004),
  ECAT_INIT_SLAVE(Beckhoff_EL2004),
  ECAT_INIT_SLAVE(Beckhoff_EL2004),

  // Block 2
  ECAT_INIT_SLAVE(Beckhoff_EK1100),
  ECAT_INIT_SLAVE(Beckhoff_EL4102),
  ECAT_INIT_SLAVE(Beckhoff_EL1014),
  ECAT_INIT_SLAVE(Beckhoff_EL3162),
  ECAT_INIT_SLAVE(Beckhoff_EL2004),
  ECAT_INIT_SLAVE(Beckhoff_EL3102),

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
};

#define ECAT_SLAVES_COUNT (sizeof(ecat_slaves) / sizeof(EtherCAT_slave_t))
#endif

double value;
int dig1;

/******************************************************************************
 *
 * Function: next2004
 *
 *****************************************************************************/

#if 0
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
 * Function: msr_controller
 *
 * Beschreibung: Zyklischer Prozess
 *
 *****************************************************************************/

#if 0
void msr_controller()
{
  static int ms = 0;
  static int cnt = 0;
  static unsigned long int k = 0;
  static int firstrun = 1;
  
  static int klemme = 12;
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
    value = EtherCAT_read_value(&ecat_master->slaves[5], 0) / 3276.7;
    dig1 = EtherCAT_read_value(&ecat_master->slaves[3], 0);
  }

  // Daten schreiben
  EtherCAT_write_value(&ecat_master->slaves[4], 0, ms > 500 ? 1 : 0);
  EtherCAT_write_value(&ecat_master->slaves[4], 1, ms > 500 ? 0 : 1);
  EtherCAT_write_value(&ecat_master->slaves[4], 2, ms > 500 ? 0 : 1);
  EtherCAT_write_value(&ecat_master->slaves[4], 3, ms > 500 ? 1 : 0);

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
    EtherCAT_write_value(&ecat_master->slaves[klemme], kanal,up_down);
  
  //EtherCAT_write_value(&ecat_master->slaves[13], 1, ms > 500 ? 0 : 1);
  //EtherCAT_write_value(&ecat_master->slaves[14], 2, ms > 500 ? 0 : 1);
  //EtherCAT_write_value(&ecat_master->slaves[15], 3, ms > 500 ? 1 : 0);
  
  // Prozessdaten schreiben
  rdtscl(k);
  EtherCAT_write_process_data(ecat_master);
  firstrun = 0;
}
#endif

/******************************************************************************
*
* Function: init
*
******************************************************************************/

//#define ECAT_OPEN

int init()
{   
#ifdef ECAT_OPEN
  int rv = -1;
#endif

  EC_DBG(KERN_INFO "=== Starting Minimal EtherCAT environment... ===\n");

  EtherCAT_device_debug(&rtl_ecat_dev);
  //mdelay(5000);

#ifdef ECAT_OPEN
  EC_DBG("Opening EtherCAT device.\n");

  // HIER PASSIERT DER FEHLER:
  if (EtherCAT_device_open(&rtl_ecat_dev) < 0)
  {
    EC_DBG(KERN_ERR "msr_modul: Could not initialize EtherCAT NIC.\n");
    goto out_nothing;
  }

  if (!rtl_ecat_dev.dev) // Es gibt kein EtherCAT-Device
  {  
    EC_DBG(KERN_ERR "msr_modul: No EtherCAT device!\n");
    goto out_close;
  }
#endif

#if 0
  // EtherCAT-Master und Slaves initialisieren
  EC_DBG("Initialising EtherCAT master\n");

  if ((ecat_master = (EtherCAT_master_t *) kmalloc(sizeof(EtherCAT_master_t), GFP_KERNEL)) == 0)
  {
    EC_DBG(KERN_ERR "msr_modul: Could not alloc memory for EtherCAT master!\n");
    goto out_close;
  }

  if (EtherCAT_master_init(ecat_master, &rtl_ecat_dev) < 0)
  {
    EC_DBG(KERN_ERR "EtherCAT could not init master!\n");
    goto out_master;
  }
#endif

#if 0
  EC_DBG("Checking EtherCAT slaves.\n");

  if (EtherCAT_check_slaves(ecat_master, ecat_slaves, ECAT_SLAVES_COUNT) != 0)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not init slaves!\n");
    goto out_masterclear;
  }

  EC_DBG("Activating all EtherCAT slaves.\n");

  if (EtherCAT_activate_all_slaves(ecat_master) != 0)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not activate slaves!\n");
    goto out_masterclear;
  }

  // Zyklischen Aufruf starten

  EC_DBG("Starting cyclic sample thread.\n");

  EtherCAT_write_process_data(ecat_master);

  EC_DBG("Initialised sample thread.\n");
#endif

  EC_DBG(KERN_INFO "=== Minimal EtherCAT environment started. ===\n");

  return 0;

#if 0
 out_masterclear:
  EC_DBG(KERN_INFO "Clearing EtherCAT master.\n");
  EtherCAT_master_clear(ecat_master);
#endif

#if 0
 out_master:
  EC_DBG(KERN_INFO "Freeing EtherCAT master.\n");
  kfree(ecat_master);
#endif

#ifdef ECAT_OPEN
 out_close:
  EC_DBG(KERN_INFO "Closing device.\n");
  EtherCAT_device_close(&rtl_ecat_dev);

 out_nothing:
  return rv;
#endif
}

/******************************************************************************
*
* Function: cleanup
*
******************************************************************************/

void cleanup()
{
  EC_DBG(KERN_INFO "=== Stopping Minimal EtherCAT environment... ===\n");

  // Noch einmal lesen
  //EC_DBG(KERN_INFO "Reading process data.\n");
  //EtherCAT_read_process_data(ecat_master);

#if 0
  if (ecat_master)
  {
#if 0
    EC_DBG(KERN_INFO "Deactivating slaves.\n");
    EtherCAT_deactivate_all_slaves(ecat_master);
#endif

    EC_DBG(KERN_INFO "Clearing EtherCAT master.\n");
    EtherCAT_master_clear(ecat_master);

    EC_DBG(KERN_INFO "Freeing EtherCAT master.\n");
    kfree(ecat_master);
    ecat_master = NULL;
  }
#endif

#ifdef ECAT_OPEN
  EC_DBG(KERN_INFO "Closing device.\n");
  EtherCAT_device_close(&rtl_ecat_dev);
#endif

  EC_DBG(KERN_INFO "=== Minimal EtherCAT environment stopped. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("Minimal EtherCAT environment");

module_init(init);
module_exit(cleanup);

/*****************************************************************************/
