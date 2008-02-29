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
   CANopen Sdo functions.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "master.h"

#include "sdo.h"

/*****************************************************************************/

ssize_t ec_show_sdo_attribute(struct kobject *, struct attribute *, char *);
void ec_sdo_clear(struct kobject *);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);

static struct attribute *sdo_def_attrs[] = {
    &attr_info,
    NULL,
};

static struct sysfs_ops sdo_sysfs_ops = {
    .show = &ec_show_sdo_attribute,
    .store = NULL
};

static struct kobj_type ktype_ec_sdo = {
    .release = ec_sdo_clear,
    .sysfs_ops = &sdo_sysfs_ops,
    .default_attrs = sdo_def_attrs
};

/** \endcond */

/*****************************************************************************/

/** Sdo constructor.
 *
 * \todo Turn parameters.
 */
int ec_sdo_init(
        ec_sdo_t *sdo, /**< Sdo. */
        uint16_t index, /**< Sdo index. */
        ec_slave_t *slave /**< Parent slave. */
        )
{
    sdo->slave = slave;
    sdo->index = index;
    sdo->object_code = 0x00;
    sdo->name = NULL;
    sdo->subindices = 0;
    INIT_LIST_HEAD(&sdo->entries);

    // Init kobject and add it to the hierarchy
    memset(&sdo->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&sdo->kobj);
    sdo->kobj.ktype = &ktype_ec_sdo;
    sdo->kobj.parent = &slave->sdo_kobj;
    if (kobject_set_name(&sdo->kobj, "%4X", sdo->index)) {
        EC_ERR("Failed to set kobj name.\n");
        kobject_put(&sdo->kobj);
        return -1;
    }
    if (kobject_add(&sdo->kobj)) {
        EC_ERR("Failed to add Sdo kobject.\n");
        kobject_put(&sdo->kobj);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/** Sdo destructor.
 *
 * Clears and frees an Sdo object.
 */
void ec_sdo_destroy(
        ec_sdo_t *sdo /**< Sdo. */
        )
{
    ec_sdo_entry_t *entry, *next;

    // free all entries
    list_for_each_entry_safe(entry, next, &sdo->entries, list) {
        list_del(&entry->list);
        ec_sdo_entry_destroy(entry);
    }

    // destroy self
    kobject_del(&sdo->kobj);
    kobject_put(&sdo->kobj);
}

/*****************************************************************************/

/** Clear and free Sdo.
 *
 * This method is called by the kobject,
 * once there are no more references to it.
 */
void ec_sdo_clear(
        struct kobject *kobj /**< Sdo's kobject. */
        )
{
    ec_sdo_t *sdo = container_of(kobj, ec_sdo_t, kobj);

    if (sdo->name) kfree(sdo->name);

    kfree(sdo);
}

/*****************************************************************************/

/** Get and Sdo entry from an Sdo via its subindex.
 * 
 * \retval >0 Pointer to the requested Sdo entry.
 * \retval NULL Sdo entry not found.
 */
ec_sdo_entry_t *ec_sdo_get_entry(
        ec_sdo_t *sdo, /**< Sdo. */
        uint8_t subindex /**< Entry subindex. */
        )
{
    ec_sdo_entry_t *entry;

    list_for_each_entry(entry, &sdo->entries, list) {
        if (entry->subindex != subindex) continue;
        return entry;
    }

    return NULL;
}

/*****************************************************************************/

/** Print Sdo information to a buffer.
 * 
 * /return size of bytes written to buffer.
 */ 
ssize_t ec_sdo_info(
        ec_sdo_t *sdo, /**< Sdo. */
        char *buffer /**< Target buffer. */
        )
{
    off_t off = 0;

    off += sprintf(buffer + off, "Index: 0x%04X\n", sdo->index);
    off += sprintf(buffer + off, "Name: %s\n", sdo->name ? sdo->name : "");
    off += sprintf(buffer + off, "Subindices: %i\n", sdo->subindices);

    return off;
}

/*****************************************************************************/

/** Show a Sysfs attribute of an Sdo.
 * 
 * /return Number of bytes written to buffer.
 */ 
ssize_t ec_show_sdo_attribute(
        struct kobject *kobj, /**< kobject */
        struct attribute *attr, /**< Requested attribute. */
        char *buffer /**< Buffer to write the data in. */
        )
{
    ec_sdo_t *sdo = container_of(kobj, ec_sdo_t, kobj);

    if (attr == &attr_info) {
        return ec_sdo_info(sdo, buffer);
    }

    return 0;
}

/*****************************************************************************/
