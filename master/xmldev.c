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
   EtherCAT XML device.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "master.h"
#include "xmldev.h"

/*****************************************************************************/

static char *test_str = "hello world!\n";

int ecxmldev_open(struct inode *, struct file *);
int ecxmldev_release(struct inode *, struct file *);
ssize_t ecxmldev_read(struct file *, char __user *, size_t, loff_t *);
ssize_t ecxmldev_write(struct file *, const char __user *, size_t, loff_t *);

/*****************************************************************************/

/** \cond */

static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = ecxmldev_open,
    .release = ecxmldev_release,
    .read    = ecxmldev_read,
    .write   = ecxmldev_write
};

/** \endcond */

/*****************************************************************************/

/**
   XML device constructor.
   \return 0 in case of success, else < 0
*/

int ec_xmldev_init(ec_xmldev_t *xmldev, /**< EtherCAT XML device */
                   ec_master_t *master, /**< EtherCAT master */
                   dev_t dev_num /**< device number */
                   )
{
    atomic_set(&xmldev->available, 1);
    cdev_init(&xmldev->cdev, &fops);
    xmldev->cdev.owner = THIS_MODULE;
    if (cdev_add(&xmldev->cdev,
		 MKDEV(MAJOR(dev_num), master->index), 1)) {
	EC_ERR("Failed to add character device!\n");
	return -1;
    }
    return 0;
}

/*****************************************************************************/

/**
   XML device destructor.
*/

void ec_xmldev_clear(ec_xmldev_t *xmldev /**< EtherCAT XML device */)
{
    cdev_del(&xmldev->cdev);
}

/*****************************************************************************/

int ec_xmldev_request(ec_xmldev_t *xmldev,
                      uint32_t vendor_id,
                      uint32_t product_code
                      )
{
    return 1;
}

/******************************************************************************
 * file_operations
 *****************************************************************************/

int ecxmldev_open(struct inode *inode, struct file *filp)
{
    ec_xmldev_t *xmldev;

    xmldev = container_of(inode->i_cdev, ec_xmldev_t, cdev);

    if (!atomic_dec_and_test(&xmldev->available)) {
        atomic_inc(&xmldev->available);
        return -EBUSY;
    }

    filp->private_data = xmldev;

    EC_DBG("File opened.\n");

    return 0;
}

/*****************************************************************************/

int ecxmldev_release(struct inode *inode, struct file *filp)
{
    ec_xmldev_t *xmldev = container_of(inode->i_cdev, ec_xmldev_t, cdev);
    atomic_inc(&xmldev->available);
    EC_DBG("File closed.\n");
    return 0;
}

/*****************************************************************************/

ssize_t ecxmldev_read(struct file *filp, char __user *buf,
		      size_t count, loff_t *f_pos)
{
    size_t len = strlen(test_str);

    if (*f_pos >= len) return 0;
    if (*f_pos + count > len) count = len - *f_pos;

    if (copy_to_user(buf, test_str + *f_pos, count)) return -EFAULT;

    *f_pos += count;

    return count;
}

/*****************************************************************************/

ssize_t ecxmldev_write(struct file *filp,
		       const char __user *buf,
		       size_t count,
		       loff_t *f_pos)
{
    return -EFAULT;
}

/*****************************************************************************/
