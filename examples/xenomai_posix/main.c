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

#include <errno.h>
#include <mqueue.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>

#include <rtdm/rtdm.h>
#include <rtdk.h>

#include "ecrt.h"

#define NSEC_PER_SEC 1000000000

static unsigned int cycle_us = 1000; /* 1 ms */

static pthread_t cyclic_thread;

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

void *my_thread(void *arg)
{
    struct timespec next_period;
    int cycle_counter = 0;
	unsigned int blink = 0;

    clock_gettime(CLOCK_REALTIME, &next_period);

    while (run) {
        next_period.tv_nsec += cycle_us * 1000;
        while (next_period.tv_nsec >= NSEC_PER_SEC) {
			next_period.tv_nsec -= NSEC_PER_SEC;
			next_period.tv_sec++;
		}

        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_period, NULL);

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

        EC_WRITE_U8(domain1_pd + off_dig_out0, blink ? 0x0 : 0x0F);

        // send process data
        ecrt_domain_queue(domain1);
        ecrt_master_send(master);
    }

    return NULL;
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
    int ret;

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
    struct sched_param param = { .sched_priority = 82 };

    pthread_attr_t thattr;
    pthread_attr_init(&thattr);
    pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&thattr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&thattr, SCHED_FIFO);
    pthread_setschedparam(cyclic_thread, SCHED_FIFO, &param);

    ret = pthread_create(&cyclic_thread, &thattr, &my_thread, NULL);
    if (ret) {
        fprintf(stderr, "%s: pthread_create cyclic task failed\n",
                strerror(-ret));
		return 1;
    }

	while (run) {
		sched_yield();
	}

    pthread_join(cyclic_thread, NULL);

    printf("End of Program\n");
    ecrt_release_master(master);

    return 0;
}

/****************************************************************************/
