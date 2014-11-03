/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C)      2011  IgH Andreas Stewering-Bone
 *                     2012  Florian Pose <fp@igh-essen.com>
 *
 *  This file is part of the IgH EtherCAT master
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT master. If not, see <http://www.gnu.org/licenses/>.
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>

#include <rtai_lxrt.h>
#include <rtdm/rtdm.h>

#include "ecrt.h"

#define rt_printf(X, Y)

#define NSEC_PER_SEC 1000000000

RT_TASK *task;

const static unsigned int cycle_us = 1000; /* 1 ms */

static int run = 1;

/****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};

static uint8_t *domain1_pd = NULL;

static ec_slave_config_t *sc_dig_out_01 = NULL;

/****************************************************************************/

// process data

#define BusCoupler01_Pos  0, 0
#define DigOutSlave01_Pos 0, 1

#define Beckhoff_EK1100 0x00000002, 0x044c2c52
#define Beckhoff_EL2004 0x00000002, 0x07d43052

// offsets for PDO entries
static unsigned int off_dig_out0 = 0;

// process data

const static ec_pdo_entry_reg_t domain1_regs[] = {
   {DigOutSlave01_Pos, Beckhoff_EL2004, 0x7000, 0x01, &off_dig_out0, NULL},
   {}
};

/****************************************************************************/

/* Slave 1, "EL2004"
 * Vendor ID:       0x00000002
 * Product code:    0x07d43052
 * Revision number: 0x00100000
 */

ec_pdo_entry_info_t slave_1_pdo_entries[] = {
   {0x7000, 0x01, 1}, /* Output */
   {0x7010, 0x01, 1}, /* Output */
   {0x7020, 0x01, 1}, /* Output */
   {0x7030, 0x01, 1}, /* Output */
};

ec_pdo_info_t slave_1_pdos[] = {
   {0x1600, 1, slave_1_pdo_entries + 0}, /* Channel 1 */
   {0x1601, 1, slave_1_pdo_entries + 1}, /* Channel 2 */
   {0x1602, 1, slave_1_pdo_entries + 2}, /* Channel 3 */
   {0x1603, 1, slave_1_pdo_entries + 3}, /* Channel 4 */
};

ec_sync_info_t slave_1_syncs[] = {
   {0, EC_DIR_OUTPUT, 4, slave_1_pdos + 0, EC_WD_ENABLE},
   {0xff}
};

/*****************************************************************************
 * Realtime task
 ****************************************************************************/

void rt_check_domain_state(void)
{
    ec_domain_state_t ds = {};

    ecrt_domain_state(domain1, &ds);

    if (ds.working_counter != domain1_state.working_counter) {
        rt_printf("Domain1: WC %u.\n", ds.working_counter);
    }

    if (ds.wc_state != domain1_state.wc_state) {
        rt_printf("Domain1: State %u.\n", ds.wc_state);
    }

    domain1_state = ds;
}

/****************************************************************************/

void rt_check_master_state(void)
{
    ec_master_state_t ms;

    ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding) {
        rt_printf("%u slave(s).\n", ms.slaves_responding);
    }

    if (ms.al_states != master_state.al_states) {
        rt_printf("AL states: 0x%02X.\n", ms.al_states);
    }

    if (ms.link_up != master_state.link_up) {
        rt_printf("Link is %s.\n", ms.link_up ? "up" : "down");
    }

    master_state = ms;
}

/****************************************************************************/

void my_cyclic(void)
{
    int cycle_counter = 0;
    int period;
    unsigned int blink = 0;

    rt_set_periodic_mode();
    period = (int) nano2count((RTIME) cycle_us * 1000);
    start_rt_timer(period);
    rt_make_hard_real_time();
    rt_task_make_periodic(task, rt_get_time() + 10 * period, period);

    while (run) {
        rt_task_wait_period();

        cycle_counter++;

        // receive EtherCAT
        ecrt_master_receive(master);
        ecrt_domain_process(domain1);

        rt_check_domain_state();

        if (!(cycle_counter % 1000)) {
            rt_check_master_state();
        }

        if (!(cycle_counter % 200)) {
            blink = !blink;
        }

        EC_WRITE_U8(domain1_pd + off_dig_out0, blink ? 0x00 : 0x0F);

        // send process data
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);
    }

    rt_make_soft_real_time();
    stop_rt_timer();
}

/****************************************************************************
 * Signal handler
 ***************************************************************************/

void signal_handler(int sig)
{
    run = 0;
}

/****************************************************************************
 * Main function
 ***************************************************************************/

int main(int argc, char *argv[])
{
    ec_slave_config_t *sc;

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    mlockall(MCL_CURRENT | MCL_FUTURE);

    printf("Requesting master...\n");
    master = ecrt_request_master(0);
    if (!master) {
        return -1;
    }

    domain1 = ecrt_master_create_domain(master);
    if (!domain1) {
        return -1;
    }

    printf("Creating slave configurations...\n");

    // Create configuration for bus coupler
    sc = ecrt_master_slave_config(master, BusCoupler01_Pos, Beckhoff_EK1100);
    if (!sc) {
        return -1;
    }

    sc_dig_out_01 =
        ecrt_master_slave_config(master, DigOutSlave01_Pos, Beckhoff_EL2004);
    if (!sc_dig_out_01) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    if (ecrt_slave_config_pdos(sc_dig_out_01, EC_END, slave_1_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "PDO entry registration failed!\n");
        return -1;
    }

    printf("Activating master...\n");
    if (ecrt_master_activate(master)) {
        return -1;
    }

    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        fprintf(stderr, "Failed to get domain data pointer.\n");
        return -1;
    }

    /* Create cyclic RT-thread */
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        puts("ERROR IN SETTING THE SCHEDULER");
        perror("errno");
        return -1;
    }

    task = rt_task_init(nam2num("ec_rtai_rtdm_example"),
            0 /* priority */, 0 /* stack size */, 0 /* msg size */);

    my_cyclic();

    rt_task_delete(task);

    printf("End of Program\n");
    ecrt_release_master(master);

    return 0;
}

/****************************************************************************/
