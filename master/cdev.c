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
int eccdev_ioctl(struct inode *, struct file *, unsigned int, unsigned long);

/*****************************************************************************/

static struct file_operations eccdev_fops = {
    .owner   = THIS_MODULE,
    .open    = eccdev_open,
    .release = eccdev_release,
    .ioctl   = eccdev_ioctl
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

    filp->private_data = cdev;
    EC_DBG("File opened.\n");
    return 0;
}

/*****************************************************************************/

int eccdev_release(struct inode *inode, struct file *filp)
{
    //ec_cdev_t *cdev = container_of(inode->i_cdev, ec_cdev_t, cdev);

    EC_DBG("File closed.\n");
    return 0;
}

/*****************************************************************************/

int eccdev_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
        unsigned long arg)
{
    ec_cdev_t *cdev = container_of(inode->i_cdev, ec_cdev_t, cdev);
    ec_master_t *master = cdev->master;

    if (master->debug_level)
        EC_DBG("ioctl(inode = %x, filp = %x, cmd = %u, arg = %u)\n",
                (u32) inode, (u32) filp, (u32) cmd, (u32) arg);

    switch (cmd) {
        case EC_IOCTL_SLAVE_COUNT:
            {
                unsigned int slave_count = master->slave_count;
                EC_INFO("EC_IOCTL_SLAVE_COUNT\n");
                if (!arg)
                    return -EFAULT;
                if (copy_to_user((void __user *) arg, &slave_count,
                            sizeof(unsigned int)))
                    return -EFAULT;
                return 0;
            }

        case EC_IOCTL_SLAVE_INFO:
            {
                struct ec_ioctl_slave_info *infos, *info;
                unsigned int slave_count = master->slave_count;
                const ec_slave_t *slave;
                unsigned int i = 0;

                if (master->debug_level)
                    EC_DBG("EC_IOCTL_SLAVE_INFOS\n");

                if (!slave_count)
                    return 0;

                if (!arg)
                    return -EFAULT;

                if (!(infos = kmalloc(slave_count *
                                sizeof(struct ec_ioctl_slave_info),
                                GFP_KERNEL)))
                    return -ENOMEM;

                list_for_each_entry(slave, &master->slaves, list) {
                    info = &infos[i++];
                    info->vendor_id = slave->sii.vendor_id;
                    info->product_code = slave->sii.product_code;
                    info->alias = slave->sii.alias;
                    info->ring_position = slave->ring_position;
                    info->state = slave->current_state;
                    if (slave->sii.name) {
                        strncpy(info->description, slave->sii.name,
                                EC_IOCTL_SLAVE_INFO_DESC_SIZE);
                        info->description[EC_IOCTL_SLAVE_INFO_DESC_SIZE - 1]
                            = 0;
                    } else {
                        info->description[0] = 0;
                    }
                }

                if (copy_to_user((void __user *) arg, infos, slave_count *
                            sizeof(struct ec_ioctl_slave_info))) {
                    kfree(infos);
                    return -EFAULT;
                }

                kfree(infos);
                return 0;
            }

        default:
            return -ENOIOCTLCMD;
    }
}

/*****************************************************************************/
