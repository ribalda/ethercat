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
#include <linux/tty_flip.h>
#include <linux/termios.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/serial.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "../master/globals.h"
#include "../include/ectty.h"

/*****************************************************************************/

#define PFX "ec_tty: "

#define EC_TTY_MAX_DEVICES 32
#define EC_TTY_TX_BUFFER_SIZE 100
#define EC_TTY_RX_BUFFER_SIZE 100

#define EC_TTY_DEBUG 0

/*****************************************************************************/

char *ec_master_version_str = EC_MASTER_VERSION; /**< Version string. */
unsigned int debug_level = 0;

static struct tty_driver *tty_driver = NULL;
ec_tty_t *ttys[EC_TTY_MAX_DEVICES];
struct semaphore tty_sem;

void ec_tty_wakeup(unsigned long);

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT TTY driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

module_param_named(debug_level, debug_level, uint, S_IRUGO);
MODULE_PARM_DESC(debug_level, "Debug level");

/** \endcond */

/** Standard termios for ec_tty devices.
 *
 * Simplest possible configuration, as you would expect.
 */
static struct ktermios ec_tty_std_termios = {
    .c_iflag = 0,
    .c_oflag = 0,
    .c_cflag = B9600 | CS8 | CREAD,
    .c_lflag = 0,
    .c_cc = INIT_C_CC,
};

struct ec_tty {
    int minor;
    struct device *dev;

    uint8_t tx_buffer[EC_TTY_TX_BUFFER_SIZE];
    unsigned int tx_read_idx;
    unsigned int tx_write_idx;
    unsigned int wakeup;

    uint8_t rx_buffer[EC_TTY_RX_BUFFER_SIZE];
    unsigned int rx_read_idx;
    unsigned int rx_write_idx;

    struct timer_list timer;
    struct tty_struct *tty;
    unsigned int open_count;
    struct semaphore sem;

    ec_tty_operations_t ops;
    void *cb_data;
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

    sema_init(&tty_sem, 1);

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

/******************************************************************************
 * ec_tty_t methods.
 *****************************************************************************/

int ec_tty_init(ec_tty_t *t, int minor,
        const ec_tty_operations_t *ops, void *cb_data)
{
    int ret;
    tcflag_t cflag;
    struct tty_struct *tty;
    struct ktermios *termios;

    t->minor = minor;
    t->tx_read_idx = 0;
    t->tx_write_idx = 0;
    t->wakeup = 0;
    t->rx_read_idx = 0;
    t->rx_write_idx = 0;
    init_timer(&t->timer);
    t->tty = NULL;
    t->open_count = 0;
    sema_init(&t->sem, 1);
    t->ops = *ops;
    t->cb_data = cb_data;

    t->dev = tty_register_device(tty_driver, t->minor, NULL);
    if (IS_ERR(t->dev)) {
        printk(KERN_ERR PFX "Failed to register tty device.\n");
        return PTR_ERR(t->dev);
    }

    // Tell the device-specific implementation about the initial cflags
    tty = tty_driver->ttys[minor];

    termios =
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
        &tty->termios
#else
        tty->termios
#endif
        ;

    if (tty && termios) { // already opened before
        cflag = termios->c_cflag;
    } else {
        cflag = tty_driver->init_termios.c_cflag;
    }
    ret = t->ops.cflag_changed(t->cb_data, cflag);
    if (ret) {
        printk(KERN_ERR PFX "ERROR: Initial cflag 0x%x not accepted.\n",
                cflag);
        tty_unregister_device(tty_driver, t->minor);
        return ret;
    }

    t->timer.function = ec_tty_wakeup;
    t->timer.data = (unsigned long) t;
    t->timer.expires = jiffies + 10;
    add_timer(&t->timer);
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

/*****************************************************************************/

unsigned int ec_tty_rx_size(ec_tty_t *tty)
{
    unsigned int ret;

    if (tty->rx_write_idx >= tty->rx_read_idx) {
        ret = tty->rx_write_idx - tty->rx_read_idx;
    } else {
        ret = EC_TTY_RX_BUFFER_SIZE + tty->rx_write_idx - tty->rx_read_idx;
    }

    return ret;
}

/*****************************************************************************/

unsigned int ec_tty_rx_space(ec_tty_t *tty)
{
    return EC_TTY_RX_BUFFER_SIZE - 1 - ec_tty_rx_size(tty);
}

/*****************************************************************************/

int ec_tty_get_serial_info(ec_tty_t *tty, struct serial_struct *data)
{
    struct serial_struct tmp;

    if (!data)
        return -EFAULT;

    memset(&tmp, 0, sizeof(tmp));

    if (copy_to_user(data, &tmp, sizeof(*data))) {
        return -EFAULT;
    }
    return 0;
}

/*****************************************************************************/

/** Timer function.
 */
void ec_tty_wakeup(unsigned long data)
{
    ec_tty_t *tty = (ec_tty_t *) data;
    size_t to_recv;

    /* Wake up any process waiting to send data */
    if (tty->wakeup) {
        if (tty->tty) {
#if EC_TTY_DEBUG >= 1
            printk(KERN_INFO PFX "Waking up.\n");
#endif
            tty_wakeup(tty->tty);
        }
        tty->wakeup = 0;
    }

    /* Push received data into TTY core. */
    to_recv = ec_tty_rx_size(tty);
    if (to_recv && tty->tty) {
        unsigned char *cbuf;
        int space = tty_prepare_flip_string(tty->tty, &cbuf, to_recv);

        if (space < to_recv) {
            printk(KERN_WARNING PFX "Insufficient space to_recv=%d space=%d\n",
                    to_recv, space);
        }

        if (space < 0) {
            to_recv = 0;
        } else {
            to_recv = space;
        }

        if (to_recv) {
            unsigned int i;

#if EC_TTY_DEBUG >= 1
            printk(KERN_INFO PFX "Pushing %u bytes to TTY core.\n", to_recv);
#endif

            for (i = 0; i < to_recv; i++) {
                cbuf[i] = tty->rx_buffer[tty->rx_read_idx];
                tty->rx_read_idx =
                    (tty->rx_read_idx + 1) % EC_TTY_RX_BUFFER_SIZE;
            }
            tty_flip_buffer_push(tty->tty);
        }
    }

    tty->timer.expires += 1;
    add_timer(&tty->timer);
}

/******************************************************************************
 * Device callbacks
 *****************************************************************************/

static int ec_tty_open(struct tty_struct *tty, struct file *file)
{
    ec_tty_t *t;
    int line = tty->index;

#if EC_TTY_DEBUG >= 1
    printk(KERN_INFO PFX "%s(tty=%p, file=%p): Opening line %i.\n",
            __func__, tty, file, line);
#endif

    if (line < 0 || line >= EC_TTY_MAX_DEVICES) {
        tty->driver_data = NULL;
        return -ENXIO;
    }

    t = ttys[line];
    if (!t) {
        tty->driver_data = NULL;
        return -ENXIO;
    }

    if (!t->tty) {
        t->tty = tty;
        tty->driver_data = t;
    }

    down(&t->sem);
    t->open_count++;
    up(&t->sem);
    return 0;
}

/*****************************************************************************/

static void ec_tty_close(struct tty_struct *tty, struct file *file)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;

#if EC_TTY_DEBUG >= 1
    printk(KERN_INFO PFX "%s(tty=%p, file=%p): Closing line %i.\n",
            __func__, tty, file, tty->index);
#endif

    if (t) {
        down(&t->sem);
        if (--t->open_count == 0) {
            t->tty = NULL;
        }
        up(&t->sem);
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

#if EC_TTY_DEBUG >= 1
    printk(KERN_INFO PFX "%s(count=%i)\n", __func__, count);
#endif

    if (count <= 0) {
        return 0;
    }

    data_size = min(ec_tty_tx_space(t), (unsigned int) count);
    for (i = 0; i < data_size; i++) {
        t->tx_buffer[t->tx_write_idx] = buffer[i];
        t->tx_write_idx = (t->tx_write_idx + 1) % EC_TTY_TX_BUFFER_SIZE;
    }

#if EC_TTY_DEBUG >= 1
    printk(KERN_INFO PFX "%s(): %u bytes written.\n", __func__, data_size);
#endif
    return data_size;
}

/*****************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
static int ec_tty_put_char(struct tty_struct *tty, unsigned char ch)
#else
static void ec_tty_put_char(struct tty_struct *tty, unsigned char ch)
#endif
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;

#if EC_TTY_DEBUG >= 1
    printk(KERN_INFO PFX "%s(): c=%02x.\n", __func__, (unsigned int) ch);
#endif

    if (ec_tty_tx_space(t)) {
        t->tx_buffer[t->tx_write_idx] = ch;
        t->tx_write_idx = (t->tx_write_idx + 1) % EC_TTY_TX_BUFFER_SIZE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
        return 1;
#endif
    } else {
        printk(KERN_WARNING PFX "%s(): Dropped a byte!\n", __func__);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
        return 0;
#endif
    }
}

/*****************************************************************************/

static int ec_tty_write_room(struct tty_struct *tty)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;
    int ret = ec_tty_tx_space(t);

#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s() = %i.\n", __func__, ret);
#endif

    return ret;
}

/*****************************************************************************/

static int ec_tty_chars_in_buffer(struct tty_struct *tty)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;
    int ret;

#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s().\n", __func__);
#endif

    ret = ec_tty_tx_size(t);

#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s() = %i.\n", __func__, ret);
#endif

    return ret;
}

/*****************************************************************************/

static void ec_tty_flush_buffer(struct tty_struct *tty)
{
#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s().\n", __func__);
#endif

    // FIXME empty ring buffer
}

/*****************************************************************************/

static int ec_tty_ioctl(struct tty_struct *tty,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
        struct file *file,
#endif
        unsigned int cmd, unsigned long arg)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;
    int ret = -ENOTTY;

#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s(tty=%p, "
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
            "file=%p, "
#endif
            "cmd=%08x, arg=%08lx).\n",
            __func__, tty,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
            file,
#endif
            cmd, arg);
    printk(KERN_INFO PFX "decoded: type=%02x nr=%u\n",
            _IOC_TYPE(cmd), _IOC_NR(cmd));
#endif

    switch (cmd) {
        case TIOCGSERIAL:
            if (access_ok(VERIFY_WRITE,
                        (void *) arg, sizeof(struct serial_struct))) {
                ret = ec_tty_get_serial_info(t, (struct serial_struct *) arg);
            } else {
                ret = -EFAULT;
            }
            break;

        default:
#if EC_TTY_DEBUG >= 2
            printk(KERN_INFO PFX "no ioctl() -> handled by tty core!\n");
#endif
            ret = -ENOIOCTLCMD;
            break;
    }

    return ret;
}

/*****************************************************************************/

static void ec_tty_set_termios(struct tty_struct *tty,
        struct ktermios *old_termios)
{
    ec_tty_t *t = (ec_tty_t *) tty->driver_data;
    int ret;
    struct ktermios *termios;

#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s().\n", __func__);
#endif

    termios =
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
        &tty->termios
#else
        tty->termios
#endif
        ;

    if (termios->c_cflag == old_termios->c_cflag)
        return;

#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO "cflag changed from %x to %x.\n",
            old_termios->c_cflag, termios->c_cflag);
#endif

    ret = t->ops.cflag_changed(t->cb_data, termios->c_cflag);
    if (ret) {
        printk(KERN_ERR PFX "ERROR: cflag 0x%x not accepted.\n",
                termios->c_cflag);
        termios->c_cflag = old_termios->c_cflag;
    }
}

/*****************************************************************************/

static void ec_tty_stop(struct tty_struct *tty)
{
#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s().\n", __func__);
#endif
}

/*****************************************************************************/

static void ec_tty_start(struct tty_struct *tty)
{
#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s().\n", __func__);
#endif
}

/*****************************************************************************/

static void ec_tty_hangup(struct tty_struct *tty)
{
#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s().\n", __func__);
#endif
}

/*****************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
static int ec_tty_break(struct tty_struct *tty, int break_state)
#else
static void ec_tty_break(struct tty_struct *tty, int break_state)
#endif
{
#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s(break_state = %i).\n", __func__, break_state);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    return -EIO; // not implemented
#endif
}

/*****************************************************************************/

static void ec_tty_send_xchar(struct tty_struct *tty, char ch)
{
#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s(ch=%02x).\n", __func__, (unsigned int) ch);
#endif
}

/*****************************************************************************/

static void ec_tty_wait_until_sent(struct tty_struct *tty, int timeout)
{
#if EC_TTY_DEBUG >= 2
    printk(KERN_INFO PFX "%s(timeout=%i).\n", __func__, timeout);
#endif
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
    .set_termios = ec_tty_set_termios,
    .stop = ec_tty_stop,
    .start = ec_tty_start,
    .hangup = ec_tty_hangup,
    .break_ctl = ec_tty_break,
    .send_xchar = ec_tty_send_xchar,
    .wait_until_sent = ec_tty_wait_until_sent,
};

/******************************************************************************
 * Public functions and methods
 *****************************************************************************/

ec_tty_t *ectty_create(const ec_tty_operations_t *ops, void *cb_data)
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

            ret = ec_tty_init(tty, minor, ops, cb_data);
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
#if EC_TTY_DEBUG >= 1
        printk(KERN_INFO PFX "Fetching %u bytes to send.\n", data_size);
#endif
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

void ectty_rx_data(ec_tty_t *tty, const uint8_t *buffer, size_t size)
{
    size_t to_recv;

    if (size)  {
        unsigned int i;

#if EC_TTY_DEBUG >= 1
        printk(KERN_INFO PFX "Received %u bytes.\n", size);
#endif

        to_recv = min(ec_tty_rx_space(tty), size);

        if (to_recv < size) {
            printk(KERN_WARNING PFX "Dropping %u bytes.\n", size - to_recv);
        }

        for (i = 0; i < size; i++) {
            tty->rx_buffer[tty->rx_write_idx] = buffer[i];
            tty->rx_write_idx =
                (tty->rx_write_idx + 1) % EC_TTY_RX_BUFFER_SIZE;
        }
    }
}

/*****************************************************************************/

/** \cond */

module_init(ec_tty_init_module);
module_exit(ec_tty_cleanup_module);

EXPORT_SYMBOL(ectty_create);
EXPORT_SYMBOL(ectty_free);
EXPORT_SYMBOL(ectty_tx_data);
EXPORT_SYMBOL(ectty_rx_data);

/** \endcond */

/*****************************************************************************/
