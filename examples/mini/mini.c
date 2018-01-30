/******************************************************************************
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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/err.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
#include <linux/slab.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

#include "../../include/ecrt.h" // EtherCAT realtime interface

/*****************************************************************************/

// Module parameters
#define FREQUENCY 100

// Optional features
#define CONFIGURE_PDOS  1
#define EL3152_ALT_PDOS 0
#define EXTERNAL_MEMORY 1
#define SDO_ACCESS      0
#define VOE_ACCESS      0

#define PFX "ec_mini: "

/*****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};
struct semaphore master_sem;

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};

static ec_slave_config_t *sc_ana_in = NULL;
static ec_slave_config_state_t sc_ana_in_state = {};

// Timer
static struct timer_list timer;

/*****************************************************************************/

// process data
static uint8_t *domain1_pd; // process data memory

#define AnaInSlavePos  0, 2
#define AnaOutSlavePos 0, 1
#define DigOutSlavePos 0, 3

#define Beckhoff_EL2004 0x00000002, 0x07D43052
#define Beckhoff_EL3152 0x00000002, 0x0c503052
#define Beckhoff_EL4102 0x00000002, 0x10063052

// offsets for PDO entries
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

static unsigned int counter = 0;
static unsigned int blink = 0;

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
    {3, EC_DIR_INPUT, 1, el3152_pdos},
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

/*****************************************************************************/

#if SDO_ACCESS
static ec_sdo_request_t *sdo;
#endif

#if VOE_ACCESS
static ec_voe_handler_t *voe;
#endif

/*****************************************************************************/

void check_domain1_state(void)
{
    ec_domain_state_t ds;

    down(&master_sem);
    ecrt_domain_state(domain1, &ds);
    up(&master_sem);

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

    down(&master_sem);
    ecrt_master_state(master, &ms);
    up(&master_sem);

    if (ms.slaves_responding != master_state.slaves_responding)
        printk(KERN_INFO PFX "%u slave(s).\n", ms.slaves_responding);
    if (ms.al_states != master_state.al_states)
        printk(KERN_INFO PFX "AL states: 0x%02X.\n", ms.al_states);
    if (ms.link_up != master_state.link_up)
        printk(KERN_INFO PFX "Link is %s.\n", ms.link_up ? "up" : "down");

    master_state = ms;
}

/*****************************************************************************/

void check_slave_config_states(void)
{
    ec_slave_config_state_t s;

    down(&master_sem);
    ecrt_slave_config_state(sc_ana_in, &s);
    up(&master_sem);

    if (s.al_state != sc_ana_in_state.al_state)
        printk(KERN_INFO PFX "AnaIn: State 0x%02X.\n", s.al_state);
    if (s.online != sc_ana_in_state.online)
        printk(KERN_INFO PFX "AnaIn: %s.\n", s.online ? "online" : "offline");
    if (s.operational != sc_ana_in_state.operational)
        printk(KERN_INFO PFX "AnaIn: %soperational.\n",
                s.operational ? "" : "Not ");

    sc_ana_in_state = s;
}

/*****************************************************************************/

#if SDO_ACCESS
void read_sdo(void)
{
    switch (ecrt_sdo_request_state(sdo)) {
        case EC_REQUEST_UNUSED: // request was not used yet
            ecrt_sdo_request_read(sdo); // trigger first read
            break;
        case EC_REQUEST_BUSY:
            printk(KERN_INFO PFX "Still busy...\n");
            break;
        case EC_REQUEST_SUCCESS:
            printk(KERN_INFO PFX "SDO value: 0x%04X\n",
                    EC_READ_U16(ecrt_sdo_request_data(sdo)));
            ecrt_sdo_request_read(sdo); // trigger next read
            break;
        case EC_REQUEST_ERROR:
            printk(KERN_INFO PFX "Failed to read SDO!\n");
            ecrt_sdo_request_read(sdo); // retry reading
            break;
    }
}
#endif

/*****************************************************************************/

#if VOE_ACCESS
void read_voe(void)
{
    switch (ecrt_voe_handler_execute(voe)) {
        case EC_REQUEST_UNUSED:
            ecrt_voe_handler_read(voe); // trigger first read
            break;
        case EC_REQUEST_BUSY:
            printk(KERN_INFO PFX "VoE read still busy...\n");
            break;
        case EC_REQUEST_SUCCESS:
            printk(KERN_INFO PFX "VoE received.\n");
            // get data via ecrt_voe_handler_data(voe)
            ecrt_voe_handler_read(voe); // trigger next read
            break;
        case EC_REQUEST_ERROR:
            printk(KERN_INFO PFX "Failed to read VoE data!\n");
            ecrt_voe_handler_read(voe); // retry reading
            break;
    }
}
#endif

/*****************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
void cyclic_task(struct timer_list *t)
#else
void cyclic_task(unsigned long data)
#endif
{
    // receive process data
    down(&master_sem);
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);
    up(&master_sem);

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

        // check for islave configuration state(s) (optional)
        check_slave_config_states();

#if SDO_ACCESS
        // read process data SDO
        read_sdo();
#endif

#if VOE_ACCESS
        read_voe();
#endif
    }

    // write process data
    EC_WRITE_U8(domain1_pd + off_dig_out, blink ? 0x06 : 0x09);

    // send process data
    down(&master_sem);
    ecrt_domain_queue(domain1);
    ecrt_master_send(master);
    up(&master_sem);

    // restart timer
    timer.expires += HZ / FREQUENCY;
    add_timer(&timer);
}

/*****************************************************************************/

void send_callback(void *cb_data)
{
    ec_master_t *m = (ec_master_t *) cb_data;
    down(&master_sem);
    ecrt_master_send_ext(m);
    up(&master_sem);
}

/*****************************************************************************/

void receive_callback(void *cb_data)
{
    ec_master_t *m = (ec_master_t *) cb_data;
    down(&master_sem);
    ecrt_master_receive(m);
    up(&master_sem);
}

/*****************************************************************************/

int __init init_mini_module(void)
{
    int ret = -1;
#if CONFIGURE_PDOS
    ec_slave_config_t *sc;
#endif
#if EXTERNAL_MEMORY
    unsigned int size;
#endif

    printk(KERN_INFO PFX "Starting...\n");

    master = ecrt_request_master(0);
    if (!master) {
        ret = -EBUSY;
        printk(KERN_ERR PFX "Requesting master 0 failed.\n");
        goto out_return;
    }

    sema_init(&master_sem, 1);
    ecrt_master_callbacks(master, send_callback, receive_callback, master);

    printk(KERN_INFO PFX "Registering domain...\n");
    if (!(domain1 = ecrt_master_create_domain(master))) {
        printk(KERN_ERR PFX "Domain creation failed!\n");
        goto out_release_master;
    }

    if (!(sc_ana_in = ecrt_master_slave_config(
                    master, AnaInSlavePos, Beckhoff_EL3152))) {
        printk(KERN_ERR PFX "Failed to get slave configuration.\n");
        goto out_release_master;
    }

#if CONFIGURE_PDOS
    printk(KERN_INFO PFX "Configuring PDOs...\n");
    if (ecrt_slave_config_pdos(sc_ana_in, EC_END, el3152_syncs)) {
        printk(KERN_ERR PFX "Failed to configure PDOs.\n");
        goto out_release_master;
    }

    if (!(sc = ecrt_master_slave_config(
                    master, AnaOutSlavePos, Beckhoff_EL4102))) {
        printk(KERN_ERR PFX "Failed to get slave configuration.\n");
        goto out_release_master;
    }

    if (ecrt_slave_config_pdos(sc, EC_END, el4102_syncs)) {
        printk(KERN_ERR PFX "Failed to configure PDOs.\n");
        goto out_release_master;
    }

    if (!(sc = ecrt_master_slave_config(
                    master, DigOutSlavePos, Beckhoff_EL2004))) {
        printk(KERN_ERR PFX "Failed to get slave configuration.\n");
        goto out_release_master;
    }

    if (ecrt_slave_config_pdos(sc, EC_END, el2004_syncs)) {
        printk(KERN_ERR PFX "Failed to configure PDOs.\n");
        goto out_release_master;
    }
#endif

#if SDO_ACCESS
    printk(KERN_INFO PFX "Creating SDO requests...\n");
    if (!(sdo = ecrt_slave_config_create_sdo_request(sc_ana_in, 0x3102, 2, 2))) {
        printk(KERN_ERR PFX "Failed to create SDO request.\n");
        goto out_release_master;
    }
    ecrt_sdo_request_timeout(sdo, 500); // ms
#endif

#if VOE_ACCESS
    printk(KERN_INFO PFX "Creating VoE handlers...\n");
    if (!(voe = ecrt_slave_config_create_voe_handler(sc_ana_in, 1000))) {
        printk(KERN_ERR PFX "Failed to create VoE handler.\n");
        goto out_release_master;
    }
#endif

    printk(KERN_INFO PFX "Registering PDO entries...\n");
    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        printk(KERN_ERR PFX "PDO entry registration failed!\n");
        goto out_release_master;
    }

#if EXTERNAL_MEMORY
    if ((size = ecrt_domain_size(domain1))) {
        if (!(domain1_pd = (uint8_t *) kmalloc(size, GFP_KERNEL))) {
            printk(KERN_ERR PFX "Failed to allocate %u bytes of process data"
                    " memory!\n", size);
            goto out_release_master;
        }
        ecrt_domain_external_memory(domain1, domain1_pd);
    }
#endif

    printk(KERN_INFO PFX "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR PFX "Failed to activate master!\n");
#if EXTERNAL_MEMORY
        goto out_free_process_data;
#else
        goto out_release_master;
#endif
    }

#if !EXTERNAL_MEMORY
    // Get internal process data for domain
    domain1_pd = ecrt_domain_data(domain1);
#endif

    printk(KERN_INFO PFX "Starting cyclic sample thread.\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
    timer_setup(&timer, cyclic_task, 0);
#else
    init_timer(&timer);
    timer.function = cyclic_task;
#endif
    timer.expires = jiffies + 10;
    add_timer(&timer);

    printk(KERN_INFO PFX "Started.\n");
    return 0;

#if EXTERNAL_MEMORY
out_free_process_data:
    kfree(domain1_pd);
#endif
out_release_master:
    printk(KERN_ERR PFX "Releasing master...\n");
    ecrt_release_master(master);
out_return:
    printk(KERN_ERR PFX "Failed to load. Aborting.\n");
    return ret;
}

/*****************************************************************************/

void __exit cleanup_mini_module(void)
{
    printk(KERN_INFO PFX "Stopping...\n");

    del_timer_sync(&timer);

#if EXTERNAL_MEMORY
    kfree(domain1_pd);
#endif

    printk(KERN_INFO PFX "Releasing master...\n");
    ecrt_release_master(master);

    printk(KERN_INFO PFX "Unloading.\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT minimal test environment");

module_init(init_mini_module);
module_exit(cleanup_mini_module);

/*****************************************************************************/
