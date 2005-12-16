/**************************************************************************************************
*
*                          msr_module.c
*
*           Kernelmodul fÅ¸r 2.6 Kernel zur MeÅﬂdatenerfassung, Steuerung und Regelung
*           Zeitgeber ist der Timerinterrupt (tq)
*           
*           Autor: Wilhelm Hagemeister
*
*           (C) Copyright IgH 2002
*           Ingenieurgemeinschaft IgH
*           Heinz-BÅ‰cker Str. 34
*           D-45356 Essen
*           Tel.: +49 201/61 99 31
*           Fax.: +49 201/61 98 36
*           E-mail: hm@igh-essen.com
*
*
*           $RCSfile: msr_module.c,v $
*           $Revision: 1.1 $
*           $Author: hm $
*           $Date: 2005/11/14 20:32:57 $
*           $State: Exp $
*
*
*           $Log: msr_module.c,v $
*           Revision 1.1  2005/11/14 20:32:57  hm
*           Initial revision
*
*           Revision 1.13  2005/06/17 11:35:13  hm
*           *** empty log message ***
*
*
*
*
**************************************************************************************************/


/*--includes-------------------------------------------------------------------------------------*/
 

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/config.h>
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h> 
#include <linux/fs.h>     /* everything... */
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/timex.h>  /* fuer get_cycles */
#include <linux/errno.h>  /* error codes */
#include <asm/msr.h> /* maschine-specific registers */
#include <linux/param.h> /* fuer HZ */
#include <linux/ipipe.h>

#include "msr_param.h"   //wird im Projektverzeichnis erwartet

//#include <msr_control.h>
#include <msr_lists.h>
#include <msr_charbuf.h>
#include <msr_reg.h>
#include <msr_error_reg.h>
#include <msr_messages.h>
#include <msr_proc.h>
#include <msr_utils.h>
#include <msr_main.h>


#include <msr_float.h>

#include "../drivers/ec_master.h"
#include "../drivers/ec_device.h"
#include "../drivers/ec_types.h"
#include "../drivers/ec_module.h"

#include "msr_jitter.h"

MODULE_AUTHOR("Wilhelm Hagemeister, Ingenieurgemeinschaft IgH");
MODULE_LICENSE("GPL");

/*--external functions---------------------------------------------------------------------------*/

/*--external data--------------------------------------------------------------------------------*/

#define HZREDUCTION (MSR_ABTASTFREQUENZ/HZ)

extern wait_queue_head_t msr_read_waitqueue;

extern struct msr_char_buf *msr_kanal_puffer;

extern int proc_abtastfrequenz;

/*--public data----------------------------------------------------------------------------------*/
/*--local data-----------------------------------------------------------------------------------*/
//struct timer_list timer;

extern struct timeval process_time;           
struct timeval msr_time_increment;                    // Increment per Interrupt

//adeos

static struct ipipe_domain this_domain;

static struct ipipe_sysinfo sys_info;

static EtherCAT_master_t *ecat_master = NULL;

static EtherCAT_slave_t ecat_slaves[] =
{


#if 1
    // Block 1
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

    // Block 2
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

#if 1
    // Block 3
   ,ECAT_INIT_SLAVE(Beckhoff_EK1100),
    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL3162),
    ECAT_INIT_SLAVE(Beckhoff_EL3102),
    ECAT_INIT_SLAVE(Beckhoff_EL3102),
    ECAT_INIT_SLAVE(Beckhoff_EL3102),
    ECAT_INIT_SLAVE(Beckhoff_EL3102),

    ECAT_INIT_SLAVE(Beckhoff_EL4102),
    ECAT_INIT_SLAVE(Beckhoff_EL4102),
    ECAT_INIT_SLAVE(Beckhoff_EL4102),
    ECAT_INIT_SLAVE(Beckhoff_EL4102)


#endif
};

#define ECAT_SLAVES_COUNT (sizeof(ecat_slaves) / sizeof(EtherCAT_slave_t))

#define USE_MSR_LIB

#ifdef USE_MSR_LIB
double value;
int dig1;
#endif

/******************************************************************************
 *
 * Function: next2004
 *
 *****************************************************************************/



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


/******************************************************************************
 *
 * Function: msr_controller_run()
 *
 *****************************************************************************/

static void msr_controller_run(void)
{
    static int ms = 0;
    static int cnt = 0;
    static unsigned long int k = 0;
    static int firstrun = 1;

    static int klemme = 0;
    static int kanal = 0;
    static int up_down = 0;
    int wrap = 0;


    // Prozessdaten lesen
    msr_jitter_run(MSR_ABTASTFREQUENZ);

    if (!firstrun)
    {
        EtherCAT_read_process_data(ecat_master);

        // Daten lesen und skalieren
#ifdef USE_MSR_LIB
        value = EtherCAT_read_value(&ecat_master->slaves[5], 0) / 3276.0; 
        dig1 = EtherCAT_read_value(&ecat_master->slaves[2], 0);
#endif
    }
    else
        klemme = next2004(&wrap);


    ms++;
    ms %= 1000;
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

}

/*
***************************************************************************************************
*
* Function: msr_run(_interrupt)
*
* Beschreibung: Routine wird zyklisch im Timerinterrupt ausgefÅ¸hrt
*               (hier muÅﬂ alles rein, was Echtzeit ist ...)
*
* Parameter: Zeiger auf msr_data
*
* RÅ¸ckgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/


void msr_run(unsigned irq)
{

    static int counter = 0;
#ifdef USE_MSR_LIB

    timeval_add(&process_time,&process_time,&msr_time_increment); 

    MSR_ADEOS_INTERRUPT_CODE(
	msr_controller_run();
	msr_write_kanal_list();
	);
#else
    msr_controller_run();
#endif
    /* und wieder in die Timerliste eintragen */
    /* und neu in die Taskqueue eintragen */    
//    timer.expires += 1;
//    add_timer(&timer);

    ipipe_control_irq(irq,0,IPIPE_ENABLE_MASK);  //nicht weiterreichen
    if(counter++ > HZREDUCTION) {
	ipipe_propagate_irq(irq);  //wie lange braucht der Rest der Pipeline ??
	counter = 0;
    }


}

void domain_entry (int iflag) {
    printk("Domain %s started.\n",	ipipe_current_domain->name);


    ipipe_get_sysinfo(&sys_info);
    ipipe_virtualize_irq(ipipe_current_domain,sys_info.archdep.tmirq,
			 &msr_run, NULL, IPIPE_HANDLE_MASK);

    ipipe_tune_timer(1000000000UL/MSR_ABTASTFREQUENZ,0); 

}

/*
*******************************************************************************
*
* Function: msr_register_channels
*
* Beschreibung: Kané‰le registrieren
*
* Parameter:
*
* Ré¸ckgabe: 
*               
* Status: exp
*
*******************************************************************************
*/

int msr_globals_register(void)
{
#ifdef USE_MSR_LIB
    msr_reg_kanal("/value", "V", &value, TDBL);
    msr_reg_kanal("/dig1", "", &dig1, TINT);
#endif
/*  msr_reg_kanal("/Taskinfo/Ecat/TX-Delay","us",&ecat_tx_delay,TUINT);
  msr_reg_kanal("/Taskinfo/Ecat/RX-Delay","us",&ecat_rx_delay,TUINT);
  msr_reg_kanal("/Taskinfo/Ecat/TX-Cnt","",&tx_intr,TUINT);
  msr_reg_kanal("/Taskinfo/Ecat/RX-Cnt","",&rx_intr,TUINT);
  msr_reg_kanal("/Taskinfo/Ecat/Total-Cnt","",&total_intr,TUINT);
*/
  return 0;
}


/****************************************************************************************************
 * the init/clean material
 ****************************************************************************************************/


int __init init_module()
{
    int result = 0;

    struct ipipe_domain_attr attr; //ipipe

    //als allererstes die RT-lib initialisieren    
#ifdef USE_MSR_LIB
    result = msr_rtlib_init(1,MSR_ABTASTFREQUENZ,10,&msr_globals_register); 

    if (result < 0) {
        msr_print_warn("msr_modul: can't initialize rtlib!");
        return result;
    }
#endif

    msr_jitter_init();
  printk(KERN_INFO "=== Starting EtherCAT environment... ===\n");

  if ((ecat_master = EtherCAT_master(0)) == NULL)
  {
    printk(KERN_ERR "No EtherCAT master available!\n");
    msr_rtlib_cleanup();    
    return -1;
  }

  printk("Checking EtherCAT slaves.\n");

  if (EtherCAT_check_slaves(ecat_master, ecat_slaves, ECAT_SLAVES_COUNT) != 0)
  {
    printk(KERN_ERR "EtherCAT: Could not init slaves!\n");
    msr_rtlib_cleanup();    
    return -1;
  }

  printk("Activating all EtherCAT slaves.\n");

  if (EtherCAT_activate_all_slaves(ecat_master) != 0)
  {
    printk(KERN_ERR "EtherCAT: Could not activate slaves!\n");
    msr_rtlib_cleanup();    
    return -1;
  }


  do_gettimeofday(&process_time);			       
  msr_time_increment.tv_sec=0;
  msr_time_increment.tv_usec=(unsigned int)(1000000/MSR_ABTASTFREQUENZ);

    ipipe_init_attr (&attr);
    attr.name     = "IPIPE-MSR-MODULE";
    attr.priority = IPIPE_ROOT_PRIO + 1;
    attr.entry    = &domain_entry;
    ipipe_register_domain(&this_domain,&attr);

    //den Timertakt
/*
  init_timer(&timer);

  timer.function = msr_run;
  timer.data = 0;
  timer.expires = jiffies+10; // Das erste Mal sofort feuern
  add_timer(&timer);
*/
  return 0; /* succeed */
}


//****************************************************************************
void __exit cleanup_module()

{
    msr_print_info("msk_modul: unloading...");


//    del_timer_sync(&timer);
    ipipe_tune_timer(1000000000UL/HZ,0); //alten Timertakt wieder herstellen

    ipipe_unregister_domain(&this_domain);



    printk(KERN_INFO "=== Stopping EtherCAT environment... ===\n");

    if (ecat_master)
    {
      EtherCAT_clear_process_data(ecat_master);
      printk(KERN_INFO "Deactivating slaves.\n");
      EtherCAT_deactivate_all_slaves(ecat_master);
    }

    printk(KERN_INFO "=== EtherCAT environment stopped. ===\n");

//    msr_controller_cleanup(); 
#ifdef USE_MSR_LIB
    msr_rtlib_cleanup();    
#endif
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Wilhelm Hagemeister <hm@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT test environment");

module_init(init_module);
module_exit(cleanup_module);
 















