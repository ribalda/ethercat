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
   CANopen-over-EtherCAT Sdo entry functions.
*/

/*****************************************************************************/

#include <linux/module.h>

#include "sdo.h"
#include "sdo_request.h"
#include "master.h"

#include "sdo_entry.h"

/*****************************************************************************/

ssize_t ec_show_sdo_entry_attribute(struct kobject *, struct attribute *,
                                    char *);
void ec_sdo_entry_clear(struct kobject *);

/*****************************************************************************/

/** \cond */

EC_SYSFS_READ_ATTR(info);
EC_SYSFS_READ_ATTR(value);

static struct attribute *sdo_entry_def_attrs[] = {
    &attr_info,
    &attr_value,
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

/** Sdo entry constructor.
 *
 * \todo Turn parameters.
 */
int ec_sdo_entry_init(
        ec_sdo_entry_t *entry, /**< Sdo entry. */
        uint8_t subindex, /**< Subindex. */
        ec_sdo_t *sdo /**< Parent Sdo. */
        )
{
    entry->sdo = sdo;
    entry->subindex = subindex;
    entry->data_type = 0x0000;
    entry->bit_length = 0;
    entry->description = NULL;

    // Init kobject and add it to the hierarchy
    memset(&entry->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&entry->kobj);
    entry->kobj.ktype = &ktype_ec_sdo_entry;
    entry->kobj.parent = &sdo->kobj;
    if (kobject_set_name(&entry->kobj, "%02X", entry->subindex)) {
        EC_ERR("Failed to set kobj name.\n");
        kobject_put(&entry->kobj);
        return -1;
    }
    if (kobject_add(&entry->kobj)) {
        EC_ERR("Failed to add entry kobject.\n");
        kobject_put(&entry->kobj);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/** Sdo entry destructor.
 *
 * Clears and frees an Sdo entry object.
 */
void ec_sdo_entry_destroy(
        ec_sdo_entry_t *entry /**< Sdo entry. */
        )
{
    // destroy self
    kobject_del(&entry->kobj);
    kobject_put(&entry->kobj);
}

/*****************************************************************************/

/** Clear and free the Sdo entry.
 *
 * This method is called by the kobject,
 * once there are no more references to it.
 */
void ec_sdo_entry_clear(
        struct kobject *kobj /**< Sdo entry's kobject. */
        )
{
    ec_sdo_entry_t *entry = container_of(kobj, ec_sdo_entry_t, kobj);

    if (entry->description) kfree(entry->description);

    kfree(entry);
}

/*****************************************************************************/
 
/** Print Sdo entry information to a buffer.
 * 
 * \return Number of bytes written.
 */
ssize_t ec_sdo_entry_info(
        ec_sdo_entry_t *entry, /**< Sdo entry. */
        char *buffer /**< Target buffer. */
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

/** Format entry data based on the CANopen data type and print it to a buffer.
 *
 * \return number of bytes written.
 */
ssize_t ec_sdo_entry_format_data(
        ec_sdo_entry_t *entry, /**< Sdo entry. */
        ec_sdo_request_t *request, /**< Sdo request. */
        char *buffer /**< Target buffer. */
        )
{
    off_t off = 0;
    unsigned int i;

    if (request->data_size * 8 != entry->bit_length) {
        EC_ERR("Dictionary size of Sdo entry 0x%04X:%02X (%u bit) does not "
                "match size of uploaded data (%u byte)!\n", entry->sdo->index,
                entry->subindex, entry->bit_length, request->data_size);
        EC_DBG("Uploaded data:\n");
        ec_print_data(request->data, request->data_size);
        return -EIO;
    }
        
    if (entry->data_type == 0x0002) { // int8
        int8_t value;
        if (entry->bit_length != 8)
            goto not_fit;
        value = EC_READ_S8(request->data);
        off += sprintf(buffer + off, "%i (0x%02X)\n", value, value);
    }
    else if (entry->data_type == 0x0003) { // int16
        int16_t value;
        if (entry->bit_length != 16)
            goto not_fit;
        value = EC_READ_S16(request->data);
        off += sprintf(buffer + off, "%i (0x%04X)\n", value, value);
    }
    else if (entry->data_type == 0x0004) { // int32
        int32_t value;
        if (entry->bit_length != 32)
            goto not_fit;
        value = EC_READ_S16(request->data);
        off += sprintf(buffer + off, "%i (0x%08X)\n", value, value);
    }
    else if (entry->data_type == 0x0005) { // uint8
        uint8_t value;
        if (entry->bit_length != 8)
            goto not_fit;
        value = EC_READ_U8(request->data);
        off += sprintf(buffer + off, "%u (0x%02X)\n", value, value);
    }
    else if (entry->data_type == 0x0006) { // uint16
        uint16_t value;
        if (entry->bit_length != 16)
            goto not_fit;
        value = EC_READ_U16(request->data); 
        off += sprintf(buffer + off, "%u (0x%04X)\n", value, value);
    }
    else if (entry->data_type == 0x0007) { // uint32
        uint32_t value;
        if (entry->bit_length != 32)
            goto not_fit;
        value = EC_READ_U32(request->data);
        off += sprintf(buffer + off, "%i (0x%08X)\n", value, value);
    }
    else if (entry->data_type == 0x0009) { // string
        off += sprintf(buffer + off, "%s\n", request->data);
    }
    else {
        off += sprintf(buffer + off, "Unknown data type %04X. Data:\n",
                entry->data_type);
        goto raw_data;
    }
    return off;

not_fit:
    off += sprintf(buffer + off,
            "Invalid bit length %u for data type 0x%04X. Data:\n",
            entry->bit_length, entry->data_type);
raw_data:
    for (i = 0; i < request->data_size; i++)
        off += sprintf(buffer + off, "%02X (%c)\n",
                request->data[i], request->data[i]);
    return off;
}

/*****************************************************************************/

/** Start blocking Sdo entry reading.
 *
 * This function blocks, until reading is finished, and is interruptible as
 * long as the master state machine has not begun with reading.
 *
 * \return number of bytes written to buffer, or error code.
 */
ssize_t ec_sdo_entry_read_value(
        ec_sdo_entry_t *entry, /**< Sdo entry. */
        char *buffer /**< Target buffer. */
        )
{
    ec_master_t *master = entry->sdo->slave->master;
    off_t off = 0;
    ec_master_sdo_request_t request;

    request.slave = entry->sdo->slave;
    ec_sdo_request_init(&request.req);
    ec_sdo_request_address(&request.req, entry->sdo->index, entry->subindex);
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
            return -EINTR;
        }
        // request already processing: interrupt not possible.
        up(&master->sdo_sem);
    }

    // wait until master FSM has finished processing
    wait_event(master->sdo_queue, request.req.state != EC_REQUEST_BUSY);

    if (request.req.state != EC_REQUEST_SUCCESS)
        return -EIO;

    off += ec_sdo_entry_format_data(entry, &request.req, buffer);

    ec_sdo_request_clear(&request.req);
    return off;
}

/*****************************************************************************/

/** Show the Sysfs attribute of an Sdo entry.
 *
 * /return Number of bytes written to buffer.
 */ 
ssize_t ec_show_sdo_entry_attribute(
        struct kobject *kobj, /**< kobject. */
        struct attribute *attr, /**< Sysfs attribute. */
        char *buffer /**< Target buffer. */
        )
{
    ec_sdo_entry_t *entry = container_of(kobj, ec_sdo_entry_t, kobj);

    if (attr == &attr_info) {
        return ec_sdo_entry_info(entry, buffer);
    }
    else if (attr == &attr_value) {
        return ec_sdo_entry_read_value(entry, buffer);
    }

    return 0;
}

/*****************************************************************************/
