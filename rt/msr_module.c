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
#include <linux/slab.h>
#include <linux/vmalloc.h>

// RT_lib
#include <msr_main.h>
#include <msr_utils.h>
#include <msr_messages.h>
#include <msr_float.h>
#include <msr_reg.h>
#include <msr_time.h>
#include "msr_param.h"

// EtherCAT
#include "../include/EtherCAT_rt.h"
#include "../include/EtherCAT_si.h"

// Defines/Makros
#define HZREDUCTION (MSR_ABTASTFREQUENZ / HZ)

/*****************************************************************************/
/* Globale Variablen */

// Adeos
static struct ipipe_domain this_domain;
static struct ipipe_sysinfo sys_info;

// EtherCAT
ec_master_t *master = NULL;
ec_domain_t *domain1 = NULL;

// Prozessdaten
uint8_t *dig_out1;
uint16_t *ssi_value;
uint16_t *inc_value;

uint32_t angle0;

ec_field_init_t domain1_fields[] = {
    {},
    {(void **) &ssi_value,   "1", "Beckhoff", "EL5001", ec_ipvalue, 0, 1},
    {(void **) &dig_out1,    "2", "Beckhoff", "EL2004", ec_opvalue, 0, 1},
    {(void **) &inc_value, "0:4", "Beckhoff", "EL5101", ec_ipvalue, 0, 1},
    {}
};

/*****************************************************************************/

static void msr_controller_run(void)
{
    // Prozessdaten lesen und schreiben
    EtherCAT_rt_domain_xio(domain1);

    //angle0 = (uint32_t) *inc_value;
}

/*****************************************************************************/

void msr_run(unsigned irq)
{
    static int counter = 0;

    MSR_ADEOS_INTERRUPT_CODE(msr_controller_run(); msr_write_kanal_list(););

    ipipe_control_irq(irq, 0, IPIPE_ENABLE_MASK); // Interrupt besté‰tigen
    if (++counter >= HZREDUCTION) {
	ipipe_propagate_irq(irq);  // und weiterreichen
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

    ipipe_tune_timer(1000000000UL / MSR_ABTASTFREQUENZ, 0);
}

/*****************************************************************************/

int msr_globals_register(void)
{
    msr_reg_kanal("/angle0", "", &angle0, TUINT);

    return 0;
}

/*****************************************************************************/

int __init init_rt_module(void)
{
    struct ipipe_domain_attr attr; //ipipe
    const ec_field_init_t *field;
    uint32_t version;

    // Als allererstes die RT-lib initialisieren
    if (msr_rtlib_init(1, MSR_ABTASTFREQUENZ, 10, &msr_globals_register) < 0) {
        msr_print_warn("msr_modul: can't initialize rtlib!");
        goto out_return;
    }

    if ((master = EtherCAT_rt_request_master(0)) == NULL) {
        printk(KERN_ERR "Error requesting master 0!\n");
        goto out_msr_cleanup;
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
            printk(KERN_ERR "Could not register field!\n");
            goto out_release_master;
        }
    }

    printk(KERN_INFO "Activating master...\n");

    if (EtherCAT_rt_master_activate(master)) {
        printk(KERN_ERR "Could not activate master!\n");
        goto out_release_master;
    }

#if 1
    if (EtherCAT_rt_canopen_sdo_addr_read(master, "1", 0x100A, 1,
                                          &version)) {
        printk(KERN_ERR "Could not read SSI version!\n");
        goto out_release_master;
    }
    printk(KERN_INFO "Klemme 3 Software-version: %u\n", version);
#endif

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

        printk(KERN_INFO "Deactivating master...\n");

        if (EtherCAT_rt_master_deactivate(master) < 0) {
          printk(KERN_WARNING "Warning - Could not deactivate master!\n");
        }

        EtherCAT_rt_release_master(master);

        printk(KERN_INFO "=== EtherCAT environment stopped. ===\n");
    }

    msr_rtlib_cleanup();
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT real-time test environment");

module_init(init_rt_module);
module_exit(cleanup_rt_module);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
