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
   Canopen-over-EtherCAT functions.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "canopen.h"

/*****************************************************************************/

ssize_t ec_show_sdo_attribute(struct kobject *, struct attribute *, char *);
ssize_t ec_show_sdo_entry_attribute(struct kobject *, struct attribute *,
                                    char *);
void ec_sdo_clear(struct kobject *);
void ec_sdo_entry_clear(struct kobject *);

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

static struct attribute *sdo_entry_def_attrs[] = {
    &attr_info,
    NULL,
};

static struct sysfs_ops sdo_entry_sysfs_ops = {
    .show = &ec_show_sdo_entry_attribute,
    .store = NULL
};

static struct kobj_type ktype_ec_sdo_entry = {
    .release = ec_sdo_entry_clear,
    .sysfs_ops = &sdo_entry_sysfs_ops,
    .default_attrs = sdo_entry_def_attrs
};

/** \endcond */

/*****************************************************************************/

/**
   SDO constructor.
*/

int ec_sdo_init(ec_sdo_t *sdo, /**< SDO */
                uint16_t index, /**< SDO index */
                ec_slave_t *slave /**< parent slave */
                )
{
    sdo->index = index;
    sdo->object_code = 0x00;
    sdo->name = NULL;
    sdo->subindices = 0;
    INIT_LIST_HEAD(&sdo->entries);

    // init kobject and add it to the hierarchy
    memset(&sdo->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&sdo->kobj);
    sdo->kobj.ktype = &ktype_ec_sdo;
    sdo->kobj.parent = &slave->sdo_kobj;
    if (kobject_set_name(&sdo->kobj, "%4X", sdo->index)) {
        EC_ERR("Failed to set kobj name.\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   SDO destructor.
*/

void ec_sdo_clear(struct kobject *kobj /**< SDO's kobject */)
{
    ec_sdo_t *sdo = container_of(kobj, ec_sdo_t, kobj);
    ec_sdo_entry_t *entry, *next;

    // free all entries
    list_for_each_entry_safe(entry, next, &sdo->entries, list) {
        list_del(&entry->list);
        kobject_del(&entry->kobj);
        kobject_put(&entry->kobj);
    }

    if (sdo->name) kfree(sdo->name);

    kfree(sdo);
}

/*****************************************************************************/

ssize_t ec_sdo_info(ec_sdo_t *sdo, /**< SDO */
                    char *buffer /**< target buffer */
                    )
{
    off_t off = 0;

    off += sprintf(buffer + off, "Index: 0x%04X\n", sdo->index);
    off += sprintf(buffer + off, "Name: %s\n", sdo->name ? sdo->name : "");
    off += sprintf(buffer + off, "Subindices: %i\n", sdo->subindices);

    return off;
}

/*****************************************************************************/

ssize_t ec_show_sdo_attribute(struct kobject *kobj, /**< kobject */
                              struct attribute *attr,
                              char *buffer
                              )
{
    ec_sdo_t *sdo = container_of(kobj, ec_sdo_t, kobj);

    if (attr == &attr_info) {
        return ec_sdo_info(sdo, buffer);
    }

    return 0;
}

/*****************************************************************************/

/**
   SDO entry constructor.
*/

int ec_sdo_entry_init(ec_sdo_entry_t *entry, /**< SDO entry */
                      uint8_t subindex, /**< SDO entry subindex */
                      ec_sdo_t *sdo /**< parent SDO */
                      )
{
    entry->subindex = subindex;
    entry->data_type = 0x0000;
    entry->bit_length = 0;
    entry->description = NULL;

    // init kobject and add it to the hierarchy
    memset(&entry->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&entry->kobj);
    entry->kobj.ktype = &ktype_ec_sdo_entry;
    entry->kobj.parent = &sdo->kobj;
    if (kobject_set_name(&entry->kobj, "%i", entry->subindex)) {
        EC_ERR("Failed to set kobj name.\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   SDO destructor.
*/

void ec_sdo_entry_clear(struct kobject *kobj /**< SDO entry's kobject */)
{
    ec_sdo_entry_t *entry = container_of(kobj, ec_sdo_entry_t, kobj);

    if (entry->description) kfree(entry->description);

    kfree(entry);
}

/*****************************************************************************/

ssize_t ec_sdo_entry_info(ec_sdo_entry_t *entry, /**< SDO entry */
                          char *buffer /**< target buffer */
                          )
{
    off_t off = 0;

    off += sprintf(buffer + off, "Subindex: 0x%02X\n", entry->subindex);
    off += sprintf(buffer + off, "Description: %s\n",
                   entry->description ? entry->description : "");
    off += sprintf(buffer + off, "Data type: 0x%04X\n", entry->data_type);
    off += sprintf(buffer + off, "Bit length: %i\n", entry->bit_length);

    return off;
}

/*****************************************************************************/

ssize_t ec_show_sdo_entry_attribute(struct kobject *kobj, /**< kobject */
                                    struct attribute *attr,
                                    char *buffer
                                    )
{
    ec_sdo_entry_t *entry = container_of(kobj, ec_sdo_entry_t, kobj);

    if (attr == &attr_info) {
        return ec_sdo_entry_info(entry, buffer);
    }

    return 0;
}

/*****************************************************************************/

#if 0
int ecrt_slave_sdo_read(ec_slave_t *slave, /**< EtherCAT slave */
                        uint16_t sdo_index, /**< SDO index */
                        uint8_t sdo_subindex, /**< SDO subindex */
                        uint8_t *target, /**< memory for value */
                        size_t *size /**< target memory size */
                        )
{
    uint8_t *data;
    size_t rec_size, data_size;
    uint32_t complete_size;
    ec_datagram_t datagram;

    ec_datagram_init(&datagram);

    if (!(data = ec_slave_mbox_prepare_send(slave, &datagram, 0x03, 6)))
        goto err;

    EC_WRITE_U16(data, 0x2 << 12); // SDO request
    EC_WRITE_U8 (data + 2, 0x2 << 5); // initiate upload request
    EC_WRITE_U16(data + 3, sdo_index);
    EC_WRITE_U8 (data + 5, sdo_subindex);

    if (!(data = ec_slave_mbox_simple_io(slave, &datagram, &rec_size)))
        goto err;

    if (EC_READ_U16(data) >> 12 == 0x2 && // SDO request
        EC_READ_U8 (data + 2) >> 5 == 0x4) { // abort SDO transfer request
        EC_ERR("SDO upload 0x%04X:%X aborted on slave %i.\n",
               sdo_index, sdo_subindex, slave->ring_position);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        goto err;
    }

    if (EC_READ_U16(data) >> 12 != 0x3 || // SDO response
        EC_READ_U8 (data + 2) >> 5 != 0x2 || // initiate upload response
        EC_READ_U16(data + 3) != sdo_index || // index
        EC_READ_U8 (data + 5) != sdo_subindex) { // subindex
        EC_ERR("SDO upload 0x%04X:%X failed:\n", sdo_index, sdo_subindex);
        EC_ERR("Invalid SDO upload response at slave %i!\n",
               slave->ring_position);
        ec_print_data(data, rec_size);
        goto err;
    }

    if (rec_size < 10) {
        EC_ERR("Received currupted SDO upload response!\n");
        ec_print_data(data, rec_size);
        goto err;
    }

    if ((complete_size = EC_READ_U32(data + 6)) > *size) {
        EC_ERR("SDO data does not fit into buffer (%i / %i)!\n",
               complete_size, *size);
        goto err;
    }

    data_size = rec_size - 10;

    if (data_size != complete_size) {
        EC_ERR("SDO data incomplete - Fragmenting not implemented.\n");
        goto err;
    }

    memcpy(target, data + 10, data_size);

    ec_datagram_clear(&datagram);
    return 0;
 err:
    ec_datagram_clear(&datagram);
    return -1;
}
#endif

/*****************************************************************************/
