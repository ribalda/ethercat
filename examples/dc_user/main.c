/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2007-2009  Florian Pose, Ingenieurgemeinschaft IgH
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
 ****************************************************************************/

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/****************************************************************************/

#include "ecrt.h"

/****************************************************************************/

// Application parameters
#define FREQUENCY 100
#define PRIORITY 1

// Optional features
#define CONFIGURE_PDOS  1

/****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};

// Timer
static unsigned int sig_alarms = 0;
static unsigned int user_alarms = 0;

/****************************************************************************/

// process data
static uint8_t *domain1_pd = NULL;

#define BusCouplerPos    0, 0
#define DigOutSlavePos   0, 1
#define CounterSlavePos  0, 2

#define Beckhoff_EK1100 0x00000002, 0x044c2c52
#define Beckhoff_EL2008 0x00000002, 0x07d83052
#define IDS_Counter     0x000012ad, 0x05de3052

// offsets for PDO entries
static int off_dig_out;
static int off_counter_in;
static int off_counter_out;

static unsigned int counter = 0;
static unsigned int blink_counter = 0;
static unsigned int blink = 0;
static unsigned int sync_ref_counter = 0;
struct timeval app_time;

/*****************************************************************************/

void check_domain1_state(void)
{
    ec_domain_state_t ds;

    ecrt_domain_state(domain1, &ds);

    if (ds.working_counter != domain1_state.working_counter)
        printf("Domain1: WC %u.\n", ds.working_counter);
    if (ds.wc_state != domain1_state.wc_state)
        printf("Domain1: State %u.\n", ds.wc_state);

    domain1_state = ds;
}

/*****************************************************************************/

void check_master_state(void)
{
    ec_master_state_t ms;

    ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding)
        printf("%u slave(s).\n", ms.slaves_responding);
    if (ms.al_states != master_state.al_states)
        printf("AL states: 0x%02X.\n", ms.al_states);
    if (ms.link_up != master_state.link_up)
        printf("Link is %s.\n", ms.link_up ? "up" : "down");

    master_state = ms;
}

/****************************************************************************/

void cyclic_task()
{
    int i;

    // receive process data
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);

    // check process data state (optional)
    check_domain1_state();

    if (counter) {
        counter--;
    } else { // do this at 1 Hz
        counter = FREQUENCY;

        // calculate new process data
        blink = !blink;

        // check for master state (optional)
        check_master_state();

    }

    if (blink_counter) {
        blink_counter--;
    } else {
        blink_counter = 9;

        // calculate new process data
        blink = !blink;
    }

    // write process data
    EC_WRITE_U8(domain1_pd + off_dig_out, blink ? 0x66 : 0x99);
    EC_WRITE_U8(domain1_pd + off_counter_out, blink ? 0x00 : 0x02);

    app_time.tv_usec += 1000000 / FREQUENCY;
    if (app_time.tv_usec >= 1000000)  {
        app_time.tv_usec -= 1000000;
        app_time.tv_sec++;
    }

    if (sync_ref_counter) {
        sync_ref_counter--;
    } else {
        sync_ref_counter = 9;
        ecrt_master_sync_reference_clock(master, EC_TIMEVAL2NANO(&app_time));
    }
    ecrt_master_sync_slave_clocks(master);

    // send process data
    ecrt_domain_queue(domain1);
    ecrt_master_send(master);
}

/****************************************************************************/

void signal_handler(int signum) {
    switch (signum) {
        case SIGALRM:
            sig_alarms++;
            break;
    }
}

/****************************************************************************/

int main(int argc, char **argv)
{
	ec_slave_config_t *sc;
    struct sigaction sa;
    struct itimerval tv;
    
    master = ecrt_request_master(0);
	if (!master)
		return -1;

    domain1 = ecrt_master_create_domain(master);
    if (!domain1)
        return -1;

    // Create configuration for bus coupler
    sc = ecrt_master_slave_config(master, BusCouplerPos, Beckhoff_EK1100);
    if (!sc)
        return -1;

    if (!(sc = ecrt_master_slave_config(master,
                    DigOutSlavePos, Beckhoff_EL2008))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    off_dig_out = ecrt_slave_config_reg_pdo_entry(sc,
            0x7000, 1, domain1, NULL);
    if (off_dig_out < 0)
        return -1;

	if (!(sc = ecrt_master_slave_config(master,
					CounterSlavePos, IDS_Counter))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
	}

	off_counter_in = ecrt_slave_config_reg_pdo_entry(sc,
			0x6020, 0x11, domain1, NULL);
	if (off_counter_in < 0)
        return -1;

	off_counter_out = ecrt_slave_config_reg_pdo_entry(sc,
			0x7020, 1, domain1, NULL);
	if (off_counter_out < 0)
        return -1;

#if 1
    // configure SYNC signals for this slave
	ecrt_slave_config_dc_assign_activate(sc, 0x0700);
	ecrt_slave_config_dc_sync_cycle_times(sc, 10000000, 0);
	ecrt_slave_config_dc_sync_shift_times(sc,  4400000, 0);
#endif

    printf("Activating master...\n");
    if (ecrt_master_activate(master))
        return -1;

    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return -1;
    }

#if PRIORITY
    pid_t pid = getpid();
    if (setpriority(PRIO_PROCESS, pid, -19))
        fprintf(stderr, "Warning: Failed to set priority: %s\n",
                strerror(errno));
#endif

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, 0)) {
        fprintf(stderr, "Failed to install signal handler!\n");
        return -1;
    }

    printf("Starting timer...\n");
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = 1000000 / FREQUENCY;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = 1000;
    if (setitimer(ITIMER_REAL, &tv, NULL)) {
        fprintf(stderr, "Failed to start timer: %s\n", strerror(errno));
        return 1;
    }

    gettimeofday(&app_time, NULL);

    printf("Started.\n");
	while (1) {
        pause();

#if 0
        struct timeval t;
        gettimeofday(&t, NULL);
        printf("%u.%06u\n", t.tv_sec, t.tv_usec);
#endif

        while (sig_alarms != user_alarms) {
            cyclic_task();
            user_alarms++;
        }
	}

	return 0;
}

/****************************************************************************/
