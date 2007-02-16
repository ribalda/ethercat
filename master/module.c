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

/**
   \file
   EtherCAT master driver module.
*/

/*****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include "globals.h"
#include "master.h"
#include "device.h"
#include "xmldev.h"

/*****************************************************************************/

int __init ec_init_module(void);
void __exit ec_cleanup_module(void);

/*****************************************************************************/

static char *main; /**< main devices parameter */
static char *backup; /**< backup devices parameter */

static LIST_HEAD(main_device_ids); /**< list of main device IDs */
static LIST_HEAD(backup_device_ids); /**< list of main device IDs */
static LIST_HEAD(masters); /**< list of masters */
static dev_t device_number; /**< XML character device number */
ec_xmldev_t xmldev; /**< XML character device */

char *ec_master_version_str = EC_MASTER_VERSION;

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION("EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

module_param(main, charp, S_IRUGO);
MODULE_PARM_DESC(main, "main device IDs");
module_param(backup, charp, S_IRUGO);
MODULE_PARM_DESC(backup, "backup device IDs");

/** \endcond */

/*****************************************************************************/

void clear_device_ids(struct list_head *device_ids)
{
    ec_device_id_t *dev_id, *next_dev_id;
    
    list_for_each_entry_safe(dev_id, next_dev_id, device_ids, list) {
        list_del(&dev_id->list);
        kfree(dev_id);
    }
}

/*****************************************************************************/

static int parse_device_id_mac(ec_device_id_t *dev_id,
        const char *src, const char **remainder)
{
    unsigned int i, value;
    char *rem;

    for (i = 0; i < ETH_ALEN; i++) {
        value = simple_strtoul(src, &rem, 16);
        if (rem != src + 2
                || value > 0xFF
                || (i < ETH_ALEN - 1 && *rem != ':')) {
            return -1;
        }
        dev_id->octets[i] = value;
        if (i < ETH_ALEN - 1)
            src = rem + 1;
    }

    dev_id->type = ec_device_id_mac;
    *remainder = rem;
    return 0;
}

/*****************************************************************************/

static int parse_device_ids(struct list_head *device_ids, const char *src)
{
    const char *rem;
    ec_device_id_t *dev_id;
    unsigned int index = 0;

    while (*src) {
        // allocate new device ID
        if (!(dev_id = kmalloc(sizeof(ec_device_id_t), GFP_KERNEL))) {
            EC_ERR("Out of memory!\n");
            goto out_free;
        }
        
        if (*src == ';') { // empty device ID
            dev_id->type = ec_device_id_empty;
        }
        else if (*src == 'M') {
            src++;
            if (parse_device_id_mac(dev_id, src, &rem)) {
                EC_ERR("Device ID %u: Invalid MAC syntax!\n", index);
                kfree(dev_id);
                goto out_free;
            }
            src = rem;
        }
        else {
            EC_ERR("Device ID %u: Unknown format \'%c\'!\n", index, *src);
            kfree(dev_id);
            goto out_free;
        }
        
        list_add_tail(&dev_id->list, device_ids); 
        if (*src) {
            if (*src != ';') {
                EC_ERR("Invalid delimiter '%c' after device ID %i!\n",
                        *src, index);
                goto out_free;
            }
            src++; // skip delimiter
        }
        index++;
    }

    return 0;

out_free:
    clear_device_ids(device_ids);
    return -1;
}

/*****************************************************************************/

static int create_device_ids(void)
{
    ec_device_id_t *id;
    unsigned int main_count = 0, backup_count = 0;
    
    if (parse_device_ids(&main_device_ids, main))
        return -1;

    if (parse_device_ids(&backup_device_ids, main))
        return -1;

    // count main device IDs and check for empty ones
    list_for_each_entry(id, &main_device_ids, list) {
        if (id->type == ec_device_id_empty) {
            EC_ERR("Main device IDs may not be empty!\n");
            return -1;
        }
        main_count++;
    }

    // count backup device IDs
    list_for_each_entry(id, &backup_device_ids, list) {
        backup_count++;
    }

    // fill up backup device IDs
    while (backup_count < main_count) {
        if (!(id = kmalloc(sizeof(ec_device_id_t), GFP_KERNEL))) {
            EC_ERR("Out of memory!\n");
            return -1;
        }
        
        id->type = ec_device_id_empty;
        list_add_tail(&id->list, &backup_device_ids);
        backup_count++;
    }

    return 0;
}

/*****************************************************************************/

static int device_id_check(const ec_device_id_t *dev_id,
        const struct net_device *dev, const char *driver_name,
        unsigned int device_index)
{
    unsigned int i;
    
    switch (dev_id->type) {
        case ec_device_id_mac:
            for (i = 0; i < ETH_ALEN; i++)
                if (dev->dev_addr[i] != dev_id->octets[i])
                    return 0;
            return 1;
        default:
            return 0;
    }
}
                

/*****************************************************************************/

/**
   Module initialization.
   Initializes \a ec_master_count masters.
   \return 0 on success, else < 0
*/

int __init ec_init_module(void)
{
    ec_master_t *master, *next;
    ec_device_id_t *main_dev_id, *backup_dev_id;
    unsigned int master_index = 0;

    EC_INFO("Master driver %s\n", EC_MASTER_VERSION);

    if (alloc_chrdev_region(&device_number, 0, 1, "EtherCAT")) {
        EC_ERR("Failed to obtain device number!\n");
        goto out_return;
    }

    if (create_device_ids())
        goto out_free_ids;
    
    if (!list_empty(&main_device_ids)) {
        main_dev_id =
            list_entry(main_device_ids.next, ec_device_id_t, list);
        backup_dev_id =
            list_entry(backup_device_ids.next, ec_device_id_t, list);
        
        while (1) {
            if (!(master = (ec_master_t *)
                        kmalloc(sizeof(ec_master_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate memory for EtherCAT master %i.\n",
                        master_index);
                goto out_free_masters;
            }

            if (ec_master_init(master, master_index,
                        main_dev_id, backup_dev_id, 0))
                goto out_free_masters;

            list_add_tail(&master->list, &masters);
            master_index++;

            // last device IDs?
            if (main_dev_id->list.next == &main_device_ids)
                break;
            
            // next device IDs
            main_dev_id =
                list_entry(main_dev_id->list.next, ec_device_id_t, list);
            backup_dev_id =
                list_entry(backup_dev_id->list.next, ec_device_id_t, list);
        }
    }
    
    EC_INFO("%u master%s waiting for devices.\n",
            master_index, (master_index == 1 ? "" : "s"));
    return 0;

out_free_masters:
    list_for_each_entry_safe(master, next, &masters, list) {
        list_del(&master->list);
        kobject_del(&master->kobj);
        kobject_put(&master->kobj);
    }
out_free_ids:
    clear_device_ids(&main_device_ids);
    clear_device_ids(&backup_device_ids);
    unregister_chrdev_region(device_number, 1);
out_return:
    return -1;
}

/*****************************************************************************/

/**
   Module cleanup.
   Clears all master instances.
*/

void __exit ec_cleanup_module(void)
{
    ec_master_t *master, *next;

    EC_INFO("Cleaning up master driver...\n");

    list_for_each_entry_safe(master, next, &masters, list) {
        list_del(&master->list);
        ec_master_destroy(master);
    }

    unregister_chrdev_region(device_number, 1);

    EC_INFO("Master driver cleaned up.\n");
}

/*****************************************************************************/

/**
   Gets a handle to a certain master.
   \returns pointer to master
*/

ec_master_t *ec_find_master(unsigned int master_index /**< master index */)
{
    ec_master_t *master;

    list_for_each_entry(master, &masters, list) {
        if (master->index == master_index) return master;
    }

    EC_ERR("Master %i does not exist!\n", master_index);
    return NULL;
}

/*****************************************************************************/

/**
   Outputs frame contents for debugging purposes.
*/

void ec_print_data(const uint8_t *data, /**< pointer to data */
                   size_t size /**< number of bytes to output */
                   )
{
    unsigned int i;

    EC_DBG("");
    for (i = 0; i < size; i++) {
        printk("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
}

/*****************************************************************************/

/**
   Outputs frame contents and differences for debugging purposes.
*/

void ec_print_data_diff(const uint8_t *d1, /**< first data */
                        const uint8_t *d2, /**< second data */
                        size_t size /** number of bytes to output */
                        )
{
    unsigned int i;

    EC_DBG("");
    for (i = 0; i < size; i++) {
        if (d1[i] == d2[i]) printk(".. ");
        else printk("%02X ", d2[i]);
        if ((i + 1) % 16 == 0) {
            printk("\n");
            EC_DBG("");
        }
    }
    printk("\n");
}

/*****************************************************************************/

/**
   Prints slave states in clear text.
*/

size_t ec_state_string(uint8_t states, /**< slave states */
                       char *buffer /**< target buffer
                                       (min. EC_STATE_STRING_SIZE bytes) */
                       )
{
    off_t off = 0;
    unsigned int first = 1;

    if (!states) {
        off += sprintf(buffer + off, "(unknown)");
        return off;
    }

    if (states & EC_SLAVE_STATE_INIT) {
        off += sprintf(buffer + off, "INIT");
        first = 0;
    }
    if (states & EC_SLAVE_STATE_PREOP) {
        if (!first) off += sprintf(buffer + off, ", ");
        off += sprintf(buffer + off, "PREOP");
        first = 0;
    }
    if (states & EC_SLAVE_STATE_SAVEOP) {
        if (!first) off += sprintf(buffer + off, ", ");
        off += sprintf(buffer + off, "SAVEOP");
        first = 0;
    }
    if (states & EC_SLAVE_STATE_OP) {
        if (!first) off += sprintf(buffer + off, ", ");
        off += sprintf(buffer + off, "OP");
    }
    if (states & EC_SLAVE_STATE_ACK_ERR) {
        if (!first) off += sprintf(buffer + off, " + ");
        off += sprintf(buffer + off, "ERROR");
    }

    return off;
}

/******************************************************************************
 *  Device interface
 *****************************************************************************/

/**
   Offers an EtherCAT device to a certain master.
   The master decides, if it wants to use the device for EtherCAT operation,
   or not. It is important, that the offered net_device is not used by
   the kernel IP stack. If the master, accepted the offer, the address of
   the newly created EtherCAT device is written to the ecdev pointer, else
   the pointer is written to zero.
   \return 0 on success, else < 0
   \ingroup DeviceInterface
*/

int ecdev_offer(struct net_device *net_dev, /**< net_device to offer */
        ec_device_t **ecdev, /**< pointer to store a device on success */
        const char *driver_name, /**< name of the network driver */
        unsigned int device_index, /**< index of the supported device */
        ec_pollfunc_t poll, /**< device poll function */
        struct module *module /**< pointer to the module */
        )
{
    ec_master_t *master;
    unsigned int i;

    list_for_each_entry(master, &masters, list) {
        if (down_interruptible(&master->device_sem)) {
            EC_ERR("Interrupted while waiting for device semaphore!\n");
            goto out_return;
        }

        if (device_id_check(master->main_device_id, net_dev,
                    driver_name, device_index)) {

            EC_INFO("Accepting device %s:%u (", driver_name, device_index);
            for (i = 0; i < ETH_ALEN; i++) {
                printk("%02X", net_dev->dev_addr[i]);
                if (i < ETH_ALEN - 1) printk(":");
            }
            printk(") for master %u.\n", master->index);

            if (master->device) {
                EC_ERR("Master already has a device.\n");
                goto out_up;
            }
            
            if (!(master->device = (ec_device_t *)
                        kmalloc(sizeof(ec_device_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate device!\n");
                goto out_up;
            }

            if (ec_device_init(master->device, master,
                        net_dev, poll, module)) {
                EC_ERR("Failed to init device!\n");
                goto out_free;
            }

            up(&master->device_sem);
            sprintf(net_dev->name, "ec%u", master->index);
            *ecdev = master->device; // offer accepted
            return 0; // no error
        }

        up(&master->device_sem);
    }

    *ecdev = NULL; // offer declined
    return 0; // no error

 out_free:
    kfree(master->device);
    master->device = NULL;
 out_up:
    up(&master->device_sem);
 out_return:
    return 1;
}

/*****************************************************************************/

/**
   Withdraws an EtherCAT device from the master.
   The device is disconnected from the master and all device ressources
   are freed.
   \attention Before calling this function, the ecdev_stop() function has
   to be called, to be sure that the master does not use the device any more.
   \ingroup DeviceInterface
*/

void ecdev_withdraw(ec_device_t *device /**< EtherCAT device */)
{
    ec_master_t *master = device->master;
    unsigned int i;

    down(&master->device_sem);
    
    EC_INFO("Master %u releasing device ", master->index);
    for (i = 0; i < ETH_ALEN; i++) {
        printk("%02X", device->dev->dev_addr[i]);
        if (i < ETH_ALEN - 1) printk(":");
    }
    printk(".\n");
    
    ec_device_clear(master->device);
    kfree(master->device);
    master->device = NULL;
    
    up(&master->device_sem);
}

/*****************************************************************************/

/**
   Opens the network device and makes the master enter IDLE mode.
   \return 0 on success, else < 0
   \ingroup DeviceInterface
*/

int ecdev_open(ec_device_t *device /**< EtherCAT device */)
{
    if (ec_device_open(device)) {
        EC_ERR("Failed to open device!\n");
        return -1;
    }

    if (ec_master_enter_idle_mode(device->master)) {
        EC_ERR("Failed to enter idle mode!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Makes the master leave IDLE mode and closes the network device.
   \return 0 on success, else < 0
   \ingroup DeviceInterface
*/

void ecdev_close(ec_device_t *device /**< EtherCAT device */)
{
    ec_master_leave_idle_mode(device->master);

    if (ec_device_close(device))
        EC_WARN("Failed to close device!\n");
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

/**
 * Returns the version magic of the realtime interface.
 * \return ECRT version magic.
 * \ingroup RealtimeInterface
 */

unsigned int ecrt_version_magic(void)
{
    return ECRT_VERSION_MAGIC;
}

/*****************************************************************************/

/**
   Reserves an EtherCAT master for realtime operation.
   \return pointer to reserved master, or NULL on error
   \ingroup RealtimeInterface
*/

ec_master_t *ecrt_request_master(unsigned int master_index
                                 /**< master index */
                                 )
{
    ec_master_t *master;

    EC_INFO("Requesting master %i...\n", master_index);

    if (!(master = ec_find_master(master_index))) goto out_return;

    if (!atomic_dec_and_test(&master->available)) {
        atomic_inc(&master->available);
        EC_ERR("Master %i is already in use!\n", master_index);
        goto out_return;
    }

    if (down_interruptible(&master->device_sem)) {
        EC_ERR("Interrupted while waiting for device!\n");
        goto out_release;
    }

    if (!master->device) {
        up(&master->device_sem);
        EC_ERR("Master %i has no assigned device!\n", master_index);
        goto out_release;
    }

    if (!try_module_get(master->device->module)) {
        up(&master->device_sem);
        EC_ERR("Device module is unloading!\n");
        goto out_release;
    }

    up(&master->device_sem);

    if (!master->device->link_state) {
        EC_ERR("Link is DOWN.\n");
        goto out_module_put;
    }

    if (ec_master_enter_operation_mode(master)) {
        EC_ERR("Failed to enter OPERATION mode!\n");
        goto out_module_put;
    }

    EC_INFO("Successfully requested master %i.\n", master_index);
    return master;

 out_module_put:
    module_put(master->device->module);
 out_release:
    atomic_inc(&master->available);
 out_return:
    return NULL;
}

/*****************************************************************************/

/**
   Releases a reserved EtherCAT master.
   \ingroup RealtimeInterface
*/

void ecrt_release_master(ec_master_t *master /**< EtherCAT master */)
{
    EC_INFO("Releasing master %i...\n", master->index);

    if (master->mode != EC_MASTER_MODE_OPERATION) {
        EC_WARN("Master %i was was not requested!\n", master->index);
        return;
    }

    ec_master_leave_operation_mode(master);

    module_put(master->device->module);
    atomic_inc(&master->available);

    EC_INFO("Released master %i.\n", master->index);
}

/*****************************************************************************/

/** \cond */

module_init(ec_init_module);
module_exit(ec_cleanup_module);

EXPORT_SYMBOL(ecdev_offer);
EXPORT_SYMBOL(ecdev_withdraw);
EXPORT_SYMBOL(ecdev_open);
EXPORT_SYMBOL(ecdev_close);
EXPORT_SYMBOL(ecrt_request_master);
EXPORT_SYMBOL(ecrt_release_master);
EXPORT_SYMBOL(ecrt_version_magic);

/** \endcond */

/*****************************************************************************/
