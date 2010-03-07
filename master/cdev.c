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

/**
   \file
   EtherCAT master character device.
*/

/*****************************************************************************/

#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

#include "cdev.h"
#include "master.h"
#include "slave_config.h"
#include "voe_handler.h"
#include "ethernet.h"
#include "ioctl.h"

/** Set to 1 to enable ioctl() command debugging.
 */
#define DEBUG_IOCTL 0

/*****************************************************************************/

static int eccdev_open(struct inode *, struct file *);
static int eccdev_release(struct inode *, struct file *);
static long eccdev_ioctl(struct file *, unsigned int, unsigned long);
static int eccdev_mmap(struct file *, struct vm_area_struct *);

/** This is the kernel version from which the .fault member of the
 * vm_operations_struct is usable.
 */
#define PAGE_FAULT_VERSION KERNEL_VERSION(2, 6, 23)

#if LINUX_VERSION_CODE >= PAGE_FAULT_VERSION
static int eccdev_vma_fault(struct vm_area_struct *, struct vm_fault *);
#else
static struct page *eccdev_vma_nopage(
        struct vm_area_struct *, unsigned long, int *);
#endif

/*****************************************************************************/

/** File operation callbacks for the EtherCAT character device.
 */
static struct file_operations eccdev_fops = {
    .owner          = THIS_MODULE,
    .open           = eccdev_open,
    .release        = eccdev_release,
    .unlocked_ioctl = eccdev_ioctl,
    .mmap           = eccdev_mmap
};

/** Callbacks for a virtual memory area retrieved with ecdevc_mmap().
 */
struct vm_operations_struct eccdev_vm_ops = {
#if LINUX_VERSION_CODE >= PAGE_FAULT_VERSION
    .fault = eccdev_vma_fault
#else
    .nopage = eccdev_vma_nopage
#endif
};

/*****************************************************************************/

/** Private data structure for file handles.
 */
typedef struct {
    ec_cdev_t *cdev; /**< Character device. */
    unsigned int requested; /**< Master wac requested via this file handle. */
    uint8_t *process_data; /**< Total process data area. */
    size_t process_data_size; /**< Size of the \a process_data. */
} ec_cdev_priv_t;

/*****************************************************************************/

/** Constructor.
 * 
 * \return 0 in case of success, else < 0
 */
int ec_cdev_init(
        ec_cdev_t *cdev, /**< EtherCAT master character device. */
        ec_master_t *master, /**< Parent master. */
        dev_t dev_num /**< Device number. */
        )
{
    int ret;

    cdev->master = master;

    cdev_init(&cdev->cdev, &eccdev_fops);
    cdev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&cdev->cdev,
            MKDEV(MAJOR(dev_num), master->index), 1);
    if (ret) {
        EC_ERR("Failed to add character device!\n");
    }

    return ret;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_cdev_clear(ec_cdev_t *cdev /**< EtherCAT XML device */)
{
    cdev_del(&cdev->cdev);
}

/*****************************************************************************/

/** Copies a string to an ioctl structure.
 */
void ec_cdev_strcpy(
        char *target, /**< Target. */
        const char *source /**< Source. */
        )
{
    if (source) {
        strncpy(target, source, EC_IOCTL_STRING_SIZE);
        target[EC_IOCTL_STRING_SIZE - 1] = 0;
    } else {
        target[0] = 0;
    }
}

/*****************************************************************************/

/** Get module information.
 */
int ec_cdev_ioctl_module(
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_module_t data;

    data.ioctl_version_magic = EC_IOCTL_VERSION_MAGIC;
    data.master_count = ec_master_count();

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get master information.
 */
int ec_cdev_ioctl_master(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_master_t data;
    unsigned int i;

    if (down_interruptible(&master->master_sem))
        return -EINTR;
    data.slave_count = master->slave_count;
    data.config_count = ec_master_config_count(master);
    data.domain_count = ec_master_domain_count(master);
#ifdef EC_EOE
    data.eoe_handler_count = ec_master_eoe_handler_count(master);
#endif
    data.phase = (uint8_t) master->phase;
    data.active = (uint8_t) master->active;
    data.scan_busy = master->scan_busy;
    up(&master->master_sem);

    if (down_interruptible(&master->device_sem))
        return -EINTR;

    if (master->main_device.dev) {
        memcpy(data.devices[0].address,
                master->main_device.dev->dev_addr, ETH_ALEN);
    } else {
        memcpy(data.devices[0].address, master->main_mac, ETH_ALEN); 
    }
    data.devices[0].attached = master->main_device.dev ? 1 : 0;
    data.devices[0].link_state = master->main_device.link_state ? 1 : 0;
    data.devices[0].tx_count = master->main_device.tx_count;
    data.devices[0].rx_count = master->main_device.rx_count;
    for (i = 0; i < EC_RATE_COUNT; i++) {
        data.devices[0].tx_rates[i] = master->main_device.tx_rates[i];
        data.devices[0].loss_rates[i] = master->main_device.loss_rates[i];
    }

    if (master->backup_device.dev) {
        memcpy(data.devices[1].address,
                master->backup_device.dev->dev_addr, ETH_ALEN); 
    } else {
        memcpy(data.devices[1].address, master->backup_mac, ETH_ALEN); 
    }
    data.devices[1].attached = master->backup_device.dev ? 1 : 0;
    data.devices[1].link_state = master->backup_device.link_state ? 1 : 0;
    data.devices[1].tx_count = master->backup_device.tx_count;
    data.devices[1].rx_count = master->backup_device.rx_count;
    for (i = 0; i < EC_RATE_COUNT; i++) {
        data.devices[1].tx_rates[i] = master->backup_device.tx_rates[i];
        data.devices[1].loss_rates[i] = master->backup_device.loss_rates[i];
    }

    up(&master->device_sem);

    data.app_time = master->app_time;
    data.ref_clock =
        master->dc_ref_clock ? master->dc_ref_clock->ring_position : 0xffff;

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave information.
 */
int ec_cdev_ioctl_slave(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_t data;
    const ec_slave_t *slave;
    int i;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.position);
        return -EINVAL;
    }

    data.vendor_id = slave->sii.vendor_id;
    data.product_code = slave->sii.product_code;
    data.revision_number = slave->sii.revision_number;
    data.serial_number = slave->sii.serial_number;
    data.alias = slave->sii.alias;
    data.boot_rx_mailbox_offset = slave->sii.boot_rx_mailbox_offset;
    data.boot_rx_mailbox_size = slave->sii.boot_rx_mailbox_size;
    data.boot_tx_mailbox_offset = slave->sii.boot_tx_mailbox_offset;
    data.boot_tx_mailbox_size = slave->sii.boot_tx_mailbox_size;
    data.std_rx_mailbox_offset = slave->sii.std_rx_mailbox_offset;
    data.std_rx_mailbox_size = slave->sii.std_rx_mailbox_size;
    data.std_tx_mailbox_offset = slave->sii.std_tx_mailbox_offset;
    data.std_tx_mailbox_size = slave->sii.std_tx_mailbox_size;
    data.mailbox_protocols = slave->sii.mailbox_protocols;
    data.has_general_category = slave->sii.has_general;
    data.coe_details = slave->sii.coe_details;
    data.general_flags = slave->sii.general_flags;
    data.current_on_ebus = slave->sii.current_on_ebus;
    for (i = 0; i < EC_MAX_PORTS; i++) {
        data.ports[i].desc = slave->ports[i].desc;
        data.ports[i].link.link_up = slave->ports[i].link.link_up;
        data.ports[i].link.loop_closed = slave->ports[i].link.loop_closed;
        data.ports[i].link.signal_detected =
            slave->ports[i].link.signal_detected;
        data.ports[i].receive_time = slave->ports[i].receive_time;
        if (slave->ports[i].next_slave) {
            data.ports[i].next_slave =
                slave->ports[i].next_slave->ring_position;
        } else {
            data.ports[i].next_slave = 0xffff;
        }
        data.ports[i].delay_to_next_dc = slave->ports[i].delay_to_next_dc;
    }
    data.fmmu_bit = slave->base_fmmu_bit_operation;
    data.dc_supported = slave->base_dc_supported;
    data.dc_range = slave->base_dc_range;
    data.has_dc_system_time = slave->has_dc_system_time;
    data.transmission_delay = slave->transmission_delay;
    data.al_state = slave->current_state;
    data.error_flag = slave->error_flag;

    data.sync_count = slave->sii.sync_count;
    data.sdo_count = ec_slave_sdo_count(slave);
    data.sii_nwords = slave->sii_nwords;
    ec_cdev_strcpy(data.group, slave->sii.group);
    ec_cdev_strcpy(data.image, slave->sii.image);
    ec_cdev_strcpy(data.order, slave->sii.order);
    ec_cdev_strcpy(data.name, slave->sii.name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave sync manager information.
 */
int ec_cdev_ioctl_slave_sync(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_sync_t data;
    const ec_slave_t *slave;
    const ec_sync_t *sync;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (data.sync_index >= slave->sii.sync_count) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not exist in slave %u!\n",
                data.sync_index, data.slave_position);
        return -EINVAL;
    }

    sync = &slave->sii.syncs[data.sync_index];

    data.physical_start_address = sync->physical_start_address;
    data.default_size = sync->default_length;
    data.control_register = sync->control_register;
    data.enable = sync->enable;
    data.pdo_count = ec_pdo_list_count(&sync->pdos);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave sync manager PDO information.
 */
int ec_cdev_ioctl_slave_sync_pdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_sync_pdo_t data;
    const ec_slave_t *slave;
    const ec_sync_t *sync;
    const ec_pdo_t *pdo;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (data.sync_index >= slave->sii.sync_count) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not exist in slave %u!\n",
                data.sync_index, data.slave_position);
        return -EINVAL;
    }

    sync = &slave->sii.syncs[data.sync_index];
    if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                    &sync->pdos, data.pdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not contain a PDO with "
                "position %u in slave %u!\n", data.sync_index,
                data.pdo_pos, data.slave_position);
        return -EINVAL;
    }

    data.index = pdo->index;
    data.entry_count = ec_pdo_entry_count(pdo);
    ec_cdev_strcpy(data.name, pdo->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave sync manager PDO entry information.
 */
int ec_cdev_ioctl_slave_sync_pdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_slave_sync_pdo_entry_t data;
    const ec_slave_t *slave;
    const ec_sync_t *sync;
    const ec_pdo_t *pdo;
    const ec_pdo_entry_t *entry;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (data.sync_index >= slave->sii.sync_count) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not exist in slave %u!\n",
                data.sync_index, data.slave_position);
        return -EINVAL;
    }

    sync = &slave->sii.syncs[data.sync_index];
    if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                    &sync->pdos, data.pdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Sync manager %u does not contain a PDO with "
                "position %u in slave %u!\n", data.sync_index,
                data.pdo_pos, data.slave_position);
        return -EINVAL;
    }

    if (!(entry = ec_pdo_find_entry_by_pos_const(
                    pdo, data.entry_pos))) {
        up(&master->master_sem);
        EC_ERR("PDO 0x%04X does not contain an entry with "
                "position %u in slave %u!\n", data.pdo_pos,
                data.entry_pos, data.slave_position);
        return -EINVAL;
    }

    data.index = entry->index;
    data.subindex = entry->subindex;
    data.bit_length = entry->bit_length;
    ec_cdev_strcpy(data.name, entry->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get domain information.
 */
int ec_cdev_ioctl_domain(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_domain_t data;
    const ec_domain_t *domain;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain_const(master, data.index))) {
        up(&master->master_sem);
        EC_ERR("Domain %u does not exist!\n", data.index);
        return -EINVAL;
    }

    data.data_size = domain->data_size;
    data.logical_base_address = domain->logical_base_address;
    data.working_counter = domain->working_counter;
    data.expected_working_counter = domain->expected_working_counter;
    data.fmmu_count = ec_domain_fmmu_count(domain);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get domain FMMU information.
 */
int ec_cdev_ioctl_domain_fmmu(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_domain_fmmu_t data;
    const ec_domain_t *domain;
    const ec_fmmu_config_t *fmmu;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain_const(master, data.domain_index))) {
        up(&master->master_sem);
        EC_ERR("Domain %u does not exist!\n", data.domain_index);
        return -EINVAL;
    }

    if (!(fmmu = ec_domain_find_fmmu(domain, data.fmmu_index))) {
        up(&master->master_sem);
        EC_ERR("Domain %u has less than %u fmmu configurations.\n",
                data.domain_index, data.fmmu_index + 1);
        return -EINVAL;
    }

    data.slave_config_alias = fmmu->sc->alias;
    data.slave_config_position = fmmu->sc->position;
    data.sync_index = fmmu->sync_index;
    data.dir = fmmu->dir;
    data.logical_address = fmmu->logical_start_address;
    data.data_size = fmmu->data_size;

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get domain data.
 */
int ec_cdev_ioctl_domain_data(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< Userspace address to store the results. */
        )
{
    ec_ioctl_domain_data_t data;
    const ec_domain_t *domain;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain_const(master, data.domain_index))) {
        up(&master->master_sem);
        EC_ERR("Domain %u does not exist!\n", data.domain_index);
        return -EINVAL;
    }

    if (domain->data_size != data.data_size) {
        up(&master->master_sem);
        EC_ERR("Data size mismatch %u/%zu!\n",
                data.data_size, domain->data_size);
        return -EFAULT;
    }

    if (copy_to_user((void __user *) data.target, domain->data,
                domain->data_size)) {
        up(&master->master_sem);
        return -EFAULT;
    }

    up(&master->master_sem);
    return 0;
}

/*****************************************************************************/

/** Set master debug level.
 */
int ec_cdev_ioctl_master_debug(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    return ec_master_debug_level(master, (unsigned int) arg);
}

/*****************************************************************************/

/** Set slave state.
 */
int ec_cdev_ioctl_slave_state(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_state_t data;
    ec_slave_t *slave;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    ec_slave_request_state(slave, data.al_state);

    up(&master->master_sem);
    return 0;
}

/*****************************************************************************/

/** Get slave SDO information.
 */
int ec_cdev_ioctl_slave_sdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_t data;
    const ec_slave_t *slave;
    const ec_sdo_t *sdo;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (!(sdo = ec_slave_get_sdo_by_pos_const(
                    slave, data.sdo_position))) {
        up(&master->master_sem);
        EC_ERR("SDO %u does not exist in slave %u!\n",
                data.sdo_position, data.slave_position);
        return -EINVAL;
    }

    data.sdo_index = sdo->index;
    data.max_subindex = sdo->max_subindex;
    ec_cdev_strcpy(data.name, sdo->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave SDO entry information.
 */
int ec_cdev_ioctl_slave_sdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_entry_t data;
    const ec_slave_t *slave;
    const ec_sdo_t *sdo;
    const ec_sdo_entry_t *entry;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (data.sdo_spec <= 0) {
        if (!(sdo = ec_slave_get_sdo_by_pos_const(
                        slave, -data.sdo_spec))) {
            up(&master->master_sem);
            EC_ERR("SDO %u does not exist in slave %u!\n",
                    -data.sdo_spec, data.slave_position);
            return -EINVAL;
        }
    } else {
        if (!(sdo = ec_slave_get_sdo_const(
                        slave, data.sdo_spec))) {
            up(&master->master_sem);
            EC_ERR("SDO 0x%04X does not exist in slave %u!\n",
                    data.sdo_spec, data.slave_position);
            return -EINVAL;
        }
    }

    if (!(entry = ec_sdo_get_entry_const(
                    sdo, data.sdo_entry_subindex))) {
        up(&master->master_sem);
        EC_ERR("SDO entry 0x%04X:%02X does not exist "
                "in slave %u!\n", sdo->index,
                data.sdo_entry_subindex, data.slave_position);
        return -EINVAL;
    }

    data.data_type = entry->data_type;
    data.bit_length = entry->bit_length;
    data.read_access[EC_SDO_ENTRY_ACCESS_PREOP] =
        entry->read_access[EC_SDO_ENTRY_ACCESS_PREOP];
    data.read_access[EC_SDO_ENTRY_ACCESS_SAFEOP] =
        entry->read_access[EC_SDO_ENTRY_ACCESS_SAFEOP];
    data.read_access[EC_SDO_ENTRY_ACCESS_OP] =
        entry->read_access[EC_SDO_ENTRY_ACCESS_OP];
    data.write_access[EC_SDO_ENTRY_ACCESS_PREOP] =
        entry->write_access[EC_SDO_ENTRY_ACCESS_PREOP];
    data.write_access[EC_SDO_ENTRY_ACCESS_SAFEOP] =
        entry->write_access[EC_SDO_ENTRY_ACCESS_SAFEOP];
    data.write_access[EC_SDO_ENTRY_ACCESS_OP] =
        entry->write_access[EC_SDO_ENTRY_ACCESS_OP];
    ec_cdev_strcpy(data.description, entry->description);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Upload SDO.
 */
int ec_cdev_ioctl_slave_sdo_upload(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_upload_t data;
    ec_master_sdo_request_t request;
    int retval;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    ec_sdo_request_init(&request.req);
    ec_sdo_request_address(&request.req,
            data.sdo_index, data.sdo_entry_subindex);
    ecrt_sdo_request_read(&request.req);

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(request.slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        ec_sdo_request_clear(&request.req);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (master->debug_level)
        EC_DBG("Schedule SDO upload request for slave %u\n",
                request.slave->ring_position);
    // schedule request.
    list_add_tail(&request.list, &request.slave->slave_sdo_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(request.slave->sdo_queue,
                request.req.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.req.state == EC_INT_REQUEST_QUEUED) {
            list_del(&request.list);
            up(&master->master_sem);
            ec_sdo_request_clear(&request.req);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(request.slave->sdo_queue,
            request.req.state != EC_INT_REQUEST_BUSY);

    if (master->debug_level)
        EC_DBG("Scheduled SDO upload request for slave %u done\n",
                request.slave->ring_position);

    data.abort_code = request.req.abort_code;

    if (request.req.state != EC_INT_REQUEST_SUCCESS) {
        data.data_size = 0;
        retval = -EIO;
    } else {
        if (request.req.data_size > data.target_size) {
            EC_ERR("Buffer too small.\n");
            ec_sdo_request_clear(&request.req);
            return -EOVERFLOW;
        }
        data.data_size = request.req.data_size;

        if (copy_to_user((void __user *) data.target,
                    request.req.data, data.data_size)) {
            ec_sdo_request_clear(&request.req);
            return -EFAULT;
        }
        retval = 0;
    }

    if (__copy_to_user((void __user *) arg, &data, sizeof(data))) {
        retval = -EFAULT;
    }

    ec_sdo_request_clear(&request.req);
    return retval;
}

/*****************************************************************************/

/** Download SDO.
 */
int ec_cdev_ioctl_slave_sdo_download(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sdo_download_t data;
    ec_master_sdo_request_t request;
    int retval;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    // copy data to download
    if (!data.data_size) {
        EC_ERR("Zero data size!\n");
        return -EINVAL;
    }

    ec_sdo_request_init(&request.req);
    ec_sdo_request_address(&request.req,
            data.sdo_index, data.sdo_entry_subindex);
    if (ec_sdo_request_alloc(&request.req, data.data_size)) {
        ec_sdo_request_clear(&request.req);
        return -ENOMEM;
    }
    if (copy_from_user(request.req.data,
                (void __user *) data.data, data.data_size)) {
        ec_sdo_request_clear(&request.req);
        return -EFAULT;
    }
    request.req.data_size = data.data_size;
    ecrt_sdo_request_write(&request.req);

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(request.slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        ec_sdo_request_clear(&request.req);
        return -EINVAL;
    }
    
    if (master->debug_level)
        EC_DBG("Schedule SDO download request for slave %u\n",
                request.slave->ring_position);
    // schedule request.
    list_add_tail(&request.list, &request.slave->slave_sdo_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(request.slave->sdo_queue,
                request.req.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.req.state == EC_INT_REQUEST_QUEUED) {
            list_del(&request.list);
            up(&master->master_sem);
            ec_sdo_request_clear(&request.req);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(request.slave->sdo_queue,
            request.req.state != EC_INT_REQUEST_BUSY);

    if (master->debug_level)
        EC_DBG("Scheduled SDO download request for slave %u done\n",
                request.slave->ring_position);

    data.abort_code = request.req.abort_code;

    retval = request.req.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;

    if (__copy_to_user((void __user *) arg, &data, sizeof(data))) {
        retval = -EFAULT;
    }

    ec_sdo_request_clear(&request.req);
    return retval;
}

/*****************************************************************************/

/** Read a slave's SII.
 */
int ec_cdev_ioctl_slave_sii_read(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sii_t data;
    const ec_slave_t *slave;
    int retval;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave_const(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    if (!data.nwords
            || data.offset + data.nwords > slave->sii_nwords) {
        up(&master->master_sem);
        EC_ERR("Invalid SII read offset/size %u/%u for slave "
                "SII size %zu!\n", data.offset,
                data.nwords, slave->sii_nwords);
        return -EINVAL;
    }

    if (copy_to_user((void __user *) data.words,
                slave->sii_words + data.offset, data.nwords * 2))
        retval = -EFAULT;
    else
        retval = 0;

    up(&master->master_sem);
    return retval;
}

/*****************************************************************************/

/** Write a slave's SII.
 */
int ec_cdev_ioctl_slave_sii_write(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_sii_t data;
    ec_slave_t *slave;
    unsigned int byte_size;
    uint16_t *words;
    ec_sii_write_request_t request;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (!data.nwords)
        return 0;

    byte_size = sizeof(uint16_t) * data.nwords;
    if (!(words = kmalloc(byte_size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes for SII contents.\n",
                byte_size);
        return -ENOMEM;
    }

    if (copy_from_user(words,
                (void __user *) data.words, byte_size)) {
        kfree(words);
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        kfree(words);
        return -EINVAL;
    }

    // init SII write request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.words = words;
    request.offset = data.offset;
    request.nwords = data.nwords;
    request.state = EC_INT_REQUEST_QUEUED;

    // schedule SII write request.
    list_add_tail(&request.list, &master->sii_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->sii_queue,
                request.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.state == EC_INT_REQUEST_QUEUED) {
            // abort request
            list_del(&request.list);
            up(&master->master_sem);
            kfree(words);
            return -EINTR;
        }
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->sii_queue, request.state != EC_INT_REQUEST_BUSY);

    kfree(words);

    return request.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;
}

/*****************************************************************************/

/** Read a slave's registers.
 */
int ec_cdev_ioctl_slave_reg_read(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_reg_t data;
    ec_slave_t *slave;
    uint8_t *contents;
    ec_reg_request_t request;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (!data.length)
        return 0;

    if (!(contents = kmalloc(data.length, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes for register data.\n",
                data.length);
        return -ENOMEM;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    // init register request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.dir = EC_DIR_INPUT;
    request.data = contents;
    request.offset = data.offset;
    request.length = data.length;
    request.state = EC_INT_REQUEST_QUEUED;

    // schedule request.
    list_add_tail(&request.list, &master->reg_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->reg_queue,
                request.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.state == EC_INT_REQUEST_QUEUED) {
            // abort request
            list_del(&request.list);
            up(&master->master_sem);
            kfree(contents);
            return -EINTR;
        }
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->reg_queue, request.state != EC_INT_REQUEST_BUSY);

    if (request.state == EC_INT_REQUEST_SUCCESS) {
        if (copy_to_user((void __user *) data.data, contents, data.length))
            return -EFAULT;
    }
    kfree(contents);

    return request.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;
}

/*****************************************************************************/

/** Write a slave's registers.
 */
int ec_cdev_ioctl_slave_reg_write(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_reg_t data;
    ec_slave_t *slave;
    uint8_t *contents;
    ec_reg_request_t request;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (!data.length)
        return 0;

    if (!(contents = kmalloc(data.length, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes for register data.\n",
                data.length);
        return -ENOMEM;
    }

    if (copy_from_user(contents, (void __user *) data.data, data.length)) {
        kfree(contents);
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        kfree(contents);
        return -EINVAL;
    }

    // init register request
    INIT_LIST_HEAD(&request.list);
    request.slave = slave;
    request.dir = EC_DIR_OUTPUT;
    request.data = contents;
    request.offset = data.offset;
    request.length = data.length;
    request.state = EC_INT_REQUEST_QUEUED;

    // schedule request.
    list_add_tail(&request.list, &master->reg_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(master->reg_queue,
                request.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.state == EC_INT_REQUEST_QUEUED) {
            // abort request
            list_del(&request.list);
            up(&master->master_sem);
            kfree(contents);
            return -EINTR;
        }
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->reg_queue, request.state != EC_INT_REQUEST_BUSY);

    kfree(contents);

    return request.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;
}

/*****************************************************************************/

/** Get slave configuration information.
 */
int ec_cdev_ioctl_config(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_config_t data;
    const ec_slave_config_t *sc;
    uint8_t i;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(
                    master, data.config_index))) {
        up(&master->master_sem);
        EC_ERR("Slave config %u does not exist!\n",
                data.config_index);
        return -EINVAL;
    }

    data.alias = sc->alias;
    data.position = sc->position;
    data.vendor_id = sc->vendor_id;
    data.product_code = sc->product_code;
    for (i = 0; i < EC_MAX_SYNC_MANAGERS; i++) {
        data.syncs[i].dir = sc->sync_configs[i].dir;
        data.syncs[i].watchdog_mode = sc->sync_configs[i].watchdog_mode;
        data.syncs[i].pdo_count =
            ec_pdo_list_count(&sc->sync_configs[i].pdos);
    }
    data.watchdog_divider = sc->watchdog_divider;
    data.watchdog_intervals = sc->watchdog_intervals;
    data.sdo_count = ec_slave_config_sdo_count(sc);
    data.slave_position = sc->slave ? sc->slave->ring_position : -1;
    data.dc_assign_activate = sc->dc_assign_activate;
    for (i = 0; i < EC_SYNC_SIGNAL_COUNT; i++) {
        data.dc_sync[i] = sc->dc_sync[i];
    }

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave configuration PDO information.
 */
int ec_cdev_ioctl_config_pdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_config_pdo_t data;
    const ec_slave_config_t *sc;
    const ec_pdo_t *pdo;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (data.sync_index >= EC_MAX_SYNC_MANAGERS) {
        EC_ERR("Invalid sync manager index %u!\n",
                data.sync_index);
        return -EINVAL;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(
                    master, data.config_index))) {
        up(&master->master_sem);
        EC_ERR("Slave config %u does not exist!\n",
                data.config_index);
        return -EINVAL;
    }

    if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                    &sc->sync_configs[data.sync_index].pdos,
                    data.pdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Invalid PDO position!\n");
        return -EINVAL;
    }

    data.index = pdo->index;
    data.entry_count = ec_pdo_entry_count(pdo);
    ec_cdev_strcpy(data.name, pdo->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave configuration PDO entry information.
 */
int ec_cdev_ioctl_config_pdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_config_pdo_entry_t data;
    const ec_slave_config_t *sc;
    const ec_pdo_t *pdo;
    const ec_pdo_entry_t *entry;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (data.sync_index >= EC_MAX_SYNC_MANAGERS) {
        EC_ERR("Invalid sync manager index %u!\n",
                data.sync_index);
        return -EINVAL;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(
                    master, data.config_index))) {
        up(&master->master_sem);
        EC_ERR("Slave config %u does not exist!\n",
                data.config_index);
        return -EINVAL;
    }

    if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                    &sc->sync_configs[data.sync_index].pdos,
                    data.pdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Invalid PDO position!\n");
        return -EINVAL;
    }

    if (!(entry = ec_pdo_find_entry_by_pos_const(
                    pdo, data.entry_pos))) {
        up(&master->master_sem);
        EC_ERR("Entry not found!\n");
        return -EINVAL;
    }

    data.index = entry->index;
    data.subindex = entry->subindex;
    data.bit_length = entry->bit_length;
    ec_cdev_strcpy(data.name, entry->name);

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get slave configuration SDO information.
 */
int ec_cdev_ioctl_config_sdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_config_sdo_t data;
    const ec_slave_config_t *sc;
    const ec_sdo_request_t *req;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(
                    master, data.config_index))) {
        up(&master->master_sem);
        EC_ERR("Slave config %u does not exist!\n",
                data.config_index);
        return -EINVAL;
    }

    if (!(req = ec_slave_config_get_sdo_by_pos_const(
                    sc, data.sdo_pos))) {
        up(&master->master_sem);
        EC_ERR("Invalid SDO position!\n");
        return -EINVAL;
    }

    data.index = req->index;
    data.subindex = req->subindex;
    data.size = req->data_size;
    memcpy(&data.data, req->data,
            min((u32) data.size, (u32) EC_MAX_SDO_DATA_SIZE));

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

#ifdef EC_EOE

/** Get EoE handler information.
 */
int ec_cdev_ioctl_eoe_handler(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_eoe_handler_t data;
    const ec_eoe_t *eoe;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(eoe = ec_master_get_eoe_handler_const(master, data.eoe_index))) {
        up(&master->master_sem);
        EC_ERR("EoE handler %u does not exist!\n", data.eoe_index);
        return -EINVAL;
    }

    if (eoe->slave) {
        data.slave_position = eoe->slave->ring_position;
    } else {
        data.slave_position = 0xffff;
    }
    snprintf(data.name, EC_DATAGRAM_NAME_SIZE, eoe->dev->name);
    data.open = eoe->opened;
    data.rx_bytes = eoe->stats.tx_bytes;
    data.rx_rate = eoe->tx_rate;
    data.tx_bytes = eoe->stats.rx_bytes;
    data.tx_rate = eoe->tx_rate;
    data.tx_queued_frames = eoe->tx_queued_frames;
    data.tx_queue_size = eoe->tx_queue_size;

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

#endif

/*****************************************************************************/

/** Request the master from userspace.
 */
int ec_cdev_ioctl_request(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_master_t *m;
    int ret = 0;

    m = ecrt_request_master_err(master->index);
    if (IS_ERR(m)) {
        ret = PTR_ERR(m);
    } else {
        priv->requested = 1;
    }

    return ret;
}

/*****************************************************************************/

/** Create a domain.
 */
int ec_cdev_ioctl_create_domain(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_domain_t *domain;

    if (unlikely(!priv->requested))
        return -EPERM;

    domain = ecrt_master_create_domain_err(master);
    if (IS_ERR(domain))
        return PTR_ERR(domain);

    return domain->index;
}

/*****************************************************************************/

/** Create a slave configuration.
 */
int ec_cdev_ioctl_create_slave_config(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc, *entry;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    sc = ecrt_master_slave_config_err(master, data.alias, data.position,
            data.vendor_id, data.product_code);
    if (IS_ERR(sc))
        return PTR_ERR(sc);

    data.config_index = 0;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    list_for_each_entry(entry, &master->configs, list) {
        if (entry == sc)
            break;
        data.config_index++;
    }

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Activates the master.
 */
int ec_cdev_ioctl_activate(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_domain_t *domain;
    off_t offset;
    int ret;
    
    if (unlikely(!priv->requested))
        return -EPERM;

    /* Get the sum of the domains' process data sizes. */
    
    priv->process_data_size = 0;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    list_for_each_entry(domain, &master->domains, list) {
        priv->process_data_size += ecrt_domain_size(domain);
    }
    
    up(&master->master_sem);

    if (priv->process_data_size) {
        priv->process_data = vmalloc(priv->process_data_size);
        if (!priv->process_data) {
            priv->process_data_size = 0;
            return -ENOMEM;
        }

        /* Set the memory as external process data memory for the domains. */

        offset = 0;
        list_for_each_entry(domain, &master->domains, list) {
            ecrt_domain_external_memory(domain, priv->process_data + offset);
            offset += ecrt_domain_size(domain);
        }
    }

    ecrt_master_callbacks(master, ec_master_internal_send_cb,
            ec_master_internal_receive_cb, master);

    ret = ecrt_master_activate(master);
    if (ret < 0)
        return ret;

    if (copy_to_user((void __user *) arg,
                &priv->process_data_size, sizeof(size_t)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Deactivates the master.
 */
int ec_cdev_ioctl_deactivate(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    if (unlikely(!priv->requested))
        return -EPERM;

    ecrt_master_deactivate(master);
    return 0;
}


/*****************************************************************************/

/** Set max. number of databytes in a cycle
 */
int ec_cdev_ioctl_set_send_interval(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    size_t send_interval;

    if (copy_from_user(&send_interval, (void __user *) arg,
                sizeof(send_interval))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;
    ec_master_set_send_interval(master,send_interval);
    up(&master->master_sem);

    return 0;
}


/*****************************************************************************/

/** Send frames.
 */
int ec_cdev_ioctl_send(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    if (unlikely(!priv->requested))
        return -EPERM;

    down(&master->io_sem);
    ecrt_master_send(master);
    up(&master->io_sem);
    return 0;
}

/*****************************************************************************/

/** Receive frames.
 */
int ec_cdev_ioctl_receive(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    if (unlikely(!priv->requested))
        return -EPERM;

    down(&master->io_sem);
    ecrt_master_receive(master);
    up(&master->io_sem);
    return 0;
}

/*****************************************************************************/

/** Get the master state.
 */
int ec_cdev_ioctl_master_state(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_master_state_t data;
    
    if (unlikely(!priv->requested))
        return -EPERM;

    ecrt_master_state(master, &data);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get the master state.
 */
int ec_cdev_ioctl_app_time(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_app_time_t data;
    
    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    ecrt_master_application_time(master, data.app_time);
    return 0;
}

/*****************************************************************************/

/** Sync the reference clock.
 */
int ec_cdev_ioctl_sync_ref(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    if (unlikely(!priv->requested))
        return -EPERM;

    down(&master->io_sem);
    ecrt_master_sync_reference_clock(master);
    up(&master->io_sem);
    return 0;
}

/*****************************************************************************/

/** Sync the slave clocks.
 */
int ec_cdev_ioctl_sync_slaves(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    if (unlikely(!priv->requested))
        return -EPERM;

    down(&master->io_sem);
    ecrt_master_sync_slave_clocks(master);
    up(&master->io_sem);
    return 0;
}

/*****************************************************************************/

/** Queue the sync monitoring datagram.
 */
int ec_cdev_ioctl_sync_mon_queue(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    if (unlikely(!priv->requested))
        return -EPERM;

    down(&master->io_sem);
    ecrt_master_sync_monitor_queue(master);
    up(&master->io_sem);
    return 0;
}

/*****************************************************************************/

/** Processes the sync monitoring datagram.
 */
int ec_cdev_ioctl_sync_mon_process(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    uint32_t time_diff;

    if (unlikely(!priv->requested))
        return -EPERM;

    down(&master->io_sem);
    time_diff = ecrt_master_sync_monitor_process(master);
    up(&master->io_sem);

    if (copy_to_user((void __user *) arg, &time_diff, sizeof(time_diff)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Configure a sync manager.
 */
int ec_cdev_ioctl_sc_sync(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc;
    unsigned int i;
    int ret = 0;

    if (unlikely(!priv->requested)) {
        ret = -EPERM;
        goto out_return;
    }

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        ret = -EFAULT;
        goto out_return;
    }

    if (down_interruptible(&master->master_sem)) {
        ret = -EINTR;
        goto out_return;
    }

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        ret = -ENOENT;
        goto out_up;
    }

    for (i = 0; i < EC_MAX_SYNC_MANAGERS; i++) {
        if (data.syncs[i].config_this) {
            if (ecrt_slave_config_sync_manager(sc, i, data.syncs[i].dir,
                        data.syncs[i].watchdog_mode)) {
                ret = -EINVAL;
                goto out_up;
            }
        }
    }

out_up:
    up(&master->master_sem);
out_return:
    return ret;
}

/*****************************************************************************/

/** Configure a slave's watchdogs.
 */
int ec_cdev_ioctl_sc_watchdog(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc;
    int ret = 0;

    if (unlikely(!priv->requested)) {
        ret = -EPERM;
        goto out_return;
    }

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        ret = -EFAULT;
        goto out_return;
    }

    if (down_interruptible(&master->master_sem)) {
        ret = -EINTR;
        goto out_return;
    }

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        ret = -ENOENT;
        goto out_up;
    }

    ecrt_slave_config_watchdog(sc,
            data.watchdog_divider, data.watchdog_intervals);

out_up:
    up(&master->master_sem);
out_return:
    return ret;
}

/*****************************************************************************/

/** Add a PDO to the assignment.
 */
int ec_cdev_ioctl_sc_add_pdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_pdo_t data;
    ec_slave_config_t *sc;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem); // FIXME

    return ecrt_slave_config_pdo_assign_add(sc, data.sync_index, data.index);
}

/*****************************************************************************/

/** Clears the PDO assignment.
 */
int ec_cdev_ioctl_sc_clear_pdos(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_pdo_t data;
    ec_slave_config_t *sc;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem); // FIXME

    ecrt_slave_config_pdo_assign_clear(sc, data.sync_index);
    return 0;
}

/*****************************************************************************/

/** Add an entry to a PDO's mapping.
 */
int ec_cdev_ioctl_sc_add_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_add_pdo_entry_t data;
    ec_slave_config_t *sc;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem); // FIXME

    return ecrt_slave_config_pdo_mapping_add(sc, data.pdo_index,
            data.entry_index, data.entry_subindex, data.entry_bit_length);
}

/*****************************************************************************/

/** Clears the mapping of a PDO.
 */
int ec_cdev_ioctl_sc_clear_entries(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_pdo_t data;
    ec_slave_config_t *sc;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem); // FIXME

    ecrt_slave_config_pdo_mapping_clear(sc, data.index);
    return 0;
}

/*****************************************************************************/

/** Registers a PDO entry.
 */
int ec_cdev_ioctl_sc_reg_pdo_entry(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_reg_pdo_entry_t data;
    ec_slave_config_t *sc;
    ec_domain_t *domain;
    int ret;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(domain = ec_master_find_domain(master, data.domain_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem); // FIXME

    ret = ecrt_slave_config_reg_pdo_entry(sc, data.entry_index,
            data.entry_subindex, domain, &data.bit_position);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return ret;
}

/*****************************************************************************/

/** Sets the DC AssignActivate word and the sync signal times.
 */
int ec_cdev_ioctl_sc_dc(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    ecrt_slave_config_dc(sc, data.dc_assign_activate,
            data.dc_sync[0].cycle_time,
            data.dc_sync[0].shift_time,
            data.dc_sync[1].cycle_time,
            data.dc_sync[1].shift_time);

    up(&master->master_sem);

    return 0;
}

/*****************************************************************************/

/** Configures an SDO.
 */
int ec_cdev_ioctl_sc_sdo(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sc_sdo_t data;
    ec_slave_config_t *sc;
    uint8_t *sdo_data = NULL;
    int ret;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (!data.size)
        return -EINVAL;

    if (!(sdo_data = kmalloc(data.size, GFP_KERNEL))) {
        return -ENOMEM;
    }

    if (copy_from_user(sdo_data, (void __user *) data.data, data.size)) {
        kfree(sdo_data);
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem)) {
        kfree(sdo_data);
        return -EINTR;
    }

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        kfree(sdo_data);
        return -ENOENT;
    }

    up(&master->master_sem); // FIXME

    if (data.complete_access) {
        ret = ecrt_slave_config_complete_sdo(sc,
                data.index, sdo_data, data.size);
    } else {
        ret = ecrt_slave_config_sdo(sc, data.index, data.subindex, sdo_data,
                data.size);
    }
    kfree(sdo_data);
    return ret;
}

/*****************************************************************************/

/** Create an SDO request.
 */
int ec_cdev_ioctl_sc_create_sdo_request(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sdo_request_t data;
    ec_slave_config_t *sc;
    ec_sdo_request_t *req;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    data.request_index = 0;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    sc = ec_master_get_config(master, data.config_index);
    if (!sc) {
        up(&master->master_sem);
        return -ENOENT;
    }

    list_for_each_entry(req, &sc->sdo_requests, list) {
        data.request_index++;
    }

    up(&master->master_sem);

    req = ecrt_slave_config_create_sdo_request_err(sc, data.sdo_index,
            data.sdo_subindex, data.size);
    if (IS_ERR(req))
        return PTR_ERR(req);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Create a VoE handler.
 */
int ec_cdev_ioctl_sc_create_voe_handler(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_voe_t data;
    ec_slave_config_t *sc;
    ec_voe_handler_t *voe;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    data.voe_index = 0;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    sc = ec_master_get_config(master, data.config_index);
    if (!sc) {
        up(&master->master_sem);
        return -ENOENT;
    }

    list_for_each_entry(voe, &sc->voe_handlers, list) {
        data.voe_index++;
    }

    up(&master->master_sem);

    voe = ecrt_slave_config_create_voe_handler_err(sc, data.size);
    if (IS_ERR(voe))
        return PTR_ERR(voe);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Get the slave configuration's state.
 */
int ec_cdev_ioctl_sc_state(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sc_state_t data;
    const ec_slave_config_t *sc;
    ec_slave_config_state_t state;
    
    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config_const(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    ecrt_slave_config_state(sc, &state);

    up(&master->master_sem);

    if (copy_to_user((void __user *) data.state, &state, sizeof(state)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Gets the domain's offset in the total process data.
 */
int ec_cdev_ioctl_domain_offset(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    int offset = 0;
    const ec_domain_t *domain;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (down_interruptible(&master->master_sem)) {
        return -EINTR;
    }

    list_for_each_entry(domain, &master->domains, list) {
        if (domain->index == arg) {
            up(&master->master_sem);
            return offset;
        }
        offset += ecrt_domain_size(domain);
    }

    up(&master->master_sem);
    return -ENOENT;
}

/*****************************************************************************/

/** Process the domain.
 */
int ec_cdev_ioctl_domain_process(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_domain_t *domain;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain(master, arg))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    ecrt_domain_process(domain);
    up(&master->master_sem);
    return 0;
}

/*****************************************************************************/

/** Queue the domain.
 */
int ec_cdev_ioctl_domain_queue(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_domain_t *domain;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain(master, arg))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    ecrt_domain_queue(domain);
    up(&master->master_sem);
    return 0;
}

/*****************************************************************************/

/** Get the domain state.
 */
int ec_cdev_ioctl_domain_state(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_domain_state_t data;
    const ec_domain_t *domain;
    ec_domain_state_t state;
    
    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(domain = ec_master_find_domain_const(master, data.domain_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    ecrt_domain_state(domain, &state);

    up(&master->master_sem);

    if (copy_to_user((void __user *) data.state, &state, sizeof(state)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Sets an SDO request's timeout.
 */
int ec_cdev_ioctl_sdo_request_timeout(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sdo_request_t data;
    ec_slave_config_t *sc;
    ec_sdo_request_t *req;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(req = ec_slave_config_find_sdo_request(sc, data.request_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    ecrt_sdo_request_timeout(req, data.timeout);
    return 0;
}

/*****************************************************************************/

/** Gets an SDO request's state.
 */
int ec_cdev_ioctl_sdo_request_state(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sdo_request_t data;
    ec_slave_config_t *sc;
    ec_sdo_request_t *req;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(req = ec_slave_config_find_sdo_request(sc, data.request_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    data.state = ecrt_sdo_request_state(req);
    if (data.state == EC_REQUEST_SUCCESS && req->dir == EC_DIR_INPUT)
        data.size = ecrt_sdo_request_data_size(req);
    else
        data.size = 0;

    up(&master->master_sem);

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Starts an SDO read operation.
 */
int ec_cdev_ioctl_sdo_request_read(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sdo_request_t data;
    ec_slave_config_t *sc;
    ec_sdo_request_t *req;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(req = ec_slave_config_find_sdo_request(sc, data.request_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    ecrt_sdo_request_read(req);
    return 0;
}

/*****************************************************************************/

/** Starts an SDO write operation.
 */
int ec_cdev_ioctl_sdo_request_write(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sdo_request_t data;
    ec_slave_config_t *sc;
    ec_sdo_request_t *req;
    int ret;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (!data.size) {
        EC_ERR("Sdo download: Data size may not be zero!\n");
        return -EINVAL;
    }

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(req = ec_slave_config_find_sdo_request(sc, data.request_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    ret = ec_sdo_request_alloc(req, data.size);
    if (ret)
        return ret;

    if (copy_from_user(req->data, (void __user *) data.data, data.size))
        return -EFAULT;

    req->data_size = data.size;
    ecrt_sdo_request_write(req);
    return 0;
}

/*****************************************************************************/

/** Read SDO data.
 */
int ec_cdev_ioctl_sdo_request_data(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_sdo_request_t data;
    ec_slave_config_t *sc;
    ec_sdo_request_t *req;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(req = ec_slave_config_find_sdo_request(sc, data.request_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    if (copy_to_user((void __user *) data.data, ecrt_sdo_request_data(req),
                ecrt_sdo_request_data_size(req)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Sets the VoE send header.
 */
int ec_cdev_ioctl_voe_send_header(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_voe_t data;
    ec_slave_config_t *sc;
    ec_voe_handler_t *voe;
    uint32_t vendor_id;
    uint16_t vendor_type;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (get_user(vendor_id, data.vendor_id))
        return -EFAULT;

    if (get_user(vendor_type, data.vendor_type))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(voe = ec_slave_config_find_voe_handler(sc, data.voe_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    ecrt_voe_handler_send_header(voe, vendor_id, vendor_type);
    return 0;
}

/*****************************************************************************/

/** Gets the received VoE header.
 */
int ec_cdev_ioctl_voe_rec_header(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_voe_t data;
    ec_slave_config_t *sc;
    ec_voe_handler_t *voe;
    uint32_t vendor_id;
    uint16_t vendor_type;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(voe = ec_slave_config_find_voe_handler(sc, data.voe_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    ecrt_voe_handler_received_header(voe, &vendor_id, &vendor_type);

    up(&master->master_sem);

    if (likely(data.vendor_id))
        if (put_user(vendor_id, data.vendor_id))
            return -EFAULT;

    if (likely(data.vendor_type))
        if (put_user(vendor_type, data.vendor_type))
            return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Starts a VoE read operation.
 */
int ec_cdev_ioctl_voe_read(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_voe_t data;
    ec_slave_config_t *sc;
    ec_voe_handler_t *voe;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(voe = ec_slave_config_find_voe_handler(sc, data.voe_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    ecrt_voe_handler_read(voe);
    return 0;
}

/*****************************************************************************/

/** Starts a VoE read operation without sending a sync message first.
 */
int ec_cdev_ioctl_voe_read_nosync(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_voe_t data;
    ec_slave_config_t *sc;
    ec_voe_handler_t *voe;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(voe = ec_slave_config_find_voe_handler(sc, data.voe_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    ecrt_voe_handler_read_nosync(voe);
    return 0;
}

/*****************************************************************************/

/** Starts a VoE write operation.
 */
int ec_cdev_ioctl_voe_write(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_voe_t data;
    ec_slave_config_t *sc;
    ec_voe_handler_t *voe;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(voe = ec_slave_config_find_voe_handler(sc, data.voe_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    if (data.size) {
        if (data.size > ec_voe_handler_mem_size(voe))
            return -EOVERFLOW;

        if (copy_from_user(ecrt_voe_handler_data(voe),
                    (void __user *) data.data, data.size))
            return -EFAULT;
    }

    ecrt_voe_handler_write(voe, data.size);
    return 0;
}

/*****************************************************************************/

/** Executes the VoE state machine.
 */
int ec_cdev_ioctl_voe_exec(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_voe_t data;
    ec_slave_config_t *sc;
    ec_voe_handler_t *voe;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(voe = ec_slave_config_find_voe_handler(sc, data.voe_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    data.state = ecrt_voe_handler_execute(voe);
    if (data.state == EC_REQUEST_SUCCESS && voe->dir == EC_DIR_INPUT)
        data.size = ecrt_voe_handler_data_size(voe);
    else
        data.size = 0;

    if (copy_to_user((void __user *) arg, &data, sizeof(data)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Reads the received VoE data.
 */
int ec_cdev_ioctl_voe_data(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg, /**< ioctl() argument. */
        ec_cdev_priv_t *priv /**< Private data structure of file handle. */
        )
{
    ec_ioctl_voe_t data;
    ec_slave_config_t *sc;
    ec_voe_handler_t *voe;

    if (unlikely(!priv->requested))
        return -EPERM;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data)))
        return -EFAULT;

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(sc = ec_master_get_config(master, data.config_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    if (!(voe = ec_slave_config_find_voe_handler(sc, data.voe_index))) {
        up(&master->master_sem);
        return -ENOENT;
    }

    up(&master->master_sem);

    if (copy_to_user((void __user *) data.data, ecrt_voe_handler_data(voe),
                ecrt_voe_handler_data_size(voe)))
        return -EFAULT;

    return 0;
}

/*****************************************************************************/

/** Read a file from a slave via FoE.
 */
int ec_cdev_ioctl_slave_foe_read(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_foe_t data;
    ec_master_foe_request_t request;
    int retval;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    ec_foe_request_init(&request.req, data.file_name);
    ec_foe_request_read(&request.req);
    ec_foe_request_alloc(&request.req, 10000); // FIXME

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(request.slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        ec_foe_request_clear(&request.req);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        return -EINVAL;
    }

    // schedule request.
    list_add_tail(&request.list, &request.slave->foe_requests);

    up(&master->master_sem);

    if (master->debug_level) {
        EC_DBG("Scheduled FoE read request on slave %u.\n",
                request.slave->ring_position);
    }

    // wait for processing through FSM
    if (wait_event_interruptible(request.slave->foe_queue,
                request.req.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.req.state == EC_INT_REQUEST_QUEUED) {
            list_del(&request.list);
            up(&master->master_sem);
            ec_foe_request_clear(&request.req);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(request.slave->foe_queue,
            request.req.state != EC_INT_REQUEST_BUSY);

    data.result = request.req.result;
    data.error_code = request.req.error_code;

    if (master->debug_level) {
        EC_DBG("Read %zd bytes via FoE (result = 0x%x).\n",
                request.req.data_size, request.req.result);
    }

    if (request.req.state != EC_INT_REQUEST_SUCCESS) {
        data.data_size = 0;
        retval = -EIO;
    } else {
        if (request.req.data_size > data.buffer_size) {
            EC_ERR("Buffer too small.\n");
            ec_foe_request_clear(&request.req);
            return -EOVERFLOW;
        }
        data.data_size = request.req.data_size;
        if (copy_to_user((void __user *) data.buffer,
                    request.req.buffer, data.data_size)) {
            ec_foe_request_clear(&request.req);
            return -EFAULT;
        }
        retval = 0;
    }

    if (__copy_to_user((void __user *) arg, &data, sizeof(data))) {
        retval = -EFAULT;
    }

    if (master->debug_level)
        EC_DBG("FoE read request finished on slave %u.\n",
                request.slave->ring_position);

    ec_foe_request_clear(&request.req);

    return retval;
}

/*****************************************************************************/

/** Write a file to a slave via FoE
 */
int ec_cdev_ioctl_slave_foe_write(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_foe_t data;
    ec_master_foe_request_t request;
    int retval;

    if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
        return -EFAULT;
    }

    INIT_LIST_HEAD(&request.list);

    ec_foe_request_init(&request.req, data.file_name);

    if (ec_foe_request_alloc(&request.req, data.buffer_size)) {
        ec_foe_request_clear(&request.req);
        return -ENOMEM;
    }
    if (copy_from_user(request.req.buffer,
                (void __user *) data.buffer, data.buffer_size)) {
        ec_foe_request_clear(&request.req);
        return -EFAULT;
    }
    request.req.data_size = data.buffer_size;
    ec_foe_request_write(&request.req);

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(request.slave = ec_master_find_slave(
                    master, 0, data.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", data.slave_position);
        ec_foe_request_clear(&request.req);
        return -EINVAL;
    }

    if (master->debug_level) {
        EC_DBG("Scheduling FoE write request.\n");
    }

    // schedule FoE write request.
    list_add_tail(&request.list, &request.slave->foe_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(request.slave->foe_queue,
                request.req.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.req.state == EC_INT_REQUEST_QUEUED) {
            // abort request
            list_del(&request.list);
            up(&master->master_sem);
            ec_foe_request_clear(&request.req);
            return -EINTR;
        }
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(request.slave->foe_queue,
            request.req.state != EC_INT_REQUEST_BUSY);

    data.result = request.req.result;
    data.error_code = request.req.error_code;

    retval = request.req.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;

    if (__copy_to_user((void __user *) arg, &data, sizeof(data))) {
        retval = -EFAULT;
    }

    ec_foe_request_clear(&request.req);

    if (master->debug_level) {
        EC_DBG("Finished FoE writing.\n");
    }

    return retval;
}

/*****************************************************************************/

/** Read an SoE IDN.
 */
int ec_cdev_ioctl_slave_soe_read(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_soe_read_t ioctl;
    ec_master_soe_request_t request;
    int retval;

    if (copy_from_user(&ioctl, (void __user *) arg, sizeof(ioctl))) {
        return -EFAULT;
    }

    ec_soe_request_init(&request.req);
    ec_soe_request_set_idn(&request.req, ioctl.idn);
    ec_soe_request_read(&request.req);

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(request.slave = ec_master_find_slave(
                    master, 0, ioctl.slave_position))) {
        up(&master->master_sem);
        ec_soe_request_clear(&request.req);
        EC_ERR("Slave %u does not exist!\n", ioctl.slave_position);
        return -EINVAL;
    }

    // schedule request.
    list_add_tail(&request.list, &request.slave->soe_requests);

    up(&master->master_sem);

    if (master->debug_level) {
        EC_DBG("Scheduled SoE read request on slave %u.\n",
                request.slave->ring_position);
    }

    // wait for processing through FSM
    if (wait_event_interruptible(request.slave->soe_queue,
                request.req.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.req.state == EC_INT_REQUEST_QUEUED) {
            list_del(&request.list);
            up(&master->master_sem);
            ec_soe_request_clear(&request.req);
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(request.slave->soe_queue,
            request.req.state != EC_INT_REQUEST_BUSY);

    ioctl.error_code = request.req.error_code;

    if (master->debug_level) {
        EC_DBG("Read %zd bytes via SoE.\n", request.req.data_size);
    }

    if (request.req.state != EC_INT_REQUEST_SUCCESS) {
        ioctl.data_size = 0;
        retval = -EIO;
    } else {
        if (request.req.data_size > ioctl.mem_size) {
            EC_ERR("Buffer too small.\n");
            ec_soe_request_clear(&request.req);
            return -EOVERFLOW;
        }
        ioctl.data_size = request.req.data_size;
        if (copy_to_user((void __user *) ioctl.data,
                    request.req.data, ioctl.data_size)) {
            ec_soe_request_clear(&request.req);
            return -EFAULT;
        }
        retval = 0;
    }

    if (__copy_to_user((void __user *) arg, &ioctl, sizeof(ioctl))) {
        retval = -EFAULT;
    }

    if (master->debug_level)
        EC_DBG("SoE read request finished on slave %u.\n",
                request.slave->ring_position);

    ec_soe_request_clear(&request.req);

    return retval;
}

/*****************************************************************************/

/** Write an IDN to a slave via SoE.
 */
int ec_cdev_ioctl_slave_soe_write(
        ec_master_t *master, /**< EtherCAT master. */
        unsigned long arg /**< ioctl() argument. */
        )
{
    ec_ioctl_slave_soe_write_t ioctl;
    ec_master_soe_request_t request;
    int retval;

    if (copy_from_user(&ioctl, (void __user *) arg, sizeof(ioctl))) {
        return -EFAULT;
    }

    INIT_LIST_HEAD(&request.list);

    ec_soe_request_init(&request.req);
    ec_soe_request_set_idn(&request.req, ioctl.idn);

    if (ec_soe_request_alloc(&request.req, ioctl.data_size)) {
        ec_soe_request_clear(&request.req);
        return -ENOMEM;
    }
    if (copy_from_user(request.req.data,
                (void __user *) ioctl.data, ioctl.data_size)) {
        ec_soe_request_clear(&request.req);
        return -EFAULT;
    }
    request.req.data_size = ioctl.data_size;
    ec_soe_request_write(&request.req);

    if (down_interruptible(&master->master_sem))
        return -EINTR;

    if (!(request.slave = ec_master_find_slave(
                    master, 0, ioctl.slave_position))) {
        up(&master->master_sem);
        EC_ERR("Slave %u does not exist!\n", ioctl.slave_position);
        ec_soe_request_clear(&request.req);
        return -EINVAL;
    }

    if (master->debug_level) {
        EC_DBG("Scheduling SoE write request.\n");
    }

    // schedule SoE write request.
    list_add_tail(&request.list, &request.slave->soe_requests);

    up(&master->master_sem);

    // wait for processing through FSM
    if (wait_event_interruptible(request.slave->soe_queue,
                request.req.state != EC_INT_REQUEST_QUEUED)) {
        // interrupted by signal
        down(&master->master_sem);
        if (request.req.state == EC_INT_REQUEST_QUEUED) {
            // abort request
            list_del(&request.list);
            up(&master->master_sem);
            ec_soe_request_clear(&request.req);
            return -EINTR;
        }
        up(&master->master_sem);
    }

    // wait until master FSM has finished processing
    wait_event(request.slave->soe_queue,
            request.req.state != EC_INT_REQUEST_BUSY);

    ioctl.error_code = request.req.error_code;
    retval = request.req.state == EC_INT_REQUEST_SUCCESS ? 0 : -EIO;

    if (__copy_to_user((void __user *) arg, &ioctl, sizeof(ioctl))) {
        retval = -EFAULT;
    }

    ec_soe_request_clear(&request.req);

    if (master->debug_level) {
        EC_DBG("Finished SoE writing.\n");
    }

    return retval;
}

/******************************************************************************
 * File operations
 *****************************************************************************/

/** Called when the cdev is opened.
 */
int eccdev_open(struct inode *inode, struct file *filp)
{
    ec_cdev_t *cdev = container_of(inode->i_cdev, ec_cdev_t, cdev);
    ec_cdev_priv_t *priv;

    priv = kmalloc(sizeof(ec_cdev_priv_t), GFP_KERNEL);
    if (!priv) {
        EC_ERR("Failed to allocate memory for private data structure.\n");
        return -ENOMEM;
    }

    priv->cdev = cdev;
    priv->requested = 0;
    priv->process_data = NULL;
    priv->process_data_size = 0;

    filp->private_data = priv;

#if DEBUG_IOCTL
    EC_DBG("File opened.\n");
#endif
    return 0;
}

/*****************************************************************************/

/** Called when the cdev is closed.
 */
int eccdev_release(struct inode *inode, struct file *filp)
{
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) filp->private_data;
    ec_master_t *master = priv->cdev->master;

    if (priv->requested)
        ecrt_release_master(master);

    if (priv->process_data)
        vfree(priv->process_data);

#if DEBUG_IOCTL
    EC_DBG("File closed.\n");
#endif

    kfree(priv);
    return 0;
}

/*****************************************************************************/

/** Called when an ioctl() command is issued.
 */
long eccdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) filp->private_data;
    ec_master_t *master = priv->cdev->master;

#if DEBUG_IOCTL
    EC_DBG("ioctl(filp = 0x%x, cmd = 0x%08x (0x%02x), arg = 0x%x)\n",
            (u32) filp, (u32) cmd, (u32) _IOC_NR(cmd), (u32) arg);
#endif

    switch (cmd) {
        case EC_IOCTL_MODULE:
            return ec_cdev_ioctl_module(arg);
        case EC_IOCTL_MASTER:
            return ec_cdev_ioctl_master(master, arg);
        case EC_IOCTL_SLAVE:
            return ec_cdev_ioctl_slave(master, arg);
        case EC_IOCTL_SLAVE_SYNC:
            return ec_cdev_ioctl_slave_sync(master, arg);
        case EC_IOCTL_SLAVE_SYNC_PDO:
            return ec_cdev_ioctl_slave_sync_pdo(master, arg);
        case EC_IOCTL_SLAVE_SYNC_PDO_ENTRY:
            return ec_cdev_ioctl_slave_sync_pdo_entry(master, arg);
        case EC_IOCTL_DOMAIN:
            return ec_cdev_ioctl_domain(master, arg);
        case EC_IOCTL_DOMAIN_FMMU:
            return ec_cdev_ioctl_domain_fmmu(master, arg);
        case EC_IOCTL_DOMAIN_DATA:
            return ec_cdev_ioctl_domain_data(master, arg);
        case EC_IOCTL_MASTER_DEBUG:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_master_debug(master, arg);
        case EC_IOCTL_SLAVE_STATE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_state(master, arg);
        case EC_IOCTL_SLAVE_SDO:
            return ec_cdev_ioctl_slave_sdo(master, arg);
        case EC_IOCTL_SLAVE_SDO_ENTRY:
            return ec_cdev_ioctl_slave_sdo_entry(master, arg);
        case EC_IOCTL_SLAVE_SDO_UPLOAD:
            return ec_cdev_ioctl_slave_sdo_upload(master, arg);
        case EC_IOCTL_SLAVE_SDO_DOWNLOAD:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_sdo_download(master, arg);
        case EC_IOCTL_SLAVE_SII_READ:
            return ec_cdev_ioctl_slave_sii_read(master, arg);
        case EC_IOCTL_SLAVE_SII_WRITE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_sii_write(master, arg);
        case EC_IOCTL_SLAVE_REG_READ:
            return ec_cdev_ioctl_slave_reg_read(master, arg);
        case EC_IOCTL_SLAVE_REG_WRITE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_reg_write(master, arg);
        case EC_IOCTL_SLAVE_FOE_READ:
            return ec_cdev_ioctl_slave_foe_read(master, arg);
        case EC_IOCTL_SLAVE_FOE_WRITE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_foe_write(master, arg);
        case EC_IOCTL_SLAVE_SOE_READ:
            return ec_cdev_ioctl_slave_soe_read(master, arg);
        case EC_IOCTL_SLAVE_SOE_WRITE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_slave_soe_write(master, arg);
        case EC_IOCTL_CONFIG:
            return ec_cdev_ioctl_config(master, arg);
        case EC_IOCTL_CONFIG_PDO:
            return ec_cdev_ioctl_config_pdo(master, arg);
        case EC_IOCTL_CONFIG_PDO_ENTRY:
            return ec_cdev_ioctl_config_pdo_entry(master, arg);
        case EC_IOCTL_CONFIG_SDO:
            return ec_cdev_ioctl_config_sdo(master, arg);
#ifdef EC_EOE
        case EC_IOCTL_EOE_HANDLER:
            return ec_cdev_ioctl_eoe_handler(master, arg);
#endif
        case EC_IOCTL_REQUEST:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_request(master, arg, priv);
        case EC_IOCTL_CREATE_DOMAIN:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_create_domain(master, arg, priv);
        case EC_IOCTL_CREATE_SLAVE_CONFIG:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_create_slave_config(master, arg, priv);
        case EC_IOCTL_ACTIVATE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_activate(master, arg, priv);
        case EC_IOCTL_DEACTIVATE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_deactivate(master, arg, priv);
        case EC_IOCTL_SEND:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_send(master, arg, priv);
        case EC_IOCTL_RECEIVE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_receive(master, arg, priv);
        case EC_IOCTL_MASTER_STATE:
            return ec_cdev_ioctl_master_state(master, arg, priv);
        case EC_IOCTL_APP_TIME:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_app_time(master, arg, priv);
        case EC_IOCTL_SYNC_REF:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sync_ref(master, arg, priv);
        case EC_IOCTL_SYNC_SLAVES:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sync_slaves(master, arg, priv);
        case EC_IOCTL_SYNC_MON_QUEUE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sync_mon_queue(master, arg, priv);
        case EC_IOCTL_SYNC_MON_PROCESS:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sync_mon_process(master, arg, priv);
        case EC_IOCTL_SC_SYNC:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_sync(master, arg, priv);
        case EC_IOCTL_SC_WATCHDOG:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_watchdog(master, arg, priv);
        case EC_IOCTL_SC_ADD_PDO:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_add_pdo(master, arg, priv);
        case EC_IOCTL_SC_CLEAR_PDOS:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_clear_pdos(master, arg, priv);
        case EC_IOCTL_SC_ADD_ENTRY:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_add_entry(master, arg, priv);
        case EC_IOCTL_SC_CLEAR_ENTRIES:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_clear_entries(master, arg, priv);
        case EC_IOCTL_SC_REG_PDO_ENTRY:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_reg_pdo_entry(master, arg, priv);
        case EC_IOCTL_SC_DC:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_dc(master, arg, priv);
        case EC_IOCTL_SC_SDO:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_sdo(master, arg, priv);
        case EC_IOCTL_SC_SDO_REQUEST:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_create_sdo_request(master, arg, priv);
        case EC_IOCTL_SC_VOE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sc_create_voe_handler(master, arg, priv);
        case EC_IOCTL_SC_STATE:
            return ec_cdev_ioctl_sc_state(master, arg, priv);
        case EC_IOCTL_DOMAIN_OFFSET:
            return ec_cdev_ioctl_domain_offset(master, arg, priv);
        case EC_IOCTL_DOMAIN_PROCESS:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_domain_process(master, arg, priv);
        case EC_IOCTL_DOMAIN_QUEUE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_domain_queue(master, arg, priv);
        case EC_IOCTL_DOMAIN_STATE:
            return ec_cdev_ioctl_domain_state(master, arg, priv);
        case EC_IOCTL_SDO_REQUEST_TIMEOUT:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sdo_request_timeout(master, arg, priv);
        case EC_IOCTL_SDO_REQUEST_STATE:
            return ec_cdev_ioctl_sdo_request_state(master, arg, priv);
        case EC_IOCTL_SDO_REQUEST_READ:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sdo_request_read(master, arg, priv);
        case EC_IOCTL_SDO_REQUEST_WRITE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_sdo_request_write(master, arg, priv);
        case EC_IOCTL_SDO_REQUEST_DATA:
            return ec_cdev_ioctl_sdo_request_data(master, arg, priv);
        case EC_IOCTL_VOE_SEND_HEADER:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_voe_send_header(master, arg, priv);
        case EC_IOCTL_VOE_REC_HEADER:
            return ec_cdev_ioctl_voe_rec_header(master, arg, priv);
        case EC_IOCTL_VOE_READ:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_voe_read(master, arg, priv);
        case EC_IOCTL_VOE_READ_NOSYNC:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_voe_read_nosync(master, arg, priv);
        case EC_IOCTL_VOE_WRITE:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_voe_write(master, arg, priv);
        case EC_IOCTL_VOE_EXEC:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_voe_exec(master, arg, priv);
        case EC_IOCTL_VOE_DATA:
            return ec_cdev_ioctl_voe_data(master, arg, priv);
        case EC_IOCTL_SET_SEND_INTERVAL:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            return ec_cdev_ioctl_set_send_interval(master,arg,priv);
        default:
            return -ENOTTY;
    }
}

/*****************************************************************************/

/** Memory-map callback for the EtherCAT character device.
 *
 * The actual mapping will be done in the eccdev_vma_nopage() callback of the
 * virtual memory area.
 */
int eccdev_mmap(
        struct file *filp,
        struct vm_area_struct *vma
        )
{
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) filp->private_data;

    if (priv->cdev->master->debug_level)
        EC_DBG("mmap()\n");

    vma->vm_ops = &eccdev_vm_ops;
    vma->vm_flags |= VM_RESERVED; /* Pages will not be swapped out */
    vma->vm_private_data = priv;

    return 0;
}

/*****************************************************************************/

#if LINUX_VERSION_CODE >= PAGE_FAULT_VERSION

/** Page fault callback for a virtual memory area.
 *
 * Called at the first access on a virtual-memory area retrieved with
 * ecdev_mmap().
 */
static int eccdev_vma_fault(
        struct vm_area_struct *vma, /**< Virtual memory area. */
        struct vm_fault *vmf /**< Fault data. */
        )
{
    unsigned long offset = vmf->pgoff << PAGE_SHIFT;
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) vma->vm_private_data;
    struct page *page;

    if (offset >= priv->process_data_size)
        return VM_FAULT_SIGBUS;

    page = vmalloc_to_page(priv->process_data + offset);
    if (!page)
        return VM_FAULT_SIGBUS;

    get_page(page);
    vmf->page = page;

    if (priv->cdev->master->debug_level)
        EC_DBG("Vma fault, virtual_address = %p, offset = %lu, page = %p\n",
                vmf->virtual_address, offset, page);

    return 0;
}

#else

/** Nopage callback for a virtual memory area.
 *
 * Called at the first access on a virtual-memory area retrieved with
 * ecdev_mmap().
 */
struct page *eccdev_vma_nopage(
        struct vm_area_struct *vma, /**< Virtual memory area initialized by
                                      the kernel. */
        unsigned long address, /**< Requested virtual address. */
        int *type /**< Type output parameter. */
        )
{
    unsigned long offset;
    struct page *page = NOPAGE_SIGBUS;
    ec_cdev_priv_t *priv = (ec_cdev_priv_t *) vma->vm_private_data;

    offset = (address - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);

    if (offset >= priv->process_data_size)
        return NOPAGE_SIGBUS;

    page = vmalloc_to_page(priv->process_data + offset);

    if (priv->cdev->master->debug_level)
        EC_DBG("Nopage fault vma, address = %#lx, offset = %#lx, page = %p\n",
                address, offset, page);

    get_page(page);
    if (type)
        *type = VM_FAULT_MINOR;

    return page;
}

#endif

/*****************************************************************************/
