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

#include <linux/module.h>
#include <linux/err.h>
#include <linux/termios.h>
#include <linux/slab.h>

#include "../../include/ecrt.h" // EtherCAT realtime interface
#include "../../include/ectty.h" // EtherCAT TTY interface

/*****************************************************************************/

// Optional features
#define PFX "ec_tty_example: "

#define DEBUG 0

/*****************************************************************************/

#define VendorIdBeckhoff 0x00000002
#define ProductCodeBeckhoffEL6002 0x17723052

#define VendorIdIds 0x000012ad
#define ProductCodeIdsCSI71A 0x17723052

/*****************************************************************************/

typedef enum {
    SER_REQUEST_INIT,
    SER_WAIT_FOR_INIT_RESPONSE,
    SER_READY,
    SER_SET_RTSCTS,
    SER_SET_BAUD_RATE,
    SER_SET_DATA_FRAME,
} el60xx_port_state;

#define EL6002_PORT_NAME_SIZE 16

typedef struct {
    ec_tty_t *tty;
    char name[EL6002_PORT_NAME_SIZE];

    size_t max_tx_data_size;
    size_t max_rx_data_size;

    u8 *tx_data;
    u8 tx_data_size;

    el60xx_port_state state;

    u8 tx_request_toggle;
    u8 tx_accepted_toggle;

    u8 rx_request_toggle;
    u8 rx_accepted_toggle;

    u16 control;

    u32 off_ctrl;
    u32 off_tx;
    u32 off_status;
    u32 off_rx;

    ec_sdo_request_t *rtscts_sdo;
    u8 requested_rtscts;
    u8 current_rtscts;

    ec_sdo_request_t *baud_sdo;
    u8 requested_baud_rate;
    u8 current_baud_rate;

    ec_sdo_request_t *frame_sdo;
    u8 requested_data_frame;
    u8 current_data_frame;

    unsigned int config_error;

} el60xx_port_t;

#define EL6002_PORTS 2

typedef struct {
    struct list_head list;
    ec_slave_config_t *sc;
    el60xx_port_t port[EL6002_PORTS];
} el6002_t;

LIST_HEAD(handlers);

/*****************************************************************************/

/* Beckhoff EL6002
 * Vendor ID:       0x00000002
 * Product code:    0x17723052
 * Revision number: 0x00100000
 */

ec_pdo_entry_info_t el6002_pdo_entries[] = {
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

ec_pdo_info_t el6002_pdos[] = {
   {0x1604, 23, el6002_pdo_entries + 0}, /* COM RxPDO-Map Outputs Ch.1 */
   {0x1605, 23, el6002_pdo_entries + 23}, /* COM RxPDO-Map Outputs Ch.2 */
   {0x1a04, 23, el6002_pdo_entries + 46}, /* COM TxPDO-Map Inputs Ch.1 */
   {0x1a05, 23, el6002_pdo_entries + 69}, /* COM TxPDO-Map Inputs Ch.2 */
};

ec_sync_info_t el6002_syncs[] = {
   {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {2, EC_DIR_OUTPUT, 2, el6002_pdos + 0, EC_WD_DISABLE},
   {3, EC_DIR_INPUT, 2, el6002_pdos + 2, EC_WD_DISABLE},
   {0xff}
};

typedef enum {
    PAR_NONE,
    PAR_ODD,
    PAR_EVEN
} parity_t;

typedef struct {
    u8 value;
    unsigned int data_bits;
    parity_t parity;
    unsigned int stop_bits;
} el600x_data_frame_t;

/** EL600x supported values for data frame SDO.
 */
el600x_data_frame_t el600x_data_frame[] = {
    {0x01, 7, PAR_EVEN, 1},
    {0x09, 7, PAR_EVEN, 2},
    {0x02, 7, PAR_ODD,  1},
    {0x0a, 7, PAR_ODD,  2},
    {0x03, 8, PAR_NONE, 1},
    {0x0b, 8, PAR_NONE, 2},
    {0x04, 8, PAR_EVEN, 1},
    {0x0c, 8, PAR_EVEN, 2},
    {0x05, 8, PAR_ODD,  1},
    {0x0d, 8, PAR_ODD,  2},
};

typedef struct {
    u8 value;
    unsigned int baud;
    tcflag_t cbaud;
} el600x_baud_rate_t;

/** EL600x supported values for baud rate SDO.
 */
el600x_baud_rate_t el600x_baud_rate[] = {
    {1,   300,    B300},
    {2,   600,    B600},
    {3,   1200,   B1200},
    {4,   2400,   B2400},
    {5,   4800,   B4800},
    {6,   9600,   B9600},
    {7,   19200,  B19200},
    {8,   38400,  B38400},
    {9,   57600,  B57600},
    {10,  115200, B115200}
};

/****************************************************************************/

int el60xx_cflag_changed(void *data, tcflag_t cflag)
{
    el60xx_port_t *port = (el60xx_port_t *) data;
    unsigned int data_bits, stop_bits;
    tcflag_t cbaud, rtscts;
    parity_t par;
    unsigned int i;
    el600x_baud_rate_t *b_to_use = NULL;
    el600x_data_frame_t *df_to_use = NULL;

#if DEBUG
    printk(KERN_INFO PFX "%s(%s, cflag=%x).\n", __func__, port->name, cflag);
#endif

    rtscts = cflag & CRTSCTS;
    printk(KERN_INFO PFX "%s: Requested RTS/CTS: %s.\n",
            port->name, rtscts ? "yes" : "no");

    cbaud = cflag & CBAUD;

    for (i = 0; i < sizeof(el600x_baud_rate) / sizeof(el600x_baud_rate_t);
            i++) {
        el600x_baud_rate_t *b = el600x_baud_rate + i;
        if (b->cbaud == cbaud) {
            b_to_use = b;
            break;
        }
    }

    if (b_to_use) {
        printk(KERN_INFO PFX "%s: Requested baud rate: %u.\n",
                port->name, b_to_use->baud);
    } else {
        printk(KERN_ERR PFX "Error: %s does not support"
                " baud rate index %x.\n", port->name, cbaud);
        return -EINVAL;
    }

    switch (cflag & CSIZE) {
        case CS5:
            data_bits = 5;
            break;
        case CS6:
            data_bits = 6;
            break;
        case CS7:
            data_bits = 7;
            break;
        case CS8:
            data_bits = 8;
            break;
        default: /* CS5 or CS6 */
            data_bits = 0;
    }

    if (cflag & PARENB) {
        par = (cflag & PARODD) ? PAR_ODD : PAR_EVEN;
    } else {
        par = PAR_NONE;
    }

    stop_bits = (cflag & CSTOPB) ? 2 : 1;

    printk(KERN_INFO PFX "%s: Requested Data frame: %u%c%u.\n",
            port->name, data_bits,
            (par == PAR_NONE ? 'N' : (par == PAR_ODD ? 'O' : 'E')),
            stop_bits);

    for (i = 0; i < sizeof(el600x_data_frame) / sizeof(el600x_data_frame_t);
            i++) {
        el600x_data_frame_t *df = el600x_data_frame + i;
        if (df->data_bits == data_bits &&
                df->parity == par &&
                df->stop_bits == stop_bits) {
            df_to_use = df;
            break;
        }
    }

    if (!df_to_use) {
        printk(KERN_ERR PFX "Error: %s does not support data frame type.\n",
                port->name);
        return -EINVAL;
    }

    port->requested_rtscts = rtscts != 0;
    port->requested_baud_rate = b_to_use->value;
    port->requested_data_frame = df_to_use->value;
    port->config_error = 0;
    return 0;
}

/****************************************************************************/

static ec_tty_operations_t el60xx_tty_ops = {
    .cflag_changed = el60xx_cflag_changed,
};

/****************************************************************************/

int el60xx_port_init(el60xx_port_t *port, ec_slave_config_t *sc,
        ec_domain_t *domain, unsigned int slot_offset, const char *name)
{
    int ret = 0;

    strncpy(port->name, name, EL6002_PORT_NAME_SIZE);

    port->tty = ectty_create(&el60xx_tty_ops, port);
    if (IS_ERR(port->tty)) {
        printk(KERN_ERR PFX "Failed to create tty for %s.\n",
                port->name);
        ret = PTR_ERR(port->tty);
        goto out_return;
    }

    port->max_tx_data_size = 22;
    port->max_rx_data_size = 22;
    port->tx_data = NULL;
    port->tx_data_size = 0;
    port->state = SER_REQUEST_INIT;
    port->tx_request_toggle = 0;
    port->rx_accepted_toggle = 0;
    port->control = 0x0000;
    port->off_ctrl = 0;
    port->off_tx = 0;
    port->off_status = 0;
    port->off_rx = 0;
    port->requested_rtscts = 0x00; // no hardware handshake
    port->current_rtscts = 0xff;
    port->requested_baud_rate = 6; // 9600
    port->current_baud_rate = 0;
    port->requested_data_frame = 0x03; // 8N1
    port->current_data_frame = 0x00;
    port->config_error = 0;

    if (!(port->rtscts_sdo = ecrt_slave_config_create_sdo_request(sc,
                    0x8000 + slot_offset, 0x01, 1))) {
        printk(KERN_ERR PFX "Failed to create SDO request for %s.\n",
                port->name);
        ret = -ENOMEM;
        goto out_free_tty;
    }

    if (!(port->baud_sdo = ecrt_slave_config_create_sdo_request(sc,
                    0x8000 + slot_offset, 0x11, 1))) {
        printk(KERN_ERR PFX "Failed to create SDO request for %s.\n",
                port->name);
        ret = -ENOMEM;
        goto out_free_tty;
    }

    if (!(port->frame_sdo = ecrt_slave_config_create_sdo_request(sc,
                    0x8000 + slot_offset, 0x15, 1))) {
        printk(KERN_ERR PFX "Failed to create SDO request for %s\n",
                port->name);
        ret = -ENOMEM;
        goto out_free_tty;
    }

    ret = ecrt_slave_config_reg_pdo_entry(
            sc, 0x7001 + slot_offset, 0x01, domain, NULL);
    if (ret < 0) {
        printk(KERN_ERR PFX "Failed to register PDO entry of %s\n",
                port->name);
        goto out_free_tty;
    }
    port->off_ctrl = ret;

    ret = ecrt_slave_config_reg_pdo_entry(
            sc, 0x7000 + slot_offset, 0x11, domain, NULL);
    if (ret < 0) {
        printk(KERN_ERR PFX "Failed to register PDO entry of %s.\n",
                port->name);
        goto out_free_tty;
    }
    port->off_tx = ret;

    ret = ecrt_slave_config_reg_pdo_entry(
            sc, 0x6001 + slot_offset, 0x01, domain, NULL);
    if (ret < 0) {
        printk(KERN_ERR PFX "Failed to register PDO entry of %s.\n",
                port->name);
        goto out_free_tty;
    }
    port->off_status = ret;

    ret = ecrt_slave_config_reg_pdo_entry(
            sc, 0x6000 + slot_offset, 0x11, domain, NULL);
    if (ret < 0) {
        printk(KERN_ERR PFX "Failed to register PDO entry of %s.\n",
                port->name);
        goto out_free_tty;
    }
    port->off_rx = ret;

    if (port->max_tx_data_size > 0) {
        port->tx_data = kmalloc(port->max_tx_data_size, GFP_KERNEL);
        if (port->tx_data == NULL) {
            printk(KERN_ERR PFX "Failed to allocate %u bytes of TX"
                    " memory for %s.\n", port->max_tx_data_size, port->name);
            ret = -ENOMEM;
            goto out_free_tty;
        }
    }

    return 0;

out_free_tty:
    ectty_free(port->tty);
out_return:
    return ret;
}

/****************************************************************************/

void el60xx_port_clear(el60xx_port_t *port)
{
    ectty_free(port->tty);
    if (port->tx_data) {
        kfree(port->tx_data);
    }
}

/****************************************************************************/

void el60xx_port_run(el60xx_port_t *port, u8 *pd)
{
    u16 status = EC_READ_U16(pd + port->off_status);
    u8 *rx_data = pd + port->off_rx;
    uint8_t tx_accepted_toggle, rx_request_toggle;

    switch (port->state) {
        case SER_READY:

            /* Check, if hardware handshaking has to be configured. */
            if (!port->config_error &&
                    port->requested_rtscts != port->current_rtscts) {
                EC_WRITE_U8(ecrt_sdo_request_data(port->rtscts_sdo),
                        port->requested_rtscts);
                ecrt_sdo_request_write(port->rtscts_sdo);
                port->state = SER_SET_RTSCTS;
                break;
            }

            /* Check, if the baud rate has to be configured. */
            if (!port->config_error &&
                    port->requested_baud_rate != port->current_baud_rate) {
                EC_WRITE_U8(ecrt_sdo_request_data(port->baud_sdo),
                        port->requested_baud_rate);
                ecrt_sdo_request_write(port->baud_sdo);
                port->state = SER_SET_BAUD_RATE;
                break;
            }

            /* Check, if the data frame has to be configured. */
            if (!port->config_error &&
                    port->requested_data_frame != port->current_data_frame) {
                EC_WRITE_U8(ecrt_sdo_request_data(port->frame_sdo),
                        port->requested_data_frame);
                ecrt_sdo_request_write(port->frame_sdo);
                port->state = SER_SET_DATA_FRAME;
                break;
            }

            /* Send data */

            tx_accepted_toggle = status & 0x0001;
            if (tx_accepted_toggle != port->tx_accepted_toggle) { // ready
                port->tx_data_size =
                    ectty_tx_data(port->tty, port->tx_data, port->max_tx_data_size);
                if (port->tx_data_size) {
#if DEBUG
                    printk(KERN_INFO PFX "%s: Sending %u bytes.\n",
                            port->name, port->tx_data_size);
#endif
                    port->tx_request_toggle = !port->tx_request_toggle;
                    port->tx_accepted_toggle = tx_accepted_toggle;
                }
            }

            /* Receive data */

            rx_request_toggle = status & 0x0002;
            if (rx_request_toggle != port->rx_request_toggle) {
                uint8_t rx_data_size = status >> 8;
                port->rx_request_toggle = rx_request_toggle;
#if DEBUG
                printk(KERN_INFO PFX "%s: Received %u bytes.\n",
                        port->name, rx_data_size);
#endif
                ectty_rx_data(port->tty, rx_data, rx_data_size);
                port->rx_accepted_toggle = !port->rx_accepted_toggle;
            }

            port->control =
                port->tx_request_toggle |
                port->rx_accepted_toggle << 1 |
                port->tx_data_size << 8;
            break;

        case SER_REQUEST_INIT:
            if (status & (1 << 2)) {
                port->control = 0x0000;
                port->state = SER_WAIT_FOR_INIT_RESPONSE;
            } else {
                port->control = 1 << 2; // CW.2, request initialization
            }
            break;

        case SER_WAIT_FOR_INIT_RESPONSE:
            if (!(status & (1 << 2))) {
                printk(KERN_INFO PFX "%s: Init successful.\n", port->name);
                port->tx_accepted_toggle = 1;
                port->control = 0x0000;
                port->state = SER_READY;
            }
            break;

        case SER_SET_RTSCTS:
            switch (ecrt_sdo_request_state(port->rtscts_sdo)) {
                case EC_REQUEST_SUCCESS:
                    printk(KERN_INFO PFX "%s: Accepted RTS/CTS.\n",
                            port->name);
                    port->current_rtscts = port->requested_rtscts;
                    port->state = SER_REQUEST_INIT;
                    break;
                case EC_REQUEST_ERROR:
                    printk(KERN_ERR PFX "Failed to set RTS/CTS on %s!\n",
                            port->name);
                    port->state = SER_REQUEST_INIT;
                    port->config_error = 1;
                    break;
                default:
                    break;
            }
            break;

        case SER_SET_BAUD_RATE:
            switch (ecrt_sdo_request_state(port->baud_sdo)) {
                case EC_REQUEST_SUCCESS:
                    printk(KERN_INFO PFX "%s: Accepted baud rate.\n",
                            port->name);
                    port->current_baud_rate = port->requested_baud_rate;
                    port->state = SER_REQUEST_INIT;
                    break;
                case EC_REQUEST_ERROR:
                    printk(KERN_ERR PFX "Failed to set baud rate on %s!\n",
                            port->name);
                    port->state = SER_REQUEST_INIT;
                    port->config_error = 1;
                    break;
                default:
                    break;
            }
            break;

        case SER_SET_DATA_FRAME:
            switch (ecrt_sdo_request_state(port->frame_sdo)) {
                case EC_REQUEST_SUCCESS:
                    printk(KERN_INFO PFX "%s: Accepted data frame.\n",
                            port->name);
                    port->current_data_frame = port->requested_data_frame;
                    port->state = SER_REQUEST_INIT;
                    break;
                case EC_REQUEST_ERROR:
                    printk(KERN_ERR PFX "Failed to set data frame on %s!\n",
                            port->name);
                    port->state = SER_REQUEST_INIT;
                    port->config_error = 1;
                    break;
                default:
                    break;
            }
            break;
    }

    EC_WRITE_U16(pd + port->off_ctrl, port->control);
    memcpy(pd + port->off_tx, port->tx_data, port->tx_data_size);
}

/****************************************************************************/

int el6002_init(el6002_t *el6002, ec_master_t *master, u16 position,
        ec_domain_t *domain, u32 vendor, u32 product)
{
    int ret = 0, i;

    if (!(el6002->sc = ecrt_master_slave_config(
                    master, 0, position, vendor, product))) {
        printk(KERN_ERR PFX "EL6002(%u): Failed to create"
                " slave configuration.\n", position);
        ret = -EBUSY;
        goto out_return;
    }

    if (ecrt_slave_config_pdos(el6002->sc, EC_END, el6002_syncs)) {
        printk(KERN_ERR PFX "EL6002(%u): Failed to configure PDOs.\n",
                position);
        ret = -ENOMEM;
        goto out_return;
    }

    for (i = 0; i < EL6002_PORTS; i++) {
        char name[EL6002_PORT_NAME_SIZE];
        snprintf(name, EL6002_PORT_NAME_SIZE, "EL6002(%u) X%u",
                position, i + 1);
        if (el60xx_port_init(el6002->port + i, el6002->sc, domain, i * 0x10,
                    name)) {
            printk(KERN_ERR PFX "EL6002(%u): Failed to init port X%u.\n",
                    position, i);
            goto out_ports;
        }
    }

    return 0;

out_ports:
    for (i--; i >= 0; i--) {
        el60xx_port_clear(el6002->port + i);
    }
out_return:
    return ret;
}

/****************************************************************************/

void el6002_clear(el6002_t *el6002)
{
    int i;

    for (i = 0; i < EL6002_PORTS; i++) {
        el60xx_port_clear(el6002->port + i);
    }
}

/****************************************************************************/

void el6002_run(el6002_t *el6002, u8 *pd)
{
    int i;

    for (i = 0; i < EL6002_PORTS; i++) {
        el60xx_port_run(el6002->port + i, pd);
    }
}

/*****************************************************************************/

void run_serial_devices(u8 *pd)
{
    el6002_t *el6002;

    list_for_each_entry(el6002, &handlers, list) {
        el6002_run(el6002, pd);
    }
}

/*****************************************************************************/

int create_el6002_handler(ec_master_t *master, ec_domain_t *domain,
        u16 position, u32 vendor, u32 product)
{
    el6002_t *el6002;
    int ret;

    printk(KERN_INFO PFX "Creating handler for EL6002 at position %u\n",
            position);

    el6002 = kmalloc(sizeof(*el6002), GFP_KERNEL);
    if (!el6002) {
        printk(KERN_ERR PFX "Failed to allocate serial device object.\n");
        return -ENOMEM;
    }

    ret = el6002_init(el6002, master, position, domain, vendor, product);
    if (ret) {
        kfree(el6002);
        return ret;
    }

    list_add_tail(&el6002->list, &handlers);
    return 0;
}

/*****************************************************************************/

int create_serial_devices(ec_master_t *master, ec_domain_t *domain)
{
    int i, ret;
    ec_master_info_t master_info;
    ec_slave_info_t slave_info;
    el6002_t *ser, *next;

    printk(KERN_INFO PFX "Registering serial devices...\n");

    ret = ecrt_master(master, &master_info);
    if (ret) {
        printk(KERN_ERR PFX "Failed to obtain master information.\n");
        goto out_return;
    }

    for (i = 0; i < master_info.slave_count; i++) {
        ret = ecrt_master_get_slave(master, i, &slave_info);
        if (ret) {
            printk(KERN_ERR PFX "Failed to obtain slave information.\n");
            goto out_free_handlers;
        }

        if (slave_info.vendor_id == VendorIdBeckhoff
                && slave_info.product_code == ProductCodeBeckhoffEL6002) {
            if (create_el6002_handler(master, domain, i,
                    slave_info.vendor_id, slave_info.product_code)) {
                goto out_free_handlers;
            }
        }

        if (slave_info.vendor_id == VendorIdIds
                && slave_info.product_code == ProductCodeIdsCSI71A) {
            if (create_el6002_handler(master, domain, i,
                    slave_info.vendor_id, slave_info.product_code)) {
                goto out_free_handlers;
            }
        }
    }

    printk(KERN_INFO PFX "Finished registering serial devices.\n");
    return 0;

out_free_handlers:
    list_for_each_entry_safe(ser, next, &handlers, list) {
        list_del(&ser->list);
        el6002_clear(ser);
        kfree(ser);
    }
out_return:
    return ret;
}

/*****************************************************************************/

void free_serial_devices(void)
{
    el6002_t *ser, *next;

    printk(KERN_INFO PFX "Cleaning up serial devices...\n");

    list_for_each_entry_safe(ser, next, &handlers, list) {
        list_del(&ser->list);
        el6002_clear(ser);
        kfree(ser);
    }

    printk(KERN_INFO PFX "Finished cleaning up serial devices.\n");
}

/*****************************************************************************/
