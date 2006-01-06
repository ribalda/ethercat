/******************************************************************************
 *
 *  msr_module.c
 *
 *  Kernelmodul fÅ¸r 2.6 Kernel zur MeÅﬂdatenerfassung, Steuerung und Regelung.
 *
 *  Autor: Wilhelm Hagemeister, Florian Pose
 *
 *  (C) Copyright IgH 2002
 *  Ingenieurgemeinschaft IgH
 *  Heinz-BÅ‰cker Str. 34
 *  D-45356 Essen
 *  Tel.: +49 201/61 99 31
 *  Fax.: +49 201/61 98 36
 *  E-mail: hm@igh-essen.com
 *
 *  $Id$
 *
 *****************************************************************************/

// Linux
#include <linux/module.h>
#include <linux/ipipe.h>

// RT_lib
#include <msr_main.h>
#include <msr_utils.h>
#include <msr_messages.h>
#include <msr_float.h>
#include <msr_reg.h>
#include "msr_param.h"
#include "msr_jitter.h"

// EtherCAT
#include "../drivers/ec_master.h"
#include "../drivers/ec_device.h"
#include "../drivers/ec_types.h"
#include "../drivers/ec_module.h"

// Defines/Makros
#define TSC2US(T1, T2) ((T2 - T1) * 1000UL / cpu_khz)
#define HZREDUCTION (MSR_ABTASTFREQUENZ / HZ)

/*****************************************************************************/
/* Globale Variablen */

// RT_lib
extern struct timeval process_time;
struct timeval msr_time_increment; // Increment per Interrupt

// Adeos
static struct ipipe_domain this_domain;
static struct ipipe_sysinfo sys_info;

// EtherCAT

static EtherCAT_master_t *ecat_master = NULL;
static unsigned int ecat_bus_time = 0;
static unsigned int ecat_timeouts = 0;

static EtherCAT_slave_t ecat_slaves[] =
{
    // Block 1
    ECAT_INIT_SLAVE(Beckhoff_EK1100, 0),
    ECAT_INIT_SLAVE(Beckhoff_EL4102, 0),
    ECAT_INIT_SLAVE(Beckhoff_EL1014, 0),
    ECAT_INIT_SLAVE(Beckhoff_EL3162, 0),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 0),
    ECAT_INIT_SLAVE(Beckhoff_EL3102, 0),
    ECAT_INIT_SLAVE(Beckhoff_EL2004, 0),

    // Block 2
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
    static int firstrun = 1;

    static int klemme = 0;
    static int kanal = 0;
    static int up_down = 0;
    int wrap = 0;

    static unsigned int debug_counter = 0;
    unsigned long t1, t2, t3;
    unsigned int bustime1, bustime2;
    static unsigned long lt = 0;

    rdtscl(t1);

    // Prozessdaten lesen
    msr_jitter_run(MSR_ABTASTFREQUENZ);

    if (firstrun) klemme = next2004(&wrap);

    ms++;
    ms %= 1000;
    if (cnt++ > 200)
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
        EtherCAT_write_value(&ecat_slaves[klemme], kanal, up_down);
    }

    // Prozessdaten schreiben

    rdtscl(t2);

    if (EtherCAT_process_data_cycle(ecat_master, 0, 40) < 0)
        ecat_timeouts++;
    bustime1 = ecat_master->bus_time;

    if (EtherCAT_process_data_cycle(ecat_master, 1, 40) < 0)
        ecat_timeouts++;
    bustime2 = ecat_master->bus_time;

    rdtscl(t3);

    ecat_bus_time = TSC2US(t2, t3);

    // Daten lesen und skalieren
#ifdef USE_MSR_LIB
    value = EtherCAT_read_value(&ecat_slaves[5], 0) / 3276.0;
    dig1 = EtherCAT_read_value(&ecat_slaves[2], 0);
#endif

    if (debug_counter == MSR_ABTASTFREQUENZ) {
      printk(KERN_DEBUG "%lu: %luéµs + %uéµs + %uéµs = %luéµs\n", TSC2US(lt, t1),
             TSC2US(t1, t2), bustime1, bustime2, TSC2US(t1, t3));
      debug_counter = 0;
    }

    lt = t1;

    firstrun = 0;
    debug_counter++;
}

/******************************************************************************
 *
 *  Function: msr_run(_interrupt)
 *
 *  Beschreibung: Routine wird zyklisch im Timerinterrupt ausgefÅ¸hrt
 *                (hier muÅﬂ alles rein, was Echtzeit ist ...)
 *
 *  Parameter: Zeiger auf msr_data
 *
 *  RÅ¸ckgabe:
 *
 *  Status: exp
 *
 *****************************************************************************/

void msr_run(unsigned irq)
{
    static int counter = 0;

#ifdef USE_MSR_LIB
    timeval_add(&process_time, &process_time, &msr_time_increment);
    MSR_ADEOS_INTERRUPT_CODE(msr_controller_run(); msr_write_kanal_list(););
#else
    msr_controller_run();
#endif

    ipipe_control_irq(irq,0,IPIPE_ENABLE_MASK);  //Interrupt besté‰tigen
    if (counter++ > HZREDUCTION) {
	ipipe_propagate_irq(irq);  //und weiterreichen
	counter = 0;
    }
}

/*****************************************************************************/

void domain_entry(void)
{
    printk("Domain %s started.\n", ipipe_current_domain->name);

    ipipe_get_sysinfo(&sys_info);
    ipipe_virtualize_irq(ipipe_current_domain,sys_info.archdep.tmirq,
			 &msr_run, NULL, IPIPE_HANDLE_MASK);

    ipipe_tune_timer(1000000000UL/MSR_ABTASTFREQUENZ,0);
}

/******************************************************************************
 *
 *  Function: msr_register_channels
 *
 *  Beschreibung: Kané‰le registrieren
 *
 *  Parameter:
 *
 *  Ré¸ckgabe:
 *
 *  Status: exp
 *
 *****************************************************************************/

int msr_globals_register(void)
{
#ifdef USE_MSR_LIB
    msr_reg_kanal("/value", "V", &value, TDBL);
    msr_reg_kanal("/dig1", "", &dig1, TINT);
#endif

    msr_reg_kanal("/Taskinfo/EtherCAT/BusTime", "us", &ecat_bus_time, TUINT);
    msr_reg_kanal("/Taskinfo/EtherCAT/Timeouts", "", &ecat_timeouts, TUINT);

    return 0;
}

/******************************************************************************
 * the init/clean material
 *****************************************************************************/

int __init init_module()
{
    unsigned int i;
    struct ipipe_domain_attr attr; //ipipe

    // Als allererstes die RT-lib initialisieren
#ifdef USE_MSR_LIB
    if (msr_rtlib_init(1,MSR_ABTASTFREQUENZ,10,&msr_globals_register) < 0) {
        msr_print_warn("msr_modul: can't initialize rtlib!");
        goto out_return;
    }
#endif

    msr_jitter_init();

    printk(KERN_INFO "=== Starting EtherCAT environment... ===\n");

    if ((ecat_master = EtherCAT_request(0)) == NULL) {
        printk(KERN_ERR "EtherCAT master 0 not available!\n");
        goto out_msr_cleanup;
    }

    printk("Checking EtherCAT slaves.\n");

    if (EtherCAT_check_slaves(ecat_master, ecat_slaves, ECAT_SLAVES_COUNT) != 0) {
        printk(KERN_ERR "EtherCAT: Could not init slaves!\n");
        goto out_release_master;
    }

    printk("Activating all EtherCAT slaves.\n");

    for (i = 0; i < ECAT_SLAVES_COUNT; i++) {
        if (EtherCAT_activate_slave(ecat_master, ecat_slaves + i) < 0) {
            printk(KERN_ERR "EtherCAT: Could not activate slave %i!\n", i);
            goto out_release_master;
        }
    }

    do_gettimeofday(&process_time);
    msr_time_increment.tv_sec=0;
    msr_time_increment.tv_usec=(unsigned int)(1000000/MSR_ABTASTFREQUENZ);

    ipipe_init_attr (&attr);
    attr.name     = "IPIPE-MSR-MODULE";
    attr.priority = IPIPE_ROOT_PRIO + 1;
    attr.entry    = &domain_entry;
    ipipe_register_domain(&this_domain,&attr);

    return 0;

 out_release_master:
    EtherCAT_release(ecat_master);

 out_msr_cleanup:
    msr_rtlib_cleanup();

 out_return:
    return -1;
}

/*****************************************************************************/

void __exit cleanup_module()
{
    unsigned int i;

    msr_print_info("msk_modul: unloading...");

    ipipe_tune_timer(1000000000UL / HZ, 0); //alten Timertakt wieder herstellen
    ipipe_unregister_domain(&this_domain);

    if (ecat_master)
    {
        printk(KERN_INFO "=== Stopping EtherCAT environment... ===\n");

        printk(KERN_INFO "Deactivating slaves.\n");

        for (i = 0; i < ECAT_SLAVES_COUNT; i++) {
            if (EtherCAT_deactivate_slave(ecat_master, ecat_slaves + i) < 0) {
                printk(KERN_WARNING "Warning - Could not deactivate slave!\n");
            }
        }

        EtherCAT_release(ecat_master);

        printk(KERN_INFO "=== EtherCAT environment stopped. ===\n");
    }

#ifdef USE_MSR_LIB
    msr_rtlib_cleanup();
#endif
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Wilhelm Hagemeister <hm@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT test environment");

module_init(init_module);
module_exit(cleanup_module);

/*****************************************************************************/
