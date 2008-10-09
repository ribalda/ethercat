/*****************************************************************************
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

// Optional features
#define CONFIGURE_PDOS  1
#define EL3152_ALT_PDOS 0
#define SDO_ACCESS      0
#define VOE_ACCESS      0

#define PRIORITY 1

#define BusCouplerPos  0, 0
#define AnaOutSlavePos 0, 1
#define AnaInSlavePos  0, 2
#define DigOutSlavePos 0, 3

#define Beckhoff_EK1100 0x00000002, 0x044C2C52
#define Beckhoff_EL2004 0x00000002, 0x07D43052
#define Beckhoff_EL3152 0x00000002, 0x0c503052
#define Beckhoff_EL4102 0x00000002, 0x10063052

/****************************************************************************/

static unsigned int sig_alarms = 0;
static unsigned int user_alarms = 0;

// offsets for Pdo entries
static unsigned int off_ana_in;
static unsigned int off_ana_out;
static unsigned int off_dig_out;

const static ec_pdo_entry_reg_t domain1_regs[] = {
#if EL3152_ALT_PDOS
    {AnaInSlavePos,  Beckhoff_EL3152, 0x6401, 1, &off_ana_in},
#else
    {AnaInSlavePos,  Beckhoff_EL3152, 0x3101, 2, &off_ana_in},
#endif
	{AnaOutSlavePos, Beckhoff_EL4102, 0x3001, 1, &off_ana_out},
	{DigOutSlavePos, Beckhoff_EL2004, 0x3001, 1, &off_dig_out},
	{}
};

static ec_slave_config_t *sc_ana_in = NULL;

/*****************************************************************************/

#if CONFIGURE_PDOS

// Analog in --------------------------

static ec_pdo_entry_info_t el3152_pdo_entries[] = {
    {0x3101, 1,  8}, // channel 1 status
    {0x3101, 2, 16}, // channel 1 value
    {0x3102, 1,  8}, // channel 2 status
    {0x3102, 2, 16}, // channel 2 value
    {0x6401, 1, 16}, // channel 1 value (alt.)
    {0x6401, 2, 16}  // channel 2 value (alt.)
};

#if EL3152_ALT_PDOS
static ec_pdo_info_t el3152_pdos[] = {
    {0x1A10, 2, el3152_pdo_entries + 4},
};

static ec_sync_info_t el3152_syncs[] = {
    {2, EC_DIR_OUTPUT},
    {3, EC_DIR_INPUT, 1, el3152_pdos},
    {0xff}
};
#else
static ec_pdo_info_t el3152_pdos[] = {
    {0x1A00, 2, el3152_pdo_entries},
    {0x1A01, 2, el3152_pdo_entries + 2}
};

static ec_sync_info_t el3152_syncs[] = {
    {2, EC_DIR_OUTPUT},
    {3, EC_DIR_INPUT, 2, el3152_pdos},
    {0xff}
};
#endif

// Analog out -------------------------

static ec_pdo_entry_info_t el4102_pdo_entries[] = {
    {0x3001, 1, 16}, // channel 1 value
    {0x3002, 1, 16}, // channel 2 value
};

static ec_pdo_info_t el4102_pdos[] = {
    {0x1600, 1, el4102_pdo_entries},
    {0x1601, 1, el4102_pdo_entries + 1}
};

static ec_sync_info_t el4102_syncs[] = {
    {2, EC_DIR_OUTPUT, 2, el4102_pdos},
    {3, EC_DIR_INPUT},
    {0xff}
};

// Digital out ------------------------

static ec_pdo_entry_info_t el2004_channels[] = {
    {0x3001, 1, 1}, // Value 1
    {0x3001, 2, 1}, // Value 2
    {0x3001, 3, 1}, // Value 3
    {0x3001, 4, 1}  // Value 4
};

static ec_pdo_info_t el2004_pdos[] = {
    {0x1600, 1, &el2004_channels[0]},
    {0x1601, 1, &el2004_channels[1]},
    {0x1602, 1, &el2004_channels[2]},
    {0x1603, 1, &el2004_channels[3]}
};

static ec_sync_info_t el2004_syncs[] = {
    {0, EC_DIR_OUTPUT, 4, el2004_pdos},
    {1, EC_DIR_INPUT},
    {0xff}
};
#endif

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
	ec_master_t *master;
	ec_domain_t *domain1;
	ec_slave_config_t *sc;
    struct sigaction sa;
    struct itimerval tv;
    
    master = ecrt_request_master(0);
	if (!master)
		return -1;

    domain1 = ecrt_master_create_domain(master);
    if (!domain1)
        return -1;

    if (!(sc_ana_in = ecrt_master_slave_config(
                    master, AnaInSlavePos, Beckhoff_EL3152))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

#if CONFIGURE_PDOS
    printf("Configuring Pdos...\n");
    if (ecrt_slave_config_pdos(sc_ana_in, EC_END, el3152_syncs)) {
        fprintf(stderr, "Failed to configure Pdos.\n");
        return -1;
    }

    if (!(sc = ecrt_master_slave_config(
                    master, AnaOutSlavePos, Beckhoff_EL4102))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    if (ecrt_slave_config_pdos(sc, EC_END, el4102_syncs)) {
        fprintf(stderr, "Failed to configure Pdos.\n");
        return -1;
    }

    if (!(sc = ecrt_master_slave_config(
                    master, DigOutSlavePos, Beckhoff_EL2004))) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    if (ecrt_slave_config_pdos(sc, EC_END, el2004_syncs)) {
        fprintf(stderr, "Failed to configure Pdos.\n");
        return -1;
    }
#endif

    // Create configuration for bus coupler
    sc = ecrt_master_slave_config(master, BusCouplerPos, Beckhoff_EK1100);
    if (!sc)
        return -1;

    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "Pdo entry registration failed!\n");
		return -1;
    }

    printf("Activating master...\n");
    if (ecrt_master_activate(master))
        return -1;

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
    tv.it_interval.tv_usec = 10000;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = 1000;
    if (setitimer(ITIMER_REAL, &tv, NULL)) {
        fprintf(stderr, "Failed to start timer: %s\n", strerror(errno));
        return 1;
    }

    printf("Started.\n");
	while (1) {
        sleep(1);

#if 0
        struct timeval t;
        gettimeofday(&t, NULL);
        printf("%u.%06u\n", t.tv_sec, t.tv_usec);
#endif

        while (sig_alarms != user_alarms) {
            ecrt_master_receive(master);
            ecrt_master_send(master);

            user_alarms++;
        }
	}

	return 0;
}

/****************************************************************************/
