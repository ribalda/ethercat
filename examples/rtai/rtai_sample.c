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

// EtherCAT
#include "../../include/ecrt.h"
#include "../../include/ecdb.h"

/*****************************************************************************/

// RTAI task frequency in Hz
#define FREQUENCY 4000
#define INHIBIT_TIME 20

#define TIMERTICKS (1000000000 / FREQUENCY)

/*****************************************************************************/

// RTAI
RT_TASK task;
SEM master_sem;
cycles_t t_last_cycle = 0, t_critical;

// EtherCAT
ec_master_t *master = NULL;
ec_domain_t *domain1 = NULL;

// data fields
void *r_dig_out;

ec_pdo_reg_t domain1_pdos[] = {
    {"2", Beckhoff_EL2004_Outputs, &r_dig_out},
    {}
};

/*****************************************************************************/

void run(long data)
{
    static unsigned int blink = 0;
    static unsigned int counter = 0;

    while (1) {
        t_last_cycle = get_cycles();

        rt_sem_wait(&master_sem);
        ecrt_master_receive(master);
        ecrt_domain_process(domain1);
        rt_sem_signal(&master_sem);

        // process data
        EC_WRITE_U8(r_dig_out, blink ? 0x0F : 0x00);

        rt_sem_wait(&master_sem);
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);
        rt_sem_signal(&master_sem);
		
        if (counter) {
            counter--;
        }
        else {
            counter = FREQUENCY;
            blink = !blink;
        }

        rt_task_wait_period();
    }
}

/*****************************************************************************/

int request_lock(void *data)
{
    // too close to the next real time cycle: deny access...
    if (get_cycles() - t_last_cycle > t_critical) return -1;

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
    RTIME tick_period, requested_ticks, now;

    printk(KERN_INFO "=== Starting EtherCAT RTAI sample module... ===\n");

    rt_sem_init(&master_sem, 1);

    t_critical = cpu_khz * 1000 / FREQUENCY - cpu_khz * INHIBIT_TIME / 1000;

    if (!(master = ecrt_request_master(0))) {
        printk(KERN_ERR "Requesting master 0 failed!\n");
        goto out_return;
    }

    ecrt_master_callbacks(master, request_lock, release_lock, NULL);

    printk(KERN_INFO "Creating domain...\n");
    if (!(domain1 = ecrt_master_create_domain(master))) {
        printk(KERN_ERR "Domain creation failed!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Registering PDOs...\n");
    if (ecrt_domain_register_pdo_list(domain1, domain1_pdos)) {
        printk(KERN_ERR "PDO registration failed!\n");
        goto out_release_master;
    }

    printk(KERN_INFO "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR "Failed to activate master!\n");
        goto out_release_master;
    }

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

    rt_task_delete(&task);
    stop_rt_timer();
    ecrt_release_master(master);
    rt_sem_delete(&master_sem);

    printk(KERN_INFO "=== EtherCAT RTAI sample module stopped. ===\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT RTAI sample module");

module_init(init_mod);
module_exit(cleanup_mod);

/*****************************************************************************/
