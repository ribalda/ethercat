/******************************************************************************
 *
 *  Distributed clocks sample for the IgH EtherCAT master.
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

// Linux
#include <linux/module.h>
#include <linux/err.h>

// RTAI
#include <rtai_sched.h>
#include <rtai_sem.h>

// EtherCAT
#include "../../include/ecrt.h"

/*****************************************************************************/

// Module parameters

#define FREQUENCY 1000 // task frequency in Hz
#define INHIBIT_TIME 20

#define TIMERTICKS (1000000000 / FREQUENCY)

#define NUM_DIG_OUT 1

#define PFX "ec_dc_rtai_sample: "

/*****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};

// RTAI
static RT_TASK task;
static SEM master_sem;
static cycles_t t_last_cycle = 0, t_critical;

/*****************************************************************************/

// process data
static uint8_t *domain1_pd; // process data memory

#define DigOutSlavePos(X) 0, (1 + (X))
#define CounterSlavePos   0, 2

#define Beckhoff_EK1100 0x00000002, 0x044c2c52
#define Beckhoff_EL2008 0x00000002, 0x07d83052
#define IDS_Counter     0x000012ad, 0x05de3052

static int off_dig_out[NUM_DIG_OUT];
static int off_counter_in;
static int off_counter_out;

static unsigned int counter = 0;
static unsigned int blink_counter = 0;
static unsigned int blink = 0;
static u32 counter_value = 0U;

/*****************************************************************************/

static ec_pdo_entry_info_t el2008_channels[] = {
    {0x7000, 1, 1},
    {0x7010, 1, 1},
    {0x7020, 1, 1},
    {0x7030, 1, 1},
    {0x7040, 1, 1},
    {0x7050, 1, 1},
    {0x7060, 1, 1},
    {0x7070, 1, 1}
};

static ec_pdo_info_t el2008_pdos[] = {
    {0x1600, 1, &el2008_channels[0]},
    {0x1601, 1, &el2008_channels[1]},
    {0x1602, 1, &el2008_channels[2]},
    {0x1603, 1, &el2008_channels[3]},
    {0x1604, 1, &el2008_channels[4]},
    {0x1605, 1, &el2008_channels[5]},
    {0x1606, 1, &el2008_channels[6]},
    {0x1607, 1, &el2008_channels[7]}
};

static ec_sync_info_t el2008_syncs[] = {
    {0, EC_DIR_OUTPUT, 8, el2008_pdos},
    {1, EC_DIR_INPUT},
    {0xff}
};

/*****************************************************************************/

void check_domain1_state(void)
{
    ec_domain_state_t ds;

    rt_sem_wait(&master_sem);
    ecrt_domain_state(domain1, &ds);
    rt_sem_signal(&master_sem);

    if (ds.working_counter != domain1_state.working_counter)
        printk(KERN_INFO PFX "Domain1: WC %u.\n", ds.working_counter);
    if (ds.wc_state != domain1_state.wc_state)
        printk(KERN_INFO PFX "Domain1: State %u.\n", ds.wc_state);

    domain1_state = ds;
}

/*****************************************************************************/

void check_master_state(void)
{
    ec_master_state_t ms;

    rt_sem_wait(&master_sem);
    ecrt_master_state(master, &ms);
    rt_sem_signal(&master_sem);

    if (ms.slaves_responding != master_state.slaves_responding)
        printk(KERN_INFO PFX "%u slave(s).\n", ms.slaves_responding);
    if (ms.al_states != master_state.al_states)
        printk(KERN_INFO PFX "AL states: 0x%02X.\n", ms.al_states);
    if (ms.link_up != master_state.link_up)
        printk(KERN_INFO PFX "Link is %s.\n", ms.link_up ? "up" : "down");

    master_state = ms;
}

/*****************************************************************************/

void run(long data)
{
    int i;
    struct timeval tv;
    unsigned int sync_ref_counter = 0;

    count2timeval(nano2count(rt_get_real_time_ns()), &tv);

    while (1) {
        t_last_cycle = get_cycles();

        // receive process data
        rt_sem_wait(&master_sem);
        ecrt_master_receive(master);
        ecrt_domain_process(domain1);
        rt_sem_signal(&master_sem);

        // check process data state (optional)
        check_domain1_state();

        if (counter) {
            counter--;
        } else {
            u32 c;

            counter = FREQUENCY;

            // check for master state (optional)
            check_master_state();

            c = EC_READ_U32(domain1_pd + off_counter_in);
            if (counter_value != c) {
                counter_value = c;
                printk(KERN_INFO PFX "counter=%u\n", counter_value);
            }

        }

        if (blink_counter) {
            blink_counter--;
        } else {
            blink_counter = 9;

            // calculate new process data
            blink = !blink;
        }

        // write process data
        for (i = 0; i < NUM_DIG_OUT; i++) {
            EC_WRITE_U8(domain1_pd + off_dig_out[i], blink ? 0x66 : 0x99);
        }

        EC_WRITE_U8(domain1_pd + off_counter_out, blink ? 0x00 : 0x02);

        rt_sem_wait(&master_sem);

        tv.tv_usec += 1000;
        if (tv.tv_usec >= 1000000)  {
            tv.tv_usec -= 1000000;
            tv.tv_sec++;
        }
        ecrt_master_application_time(master, EC_TIMEVAL2NANO(tv));

        if (sync_ref_counter) {
            sync_ref_counter--;
        } else {
            sync_ref_counter = 9;
            ecrt_master_sync_reference_clock(master);
        }
        ecrt_master_sync_slave_clocks(master);
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);
        rt_sem_signal(&master_sem);

        rt_task_wait_period();
    }
}

/*****************************************************************************/

void send_callback(void *cb_data)
{
    ec_master_t *m = (ec_master_t *) cb_data;

    // too close to the next real time cycle: deny access...
    if (get_cycles() - t_last_cycle <= t_critical) {
        rt_sem_wait(&master_sem);
        ecrt_master_send_ext(m);
        rt_sem_signal(&master_sem);
    }
}

/*****************************************************************************/

void receive_callback(void *cb_data)
{
    ec_master_t *m = (ec_master_t *) cb_data;

    // too close to the next real time cycle: deny access...
    if (get_cycles() - t_last_cycle <= t_critical) {
        rt_sem_wait(&master_sem);
        ecrt_master_receive(m);
        rt_sem_signal(&master_sem);
    }
}

/*****************************************************************************/

int __init init_mod(void)
{
    int ret = -1, i;
    RTIME tick_period, requested_ticks, now;
    ec_slave_config_t *sc;

    printk(KERN_INFO PFX "Starting...\n");

    rt_sem_init(&master_sem, 1);

    t_critical = cpu_khz * 1000 / FREQUENCY - cpu_khz * INHIBIT_TIME / 1000;

    master = ecrt_request_master(0);
    if (!master) {
        ret = -EBUSY;
        printk(KERN_ERR PFX "Requesting master 0 failed!\n");
        goto out_return;
    }

    ecrt_master_callbacks(master, send_callback, receive_callback, master);

    printk(KERN_INFO PFX "Registering domain...\n");
    if (!(domain1 = ecrt_master_create_domain(master))) {
        printk(KERN_ERR PFX "Domain creation failed!\n");
        goto out_release_master;
    }

    printk(KERN_INFO PFX "Configuring PDOs...\n");

    // create configuration for reference clock FIXME
    if (!(sc = ecrt_master_slave_config(master, 0, 0, Beckhoff_EK1100))) {
        printk(KERN_ERR PFX "Failed to get slave configuration.\n");
        goto out_release_master;
    }

    for (i = 0; i < NUM_DIG_OUT; i++) {
        if (!(sc = ecrt_master_slave_config(master,
                        DigOutSlavePos(i), Beckhoff_EL2008))) {
            printk(KERN_ERR PFX "Failed to get slave configuration.\n");
            goto out_release_master;
        }

        if (ecrt_slave_config_pdos(sc, EC_END, el2008_syncs)) {
            printk(KERN_ERR PFX "Failed to configure PDOs.\n");
            goto out_release_master;
        }

        off_dig_out[i] = ecrt_slave_config_reg_pdo_entry(sc,
                0x7000, 1, domain1, NULL);

        if (off_dig_out[i] < 0)
            goto out_release_master;
    }

    if (!(sc = ecrt_master_slave_config(master,
                    CounterSlavePos, IDS_Counter))) {
        printk(KERN_ERR PFX "Failed to get slave configuration.\n");
        goto out_release_master;
    }
    off_counter_in = ecrt_slave_config_reg_pdo_entry(sc,
            0x6020, 0x11, domain1, NULL);
    if (off_counter_in < 0)
        goto out_release_master;
    off_counter_out = ecrt_slave_config_reg_pdo_entry(sc,
            0x7020, 1, domain1, NULL);
    if (off_counter_out < 0)
        goto out_release_master;

    // configure SYNC signals for this slave
    ecrt_slave_config_dc(sc, 0x0700, 1000000, 440000, 0, 0);

    printk(KERN_INFO PFX "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR PFX "Failed to activate master!\n");
        goto out_release_master;
    }

    // Get internal process data for domain
    domain1_pd = ecrt_domain_data(domain1);

    printk(KERN_INFO PFX "Starting cyclic sample thread...\n");
    requested_ticks = nano2count(TIMERTICKS);
    tick_period = start_rt_timer(requested_ticks);
    printk(KERN_INFO PFX "RT timer started with %i/%i ticks.\n",
           (int) tick_period, (int) requested_ticks);

    if (rt_task_init(&task, run, 0, 2000, 0, 1, NULL)) {
        printk(KERN_ERR PFX "Failed to init RTAI task!\n");
        goto out_stop_timer;
    }

    now = rt_get_time();
    if (rt_task_make_periodic(&task, now + tick_period, tick_period)) {
        printk(KERN_ERR PFX "Failed to run RTAI task!\n");
        goto out_stop_task;
    }

    printk(KERN_INFO PFX "Initialized.\n");
    return 0;

 out_stop_task:
    rt_task_delete(&task);
 out_stop_timer:
    stop_rt_timer();
 out_release_master:
    printk(KERN_ERR PFX "Releasing master...\n");
    ecrt_release_master(master);
 out_return:
    rt_sem_delete(&master_sem);
    printk(KERN_ERR PFX "Failed to load. Aborting.\n");
    return ret;
}

/*****************************************************************************/

void __exit cleanup_mod(void)
{
    printk(KERN_INFO PFX "Stopping...\n");

    rt_task_delete(&task);
    stop_rt_timer();
    ecrt_release_master(master);
    rt_sem_delete(&master_sem);

    printk(KERN_INFO PFX "Unloading.\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT distributed clocks sample module");

module_init(init_mod);
module_exit(cleanup_mod);

/*****************************************************************************/
