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
#include "../include/EtherCAT_rt.h"
#include "../include/EtherCAT_si.h"

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
ec_master_t *master = NULL;
ec_slave_t *s_in1, *s_out1, *s_out2, *s_out3;

double value;
int dig1;

ec_slave_init_t slaves[] = {
    {&s_in1, 1, "Beckhoff", "EL3102", 0},
    {&s_out1, 8, "Beckhoff", "EL2004", 0},
    {&s_out2, 9, "Beckhoff", "EL2004", 0},
    {&s_out3, 10, "Beckhoff", "EL2004", 0}
};

#define SLAVE_COUNT (sizeof(slaves) / sizeof(ec_slave_init_t))

/******************************************************************************
 *
 * Function: msr_controller_run()
 *
 *****************************************************************************/

static void msr_controller_run(void)
{
    static unsigned int counter = 0;

    msr_jitter_run(MSR_ABTASTFREQUENZ);

    if (counter) {
        counter--;
    }
    else {
        // "Star Trek"-Effekte
        EC_WRITE_EL20XX(s_out1, 0, jiffies & 1);
        EC_WRITE_EL20XX(s_out1, 1, (jiffies >> 1) & 1);
        EC_WRITE_EL20XX(s_out1, 2, (jiffies >> 2) & 1);
        EC_WRITE_EL20XX(s_out1, 3, (jiffies >> 3) & 1);
        EC_WRITE_EL20XX(s_out2, 0, (jiffies >> 4) & 1);
        EC_WRITE_EL20XX(s_out2, 1, (jiffies >> 3) & 1);
        EC_WRITE_EL20XX(s_out2, 2, (jiffies >> 2) & 1);
        EC_WRITE_EL20XX(s_out2, 3, (jiffies >> 6) & 1);
        EC_WRITE_EL20XX(s_out3, 0, (jiffies >> 7) & 1);
        EC_WRITE_EL20XX(s_out3, 1, (jiffies >> 2) & 1);
        EC_WRITE_EL20XX(s_out3, 2, (jiffies >> 8) & 1);

        counter = MSR_ABTASTFREQUENZ / 4;
    }

    EC_WRITE_EL20XX(s_out3, 3, EC_READ_EL31XX(s_in1, 0) < 0);

    // Prozessdaten lesen und schreiben
    EtherCAT_rt_domain_xio(master, 0, 40);
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

    timeval_add(&process_time, &process_time, &msr_time_increment);
    MSR_ADEOS_INTERRUPT_CODE(msr_controller_run(); msr_write_kanal_list(););

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
    msr_reg_kanal("/value", "V", &value, TDBL);
    msr_reg_kanal("/dig1", "", &dig1, TINT);

    return 0;
}

/******************************************************************************
 * the init/clean material
 *****************************************************************************/

int __init init_rt_module(void)
{
    struct ipipe_domain_attr attr; //ipipe

    // Als allererstes die RT-lib initialisieren
    if (msr_rtlib_init(1,MSR_ABTASTFREQUENZ,10,&msr_globals_register) < 0) {
        msr_print_warn("msr_modul: can't initialize rtlib!");
        goto out_return;
    }

    msr_jitter_init();

    printk(KERN_INFO "=== Starting EtherCAT environment... ===\n");

    if ((master = EtherCAT_rt_request_master(0)) == NULL) {
        printk(KERN_ERR "Error requesting master 0!\n");
        goto out_msr_cleanup;
    }

    if (EtherCAT_rt_register_slave_list(master, slaves, SLAVE_COUNT)) {
        printk(KERN_ERR "EtherCAT: Could not register slaves!\n");
        goto out_release_master;
    }

    if (EtherCAT_rt_activate_slaves(master) < 0) {
        printk(KERN_ERR "EtherCAT: Could not activate slaves!\n");
        goto out_release_master;
    }

    do_gettimeofday(&process_time);
    msr_time_increment.tv_sec = 0;
    msr_time_increment.tv_usec = (unsigned int) (1000000 / MSR_ABTASTFREQUENZ);

    ipipe_init_attr(&attr);
    attr.name = "IPIPE-MSR-MODULE";
    attr.priority = IPIPE_ROOT_PRIO + 1;
    attr.entry = &domain_entry;
    ipipe_register_domain(&this_domain, &attr);

    return 0;

 out_release_master:
    EtherCAT_rt_release_master(master);

 out_msr_cleanup:
    msr_rtlib_cleanup();

 out_return:
    return -1;
}

/*****************************************************************************/

void __exit cleanup_rt_module(void)
{
    msr_print_info("msk_modul: unloading...");

    ipipe_tune_timer(1000000000UL / HZ, 0); //alten Timertakt wieder herstellen
    ipipe_unregister_domain(&this_domain);

    if (master)
    {
        printk(KERN_INFO "=== Stopping EtherCAT environment... ===\n");

        printk(KERN_INFO "Deactivating slaves.\n");

        if (EtherCAT_rt_deactivate_slaves(master) < 0) {
          printk(KERN_WARNING "Warning - Could not deactivate slaves!\n");
        }

        EtherCAT_rt_release_master(master);

        printk(KERN_INFO "=== EtherCAT environment stopped. ===\n");
    }

    msr_rtlib_cleanup();
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Wilhelm Hagemeister <hm@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT test environment");

module_init(init_rt_module);
module_exit(cleanup_rt_module);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
