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
   EtherCAT master character device.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "cdev.h"
#include "master.h"
#include "slave_config.h"
#include "ioctl.h"

/*****************************************************************************/

/** \cond */

int eccdev_open(struct inode *, struct file *);
int eccdev_release(struct inode *, struct file *);
long eccdev_ioctl(struct file *, unsigned int, unsigned long);

/*****************************************************************************/

static struct file_operations eccdev_fops = {
    .owner          = THIS_MODULE,
    .open           = eccdev_open,
    .release        = eccdev_release,
    .unlocked_ioctl = eccdev_ioctl
};

/** \endcond */

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
    cdev->master = master;

    cdev_init(&cdev->cdev, &eccdev_fops);
    cdev->cdev.owner = THIS_MODULE;

    if (cdev_add(&cdev->cdev,
		 MKDEV(MAJOR(dev_num), master->index), 1)) {
		EC_ERR("Failed to add character device!\n");
		return -1;
    }

    return 0;
}

/*****************************************************************************/

/** Destructor.
 */
void ec_cdev_clear(ec_cdev_t *cdev /**< EtherCAT XML device */)
{
    cdev_del(&cdev->cdev);
}

/******************************************************************************
 * File operations
 *****************************************************************************/

int eccdev_open(struct inode *inode, struct file *filp)
{
    ec_cdev_t *cdev = container_of(inode->i_cdev, ec_cdev_t, cdev);
    ec_master_t *master = cdev->master;

    filp->private_data = cdev;
    if (master->debug_level)
        EC_DBG("File opened.\n");
    return 0;
}

/*****************************************************************************/

int eccdev_release(struct inode *inode, struct file *filp)
{
    ec_cdev_t *cdev = (ec_cdev_t *) filp->private_data;
    ec_master_t *master = cdev->master;

    if (master->debug_level)
        EC_DBG("File closed.\n");
    return 0;
}

/*****************************************************************************/

long eccdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    ec_cdev_t *cdev = (ec_cdev_t *) filp->private_data;
    ec_master_t *master = cdev->master;
    long retval = 0;

    if (master->debug_level)
        EC_DBG("ioctl(filp = %x, cmd = %u (%u), arg = %x)\n",
                (u32) filp, (u32) cmd, (u32) _IOC_NR(cmd), (u32) arg);

    // FIXME lock
    
    switch (cmd) {
        case EC_IOCTL_MASTER:
            {
                ec_ioctl_master_t data;

                data.slave_count = master->slave_count;
                data.config_count = ec_master_config_count(master);
                data.domain_count = ec_master_domain_count(master);
                data.phase = (uint8_t) master->phase;
                
                memcpy(data.devices[0].address, master->main_mac, ETH_ALEN); 
                data.devices[0].attached = master->main_device.dev ? 1 : 0;
                data.devices[0].tx_count = master->main_device.tx_count;
                data.devices[0].rx_count = master->main_device.rx_count;
                memcpy(data.devices[1].address, master->backup_mac, ETH_ALEN); 
                data.devices[1].attached = master->backup_device.dev ? 1 : 0;
                data.devices[1].tx_count = master->backup_device.tx_count;
                data.devices[1].rx_count = master->backup_device.rx_count;

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_SLAVE:
            {
                ec_ioctl_slave_t data;
                const ec_slave_t *slave;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }

                if (!(slave = ec_master_find_slave(
                                master, 0, data.position))) {
                    EC_ERR("Slave %u does not exist!\n", data.position);
                    retval = -EINVAL;
                    break;
                }

                data.vendor_id = slave->sii.vendor_id;
                data.product_code = slave->sii.product_code;
                data.revision_number = slave->sii.revision_number;
                data.serial_number = slave->sii.serial_number;
                data.alias = slave->sii.alias;
                data.rx_mailbox_offset = slave->sii.rx_mailbox_offset;
                data.rx_mailbox_size = slave->sii.rx_mailbox_size;
                data.tx_mailbox_offset = slave->sii.tx_mailbox_offset;
                data.tx_mailbox_size = slave->sii.tx_mailbox_size;
                data.mailbox_protocols = slave->sii.mailbox_protocols;
                data.has_general_category = slave->sii.has_general;
                data.coe_details = slave->sii.coe_details;
                data.general_flags = slave->sii.general_flags;
                data.current_on_ebus = slave->sii.current_on_ebus;
                data.state = slave->current_state;
                data.error_flag = slave->error_flag;

                data.sync_count = slave->sii.sync_count;
                data.sdo_count = ec_slave_sdo_count(slave);
                data.sii_nwords = slave->sii_nwords;

                if (slave->sii.name) {
                    strncpy(data.name, slave->sii.name,
                            EC_IOCTL_STRING_SIZE);
                    data.name[EC_IOCTL_STRING_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_SYNC:
            {
                ec_ioctl_sync_t data;
                const ec_slave_t *slave;
                const ec_sync_t *sync;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                if (data.sync_index >= slave->sii.sync_count) {
                    EC_ERR("Sync manager %u does not exist in slave %u!\n",
                            data.sync_index, data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                sync = &slave->sii.syncs[data.sync_index];

                data.physical_start_address = sync->physical_start_address;
                data.default_size = sync->default_length;
                data.control_register = sync->control_register;
                data.enable = sync->enable;
                data.assign_source = sync->assign_source;
                data.pdo_count = ec_pdo_list_count(&sync->pdos);

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_PDO:
            {
                ec_ioctl_pdo_t data;
                const ec_slave_t *slave;
                const ec_sync_t *sync;
                const ec_pdo_t *pdo;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                if (data.sync_index >= slave->sii.sync_count) {
                    EC_ERR("Sync manager %u does not exist in slave %u!\n",
                            data.sync_index, data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                sync = &slave->sii.syncs[data.sync_index];
                if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                                &sync->pdos, data.pdo_pos))) {
                    EC_ERR("Sync manager %u does not contain a Pdo with "
                            "position %u in slave %u!\n", data.sync_index,
                            data.pdo_pos, data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                data.index = pdo->index;
                data.entry_count = ec_pdo_entry_count(pdo);

                if (pdo->name) {
                    strncpy(data.name, pdo->name, EC_IOCTL_STRING_SIZE);
                    data.name[EC_IOCTL_STRING_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_PDO_ENTRY:
            {
                ec_ioctl_pdo_entry_t data;
                const ec_slave_t *slave;
                const ec_sync_t *sync;
                const ec_pdo_t *pdo;
                const ec_pdo_entry_t *entry;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                if (data.sync_index >= slave->sii.sync_count) {
                    EC_ERR("Sync manager %u does not exist in slave %u!\n",
                            data.sync_index, data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                sync = &slave->sii.syncs[data.sync_index];
                if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                                &sync->pdos, data.pdo_pos))) {
                    EC_ERR("Sync manager %u does not contain a Pdo with "
                            "position %u in slave %u!\n", data.sync_index,
                            data.pdo_pos, data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                if (!(entry = ec_pdo_find_entry_by_pos_const(
                                pdo, data.entry_pos))) {
                    EC_ERR("Pdo 0x%04X does not contain an entry with "
                            "position %u in slave %u!\n", data.pdo_pos,
                            data.entry_pos, data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                data.index = entry->index;
                data.subindex = entry->subindex;
                data.bit_length = entry->bit_length;
                if (entry->name) {
                    strncpy(data.name, entry->name, EC_IOCTL_STRING_SIZE);
                    data.name[EC_IOCTL_STRING_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_DOMAIN:
            {
                ec_ioctl_domain_t data;
                const ec_domain_t *domain;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(domain = ec_master_find_domain(master, data.index))) {
                    EC_ERR("Domain %u does not exist!\n", data.index);
                    retval = -EINVAL;
                    break;
                }

                data.data_size = domain->data_size;
                data.logical_base_address = domain->logical_base_address;
                data.working_counter = domain->working_counter;
                data.expected_working_counter = domain->expected_working_counter;
                data.fmmu_count = ec_domain_fmmu_count(domain);

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_DOMAIN_FMMU:
            {
                ec_ioctl_domain_fmmu_t data;
                const ec_domain_t *domain;
                const ec_fmmu_config_t *fmmu;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(domain = ec_master_find_domain(master, data.domain_index))) {
                    EC_ERR("Domain %u does not exist!\n", data.domain_index);
                    retval = -EINVAL;
                    break;
                }

                if (!(fmmu = ec_domain_find_fmmu(domain, data.fmmu_index))) {
                    EC_ERR("Domain %u has less than %u fmmu configurations.\n",
                            data.domain_index, data.fmmu_index + 1);
                    retval = -EINVAL;
                    break;
                }

                data.slave_config_alias = fmmu->sc->alias;
                data.slave_config_position = fmmu->sc->position;
                data.sync_index = fmmu->sync_index;
                data.dir = fmmu->dir;
                data.logical_address = fmmu->logical_start_address;
                data.data_size = fmmu->data_size;

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_DATA:
            {
                ec_ioctl_data_t data;
                const ec_domain_t *domain;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(domain = ec_master_find_domain(master, data.domain_index))) {
                    EC_ERR("Domain %u does not exist!\n", data.domain_index);
                    retval = -EINVAL;
                    break;
                }

                if (domain->data_size != data.data_size) {
                    EC_ERR("Data size mismatch %u/%u!\n",
                            data.data_size, domain->data_size);
                    retval = -EFAULT;
                    break;
                }

                if (copy_to_user((void __user *) data.target, domain->data,
                            domain->data_size))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_SET_DEBUG:
            if (!(filp->f_mode & FMODE_WRITE))
                return -EPERM;
            if (ec_master_debug_level(master, (unsigned int) arg))
                retval = -EINVAL;
            break;

        case EC_IOCTL_SLAVE_STATE:
            {
                ec_ioctl_slave_state_t data;
                ec_slave_t *slave;

                if (!(filp->f_mode & FMODE_WRITE))
                    return -EPERM;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                ec_slave_request_state(slave, data.requested_state);
                break;
            }

        case EC_IOCTL_SDO:
            {
                ec_ioctl_sdo_t data;
                const ec_slave_t *slave;
                const ec_sdo_t *sdo;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                if (!(sdo = ec_slave_get_sdo_by_pos_const(
                                slave, data.sdo_position))) {
                    EC_ERR("Sdo %u does not exist in slave %u!\n",
                            data.sdo_position, data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                data.sdo_index = sdo->index;
                data.max_subindex = sdo->max_subindex;

                if (sdo->name) {
                    strncpy(data.name, sdo->name, EC_IOCTL_STRING_SIZE);
                    data.name[EC_IOCTL_STRING_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_SDO_ENTRY:
            {
                ec_ioctl_sdo_entry_t data;
                const ec_slave_t *slave;
                const ec_sdo_t *sdo;
                const ec_sdo_entry_t *entry;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                if (data.sdo_spec <= 0) {
                    if (!(sdo = ec_slave_get_sdo_by_pos_const(
                                    slave, -data.sdo_spec))) {
                        EC_ERR("Sdo %u does not exist in slave %u!\n",
                                -data.sdo_spec, data.slave_position);
                        retval = -EINVAL;
                        break;
                    }
                } else {
                    if (!(sdo = ec_slave_get_sdo_const(
                                    slave, data.sdo_spec))) {
                        EC_ERR("Sdo 0x%04X does not exist in slave %u!\n",
                                data.sdo_spec, data.slave_position);
                        retval = -EINVAL;
                        break;
                    }
                }

                if (!(entry = ec_sdo_get_entry_const(
                                sdo, data.sdo_entry_subindex))) {
                    EC_ERR("Sdo entry 0x%04X:%02X does not exist "
                            "in slave %u!\n", sdo->index,
                            data.sdo_entry_subindex, data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                data.data_type = entry->data_type;
                data.bit_length = entry->bit_length;

                if (entry->description) {
                    strncpy(data.description, entry->description,
                            EC_IOCTL_STRING_SIZE);
                    data.description[EC_IOCTL_STRING_SIZE - 1]
                        = 0;
                } else {
                    data.description[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_SDO_UPLOAD:
            {
                ec_ioctl_sdo_upload_t data;
                ec_master_sdo_request_t request;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(request.slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                ec_sdo_request_init(&request.req);
                ec_sdo_request_address(&request.req,
                        data.sdo_index, data.sdo_entry_subindex);
                ecrt_sdo_request_read(&request.req);

                // schedule request.
                down(&master->sdo_sem);
                list_add_tail(&request.list, &master->slave_sdo_requests);
                up(&master->sdo_sem);

                // wait for processing through FSM
                if (wait_event_interruptible(master->sdo_queue,
                            request.req.state != EC_REQUEST_QUEUED)) {
                    // interrupted by signal
                    down(&master->sdo_sem);
                    if (request.req.state == EC_REQUEST_QUEUED) {
                        list_del(&request.req.list);
                        up(&master->sdo_sem);
                        ec_sdo_request_clear(&request.req);
                        retval = -EINTR;
                        break;
                    }
                    // request already processing: interrupt not possible.
                    up(&master->sdo_sem);
                }

                // wait until master FSM has finished processing
                wait_event(master->sdo_queue, request.req.state != EC_REQUEST_BUSY);
                
                data.abort_code = request.req.abort_code;

                if (request.req.state != EC_REQUEST_SUCCESS) {
                    data.data_size = 0;
                    retval = -EIO;
                } else {
                    if (request.req.data_size > data.target_size) {
                        EC_ERR("Buffer too small.\n");
                        ec_sdo_request_clear(&request.req);
                        retval = -EOVERFLOW;
                        break;
                    }
                    data.data_size = request.req.data_size;

                    if (copy_to_user((void __user *) data.target,
                                request.req.data, data.data_size)) {
                        ec_sdo_request_clear(&request.req);
                        retval = -EFAULT;
                        break;
                    }
                }

                if (__copy_to_user((void __user *) arg, &data, sizeof(data))) {
                    retval = -EFAULT;
                }

                ec_sdo_request_clear(&request.req);
                break;
            }

        case EC_IOCTL_SDO_DOWNLOAD:
            {
                ec_ioctl_sdo_download_t data;
                ec_master_sdo_request_t request;

                if (!(filp->f_mode & FMODE_WRITE))
                    return -EPERM;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(request.slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                // copy data to download
                if (!data.data_size) {
                    EC_ERR("Zero data size!\n");
                    retval = -EINVAL;
                    break;
                }

                ec_sdo_request_init(&request.req);
                ec_sdo_request_address(&request.req,
                        data.sdo_index, data.sdo_entry_subindex);
                if (ec_sdo_request_alloc(&request.req, data.data_size)) {
                    ec_sdo_request_clear(&request.req);
                    retval = -ENOMEM;
                    break;
                }
                if (copy_from_user(request.req.data,
                            (void __user *) data.data, data.data_size)) {
                    ec_sdo_request_clear(&request.req);
                    retval = -EFAULT;
                    break;
                }
                request.req.data_size = data.data_size;
                ecrt_sdo_request_write(&request.req);

                // schedule request.
                down(&master->sdo_sem);
                list_add_tail(&request.list, &master->slave_sdo_requests);
                up(&master->sdo_sem);

                // wait for processing through FSM
                if (wait_event_interruptible(master->sdo_queue,
                            request.req.state != EC_REQUEST_QUEUED)) {
                    // interrupted by signal
                    down(&master->sdo_sem);
                    if (request.req.state == EC_REQUEST_QUEUED) {
                        list_del(&request.req.list);
                        up(&master->sdo_sem);
                        ec_sdo_request_clear(&request.req);
                        retval = -EINTR;
                        break;
                    }
                    // request already processing: interrupt not possible.
                    up(&master->sdo_sem);
                }

                // wait until master FSM has finished processing
                wait_event(master->sdo_queue, request.req.state != EC_REQUEST_BUSY);

                data.abort_code = request.req.abort_code;

                if (request.req.state != EC_REQUEST_SUCCESS) {
                    retval = -EIO;
                }

                if (__copy_to_user((void __user *) arg, &data, sizeof(data))) {
                    retval = -EFAULT;
                }

                ec_sdo_request_clear(&request.req);
                break;
            }

        case EC_IOCTL_SII_READ:
            {
                ec_ioctl_sii_t data;
                const ec_slave_t *slave;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                if (!data.nwords
                        || data.offset + data.nwords > slave->sii_nwords) {
                    EC_ERR("Invalid SII read offset/size %u/%u for slave "
                            "SII size %u!\n", data.offset,
                            data.nwords, slave->sii_nwords);
                    retval = -EINVAL;
                    break;
                }

                if (copy_to_user((void __user *) data.words,
                            slave->sii_words + data.offset, data.nwords * 2))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_SII_WRITE:
            {
                ec_ioctl_sii_t data;
                ec_slave_t *slave;
                unsigned int byte_size;
                uint16_t *words;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(slave = ec_master_find_slave(
                                master, 0, data.slave_position))) {
                    EC_ERR("Slave %u does not exist!\n", data.slave_position);
                    retval = -EINVAL;
                    break;
                }

                if (!data.nwords)
                    break;

                byte_size = sizeof(uint16_t) * data.nwords;
                if (!(words = kmalloc(byte_size, GFP_KERNEL))) {
                    EC_ERR("Failed to allocate %u bytes for SII contents.\n",
                            byte_size);
                    retval = -ENOMEM;
                    break;
                }

                if (copy_from_user(words,
                            (void __user *) data.words, byte_size)) {
                    retval = -EFAULT;
                    kfree(words);
                    break;
                }

                if (ec_slave_write_sii(slave,
                            data.offset, data.nwords, words))
                    retval = -EIO;

                kfree(words);
                break;
            }

        case EC_IOCTL_CONFIG:
            {
                ec_ioctl_config_t data;
                const ec_slave_config_t *sc;
                uint8_t i;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(sc = ec_master_get_config_const(
                                master, data.config_index))) {
                    EC_ERR("Slave config %u does not exist!\n",
                            data.config_index);
                    retval = -EINVAL;
                    break;
                }

                data.alias = sc->alias;
                data.position = sc->position;
                data.vendor_id = sc->vendor_id;
                data.product_code = sc->product_code;
                for (i = 0; i < EC_MAX_SYNCS; i++) {
                    data.syncs[i].dir = sc->sync_configs[i].dir;
                    data.syncs[i].pdo_count =
                        ec_pdo_list_count(&sc->sync_configs[i].pdos);
                }
                data.sdo_count = ec_slave_config_sdo_count(sc);
                data.attached = sc->slave != NULL;
                data.operational = sc->slave &&
                    sc->slave->current_state == EC_SLAVE_STATE_OP;

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_CONFIG_PDO:
            {
                ec_ioctl_config_pdo_t data;
                const ec_slave_config_t *sc;
                const ec_pdo_t *pdo;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(sc = ec_master_get_config_const(
                                master, data.config_index))) {
                    EC_ERR("Slave config %u does not exist!\n",
                            data.config_index);
                    retval = -EINVAL;
                    break;
                }

                if (data.sync_index >= EC_MAX_SYNCS) {
                    EC_ERR("Invalid sync manager index %u!\n",
                            data.sync_index);
                    retval = -EINVAL;
                    break;
                }
                
                if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                                &sc->sync_configs[data.sync_index].pdos,
                                data.pdo_pos))) {
                    EC_ERR("Invalid Pdo position!\n");
                    retval = -EINVAL;
                    break;
                }

                data.index = pdo->index;
                data.entry_count = ec_pdo_entry_count(pdo);

                if (pdo->name) {
                    strncpy(data.name, pdo->name, EC_IOCTL_STRING_SIZE);
                    data.name[EC_IOCTL_STRING_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_CONFIG_PDO_ENTRY:
            {
                ec_ioctl_config_pdo_entry_t data;
                const ec_slave_config_t *sc;
                const ec_pdo_t *pdo;
                const ec_pdo_entry_t *entry;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(sc = ec_master_get_config_const(
                                master, data.config_index))) {
                    EC_ERR("Slave config %u does not exist!\n",
                            data.config_index);
                    retval = -EINVAL;
                    break;
                }

                if (data.sync_index >= EC_MAX_SYNCS) {
                    EC_ERR("Invalid sync manager index %u!\n",
                            data.sync_index);
                    retval = -EINVAL;
                    break;
                }
                
                if (!(pdo = ec_pdo_list_find_pdo_by_pos_const(
                                &sc->sync_configs[data.sync_index].pdos,
                                data.pdo_pos))) {
                    EC_ERR("Invalid Pdo position!\n");
                    retval = -EINVAL;
                    break;
                }

                if (!(entry = ec_pdo_find_entry_by_pos_const(
                                pdo, data.entry_pos))) {
                    EC_ERR("Entry not found!\n");
                    retval = -EINVAL;
                    break;
                }

                data.index = entry->index;
                data.subindex = entry->subindex;
                data.bit_length = entry->bit_length;
                if (entry->name) {
                    strncpy(data.name, entry->name, EC_IOCTL_STRING_SIZE);
                    data.name[EC_IOCTL_STRING_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        case EC_IOCTL_CONFIG_SDO:
            {
                ec_ioctl_config_sdo_t data;
                const ec_slave_config_t *sc;
                const ec_sdo_request_t *req;

                if (copy_from_user(&data, (void __user *) arg, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                
                if (!(sc = ec_master_get_config_const(
                                master, data.config_index))) {
                    EC_ERR("Slave config %u does not exist!\n",
                            data.config_index);
                    retval = -EINVAL;
                    break;
                }

                if (!(req = ec_slave_config_get_sdo_by_pos_const(
                                sc, data.sdo_pos))) {
                    EC_ERR("Invalid Sdo position!\n");
                    retval = -EINVAL;
                    break;
                }

                data.index = req->index;
                data.subindex = req->subindex;
                data.size = req->data_size;
                memcpy(&data.data, req->data, min((u32) data.size, (u32) 4));

                if (copy_to_user((void __user *) arg, &data, sizeof(data)))
                    retval = -EFAULT;
                break;
            }

        default:
            retval = -ENOTTY;
    }

    return retval;
}

/*****************************************************************************/
