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

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include "../../include/ecrt.h" // EtherCAT realtime interface
#include "../../include/ecdb.h" // EtherCAT slave database

#define PFX "ec_mini: "

#define FREQUENCY 100

//#define KBUS
#define PDOS
#define MAPPING
#define EXTERNAL_MEMORY

/*****************************************************************************/

static struct timer_list timer;

// EtherCAT
static ec_master_t *master = NULL;
static ec_domain_t *domain1 = NULL;
spinlock_t master_lock = SPIN_LOCK_UNLOCKED;
static ec_master_state_t master_state, old_state = {};

// data fields
#ifdef KBUS
static void *r_inputs;
static void *r_outputs;
#endif

#ifdef MAPPING
const ec_pdo_entry_info_t el3162_channel1[] = {
    {0x3101, 1,  8}, // status
    {0x3101, 2, 16}  // value
};

const ec_pdo_entry_info_t el3162_channel2[] = {
    {0x3102, 1,  8}, // status
    {0x3102, 2, 16}  // value
};

const ec_pdo_info_t el3162_mapping[] = {
    {EC_DIR_INPUT, 0x1A00, 2, el3162_channel1},
    {EC_DIR_INPUT, 0x1A01, 2, el3162_channel2},
};

const ec_pdo_entry_info_t el2004_channels[] = {
    {0x3001, 1, 1}, // Value 1
    {0x3001, 2, 1}, // Value 2
    {0x3001, 3, 1}, // Value 3
    {0x3001, 4, 1}  // Value 4
};

const ec_pdo_info_t el2004_mapping[] = {
    {EC_DIR_OUTPUT, 0x1600, 1, &el2004_channels[0]},
    {EC_DIR_OUTPUT, 0x1601, 1, &el2004_channels[1]},
    {EC_DIR_OUTPUT, 0x1602, 1, &el2004_channels[2]},
    {EC_DIR_OUTPUT, 0x1603, 1, &el2004_channels[3]},
};
#endif

#ifdef PDOS
static uint8_t *pd; /**< Process data. */
static unsigned int off_ana_in;
static unsigned int off_dig_out;

const static ec_pdo_entry_reg_t domain1_regs[] = {
    {0, 1, Beckhoff_EL3162, 0x3101, 2, &off_ana_in},
    {0, 3, Beckhoff_EL2004, 0x3001, 1, &off_dig_out},
    {}
};
#endif

/*****************************************************************************/

void run(unsigned long data)
{
    static unsigned int counter = 0;
    static unsigned int blink = 0;

    // receive
    spin_lock(&master_lock);
    ecrt_master_receive(master);
    ecrt_domain_process(domain1);
    spin_unlock(&master_lock);

    // process data
    EC_WRITE_U8(pd + off_dig_out, blink ? 0x0F : 0x00);

    if (counter) {
        counter--;
    }
    else {
        counter = FREQUENCY;
        blink = !blink;

        spin_lock(&master_lock);
        ecrt_master_state(master, &master_state);
        spin_unlock(&master_lock);

        if (master_state.bus_state != old_state.bus_state) {
            printk(KERN_INFO PFX "bus state changed to %i.\n",
                    master_state.bus_state);
        }
        if (master_state.bus_tainted != old_state.bus_tainted) {
            printk(KERN_INFO PFX "tainted flag changed to %u.\n",
                    master_state.bus_tainted);
        }
        if (master_state.slaves_responding !=
                old_state.slaves_responding) {
            printk(KERN_INFO PFX "slaves_responding changed to %u.\n",
                    master_state.slaves_responding);
        }
       
        old_state = master_state;
    }

#ifdef KBUS
    EC_WRITE_U8(r_outputs + 2, blink ? 0xFF : 0x00);
#endif

    // send
    spin_lock(&master_lock);
    ecrt_domain_queue(domain1);
    spin_unlock(&master_lock);

    spin_lock(&master_lock);
    ecrt_master_send(master);
    spin_unlock(&master_lock);

    // restart timer
    timer.expires += HZ / FREQUENCY;
    add_timer(&timer);
}

/*****************************************************************************/

int request_lock(void *data)
{
    spin_lock(&master_lock);
    return 0; // access allowed
}

/*****************************************************************************/

void release_lock(void *data)
{
    spin_unlock(&master_lock);
}

/*****************************************************************************/

int __init init_mini_module(void)
{
#ifdef MAPPING
    ec_slave_config_t *sc;
#endif
#ifdef EXTERNAL_MEMORY
    unsigned int size;
#endif
    
    printk(KERN_INFO PFX "Starting...\n");

    if (!(master = ecrt_request_master(0))) {
        printk(KERN_ERR PFX "Requesting master 0 failed!\n");
        goto out_return;
    }

    ecrt_master_callbacks(master, request_lock, release_lock, NULL);

    printk(KERN_INFO PFX "Registering domain...\n");
    if (!(domain1 = ecrt_master_create_domain(master))) {
        printk(KERN_ERR PFX "Domain creation failed!\n");
        goto out_release_master;
    }

#ifdef MAPPING
    printk(KERN_INFO PFX "Configuring Pdo mapping...\n");
    if (!(sc = ecrt_master_slave_config(master, 0, 1, Beckhoff_EL3162))) {
        printk(KERN_ERR PFX "Failed to get slave configuration.\n");
        goto out_release_master;
    }

    if (ecrt_slave_config_mapping(sc, 2, el3162_mapping)) {
        printk(KERN_ERR PFX "Failed to configure Pdo mapping.\n");
        goto out_release_master;
    }

    if (!(sc = ecrt_master_slave_config(master, 0, 3, Beckhoff_EL2004))) {
        printk(KERN_ERR PFX "Failed to get slave configuration.\n");
        goto out_release_master;
    }

    if (ecrt_slave_config_mapping(sc, 4, el2004_mapping)) {
        printk(KERN_ERR PFX "Failed to configure Pdo mapping.\n");
        goto out_release_master;
    }
#endif

    printk(KERN_INFO PFX "Registering Pdo entries...\n");
#ifdef PDOS
    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        printk(KERN_ERR PFX "Pdo entry registration failed!\n");
        goto out_release_master;
    }
#endif

#ifdef EXTERNAL_MEMORY
    if ((size = ecrt_domain_size(domain1))) {
        if (!(pd = (uint8_t *) kmalloc(size, GFP_KERNEL))) {
            printk(KERN_ERR PFX "Failed to allocate %u bytes of process data"
                    " memory!\n", size);
            goto out_release_master;
        }
        ecrt_domain_external_memory(domain1, pd);
    }
#endif

    printk(KERN_INFO PFX "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR PFX "Failed to activate master!\n");
#ifdef EXTERNAL_MEMORY
        goto out_free_process_data;
#else
        goto out_release_master;
#endif
    }

#ifndef EXTERNAL_MEMORY
    // Get internal process data for domain
    pd = ecrt_domain_data(domain1);
#endif

    printk(KERN_INFO PFX "Starting cyclic sample thread.\n");
    init_timer(&timer);
    timer.function = run;
    timer.expires = jiffies + 10;
    add_timer(&timer);

    printk(KERN_INFO PFX "Started.\n");
    return 0;

#ifdef EXTERNAL_MEMORY
out_free_process_data:
    kfree(pd);
#endif
out_release_master:
    printk(KERN_ERR PFX "Releasing master...\n");
    ecrt_release_master(master);
out_return:
    printk(KERN_ERR PFX "Failed to load. Aborting.\n");
    return -1;
}

/*****************************************************************************/

void __exit cleanup_mini_module(void)
{
    printk(KERN_INFO PFX "Stopping...\n");

    del_timer_sync(&timer);

#ifdef EXTERNAL_MEMORY
    kfree(pd);
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
