/******************************************************************************
 *
 *  m s r _ r t . c
 *
 *  Kernelmodul fÅ¸r 2.6 Kernel zur MeÅﬂdatenerfassung, Steuerung und Regelung.
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2 of the License.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

// Linux
#include <linux/module.h>
#include <linux/ipipe.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>

// RT_lib
#include <msr_main.h>
#include <msr_utils.h>
#include <msr_messages.h>
#include <msr_float.h>
#include <msr_reg.h>
#include <msr_time.h>
#include "msr_param.h"

// EtherCAT
#include "../include/ecrt.h"

#define ASYNC

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
void *r_ssi;
void *r_ssi_st;

// Kané‰le
uint32_t k_ssi;
uint32_t k_ssi_st;

ec_field_init_t domain1_fields[] = {
    {&r_ssi,    "0:3", "Beckhoff", "EL5001", "InputValue", 0},
    {&r_ssi_st, "0:3", "Beckhoff", "EL5001", "Status",     0},
    {}
};

/*****************************************************************************/

static void msr_controller_run(void)
{
#ifdef ASYNC
    // Empfangen
    ecrt_master_async_receive(master);
    ecrt_domain_process(domain1);
#else
    // Senden und empfangen
    ecrt_domain_queue(domain1);
    ecrt_master_run(master);
    ecrt_master_sync_io(master);
    ecrt_domain_process(domain1);
#endif

    // Prozessdaten verarbeiten
    k_ssi =    EC_READ_U32(r_ssi);
    k_ssi_st = EC_READ_U8 (r_ssi_st);

#ifdef ASYNC
    // Senden
    ecrt_domain_queue(domain1);
    ecrt_master_run(master);
    ecrt_master_async_send(master);
#endif
}

/*****************************************************************************/

int msr_globals_register(void)
{
    msr_reg_kanal("/ssi_position", "", &k_ssi,    TUINT);
    msr_reg_kanal("/ssi_status",   "", &k_ssi_st, TUINT);
    return 0;
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

int __init init_rt_module(void)
{
    struct ipipe_domain_attr attr; //ipipe
#if 1
    ec_slave_t *slave;
#endif

    // Als allererstes die RT-Lib initialisieren
    if (msr_rtlib_init(1, MSR_ABTASTFREQUENZ, 10, &msr_globals_register) < 0) {
        printk(KERN_ERR "Failed to initialize rtlib!\n");
        goto out_return;
    }

    if ((master = ecrt_request_master(0)) == NULL) {
        printk(KERN_ERR "Failed to request master 0!\n");
        goto out_msr_cleanup;
    }

    //ecrt_master_print(master, 2);

    printk(KERN_INFO "Creating domains...\n");
    if (!(domain1 = ecrt_master_create_domain(master))) {
        printk(KERN_ERR "Failed to create domains!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Registering domain fields...\n");
    if (ecrt_domain_register_field_list(domain1, domain1_fields)) {
        printk(KERN_ERR "Failed to register domain fields.\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR "Could not activate master!\n");
        goto out_release_master;
    }

#if 0
    if (ecrt_master_start_eoe(master)) {
        printk(KERN_ERR "Failed to start EoE processing!\n");
        goto out_deactivate;
    }
#endif

#if 0
    if (ecrt_master_fetch_sdo_lists(master)) {
        printk(KERN_ERR "Failed to fetch SDO lists!\n");
        goto out_deactivate;
    }
    ecrt_master_print(master, 2);
#else
    ecrt_master_print(master, 0);
#endif

#if 1
    if (!(slave = ecrt_master_get_slave(master, "0:3"))) {
        printk(KERN_ERR "Failed to get slave!\n");
        goto out_deactivate;
    }

    if (
        ecrt_slave_sdo_write_exp8(slave, 0x4061, 1,  1) || // disable frame error bit
        ecrt_slave_sdo_write_exp8(slave, 0x4061, 2,  0) || // power failure bit
        ecrt_slave_sdo_write_exp8(slave, 0x4061, 3,  1) || // inhibit time
        ecrt_slave_sdo_write_exp8(slave, 0x4061, 4,  0) || // test mode
        ecrt_slave_sdo_write_exp8(slave, 0x4066, 0,  1) || // dualcode
        ecrt_slave_sdo_write_exp8(slave, 0x4067, 0,  5) || // 125kbaud
        ecrt_slave_sdo_write_exp8(slave, 0x4068, 0,  0) || // single-turn
        ecrt_slave_sdo_write_exp8(slave, 0x4069, 0, 25) || // frame size
        ecrt_slave_sdo_write_exp8(slave, 0x406A, 0, 25) || // data length
        ecrt_slave_sdo_write_exp16(slave, 0x406B, 0, 30000) // inhibit time in us
        ) {
        printk(KERN_ERR "Failed to configure SSI slave!\n");
        goto out_deactivate;
    }
#endif

#if 0
    if (!(slave = ecrt_master_get_slave(master, "1:0"))) {
        printk(KERN_ERR "Failed to get slave!\n");
        goto out_deactivate;
    }
    if (ecrt_slave_write_alias(slave, 0x5678)) {
        printk(KERN_ERR "Failed to write alias!\n");
        goto out_deactivate;
    }
#endif

#ifdef ASYNC
    // Einmal senden und warten...
    ecrt_master_prepare_async_io(master);
#endif

    ipipe_init_attr(&attr);
    attr.name = "IPIPE-MSR-MODULE";
    attr.priority = IPIPE_ROOT_PRIO + 1;
    attr.entry = &domain_entry;
    ipipe_register_domain(&this_domain, &attr);
    return 0;

#if 1
 out_deactivate:
    ecrt_master_deactivate(master);
#endif
 out_release_master:
    ecrt_release_master(master);
 out_msr_cleanup:
    msr_rtlib_cleanup();
 out_return:
    return -1;
}

/*****************************************************************************/

void __exit cleanup_rt_module(void)
{
    printk(KERN_INFO "Cleanign up rt module...\n");

    ipipe_tune_timer(1000000000UL / HZ, 0); // Alten Timertakt wiederherstellen
    ipipe_unregister_domain(&this_domain);

    printk(KERN_INFO "=== Stopping EtherCAT environment... ===\n");
    ecrt_master_deactivate(master);
    ecrt_release_master(master);
    printk(KERN_INFO "=== EtherCAT environment stopped. ===\n");

    msr_rtlib_cleanup();
}

/*****************************************************************************/

#define EC_LIT(X) #X
#define EC_STR(X) EC_LIT(X)
#define COMPILE_INFO "Revision " EC_STR(SVNREV) \
                     ", compiled by " EC_STR(USER) \
                     " at " __DATE__ " " __TIME__

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT real-time test environment");
MODULE_VERSION(COMPILE_INFO);

module_init(init_rt_module);
module_exit(cleanup_rt_module);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
