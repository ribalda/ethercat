/******************************************************************************
 *
 * msr_io.c
 *
 * Sample Modul für EtherCAT
 *           
 * Autoren: Wilhelm Hagemeister, Florian Pose
 *
 * $Date$
 * $Author$
 *
 * (C) Copyright IgH 2005
 * Ingenieurgemeinschaft IgH
 * Heinz-Bäcker Str. 34
 * D-45356 Essen
 * Tel.: +49 201/61 99 31
 * Fax.: +49 201/61 98 36
 * E-mail: hm@igh-essen.com
 *
 * /bin/setserial /dev/ttyS0 uart none
 * /bin/setserial /dev/ttyS1 uart none
 *
 ******************************************************************************/

/*--Includes-----------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/tqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <msr_reg.h>
#include <msr_messages.h>
#include <msr_main.h>

#include <rtai.h>
#include <rtai_sched.h>

#include "msr_io.h"

//#include <msr_float.h>

#include "../drivers/ec_master.h"
#include "../drivers/ec_device.h"
#include "../drivers/ec_types.h"
#include "../rs232dbg/rs232dbg.h"

/*--Defines------------------------------------------------------------------*/

#define TIMERTICS 1e6	// in ns; Thus have a task time of 1ms
#define MSR_ABTASTFREQUENZ (1e9/TIMERTICS)

//#define MSR_ABTASTFREQUENZ (1000) //1e9/TIMERTICS)
#define TICK ((1000000 / MSR_ABTASTFREQUENZ) * 1000)
#define TIMER_FREQ (APIC_TIMER ? FREQ_APIC : FREQ_8254)
#define APIC_TIMER 0

//#define MSR_SLOW_DEBUG

/*--Globale Variablen--------------------------------------------------------*/

RT_TASK process_image;

const int Tick = TICK;

unsigned int ecat_tx_delay = 0; //Zeit vom Ende der TimerInterruptRoutine bis
                                //TX-Interrupt der Netzwerkkarte 
unsigned int ecat_rx_delay = 0; //RX-Interrupt der Netzwerkkarte

unsigned int tx_intr = 0;
unsigned int rx_intr = 0;
unsigned int total_intr = 0;

unsigned int thread_end = 0;

#define USE_ETHERCAT

#ifdef USE_ETHERCAT

static EtherCAT_master_t *ecat_master = NULL;

extern EtherCAT_device_t rtl_ecat_dev;

//#define ECAT_SLAVES_COUNT 16

static EtherCAT_slave_t ecat_slaves[] =
{
    //Block 1
/*  ECAT_INIT_SLAVE(Beckhoff_EK1100),
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
  ECAT_INIT_SLAVE(Beckhoff_EL2004), */
  //Block 2
  ECAT_INIT_SLAVE(Beckhoff_EK1100),
  ECAT_INIT_SLAVE(Beckhoff_EL4102),
  ECAT_INIT_SLAVE(Beckhoff_EL1014),
  ECAT_INIT_SLAVE(Beckhoff_EL3162),
  ECAT_INIT_SLAVE(Beckhoff_EL2004),
  ECAT_INIT_SLAVE(Beckhoff_EL3102),
  //Block 3
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

#define ECAT_SLAVES_COUNT (sizeof(ecat_slaves)/sizeof(EtherCAT_slave_t))

#endif

double value;
int dig1;


static int next2004(int *wrap)
{
    static int i=0;

    int j=0;

    *wrap = 0;
    for(j=0;j<ECAT_SLAVES_COUNT;j++) {
	i++;
	i %= ECAT_SLAVES_COUNT;
	if(i == 0) *wrap = 1;
	if(ecat_slaves[i].desc == Beckhoff_EL2004) {
	    return i;
	}
    }
    return -1;
}

/*
*******************************************************************************
*
* Function: msr_controller
*
* Beschreibung: Zyklischer Prozess
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
*******************************************************************************
*/

void msr_controller(void)
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

#ifdef USE_ETHERCAT
  ecat_tx_delay = ((unsigned int) (100000 / HZ) * (ecat_master->dev->tx_time-k))
    / (current_cpu_data.loops_per_jiffy / 10);  
  ecat_rx_delay = ((unsigned int) (100000 / HZ) * (ecat_master->dev->rx_time-k))
    / (current_cpu_data.loops_per_jiffy / 10);  

  rx_intr = ecat_master->dev->rx_intr_cnt;
  tx_intr = ecat_master->dev->tx_intr_cnt;
  total_intr = ecat_master->dev->intr_cnt;
  // Prozessdaten lesen
  if(!firstrun) {
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


   if(cnt++ > 20) {
      cnt = 0;
      if(++kanal > 3) {
	  kanal = 0;
	  klemme = next2004(&wrap);
	  if (wrap == 1) { 
	      if(up_down == 1) 
		  up_down = 0;
	      else up_down = 1;
	      }
	  }
      }
   if (klemme >=0)
    EtherCAT_write_value(&ecat_master->slaves[klemme], kanal,up_down);
  
//  EtherCAT_write_value(&ecat_master->slaves[13], 1, ms > 500 ? 0 : 1);
//  EtherCAT_write_value(&ecat_master->slaves[14], 2, ms > 500 ? 0 : 1);
//  EtherCAT_write_value(&ecat_master->slaves[15], 3, ms > 500 ? 1 : 0);

  // Prozessdaten schreiben
  rdtscl(k);
  EtherCAT_write_process_data(ecat_master);
  firstrun = 0;
#endif
}

/*
*******************************************************************************
*
* Function: msr_run_interrupt
*
* Beschreibung: Interrupt abarbeiten
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
*******************************************************************************
*/

void process_thread(int priv_data)
{
  while (1)
  {

#ifdef USE_ETHERCAT
    MSR_RTAITHREAD_CODE(msr_controller(); msr_write_kanal_list(); );  
#else
    MSR_RTAITHREAD_CODE( msr_write_kanal_list(); );  
#endif



    /*   if(counter++ >=MSR_ABTASTFREQUENZ) {
	counter = 0;
	sprintf(buf,"rt:life");
	msr_print_info(buf);
    }
    */
    rt_task_wait_period();
  }
  thread_end = 1;
}

/*
*******************************************************************************
*
* Function: msr_register_channels
*
* Beschreibung: Kanäle registrieren
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
*******************************************************************************
*/

int msr_globals_register(void)
{
  msr_reg_kanal("/value", "V", &value, TDBL);
  msr_reg_kanal("/dig1", "", &dig1, TINT);
  msr_reg_kanal("/Taskinfo/Ecat/TX-Delay","us",&ecat_tx_delay,TUINT);
  msr_reg_kanal("/Taskinfo/Ecat/RX-Delay","us",&ecat_rx_delay,TUINT);
  msr_reg_kanal("/Taskinfo/Ecat/TX-Cnt","",&tx_intr,TUINT);
  msr_reg_kanal("/Taskinfo/Ecat/RX-Cnt","",&rx_intr,TUINT);
  msr_reg_kanal("/Taskinfo/Ecat/Total-Cnt","",&total_intr,TUINT);

  return 0;
}

/*
*******************************************************************************
*
* Function: msr_init
*
* Beschreibung: MSR initialisieren
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
*******************************************************************************
*/

int msr_init(void)
{   
  int rv = -1;
  RTIME tick_period, now;

//  rt_mount_rtai();

  msr_print_info("Initialising rtlib.");

  // RT-lib initialisieren    
  if (msr_rtlib_init(1, MSR_ABTASTFREQUENZ, 10, msr_globals_register) < 0)
  {
    msr_print_warn("msr_modul: can't initialize rtlib!");
    goto out_umount;
  }

#ifdef USE_ETHERCAT
  msr_print_info("Opening EtherCAT device.");

  mdelay(100);

  if (EtherCAT_device_open(&rtl_ecat_dev) < 0)
  {
    msr_print_warn("msr_modul: Could not initialize EtherCAT NIC.");
    goto out_rtlib;
  }

  if (!rtl_ecat_dev.dev) // Es gibt kein EtherCAT-Device
  {  
    msr_print_warn("msr_modul: No EtherCAT device!");
    goto out_close;
  }
//    goto out_close;

  // EtherCAT-Master und Slaves initialisieren

  msr_print_info("Initialising EtherCAT master");

  if ((ecat_master = (EtherCAT_master_t *) kmalloc(sizeof(EtherCAT_master_t), GFP_KERNEL)) == 0)
  {
    msr_print_warn(KERN_ERR "msr_modul: Could not alloc memory for EtherCAT master!\n");
    goto out_close;
  }

  if (EtherCAT_master_init(ecat_master, &rtl_ecat_dev) < 0)
  {
    msr_print_warn(KERN_ERR "EtherCAT could not init master!\n");
    goto out_master;
  }

  msr_print_info("Checking EtherCAT slaves.");
  mdelay(10); //Nachricht abwarten

  if (EtherCAT_check_slaves(ecat_master, ecat_slaves, ECAT_SLAVES_COUNT) != 0)
  {
    msr_print_warn(KERN_ERR "EtherCAT: Could not init slaves!\n");
    goto out_masterclear;
  }

  msr_print_info("Activating all EtherCAT slaves.");
  mdelay(10); //Nachricht abwarten

  if (EtherCAT_activate_all_slaves(ecat_master) != 0)
  {
    printk(KERN_ERR "EtherCAT: Could not activate slaves!\n");
    goto out_masterclear;
  }

  // Zyklischen Aufruf starten

#endif
  msr_print_info("Starting cyclic sample thread.");
  mdelay(10); //Nachricht abwarten

  EtherCAT_write_process_data(ecat_master);

  //mdelay(100);
  tick_period = start_rt_timer(nano2count(TIMERTICS));
  now = rt_get_time();

  if ((rv = rt_task_init(&process_image, process_thread, 0/*data*/, 64000/*stacksize*/, 0/*prio*/, 1/*use fpu*/, 0/*signal*/)))
  {
      msr_print_error("Could not initialise process_thread\n");
      goto out_stoptimer;
  }

  msr_print_info("Initialised sample thread\n");

  if ((rv = rt_task_make_periodic(&process_image, 
				  now + tick_period, 
				  tick_period)))
  {
      msr_print_error("Could not start process_thread\n");
      goto out_stoptask;
  }

  msr_print_info("Started sample thread.");

  return 0;

 out_stoptask:
  msr_print_info("Deleting task....");
  rt_task_delete(&process_image);

 out_stoptimer:
  msr_print_info("Stopping timer.");
  stop_rt_timer();

#ifdef USE_ETHERCAT
 out_masterclear:
  msr_print_info("Clearing EtherCAT master.");
  EtherCAT_master_clear(ecat_master);

 out_master:
    msr_print_info("Freeing EtherCAT master.");
  kfree(ecat_master);

 out_close:
  msr_print_info("Closing device.");

  EtherCAT_device_close(&rtl_ecat_dev);
#endif

 out_rtlib:
  msr_print_info("msr_rtlib_cleanup()");
  mdelay(10);
  msr_rtlib_cleanup();

 out_umount:
//  rt_umount_rtai();

  return rv;
}

/*
*******************************************************************************
*
* Function: msr_io_cleanup
*
* Beschreibung: Aufräumen
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
*******************************************************************************
*/

void msr_io_cleanup(void)
{

  msr_print_info("Stopping timer.");

  stop_rt_timer();

  msr_print_info("Deleting task....");

  rt_task_delete(&process_image);
/*
  for(i=0;i<1000;i++) {
      udelay(100);
      if(thread_end == 1) {
	  msr_print_info("Task ended at count %d",i);
	  break;
      }
  }
*/
  //noch einmal lesen

  msr_print_info("Read Processdata");
  EtherCAT_read_process_data(ecat_master);

  //EtherCAT_read_process_data(ecat_master);
#ifdef USE_ETHERCAT
  if (ecat_master)
  {

    msr_print_info("Deactivating slaves.");


    EtherCAT_deactivate_all_slaves(ecat_master);    


    msr_print_info("Clearing EtherCAT master.");

    EtherCAT_master_clear(ecat_master);

    msr_print_info("Freeing EtherCAT master.");


    kfree(ecat_master);
    ecat_master = NULL;
  }

  msr_print_info("Closing device.");

  EtherCAT_device_close(&rtl_ecat_dev);

#endif
  msr_print_info("msr_rtlib_cleanup()");

  msr_rtlib_cleanup();
  //rt_umount_rtai();
}

/*---Treiber-Einsprungspunkte etc.-------------------------------------------*/

MODULE_LICENSE("GPL");

module_init(msr_init);
module_exit(msr_io_cleanup);

/*---Ende--------------------------------------------------------------------*/
