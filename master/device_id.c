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
   EtherCAT device ID.
*/

/*****************************************************************************/

#include <linux/list.h>
#include <linux/netdevice.h>

#include "globals.h"
#include "device_id.h"

/*****************************************************************************/

static int ec_device_id_parse_mac(ec_device_id_t *dev_id,
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

void ec_device_id_clear_list(struct list_head *ids)
{
    ec_device_id_t *dev_id, *next_dev_id;
    
    list_for_each_entry_safe(dev_id, next_dev_id, ids, list) {
        list_del(&dev_id->list);
        kfree(dev_id);
    }
}

/*****************************************************************************/

static int ec_device_id_create_list(struct list_head *ids, const char *src)
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
            if (ec_device_id_parse_mac(dev_id, src, &rem)) {
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
        
        list_add_tail(&dev_id->list, ids); 

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
    ec_device_id_clear_list(ids);
    return -1;
}

/*****************************************************************************/

int ec_device_id_process_params(const char *main, const char *backup,
        struct list_head *main_ids, struct list_head *backup_ids)
{
    ec_device_id_t *id;
    unsigned int main_count = 0, backup_count = 0;
    
    if (ec_device_id_create_list(main_ids, main))
        return -1;

    if (ec_device_id_create_list(backup_ids, backup))
        return -1;

    // count main device IDs and check for empty ones
    list_for_each_entry(id, main_ids, list) {
        if (id->type == ec_device_id_empty) {
            EC_ERR("Main device IDs may not be empty!\n");
            return -1;
        }
        main_count++;
    }

    // count backup device IDs
    list_for_each_entry(id, backup_ids, list) {
        backup_count++;
    }

    // fill up backup device IDs
    while (backup_count < main_count) {
        if (!(id = kmalloc(sizeof(ec_device_id_t), GFP_KERNEL))) {
            EC_ERR("Out of memory!\n");
            return -1;
        }
        
        id->type = ec_device_id_empty;
        list_add_tail(&id->list, backup_ids);
        backup_count++;
    }

    return 0;
}

/*****************************************************************************/

int ec_device_id_check(const ec_device_id_t *dev_id,
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

ssize_t ec_device_id_print(const ec_device_id_t *dev_id, char *buffer)
{
    off_t off = 0;
    unsigned int i;
    
    switch (dev_id->type) {
        case ec_device_id_empty:
            off += sprintf(buffer + off, "none");
            break;
        case ec_device_id_mac:
            off += sprintf(buffer + off, "MAC ");
            for (i = 0; i < ETH_ALEN; i++) {
                off += sprintf(buffer + off, "%02X", dev_id->octets[i]);
                if (i < ETH_ALEN - 1) off += sprintf(buffer + off, ":");
            }
            break;
    }

    return off;
}
                
/*****************************************************************************/
