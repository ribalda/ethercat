/******************************************************************************
 *
 *  Sample module for use with IgH MSR library.
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

// RTAI
#include "rtai_sched.h"
#include "rtai_sem.h"

// RT_lib
#include <msr_main.h>
#include <msr_reg.h>
#include <msr_time.h>
#include "msr_param.h"

// EtherCAT
#include "../../include/ecrt.h"

#define ASYNC

#define HZREDUCTION (MSR_ABTASTFREQUENZ / HZ)
#define TIMERTICKS (1000000000 / MSR_ABTASTFREQUENZ)

/*****************************************************************************/

// RTAI
RT_TASK task;
SEM master_sem;
cycles_t t_start = 0, t_critical;

// EtherCAT
ec_master_t *master = NULL;
ec_domain_t *domain1 = NULL;

// raw process data
void *r_ssi;
void *r_ssi_st;

// Channels
uint32_t k_ssi;
uint32_t k_ssi_st;

ec_field_init_t domain1_fields[] = {
    {&r_ssi,    "0:3", "Beckhoff", "EL5001", "InputValue", 0},
    {&r_ssi_st, "0:3", "Beckhoff", "EL5001", "Status",     0},
    {}
};

/*****************************************************************************/

void msr_controller_run(void)
{
    rt_sem_wait(&master_sem);

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
    k_ssi    = EC_READ_U32(r_ssi);
    k_ssi_st = EC_READ_U8 (r_ssi_st);

#ifdef ASYNC
    // Senden
    ecrt_domain_queue(domain1);
    ecrt_master_run(master);
    ecrt_master_async_send(master);
#endif

    rt_sem_signal(&master_sem);

    msr_write_kanal_list();
}

/*****************************************************************************/

void msr_run(long data)
{
    while (1) {
        t_start = get_cycles();
        MSR_RTAITHREAD_CODE(msr_controller_run(););
        rt_task_wait_period();
    }
}

/*****************************************************************************/

int msr_reg(void)
{
    msr_reg_kanal("/ssi_position", "", &k_ssi,    TUINT);
    msr_reg_kanal("/ssi_status",   "", &k_ssi_st, TUINT);
    return 0;
}

/*****************************************************************************/

int request_lock(void *data)
{
    // too close to the next RT cycle: deny access...
    if (get_cycles() - t_start > t_critical) return -1;

    // allow access
    rt_sem_wait(&master_sem);
    return 0;
}

/*****************************************************************************/

void release_lock(void *data)
{
    rt_sem_signal(&master_sem);
}

/*****************************************************************************/

int __init init_mod(void)
{
    RTIME ticks;
#if 0
    ec_slave_t *slave;
#endif

    printk(KERN_INFO "=== Starting EtherCAT RTAI MSR sample module... ===\n");

    rt_sem_init(&master_sem, 1);
    t_critical = cpu_khz * 800 / MSR_ABTASTFREQUENZ; // ticks for 80%

    if (msr_rtlib_init(1, MSR_ABTASTFREQUENZ, 10, &msr_reg) < 0) {
        printk(KERN_ERR "Failed to initialize rtlib!\n");
        goto out_return;
    }

    if ((master = ecrt_request_master(0)) == NULL) {
        printk(KERN_ERR "Failed to request master 0!\n");
        goto out_msr_cleanup;
    }

    ecrt_master_callbacks(master, request_lock, release_lock, NULL);

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
    if (ecrt_master_fetch_sdo_lists(master)) {
        printk(KERN_ERR "Failed to fetch SDO lists!\n");
        goto out_deactivate;
    }
    ecrt_master_print(master, 2);
#else
    ecrt_master_print(master, 0);
#endif

#if 0
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

    if (ecrt_master_start_eoe(master)) {
        printk(KERN_ERR "Failed to start EoE processing!\n");
        goto out_deactivate;
    }

    printk("Starting cyclic sample thread...\n");
    ticks = start_rt_timer(nano2count(TIMERTICKS));
    if (rt_task_init(&task, msr_run, 0, 2000, 0, 1, NULL)) {
        printk(KERN_ERR "Failed to init RTAI task!\n");
        goto out_stop_timer;
    }
    if (rt_task_make_periodic(&task, rt_get_time() + ticks, ticks)) {
        printk(KERN_ERR "Failed to run RTAI task!\n");
        goto out_stop_task;
    }

    printk(KERN_INFO "=== EtherCAT RTAI MSR sample module started. ===\n");
    return 0;

 out_stop_task:
    rt_task_delete(&task);
 out_stop_timer:
    stop_rt_timer();
 out_deactivate:
    ecrt_master_deactivate(master);
 out_release_master:
    ecrt_release_master(master);
 out_msr_cleanup:
    msr_rtlib_cleanup();
 out_return:
    rt_sem_delete(&master_sem);
    return -1;
}

/*****************************************************************************/

void __exit cleanup_mod(void)
{
    printk(KERN_INFO "=== Unloading EtherCAT RTAI MSR sample module... ===\n");

    rt_task_delete(&task);
    stop_rt_timer();
    ecrt_master_deactivate(master);
    ecrt_release_master(master);
    rt_sem_delete(&master_sem);
    msr_rtlib_cleanup();

    printk(KERN_INFO "=== EtherCAT RTAI MSR sample module unloaded. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT RTAI MSR sample module");

module_init(init_mod);
module_exit(cleanup_mod);

/*****************************************************************************/
