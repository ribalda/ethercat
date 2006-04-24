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
 *  as published by the Free Software Foundation; version 2 of the License.
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

/*****************************************************************************/

int __init ec_init_module(void);
void __exit ec_cleanup_module(void);

/*****************************************************************************/

/**
   Compile version info.
*/

#define COMPILE_INFO EC_STR(EC_MASTER_VERSION_MAIN) \
                     "." EC_STR(EC_MASTER_VERSION_SUB) \
                     " (" EC_MASTER_VERSION_EXTRA ")" \
                     " - rev. " EC_STR(SVNREV) \
                     ", compiled by " EC_STR(USER) \
                     " at " __DATE__ " " __TIME__

/*****************************************************************************/

static int ec_master_count = 1;
static struct list_head ec_masters;

/*****************************************************************************/

/** \cond */

MODULE_AUTHOR ("Florian Pose <fp@igh-essen.com>");
MODULE_DESCRIPTION ("EtherCAT master driver module");
MODULE_LICENSE("GPL");
MODULE_VERSION(COMPILE_INFO);

module_param(ec_master_count, int, 1);
MODULE_PARM_DESC(ec_master_count, "number of EtherCAT masters to initialize");

/** \endcond */

/*****************************************************************************/

/**
   Module initialization.
   Initializes \a ec_master_count masters.
   \return 0 on success, else < 0
*/

int __init ec_init_module(void)
{
    unsigned int i;
    ec_master_t *master, *next;

    EC_INFO("Master driver, %s\n", COMPILE_INFO);

    if (ec_master_count < 1) {
        EC_ERR("Error - Invalid ec_master_count: %i\n", ec_master_count);
        goto out_return;
    }

    EC_INFO("Initializing %i EtherCAT master(s)...\n", ec_master_count);

    INIT_LIST_HEAD(&ec_masters);

    for (i = 0; i < ec_master_count; i++) {
        if (!(master =
              (ec_master_t *) kmalloc(sizeof(ec_master_t), GFP_KERNEL))) {
            EC_ERR("Failed to allocate memory for EtherCAT master %i.\n", i);
            goto out_free;
        }

        if (ec_master_init(master, i)) // kobject_put is done inside...
            goto out_free;

        if (kobject_add(&master->kobj)) {
            EC_ERR("Failed to add kobj.\n");
            kobject_put(&master->kobj); // free master
            goto out_free;
        }

        list_add_tail(&master->list, &ec_masters);
    }

    EC_INFO("Master driver initialized.\n");
    return 0;

 out_free:
    list_for_each_entry_safe(master, next, &ec_masters, list) {
        list_del(&master->list);
        kobject_del(&master->kobj);
        kobject_put(&master->kobj);
    }
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

    list_for_each_entry_safe(master, next, &ec_masters, list) {
        list_del(&master->list);
        kobject_del(&master->kobj);
        kobject_put(&master->kobj); // free master
    }

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

    list_for_each_entry(master, &ec_masters, list) {
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

/******************************************************************************
 *  Device interface
 *****************************************************************************/

/**
   Connects an EtherCAT device to a certain master.
   The master will use the device for sending and receiving frames. It is
   required that no other instance (for example the kernel IP stack) uses
   the device.
   \return 0 on success, else < 0
   \ingroup DeviceInterface
*/

ec_device_t *ecdev_register(unsigned int master_index, /**< master index */
                            struct net_device *net_dev, /**< net_device of
                                                           the device */
                            ec_isr_t isr, /**< interrupt service routine */
                            struct module *module /**< pointer to the module */
                            )
{
    ec_master_t *master;

    if (!net_dev) {
        EC_WARN("Device is NULL!\n");
        goto out_return;
    }

    if (!(master = ec_find_master(master_index))) return NULL;

    // critical section start
    if (master->device) {
        EC_ERR("Master %i already has a device!\n", master_index);
        // critical section leave
        goto out_return;
    }

    if (!(master->device =
          (ec_device_t *) kmalloc(sizeof(ec_device_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate device!\n");
        // critical section leave
        goto out_return;
    }
    // critical section end

    if (ec_device_init(master->device, master, net_dev, isr, module)) {
        EC_ERR("Failed to init device!\n");
        goto out_free;
    }

    return master->device;

 out_free:
    kfree(master->device);
    master->device = NULL;
 out_return:
    return NULL;
}

/*****************************************************************************/

/**
   Disconnect an EtherCAT device from the master.
   The device is disconnected from the master and all device ressources
   are freed.
   \attention Before calling this function, the ecdev_stop() function has
   to be called, to be sure that the master does not use the device any more.
   \ingroup DeviceInterface
*/

void ecdev_unregister(unsigned int master_index, /**< master index */
                      ec_device_t *device /**< EtherCAT device */
                      )
{
    ec_master_t *master;

    if (!(master = ec_find_master(master_index))) return;

    if (!master->device || master->device != device) {
        EC_WARN("Unable to unregister device!\n");
        return;
    }

    ec_device_clear(master->device);
    kfree(master->device);
    master->device = NULL;
}

/*****************************************************************************/

/**
   Starts the master associated with the device.
   This function has to be called by the network device driver to tell the
   master that the device is ready to send and receive data. The master
   will enter the free-run mode then.
   \ingroup DeviceInterface
*/

int ecdev_start(unsigned int master_index /**< master index */)
{
    ec_master_t *master;
    if (!(master = ec_find_master(master_index))) return -1;

    if (ec_device_open(master->device)) {
        EC_ERR("Failed to open device!\n");
        return -1;
    }

    ec_master_freerun_start(master);
    return 0;
}

/*****************************************************************************/

/**
   Stops the master associated with the device.
   Tells the master to stop using the device for frame IO. Has to be called
   before unregistering the device.
   \ingroup DeviceInterface
*/

void ecdev_stop(unsigned int master_index /**< master index */)
{
    ec_master_t *master;
    if (!(master = ec_find_master(master_index))) return;

    ec_master_freerun_stop(master);

    if (ec_device_close(master->device))
        EC_WARN("Failed to close device!\n");
}

/******************************************************************************
 *  Realtime interface
 *****************************************************************************/

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

    // begin critical section
    if (master->reserved) {
        EC_ERR("Master %i is already in use!\n", master_index);
        goto out_return;
    }
    master->reserved = 1;
    // end critical section

    if (!master->device) {
        EC_ERR("Master %i has no assigned device!\n", master_index);
        goto out_release;
    }

    if (!try_module_get(master->device->module)) {
        EC_ERR("Failed to reserve device module!\n");
        goto out_release;
    }

    ec_master_freerun_stop(master);
    ec_master_reset(master);
    master->mode = EC_MASTER_MODE_RUNNING;

    if (!master->device->link_state) EC_WARN("Link is DOWN.\n");

    if (ec_master_bus_scan(master)) {
        EC_ERR("Bus scan failed!\n");
        goto out_module_put;
    }

    EC_INFO("Master %i is ready.\n", master_index);
    return master;

 out_module_put:
    module_put(master->device->module);
    ec_master_reset(master);
 out_release:
    master->reserved = 0;
 out_return:
    EC_ERR("Failed requesting master %i.\n", master_index);
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

    if (!master->reserved) {
        EC_ERR("Master %i was never requested!\n", master->index);
        return;
    }

    ec_master_reset(master);

    master->mode = EC_MASTER_MODE_IDLE;
    ec_master_freerun_start(master);

    module_put(master->device->module);
    master->reserved = 0;

    EC_INFO("Released master %i.\n", master->index);
    return;
}

/*****************************************************************************/

/** \cond */

module_init(ec_init_module);
module_exit(ec_cleanup_module);

EXPORT_SYMBOL(ecdev_register);
EXPORT_SYMBOL(ecdev_unregister);
EXPORT_SYMBOL(ecdev_start);
EXPORT_SYMBOL(ecdev_stop);
EXPORT_SYMBOL(ecrt_request_master);
EXPORT_SYMBOL(ecrt_release_master);

/** \endcond */

/*****************************************************************************/
