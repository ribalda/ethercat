/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
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
 *  The right to use EtherCAT Technology is granted and comes free of
 *  charge under condition of compatibility of product made by
 *  Licensee. People intending to distribute/sell products based on the
 *  code, have to sign an agreement to guarantee that products using
 *  software based on IgH EtherCAT master stay compatible with the actual
 *  EtherCAT specification (which are released themselves as an open
 *  standard) as the (only) precondition to have the right to use EtherCAT
 *  Technology, IP and trade marks.
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

// EtherCAT
#include "../../include/ecrt.h"
#include "../../include/ecdb.h"

#define MSR_ABTASTFREQUENZ 1000

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
void *r_ana_out;

// channels
double k_ana_out;

ec_pdo_reg_t domain1_pdos[] = {
    {"3", Beckhoff_EL4132_Output1, &r_ana_out},
    {}
};

/*****************************************************************************/

void msr_controller_run(void)
{
    // receive
    rt_sem_wait(&master_sem);
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);
    rt_sem_signal(&master_sem);

    // Process data
    EC_WRITE_S16(r_ana_out, k_ana_out / 10.0 * 0x7FFF);

    // Send
    rt_sem_wait(&master_sem);
    ecrt_domain_queue(domain1);
    ecrt_master_run(master);
    ecrt_master_send(master);
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
    msr_reg_kanal("/ana_out", "", &k_ana_out, TDBL);
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

    printk(KERN_INFO "=== Starting EtherCAT RTAI MSR sample module... ===\n");

    rt_sem_init(&master_sem, 1);
    t_critical = cpu_khz * 800 / MSR_ABTASTFREQUENZ; // ticks for 80%

    if (msr_rtlib_init(1, MSR_ABTASTFREQUENZ, 10, &msr_reg) < 0) {
        printk(KERN_ERR "Failed to initialize rtlib!\n");
        goto out_return;
    }

    if (!(master = ecrt_request_master(0))) {
        printk(KERN_ERR "Failed to request master 0!\n");
        goto out_msr_cleanup;
    }

    ecrt_master_callbacks(master, request_lock, release_lock, NULL);

    printk(KERN_INFO "Creating domains...\n");
    if (!(domain1 = ecrt_master_create_domain(master))) {
        printk(KERN_ERR "Failed to create domains!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Registering PDOs...\n");
    if (ecrt_domain_register_pdo_list(domain1, domain1_pdos)) {
        printk(KERN_ERR "Failed to register PDOs.\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR "Could not activate master!\n");
        goto out_release_master;
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
    ecrt_release_master(master);
    rt_sem_delete(&master_sem);
    msr_rtlib_cleanup();

    printk(KERN_INFO "=== EtherCAT RTAI MSR sample module unloaded. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT RTAI MSR sample module");

module_init(init_mod);
module_exit(cleanup_mod);

/*****************************************************************************/
