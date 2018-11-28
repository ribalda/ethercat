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

static unsigned int cycle_ns = 1000000; /* 1 ms */

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

// EtherCAT distributed clock variables

#define DC_FILTER_CNT          1024
#define SYNC_MASTER_TO_REF        1

static uint64_t dc_start_time_ns = 0LL;
static uint64_t dc_time_ns = 0;
#if SYNC_MASTER_TO_REF
static uint8_t  dc_started = 0;
static int32_t  dc_diff_ns = 0;
static int32_t  prev_dc_diff_ns = 0;
static int64_t  dc_diff_total_ns = 0LL;
static int64_t  dc_delta_total_ns = 0LL;
static int      dc_filter_idx = 0;
static int64_t  dc_adjust_ns;
#endif
static int64_t  system_time_base = 0LL;
static uint64_t wakeup_time = 0LL;
static uint64_t overruns = 0LL;

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

/** Get the time in ns for the current cpu, adjusted by system_time_base.
 *
 * \attention Rather than calling rt_get_time_ns() directly, all application
 * time calls should use this method instead.
 *
 * \ret The time in ns.
 */
uint64_t system_time_ns(void)
{
    RTIME time = rt_get_time_ns();

    if (system_time_base > time) {
        rt_printk("%s() error: system_time_base greater than"
                " system time (system_time_base: %lld, time: %llu\n",
                __func__, system_time_base, time);
        return time;
    }
    else {
        return time - system_time_base;
    }
}

/****************************************************************************/

/** Convert system time to RTAI time in counts (via the system_time_base).
 */
RTIME system2count(
        uint64_t time
        )
{
    RTIME ret;

    if ((system_time_base < 0) &&
            ((uint64_t) (-system_time_base) > time)) {
        rt_printk("%s() error: system_time_base less than"
                " system time (system_time_base: %lld, time: %llu\n",
                __func__, system_time_base, time);
        ret = time;
    }
    else {
        ret = time + system_time_base;
    }

    return nano2count(ret);
}

/*****************************************************************************/

/** Synchronise the distributed clocks
 */
void sync_distributed_clocks(void)
{
#if SYNC_MASTER_TO_REF
    uint32_t ref_time = 0;
    uint64_t prev_app_time = dc_time_ns;
#endif

    dc_time_ns = system_time_ns();

#if SYNC_MASTER_TO_REF
    // get reference clock time to synchronize master cycle
    ecrt_master_reference_clock_time(master, &ref_time);
    dc_diff_ns = (uint32_t) prev_app_time - ref_time;
#else
    // sync reference clock to master
    ecrt_master_sync_reference_clock_to(master, dc_time_ns);
#endif

    // call to sync slaves to ref slave
    ecrt_master_sync_slave_clocks(master);
}

/*****************************************************************************/

/** Return the sign of a number
 *
 * ie -1 for -ve value, 0 for 0, +1 for +ve value
 *
 * \retval the sign of the value
 */
#define sign(val) \
    ({ typeof (val) _val = (val); \
    ((_val > 0) - (_val < 0)); })

/*****************************************************************************/

/** Update the master time based on ref slaves time diff
 *
 * called after the ethercat frame is sent to avoid time jitter in
 * sync_distributed_clocks()
 */
void update_master_clock(void)
{
#if SYNC_MASTER_TO_REF
    // calc drift (via un-normalised time diff)
    int32_t delta = dc_diff_ns - prev_dc_diff_ns;
    prev_dc_diff_ns = dc_diff_ns;

    // normalise the time diff
    dc_diff_ns =
        ((dc_diff_ns + (cycle_ns / 2)) % cycle_ns) - (cycle_ns / 2);

    // only update if primary master
    if (dc_started) {

        // add to totals
        dc_diff_total_ns += dc_diff_ns;
        dc_delta_total_ns += delta;
        dc_filter_idx++;

        if (dc_filter_idx >= DC_FILTER_CNT) {
            // add rounded delta average
            dc_adjust_ns +=
                ((dc_delta_total_ns + (DC_FILTER_CNT / 2)) / DC_FILTER_CNT);

            // and add adjustment for general diff (to pull in drift)
            dc_adjust_ns += sign(dc_diff_total_ns / DC_FILTER_CNT);

            // limit crazy numbers (0.1% of std cycle time)
            if (dc_adjust_ns < -1000) {
                dc_adjust_ns = -1000;
            }
            if (dc_adjust_ns > 1000) {
                dc_adjust_ns =  1000;
            }

            // reset
            dc_diff_total_ns = 0LL;
            dc_delta_total_ns = 0LL;
            dc_filter_idx = 0;
        }

        // add cycles adjustment to time base (including a spot adjustment)
        system_time_base += dc_adjust_ns + sign(dc_diff_ns);
    }
    else {
        dc_started = (dc_diff_ns != 0);

        if (dc_started) {
            // output first diff
            rt_printk("First master diff: %d.\n", dc_diff_ns);

            // record the time of this initial cycle
            dc_start_time_ns = dc_time_ns;
        }
    }
#endif
}

/****************************************************************************/

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

/** Wait for the next period
 */
void wait_period(void)
{
    while (1)
    {
        RTIME wakeup_count = system2count(wakeup_time);
        RTIME current_count = rt_get_time();

        if ((wakeup_count < current_count)
                || (wakeup_count > current_count + (50 * cycle_ns))) {
            rt_printk("%s(): unexpected wake time!\n", __func__);
        }

        switch (rt_sleep_until(wakeup_count)) {
            case RTE_UNBLKD:
                rt_printk("rt_sleep_until(): RTE_UNBLKD\n");
                continue;

            case RTE_TMROVRN:
                rt_printk("rt_sleep_until(): RTE_TMROVRN\n");
                overruns++;

                if (overruns % 100 == 0) {
                    // in case wake time is broken ensure other processes get
                    // some time slice (and error messages can get displayed)
                    rt_sleep(cycle_ns / 100);
                }
                break;

            default:
                break;
        }

        // done if we got to here
        break;
    }

    // set master time in nano-seconds
    ecrt_master_application_time(master, wakeup_time);

    // calc next wake time (in sys time)
    wakeup_time += cycle_ns;
}

/****************************************************************************/

void my_cyclic(void)
{
    int cycle_counter = 0;
    unsigned int blink = 0;

    // oneshot mode to allow adjustable wake time
    rt_set_oneshot_mode();

    // set first wake time in a few cycles
    wakeup_time = system_time_ns() + 10 * cycle_ns;

    // start the timer
    start_rt_timer(nano2count(cycle_ns));

    rt_make_hard_real_time();

    while (run) {
        // wait for next period (using adjustable system time)
        wait_period();

        cycle_counter++;

        if (!run) {
            break;
        }

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

        // queue process data
        ecrt_domain_queue(domain1);

        // sync distributed clock just before master_send to set
        // most accurate master clock time
        sync_distributed_clocks();

        // send EtherCAT data
        ecrt_master_send(master);

        // update the master clock
        // Note: called after ecrt_master_send() to reduce time
        // jitter in the sync_distributed_clocks() call
        update_master_clock();
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
    ec_slave_config_t *sc_ek1100;
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
    sc_ek1100 =
        ecrt_master_slave_config(master, BusCoupler01_Pos, Beckhoff_EK1100);
    if (!sc_ek1100) {
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

    /* Set the initial master time and select a slave to use as the DC
     * reference clock, otherwise pass NULL to auto select the first capable
     * slave. Note: This can be used whether the master or the ref slave will
     * be used as the systems master DC clock.
     */
    dc_start_time_ns = system_time_ns();
    dc_time_ns = dc_start_time_ns;

    ret = ecrt_master_select_reference_clock(master, sc_ek1100);
    if (ret < 0) {
        fprintf(stderr, "Failed to select reference clock: %s\n",
                strerror(-ret));
        return ret;
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
