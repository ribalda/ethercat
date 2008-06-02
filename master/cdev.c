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
#include "ioctl.h"

/*****************************************************************************/

/** \cond */

int eccdev_open(struct inode *, struct file *);
int eccdev_release(struct inode *, struct file *);
ssize_t eccdev_read(struct file *, char __user *, size_t, loff_t *);
ssize_t eccdev_write(struct file *, const char __user *, size_t, loff_t *);
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
        EC_DBG("ioctl(filp = %x, cmd = %u, arg = %u)\n",
                (u32) filp, (u32) cmd, (u32) arg);

    // FIXME lock
    
    switch (cmd) {
        case EC_IOCTL_SLAVE_COUNT:
            retval = master->slave_count;
            break;

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
                data.alias = slave->sii.alias;
                data.state = slave->current_state;

                data.sync_count = slave->sii.sync_count;

                if (slave->sii.name) {
                    strncpy(data.name, slave->sii.name,
                            EC_IOCTL_SLAVE_NAME_SIZE);
                    data.name[EC_IOCTL_SLAVE_NAME_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }

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
                data.default_size = sync->length;
                data.control_register = sync->control_register;
                data.enable = sync->enable;
                data.assign_source = sync->assign_source;
                data.pdo_count = ec_pdo_list_count(&sync->pdos);

                if (copy_to_user((void __user *) arg, &data, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
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

                data.dir = pdo->dir;
                data.index = pdo->index;
                data.entry_count = ec_pdo_entry_count(pdo);

                if (pdo->name) {
                    strncpy(data.name, pdo->name, EC_IOCTL_PDO_NAME_SIZE);
                    data.name[EC_IOCTL_PDO_NAME_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
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
                    strncpy(data.name, entry->name,
                            EC_IOCTL_PDO_ENTRY_NAME_SIZE);
                    data.name[EC_IOCTL_PDO_ENTRY_NAME_SIZE - 1] = 0;
                } else {
                    data.name[0] = 0;
                }

                if (copy_to_user((void __user *) arg, &data, sizeof(data))) {
                    retval = -EFAULT;
                    break;
                }
                break;
            }

        default:
            retval = -ENOIOCTLCMD;
    }

    return retval;
}

/*****************************************************************************/
