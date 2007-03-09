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

/*****************************************************************************/

static struct timer_list timer;

// EtherCAT
static ec_master_t *master = NULL;
static ec_domain_t *domain1 = NULL;
spinlock_t master_lock = SPIN_LOCK_UNLOCKED;
static ec_master_status_t master_status, old_status = {};

// data fields
#ifdef KBUS
static void *r_inputs;
static void *r_outputs;
#endif

static void *r_dig_out;
static void *r_ana_out;
static void *r_count;
static void *r_freq;

#if 1
const static ec_pdo_reg_t domain1_pdo_regs[] = {
    {"2",      Beckhoff_EL2004_Outputs,   &r_dig_out},
    {"3",      Beckhoff_EL4132_Output1,   &r_ana_out},
    {"#888:1", Beckhoff_EL5101_Value,     &r_count},
    {"4",      Beckhoff_EL5101_Frequency, &r_freq},
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
    // k_pos = EC_READ_U32(r_ssi);
    EC_WRITE_U8(r_dig_out, blink ? 0x0F : 0x00);

    if (counter) {
        counter--;
    }
    else {
        counter = FREQUENCY;
        blink = !blink;

        spin_lock(&master_lock);
        ecrt_master_get_status(master, &master_status);
        spin_unlock(&master_lock);

        if (master_status.bus_status != old_status.bus_status) {
            printk(KERN_INFO PFX "bus status changed to %i.\n",
                    master_status.bus_status);
        }
        if (master_status.bus_tainted != old_status.bus_tainted) {
            printk(KERN_INFO PFX "tainted flag changed to %u.\n",
                    master_status.bus_tainted);
        }
        if (master_status.slaves_responding !=
                old_status.slaves_responding) {
            printk(KERN_INFO PFX "slaves_responding changed to %u.\n",
                    master_status.slaves_responding);
        }
       
        old_status = master_status;
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
#if 1
    ec_slave_t *slave;
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

#if 1
    printk(KERN_INFO PFX "Configuring alternative PDO mapping...\n");
    if (!(slave = ecrt_master_get_slave(master, "4", Beckhoff_EL5101)))
        goto out_release_master;

    if (ecrt_slave_pdo_mapping(slave, EC_DIR_INPUT, 2, 0x1A00, 0x1A02))
        goto out_release_master;
#endif

    printk(KERN_INFO PFX "Registering PDOs...\n");
#if 1
    if (ecrt_domain_register_pdo_list(domain1, domain1_pdo_regs)) {
        printk(KERN_ERR PFX "PDO registration failed!\n");
        goto out_release_master;
    }
#endif

#ifdef KBUS
    if (!(slave = ecrt_master_get_slave(master, "0", Beckhoff_BK1120)))
        goto out_release_master;
    
    if (!ecrt_domain_register_pdo_range(
                domain1, slave, EC_DIR_OUTPUT, 0, 4, &r_outputs)) {
        printk(KERN_ERR PFX "PDO registration failed!\n");
        goto out_release_master;
    }
    
    if (!ecrt_domain_register_pdo_range(
                domain1, slave, EC_DIR_INPUT, 0, 4, &r_inputs)) {
        printk(KERN_ERR PFX "PDO registration failed!\n");
        goto out_release_master;
    }
#endif

#if 0
    if (!(slave = ecrt_master_get_slave(master, "4", Beckhoff_EL5001)))
        goto out_release_master;

    if (ecrt_slave_conf_sdo8(slave, 0x4061, 1, 0))
        goto out_release_master;
#endif

#if 1
#endif

    printk(KERN_INFO PFX "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR PFX "Failed to activate master!\n");
        goto out_release_master;
    }

    printk(KERN_INFO PFX "Starting cyclic sample thread.\n");
    init_timer(&timer);
    timer.function = run;
    timer.expires = jiffies + 10;
    add_timer(&timer);

    printk(KERN_INFO PFX "Started.\n");
    return 0;

 out_release_master:
    ecrt_release_master(master);
 out_return:
    return -1;
}

/*****************************************************************************/

void __exit cleanup_mini_module(void)
{
    printk(KERN_INFO PFX "Stopping...\n");

    del_timer_sync(&timer);
    printk(KERN_INFO PFX "Releasing master...\n");
    ecrt_release_master(master);

    printk(KERN_INFO PFX "Stopped.\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT minimal test environment");

module_init(init_mini_module);
module_exit(cleanup_mini_module);

/*****************************************************************************/
