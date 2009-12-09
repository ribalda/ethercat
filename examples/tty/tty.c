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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

#include "../../include/ecrt.h" // EtherCAT realtime interface
#include "../../include/ectty.h" // EtherCAT TTY interface

/*****************************************************************************/

// Module parameters
#define FREQUENCY 100

// Optional features

#define PFX "ec_tty_example: "

/*****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};
struct semaphore master_sem;

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};

// Timer
static struct timer_list timer;

/*****************************************************************************/

// process data
static uint8_t *domain1_pd; // process data memory

#define BusCouplerPos  0, 0
#define SerialPos      0, 1

#define Beckhoff_EK1100 0x00000002, 0x044c2c52
#define Beckhoff_EL6002 0x00000002, 0x17723052

// offsets for PDO entries
static unsigned int off_ctrl;
static unsigned int off_tx;
static unsigned int off_status;
static unsigned int off_rx;

const static ec_pdo_entry_reg_t domain1_regs[] = {
    {SerialPos,  Beckhoff_EL6002, 0x7001, 0x01, &off_ctrl},
    {SerialPos,  Beckhoff_EL6002, 0x7000, 0x11, &off_tx},
    {SerialPos,  Beckhoff_EL6002, 0x6001, 0x01, &off_status},
    {SerialPos,  Beckhoff_EL6002, 0x6000, 0x11, &off_rx},
    {}
};

static unsigned int counter = 0;

typedef enum {
    SER_REQUEST_INIT,
    SER_WAIT_FOR_INIT_RESPONSE,
    SER_READY
} serial_state_t;

typedef struct {
    size_t max_tx_data_size;
    size_t max_rx_data_size;

    uint8_t *tx_data;
    uint8_t tx_data_size;

    serial_state_t state;

    uint8_t tx_request_toggle;
    uint8_t tx_accepted_toggle;

    uint8_t rx_request_toggle;
    uint8_t rx_accepted_toggle;

    uint16_t control;
} serial_device_t;

static serial_device_t *ser = NULL;
static ec_tty_t *tty = NULL;
        
/*****************************************************************************/

/* Slave 1, "EL6002"
 * Vendor ID:       0x00000002
 * Product code:    0x17723052
 * Revision number: 0x00100000
 */

ec_pdo_entry_info_t slave_1_pdo_entries[] = {
   {0x7001, 0x01, 16}, /* Ctrl */
   {0x7000, 0x11, 8}, /* Data Out 0 */
   {0x7000, 0x12, 8}, /* Data Out 1 */
   {0x7000, 0x13, 8}, /* Data Out 2 */
   {0x7000, 0x14, 8}, /* Data Out 3 */
   {0x7000, 0x15, 8}, /* Data Out 4 */
   {0x7000, 0x16, 8}, /* Data Out 5 */
   {0x7000, 0x17, 8}, /* Data Out 6 */
   {0x7000, 0x18, 8}, /* Data Out 7 */
   {0x7000, 0x19, 8}, /* Data Out 8 */
   {0x7000, 0x1a, 8}, /* Data Out 9 */
   {0x7000, 0x1b, 8}, /* Data Out 10 */
   {0x7000, 0x1c, 8}, /* Data Out 11 */
   {0x7000, 0x1d, 8}, /* Data Out 12 */
   {0x7000, 0x1e, 8}, /* Data Out 13 */
   {0x7000, 0x1f, 8}, /* Data Out 14 */
   {0x7000, 0x20, 8}, /* Data Out 15 */
   {0x7000, 0x21, 8}, /* Data Out 16 */
   {0x7000, 0x22, 8}, /* Data Out 17 */
   {0x7000, 0x23, 8}, /* Data Out 18 */
   {0x7000, 0x24, 8}, /* Data Out 19 */
   {0x7000, 0x25, 8}, /* Data Out 20 */
   {0x7000, 0x26, 8}, /* Data Out 21 */
   {0x7011, 0x01, 16}, /* Ctrl */
   {0x7010, 0x11, 8}, /* Data Out 0 */
   {0x7010, 0x12, 8}, /* Data Out 1 */
   {0x7010, 0x13, 8}, /* Data Out 2 */
   {0x7010, 0x14, 8}, /* Data Out 3 */
   {0x7010, 0x15, 8}, /* Data Out 4 */
   {0x7010, 0x16, 8}, /* Data Out 5 */
   {0x7010, 0x17, 8}, /* Data Out 6 */
   {0x7010, 0x18, 8}, /* Data Out 7 */
   {0x7010, 0x19, 8}, /* Data Out 8 */
   {0x7010, 0x1a, 8}, /* Data Out 9 */
   {0x7010, 0x1b, 8}, /* Data Out 10 */
   {0x7010, 0x1c, 8}, /* Data Out 11 */
   {0x7010, 0x1d, 8}, /* Data Out 12 */
   {0x7010, 0x1e, 8}, /* Data Out 13 */
   {0x7010, 0x1f, 8}, /* Data Out 14 */
   {0x7010, 0x20, 8}, /* Data Out 15 */
   {0x7010, 0x21, 8}, /* Data Out 16 */
   {0x7010, 0x22, 8}, /* Data Out 17 */
   {0x7010, 0x23, 8}, /* Data Out 18 */
   {0x7010, 0x24, 8}, /* Data Out 19 */
   {0x7010, 0x25, 8}, /* Data Out 20 */
   {0x7010, 0x26, 8}, /* Data Out 21 */
   {0x6001, 0x01, 16}, /* Status */
   {0x6000, 0x11, 8}, /* Data In 0 */
   {0x6000, 0x12, 8}, /* Data In 1 */
   {0x6000, 0x13, 8}, /* Data In 2 */
   {0x6000, 0x14, 8}, /* Data In 3 */
   {0x6000, 0x15, 8}, /* Data In 4 */
   {0x6000, 0x16, 8}, /* Data In 5 */
   {0x6000, 0x17, 8}, /* Data In 6 */
   {0x6000, 0x18, 8}, /* Data In 7 */
   {0x6000, 0x19, 8}, /* Data In 8 */
   {0x6000, 0x1a, 8}, /* Data In 9 */
   {0x6000, 0x1b, 8}, /* Data In 10 */
   {0x6000, 0x1c, 8}, /* Data In 11 */
   {0x6000, 0x1d, 8}, /* Data In 12 */
   {0x6000, 0x1e, 8}, /* Data In 13 */
   {0x6000, 0x1f, 8}, /* Data In 14 */
   {0x6000, 0x20, 8}, /* Data In 15 */
   {0x6000, 0x21, 8}, /* Data In 16 */
   {0x6000, 0x22, 8}, /* Data In 17 */
   {0x6000, 0x23, 8}, /* Data In 18 */
   {0x6000, 0x24, 8}, /* Data In 19 */
   {0x6000, 0x25, 8}, /* Data In 20 */
   {0x6000, 0x26, 8}, /* Data In 21 */
   {0x6011, 0x01, 16}, /* Status */
   {0x6010, 0x11, 8}, /* Data In 0 */
   {0x6010, 0x12, 8}, /* Data In 1 */
   {0x6010, 0x13, 8}, /* Data In 2 */
   {0x6010, 0x14, 8}, /* Data In 3 */
   {0x6010, 0x15, 8}, /* Data In 4 */
   {0x6010, 0x16, 8}, /* Data In 5 */
   {0x6010, 0x17, 8}, /* Data In 6 */
   {0x6010, 0x18, 8}, /* Data In 7 */
   {0x6010, 0x19, 8}, /* Data In 8 */
   {0x6010, 0x1a, 8}, /* Data In 9 */
   {0x6010, 0x1b, 8}, /* Data In 10 */
   {0x6010, 0x1c, 8}, /* Data In 11 */
   {0x6010, 0x1d, 8}, /* Data In 12 */
   {0x6010, 0x1e, 8}, /* Data In 13 */
   {0x6010, 0x1f, 8}, /* Data In 14 */
   {0x6010, 0x20, 8}, /* Data In 15 */
   {0x6010, 0x21, 8}, /* Data In 16 */
   {0x6010, 0x22, 8}, /* Data In 17 */
   {0x6010, 0x23, 8}, /* Data In 18 */
   {0x6010, 0x24, 8}, /* Data In 19 */
   {0x6010, 0x25, 8}, /* Data In 20 */
   {0x6010, 0x26, 8}, /* Data In 21 */
};

ec_pdo_info_t slave_1_pdos[] = {
   {0x1604, 23, slave_1_pdo_entries + 0}, /* COM RxPDO-Map Outputs Ch.1 */
   {0x1605, 23, slave_1_pdo_entries + 23}, /* COM RxPDO-Map Outputs Ch.2 */
   {0x1a04, 23, slave_1_pdo_entries + 46}, /* COM TxPDO-Map Inputs Ch.1 */
   {0x1a05, 23, slave_1_pdo_entries + 69}, /* COM TxPDO-Map Inputs Ch.2 */
};

ec_sync_info_t slave_1_syncs[] = {
   {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {2, EC_DIR_OUTPUT, 2, slave_1_pdos + 0, EC_WD_DISABLE},
   {3, EC_DIR_INPUT, 2, slave_1_pdos + 2, EC_WD_DISABLE},
   {0xff}
};

/****************************************************************************/

int serial_init(serial_device_t *ser, size_t max_tx, size_t max_rx)
{
    ser->max_tx_data_size = max_tx;
    ser->max_rx_data_size = max_rx;
    ser->tx_data = NULL;
    ser->tx_data_size = 0;
    ser->state = SER_REQUEST_INIT;
    ser->tx_request_toggle = 0;
    ser->rx_accepted_toggle = 0;
    ser->control = 0x0000;

    if (max_tx > 0) {
        ser->tx_data = kmalloc(max_tx, GFP_KERNEL);
        if (ser->tx_data == NULL) {
            return -ENOMEM;
        }
    }

    return 0;
}

/****************************************************************************/

void serial_clear(serial_device_t *ser)
{
    if (ser->tx_data) {
        kfree(ser->tx_data);
    }
}

/****************************************************************************/

void serial_run(serial_device_t *ser, uint16_t status, uint8_t *rx_data)
{
    uint8_t tx_accepted_toggle, rx_request_toggle;

    switch (ser->state) {
        case SER_READY:

            /* Send data */
            
            tx_accepted_toggle = status & 0x0001;
            if (tx_accepted_toggle != ser->tx_accepted_toggle) { // ready
                ser->tx_data_size =
                    ectty_tx_data(tty, ser->tx_data, ser->max_tx_data_size);
                if (ser->tx_data_size) {
                    printk(KERN_INFO PFX "Sending %u bytes.\n", ser->tx_data_size);
                    ser->tx_request_toggle = !ser->tx_request_toggle;
                    ser->tx_accepted_toggle = tx_accepted_toggle;
                }
            }

            /* Receive data */

            rx_request_toggle = status & 0x0002;
            if (rx_request_toggle != ser->rx_request_toggle) {
                uint8_t rx_data_size = status >> 8;
                ser->rx_request_toggle = rx_request_toggle;
                printk(KERN_INFO PFX "Received %u bytes.\n", rx_data_size);
                ectty_rx_data(tty, rx_data, rx_data_size);
                ser->rx_accepted_toggle = !ser->rx_accepted_toggle;
            }

            ser->control =
                ser->tx_request_toggle |
                ser->rx_accepted_toggle << 1 |
                ser->tx_data_size << 8;
            break;

        case SER_REQUEST_INIT:
            if (status & (1 << 2)) {
                ser->control = 0x0000;
                ser->state = SER_WAIT_FOR_INIT_RESPONSE;
            } else {
                ser->control = 1 << 2; // CW.2, request initialization
            }
            break;

        case SER_WAIT_FOR_INIT_RESPONSE:
            if (!(status & (1 << 2))) {
                printk(KERN_INFO PFX "Init successful.\n");
                ser->tx_accepted_toggle = 1;
                ser->control = 0x0000;
                ser->state = SER_READY;
            }
            break;
    }

}

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

void cyclic_task(unsigned long data)
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

        // check for master state (optional)
        check_master_state();
    }

    serial_run(ser, EC_READ_U16(domain1_pd + off_status), domain1_pd + off_rx);
    EC_WRITE_U16(domain1_pd + off_ctrl, ser->control);
    memcpy(domain1_pd + off_tx, ser->tx_data, 22);

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
    ec_slave_config_t *sc;
    
    printk(KERN_INFO PFX "Starting...\n");

    ser = kmalloc(sizeof(*ser), GFP_KERNEL);
    if (!ser) {
        printk(KERN_ERR PFX "Failed to allocate serial device object.\n");
        ret = -ENOMEM;
        goto out_return;
    }

    ret = serial_init(ser, 22, 22);
    if (ret) {
        printk(KERN_ERR PFX "Failed to init serial device object.\n");
        goto out_free_serial;
    }

    tty = ectty_create();
    if (IS_ERR(tty)) {
        printk(KERN_ERR PFX "Failed to create tty.\n");
        ret = PTR_ERR(tty);
        goto out_serial;
    }
    
    master = ecrt_request_master(0);
    if (!master) {
        ret = -EBUSY; 
        printk(KERN_ERR PFX "Requesting master 0 failed.\n");
        goto out_tty;
    }

    init_MUTEX(&master_sem);
    ecrt_master_callbacks(master, send_callback, receive_callback, master);

    printk(KERN_INFO PFX "Registering domain...\n");
    if (!(domain1 = ecrt_master_create_domain(master))) {
        printk(KERN_ERR PFX "Domain creation failed!\n");
        goto out_release_master;
    }

    // Create configuration for bus coupler
    sc = ecrt_master_slave_config(master, BusCouplerPos, Beckhoff_EK1100);
    if (!sc)
        return -1;

    if (!(sc = ecrt_master_slave_config(
                    master, SerialPos, Beckhoff_EL6002))) {
        printk(KERN_ERR PFX "Failed to get slave configuration.\n");
        return -1;
    }

    printk("Configuring PDOs...\n");
    if (ecrt_slave_config_pdos(sc, EC_END, slave_1_syncs)) {
        printk(KERN_ERR PFX "Failed to configure PDOs.\n");
        return -1;
    }

    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        printk(KERN_ERR PFX "PDO entry registration failed!\n");
        return -1;
    }

    printk(KERN_INFO PFX "Activating master...\n");
    if (ecrt_master_activate(master)) {
        printk(KERN_ERR PFX "Failed to activate master!\n");
        goto out_release_master;
    }

    // Get internal process data for domain
    domain1_pd = ecrt_domain_data(domain1);

    printk(KERN_INFO PFX "Starting cyclic sample thread.\n");
    init_timer(&timer);
    timer.function = cyclic_task;
    timer.expires = jiffies + 10;
    add_timer(&timer);

    printk(KERN_INFO PFX "Started.\n");
    return 0;

out_release_master:
    printk(KERN_ERR PFX "Releasing master...\n");
    ecrt_release_master(master);
out_tty:
    ectty_free(tty);
out_serial:
    serial_clear(ser);
out_free_serial:
    kfree(ser);
out_return:
    printk(KERN_ERR PFX "Failed to load. Aborting.\n");
    return ret;
}

/*****************************************************************************/

void __exit cleanup_mini_module(void)
{
    printk(KERN_INFO PFX "Stopping...\n");

    del_timer_sync(&timer);

    printk(KERN_INFO PFX "Releasing master...\n");
    ecrt_release_master(master);

    ectty_free(tty);
    serial_clear(ser);
    kfree(ser);

    printk(KERN_INFO PFX "Unloading.\n");
}

/*****************************************************************************/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT minimal test environment");

module_init(init_mini_module);
module_exit(cleanup_mini_module);

/*****************************************************************************/
