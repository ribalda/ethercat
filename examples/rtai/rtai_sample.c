/******************************************************************************
 *
 *  RTAI sample for the IgH EtherCAT master.
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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>

#include "rtai.h"
#include "rtai_sched.h"
#include "rtai_sem.h"

#include "../../include/ecrt.h" // EtherCAT realtime interface

#define ASYNC
#define TIMERTICKS 1000000

/*****************************************************************************/

// RTAI
RT_TASK task;
SEM master_sem;

// EtherCAT
ec_master_t *master = NULL;
ec_domain_t *domain1 = NULL;

// data fields
//void *r_ssi_input, *r_ssi_status, *r_4102[3];

// channels
uint32_t k_pos;
uint8_t k_stat;

ec_field_init_t domain1_fields[] = {
    {NULL, "3", "Beckhoff", "EL5001", "InputValue",   0},
    {NULL, "2", "Beckhoff", "EL4132", "OutputValue",  0},
    {}
};

/*****************************************************************************/

void run(long data)
{
    while (1) {
        rt_sem_wait(&master_sem);

#ifdef ASYNC
        // receive
        ecrt_master_async_receive(master);
        ecrt_domain_process(domain1);
#else
        // send and receive
        ecrt_domain_queue(domain1);
        ecrt_master_run(master);
        ecrt_master_sync_io(master);
        ecrt_domain_process(domain1);
#endif

        // process data
        //k_pos = EC_READ_U32(r_ssi);

#ifdef ASYNC
        // send
        ecrt_domain_queue(domain1);
        ecrt_master_run(master);
        ecrt_master_async_send(master);
#endif

        rt_sem_signal(&master_sem);

        rt_task_wait_period();
    }
}

/*****************************************************************************/

int request_lock(void *data)
{
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
    RTIME tick_period, requested_ticks, now;

    printk(KERN_INFO "=== Starting EtherCAT RTAI sample module... ===\n");

    rt_sem_init(&master_sem, 1);

    if ((master = ecrt_request_master(0)) == NULL) {
        printk(KERN_ERR "Requesting master 0 failed!\n");
        goto out_return;
    }

    ecrt_master_callbacks(master, request_lock, release_lock, NULL);

    printk(KERN_INFO "Registering domain...\n");
    if (!(domain1 = ecrt_master_create_domain(master)))
    {
        printk(KERN_ERR "Domain creation failed!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Registering domain fields...\n");
    if (ecrt_domain_register_field_list(domain1, domain1_fields)) {
        printk(KERN_ERR "Field registration failed!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR "Failed to activate master!\n");
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
    if (!(slave = ecrt_master_get_slave(master, "5"))) {
        printk(KERN_ERR "Failed to get slave 5!\n");
        goto out_deactivate;
    }

    if (ecrt_slave_sdo_write_exp8(slave, 0x4061, 1,  0) ||
        ecrt_slave_sdo_write_exp8(slave, 0x4061, 2,  1) ||
        ecrt_slave_sdo_write_exp8(slave, 0x4061, 3,  1) ||
        ecrt_slave_sdo_write_exp8(slave, 0x4066, 0,  0) ||
        ecrt_slave_sdo_write_exp8(slave, 0x4067, 0,  4) ||
        ecrt_slave_sdo_write_exp8(slave, 0x4068, 0,  0) ||
        ecrt_slave_sdo_write_exp8(slave, 0x4069, 0, 25) ||
        ecrt_slave_sdo_write_exp8(slave, 0x406A, 0, 25) ||
        ecrt_slave_sdo_write_exp8(slave, 0x406B, 0, 50)) {
        printk(KERN_ERR "Failed to configure SSI slave!\n");
        goto out_deactivate;
    }
#endif

#if 0
    printk(KERN_INFO "Writing alias...\n");
    if (ecrt_slave_sdo_write_exp16(slave, 0xBEEF)) {
        printk(KERN_ERR "Failed to write alias!\n");
        goto out_deactivate;
    }
#endif

#ifdef ASYNC
    // send once and wait...
    ecrt_master_prepare_async_io(master);
#endif

#if 0
    if (ecrt_master_start_eoe(master)) {
        printk(KERN_ERR "Failed to start EoE processing!\n");
        goto out_deactivate;
    }
#endif

    printk("Starting cyclic sample thread...\n");
    requested_ticks = nano2count(TIMERTICKS);
    tick_period = start_rt_timer(requested_ticks);
    printk(KERN_INFO "RT timer started with %i/%i ticks.\n",
           (int) tick_period, (int) requested_ticks);

    if (rt_task_init(&task, run, 0, 2000, 0, 1, NULL)) {
        printk(KERN_ERR "Failed to init RTAI task!\n");
        goto out_stop_timer;
    }

    now = rt_get_time();
    if (rt_task_make_periodic(&task, now + tick_period, tick_period)) {
        printk(KERN_ERR "Failed to run RTAI task!\n");
        goto out_stop_task;
    }

    printk(KERN_INFO "=== EtherCAT RTAI sample module started. ===\n");
    return 0;

 out_stop_task:
    rt_task_delete(&task);
 out_stop_timer:
    stop_rt_timer();
 out_deactivate:
    ecrt_master_deactivate(master);
 out_release_master:
    ecrt_release_master(master);
 out_return:
    rt_sem_delete(&master_sem);
    return -1;
}

/*****************************************************************************/

void __exit cleanup_mod(void)
{
    printk(KERN_INFO "=== Stopping EtherCAT RTAI sample module... ===\n");

    printk(KERN_INFO "Stopping RT task...\n");
    rt_task_delete(&task);
    stop_rt_timer();
    printk(KERN_INFO "Deactivating EtherCAT master...\n");
    ecrt_master_deactivate(master);
    ecrt_release_master(master);
    rt_sem_delete(&master_sem);

    printk(KERN_INFO "=== EtherCAT RTAI sample module stopped. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT RTAI sample module");

module_init(init_mod);
module_exit(cleanup_mod);

/*****************************************************************************/

