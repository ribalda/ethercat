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

/** \file
 * EtherCAT tty driver module.
 */

/*****************************************************************************/

#include <linux/module.h>
#include <linux/err.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/termios.h>
#include <linux/timer.h>

#include "../master/globals.h"
#include "../include/ectty.h"

/*****************************************************************************/

#define PFX "ec_tty: "

#define EC_TTY_MAX_DEVICES 10
#define EC_TTY_TX_BUFFER_SIZE 100

/*****************************************************************************/

char *ec_master_version_str = EC_MASTER_VERSION; /**< Version string. */
unsigned int debug_level = 0;

static struct tty_driver *tty_driver = NULL;
ec_tty_t *ttys[EC_TTY_MAX_DEVICES];
struct semaphore tty_sem;

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT TTY driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

module_param_named(debug_level, debug_level, uint, S_IRUGO);
MODULE_PARM_DESC(debug_level, "Debug level");

/** \endcond */

static struct ktermios ec_tty_std_termios = {
    .c_iflag = ICRNL | IXON,
    .c_oflag = OPOST | ONLCR,
    .c_cflag = B38400 | CS8 | CREAD | HUPCL,
    .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN,
    .c_cc = INIT_C_CC,
};

struct ec_tty {
    int minor;
    struct device *dev;
    uint8_t tx_buffer[EC_TTY_TX_BUFFER_SIZE];
    unsigned int tx_read_idx;
    unsigned int tx_write_idx;
    unsigned int wakeup;
    struct timer_list timer;
    struct tty_struct *tty;
};

static const struct tty_operations ec_tty_ops; // see below

/*****************************************************************************/

/** Module initialization.
 *
 * \return 0 on success, else < 0
 */
int __init ec_tty_init_module(void)
{
    int i, ret = 0;

    printk(KERN_INFO PFX "TTY driver %s\n", EC_MASTER_VERSION);

    init_MUTEX(&tty_sem);

    for (i = 0; i < EC_TTY_MAX_DEVICES; i++) {
        ttys[i] = NULL;
    }

    tty_driver = alloc_tty_driver(EC_TTY_MAX_DEVICES);
    if (!tty_driver) {
        printk(KERN_ERR PFX "Failed to allocate tty driver.\n");
        ret = -ENOMEM;
        goto out_return;
    }

    tty_driver->owner = THIS_MODULE;
    tty_driver->driver_name = "EtherCAT TTY";
    tty_driver->name = "ttyEC";
    tty_driver->major = 0;
    tty_driver->minor_start = 0;
    tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
    tty_driver->subtype = SERIAL_TYPE_NORMAL;
    tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
    tty_driver->init_termios = ec_tty_std_termios;
    tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    tty_set_operations(tty_driver, &ec_tty_ops);

    ret = tty_register_driver(tty_driver);
    if (ret) {
        printk(KERN_ERR PFX "Failed to register tty driver.\n");
        goto out_put;
    }

    return ret;
        
out_put:
    put_tty_driver(tty_driver);
out_return:
    return ret;
}

/*****************************************************************************/

/** Module cleanup.
 *
 * Clears all master instances.
 */
void __exit ec_tty_cleanup_module(void)
{
    tty_unregister_driver(tty_driver);
    put_tty_driver(tty_driver);
    printk(KERN_INFO PFX "Module unloading.\n");
}

/*****************************************************************************/

void ec_tty_wakeup(unsigned long data)
{
    ec_tty_t *tty = (ec_tty_t *) data;

    if (tty->wakeup) {
        if (tty->tty) {
            printk("Waking up.\n");
            tty_wakeup(tty->tty);
        }
        tty->wakeup = 0;
    }

    tty->timer.expires += 1;
    add_timer(&tty->timer);
}

/*****************************************************************************/

int ec_tty_init(ec_tty_t *tty, int minor)
{
    tty->minor = minor;
    tty->tx_read_idx = 0;
    tty->tx_write_idx = 0;
    tty->wakeup = 0;
    init_timer(&tty->timer);
    tty->tty = NULL;

    tty->dev = tty_register_device(tty_driver, tty->minor, NULL);
    if (IS_ERR(tty->dev)) {
        printk(KERN_ERR PFX "Failed to register tty device.\n");
        return PTR_ERR(tty->dev);
    }

    tty->timer.function = ec_tty_wakeup;
    tty->timer.data = (unsigned long) tty;
    tty->timer.expires = jiffies + 10;
    add_timer(&tty->timer);
    return 0;
}

/*****************************************************************************/

void ec_tty_clear(ec_tty_t *tty)
{
    del_timer_sync(&tty->timer);
    tty_unregister_device(tty_driver, tty->minor);
}

/*****************************************************************************/

unsigned int ec_tty_tx_size(ec_tty_t *tty)
{
    unsigned int ret;
    
    if (tty->tx_write_idx >= tty->tx_read_idx) {
        ret = tty->tx_write_idx - tty->tx_read_idx;
    } else {
        ret = EC_TTY_TX_BUFFER_SIZE + tty->tx_write_idx - tty->tx_read_idx;
    }

    return ret;
}

/*****************************************************************************/

unsigned int ec_tty_tx_space(ec_tty_t *tty)
{
    return EC_TTY_TX_BUFFER_SIZE - 1 - ec_tty_tx_size(tty);
}

/******************************************************************************
 * Device callbacks
 *****************************************************************************/

static int ec_tty_open(struct tty_struct *tty, struct file *file)
{
    ec_tty_t *t;
    int line = tty->index;

    printk(KERN_INFO PFX "Opening line %i.\n", line);

	if (line < 0 || line >= EC_TTY_MAX_DEVICES) {
		return -ENXIO;
    }

    t = ttys[line];
    if (!t) {
        return -ENXIO;
    }

    if (t->tty) {
        return -EBUSY;
    }

    t->tty = tty;
    tty->driver_data = t;
    return 0;
}

/*****************************************************************************/

static void ec_tty_close(struct tty_struct *tty, struct file *file)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;

    printk(KERN_INFO PFX "Closing line %i.\n", tty->index);

    if (t->tty == tty) {
        t->tty = NULL;
    }
}

/*****************************************************************************/

static int ec_tty_write(
        struct tty_struct *tty,
        const unsigned char *buffer,
        int count
        )
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;
    unsigned int data_size, i;
    
    printk(KERN_INFO PFX "%s(count=%i)\n", __func__, count);

    if (count <= 0) {
        return 0;
    }

    data_size = min(ec_tty_tx_space(t), (unsigned int) count);
    for (i = 0; i < data_size; i++) {
        t->tx_buffer[t->tx_write_idx] = buffer[i];
        t->tx_write_idx = (t->tx_write_idx + 1) % EC_TTY_TX_BUFFER_SIZE;
    }

    printk(KERN_INFO PFX "%s(): %u bytes written.\n", __func__, data_size);
    return data_size;
}

/*****************************************************************************/

static void ec_tty_put_char(struct tty_struct *tty, unsigned char ch)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;

    printk(KERN_INFO PFX "%s(): c=%02x.\n", __func__, (unsigned int) ch);

    if (ec_tty_tx_space(t)) {
        t->tx_buffer[t->tx_write_idx] = ch;
        t->tx_write_idx = (t->tx_write_idx + 1) % EC_TTY_TX_BUFFER_SIZE;
    } else {
        printk(KERN_WARNING PFX "%s(): Dropped a byte!\n", __func__);
    }
}

/*****************************************************************************/

static int ec_tty_write_room(struct tty_struct *tty)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;
    int ret = ec_tty_tx_space(t);
    
    printk(KERN_INFO PFX "%s() = %i.\n", __func__, ret);

    return ret;
}

/*****************************************************************************/

static int ec_tty_chars_in_buffer(struct tty_struct *tty)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;
    int ret;
    
    printk(KERN_INFO PFX "%s().\n", __func__);

    ret = ec_tty_tx_size(t);

    printk(KERN_INFO PFX "%s() = %i.\n", __func__, ret);
    
    return ret;
}

/*****************************************************************************/

static void ec_tty_flush_buffer(struct tty_struct *tty)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
}

/*****************************************************************************/

static int ec_tty_ioctl(struct tty_struct *tty, struct file *file,
		    unsigned int cmd, unsigned long arg)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
    return -ENOTTY;
}

/*****************************************************************************/

static void ec_tty_throttle(struct tty_struct *tty)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
}

/*****************************************************************************/

static void ec_tty_unthrottle(struct tty_struct *tty)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
}

/*****************************************************************************/

static void ec_tty_set_termios(struct tty_struct *tty,
			   struct ktermios *old_termios)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
}

/*****************************************************************************/

static void ec_tty_stop(struct tty_struct *tty)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
}

/*****************************************************************************/

static void ec_tty_start(struct tty_struct *tty)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
}

/*****************************************************************************/

static void ec_tty_hangup(struct tty_struct *tty)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
}

/*****************************************************************************/

static void ec_tty_break(struct tty_struct *tty, int break_state)
{
    printk(KERN_INFO PFX "%s(break_state = %i).\n", __func__, break_state);
}

/*****************************************************************************/

static void ec_tty_send_xchar(struct tty_struct *tty, char ch)
{
    printk(KERN_INFO PFX "%s(ch=%02x).\n", __func__, (unsigned int) ch);
}

/*****************************************************************************/

static void ec_tty_wait_until_sent(struct tty_struct *tty, int timeout)
{
    printk(KERN_INFO PFX "%s(timeout=%i).\n", __func__, timeout);
}

/*****************************************************************************/

static int ec_tty_tiocmget(struct tty_struct *tty, struct file *file)
{
    printk(KERN_INFO PFX "%s().\n", __func__);
    return -EBUSY;
}

/*****************************************************************************/

static int ec_tty_tiocmset(struct tty_struct *tty, struct file *file,
		    unsigned int set, unsigned int clear)
{
    printk(KERN_INFO PFX "%s(set=%u, clear=%u).\n", __func__, set, clear);
    return -EBUSY;
}

/*****************************************************************************/

static const struct tty_operations ec_tty_ops = {
    .open = ec_tty_open,
    .close = ec_tty_close,
    .write = ec_tty_write,
	.put_char = ec_tty_put_char,
	.write_room = ec_tty_write_room,
	.chars_in_buffer = ec_tty_chars_in_buffer,
	.flush_buffer = ec_tty_flush_buffer,
	.ioctl = ec_tty_ioctl,
	.throttle = ec_tty_throttle,
	.unthrottle = ec_tty_unthrottle,
	.set_termios = ec_tty_set_termios,
	.stop = ec_tty_stop,
	.start = ec_tty_start,
	.hangup = ec_tty_hangup,
	.break_ctl = ec_tty_break,
	.send_xchar = ec_tty_send_xchar,
	.wait_until_sent = ec_tty_wait_until_sent,
	.tiocmget = ec_tty_tiocmget,
	.tiocmset = ec_tty_tiocmset,
};

/******************************************************************************
 * Public functions and methods
 *****************************************************************************/

ec_tty_t *ectty_create(void)
{
    ec_tty_t *tty;
    int minor, ret;

    if (down_interruptible(&tty_sem)) {
        return ERR_PTR(-EINTR);
    }

    for (minor = 0; minor < EC_TTY_MAX_DEVICES; minor++) {
        if (!ttys[minor]) {
            printk(KERN_INFO PFX "Creating TTY interface %i.\n", minor);

            tty = kmalloc(sizeof(ec_tty_t), GFP_KERNEL);
            if (!tty) {
                up(&tty_sem);
                printk(KERN_ERR PFX "Failed to allocate memory.\n");
                return ERR_PTR(-ENOMEM);
            }

            ret = ec_tty_init(tty, minor);
            if (ret) {
                up(&tty_sem);
                kfree(tty);
                return ERR_PTR(ret);
            }

            ttys[minor] = tty;
            up(&tty_sem);
            return tty;
        }
    }

    up(&tty_sem);
    printk(KERN_ERR PFX "No free interfaces avaliable.\n");
    return ERR_PTR(-EBUSY);
}

/*****************************************************************************/

void ectty_free(ec_tty_t *tty)
{
    printk(KERN_INFO PFX "Freeing TTY interface %i.\n", tty->minor);

    ec_tty_clear(tty);
    ttys[tty->minor] = NULL;
    kfree(tty);
}

/*****************************************************************************/

unsigned int ectty_tx_data(ec_tty_t *tty, uint8_t *buffer, size_t size)
{
    unsigned int data_size = min(ec_tty_tx_size(tty), size), i;

    if (data_size)  {
        printk(KERN_INFO PFX "Fetching %u bytes to send.\n", data_size);
    }

    for (i = 0; i < data_size; i++) {
        buffer[i] = tty->tx_buffer[tty->tx_read_idx];
        tty->tx_read_idx = (tty->tx_read_idx + 1) % EC_TTY_TX_BUFFER_SIZE;
    }

    if (data_size) {
        tty->wakeup = 1;
    }

    return data_size;
}

/*****************************************************************************/

/** \cond */

module_init(ec_tty_init_module);
module_exit(ec_tty_cleanup_module);

EXPORT_SYMBOL(ectty_create);
EXPORT_SYMBOL(ectty_free);
EXPORT_SYMBOL(ectty_tx_data);

/** \endcond */

/*****************************************************************************/
