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

#include "../master/globals.h"
#include "../include/ectty.h"

/*****************************************************************************/

#define PFX "ec_tty: "

#define EC_TTY_MAX_DEVICES 10

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

static int ec_tty_open(struct tty_struct *, struct file *);
static void ec_tty_close(struct tty_struct *, struct file *);
static int ec_tty_write(struct tty_struct *, const unsigned char *, int);

static const struct tty_operations ec_tty_ops = {
    .open = ec_tty_open,
    .close = ec_tty_close,
    .write = ec_tty_write,
};

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
};

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

int ec_tty_init(ec_tty_t *tty, int minor)
{
    tty->minor = minor;

    tty->dev = tty_register_device(tty_driver, tty->minor, NULL);
    if (IS_ERR(tty->dev)) {
        printk(KERN_ERR PFX "Failed to register tty device.\n");
        return PTR_ERR(tty->dev);
    }

    return 0;
}

/*****************************************************************************/

void ec_tty_clear(ec_tty_t *tty)
{
    tty_unregister_device(tty_driver, tty->minor);
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

    tty->driver_data = t;
    return 0;
}

/*****************************************************************************/

static void ec_tty_close(struct tty_struct *tty, struct file *file)
{
    printk(KERN_INFO PFX "Closing line %i.\n", tty->index);
}

/*****************************************************************************/

static int ec_tty_write(
        struct tty_struct *tty,
        const unsigned char *buffer,
        int count
        )
{
    return -EIO;
}

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
    ec_tty_clear(tty);
    ttys[tty->minor] = NULL;
    kfree(tty);
}

/*****************************************************************************/

/** \cond */

module_init(ec_tty_init_module);
module_exit(ec_tty_cleanup_module);

EXPORT_SYMBOL(ectty_create);
EXPORT_SYMBOL(ectty_free);

/** \endcond */

/*****************************************************************************/
