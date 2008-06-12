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
   Methods of an EtherCAT datagram.
*/

/*****************************************************************************/

#include <linux/slab.h>

#include "datagram.h"
#include "master.h"

/*****************************************************************************/

/** \cond */

#define EC_FUNC_HEADER \
    if (unlikely(ec_datagram_prealloc(datagram, data_size))) \
        return -1; \
    datagram->index = 0; \
    datagram->working_counter = 0; \
    datagram->state = EC_DATAGRAM_INIT;

#define EC_FUNC_FOOTER \
    datagram->data_size = data_size; \
    memset(datagram->data, 0x00, data_size); \
    return 0;

/** \endcond */

/*****************************************************************************/

/** Array of datagram type strings used in ec_datagram_type_string().
 *
 * \attention This is indexed by ec_datagram_type_t.
 */
static const char *type_strings[] = {
    "?",
    "APRD",
    "APWR",
    "APRW",
    "FPRD",
    "FPWR",
    "FPRW",
    "BRD",
    "BWR",
    "BRW",
    "LRD",
    "LWR",
    "LRW",
    "ARMW",
    "FRMW"
};
    
/*****************************************************************************/

/** Constructor.
 */
void ec_datagram_init(ec_datagram_t *datagram /**< EtherCAT datagram. */)
{
    INIT_LIST_HEAD(&datagram->queue); // mark as unqueued
    datagram->type = EC_DATAGRAM_NONE;
    memset(datagram->address, 0x00, EC_ADDR_LEN);
    datagram->data = NULL;
    datagram->data_origin = EC_ORIG_INTERNAL;
    datagram->mem_size = 0;
    datagram->data_size = 0;
    datagram->index = 0x00;
    datagram->working_counter = 0x0000;
    datagram->state = EC_DATAGRAM_INIT;
    datagram->cycles_sent = 0;
    datagram->jiffies_sent = 0;
    datagram->cycles_received = 0;
    datagram->jiffies_received = 0;
    datagram->skip_count = 0;
    datagram->stats_output_jiffies = 0;
    memset(datagram->name, 0x00, EC_DATAGRAM_NAME_SIZE);
}

/*****************************************************************************/

/** Destructor.
 */
void ec_datagram_clear(ec_datagram_t *datagram /**< EtherCAT datagram. */)
{
    if (datagram->data_origin == EC_ORIG_INTERNAL && datagram->data)
        kfree(datagram->data);
}

/*****************************************************************************/

/** Allocates internal payload memory.
 *
 * If the allocated memory is already larger than requested, nothing ist done.
 *
 * \attention If external payload memory has been provided, no range checking
 *            is done!
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_prealloc(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        size_t size /**< New payload size in bytes. */
        )
{
    if (datagram->data_origin == EC_ORIG_EXTERNAL
            || size <= datagram->mem_size)
        return 0;

    if (datagram->data) {
        kfree(datagram->data);
        datagram->data = NULL;
        datagram->mem_size = 0;
    }

    if (!(datagram->data = kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %u bytes of datagram memory!\n", size);
        return -1;
    }

    datagram->mem_size = size;
    return 0;
}

/*****************************************************************************/

/** Initializes an EtherCAT APRD datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_aprd(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t ring_position, /**< Auto-increment address. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to read. */
        )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_APRD;
    EC_WRITE_S16(datagram->address, (int16_t) ring_position * (-1));
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT APWR datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_apwr(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t ring_position, /**< Auto-increment address. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to write. */
        )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_APWR;
    EC_WRITE_S16(datagram->address, (int16_t) ring_position * (-1));
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT APRW datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_aprw(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t ring_position, /**< Auto-increment address. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to write. */
        )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_APRW;
    EC_WRITE_S16(datagram->address, (int16_t) ring_position * (-1));
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT ARMW datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_armw(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t ring_position, /**< Auto-increment address. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to read. */
        )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_ARMW;
    EC_WRITE_S16(datagram->address, (int16_t) ring_position * (-1));
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT FPRD datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_fprd(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t configured_address, /**< Configured station address. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to read. */
        )
{
    if (unlikely(configured_address == 0x0000))
        EC_WARN("Using configured station address 0x0000!\n");

    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_FPRD;
    EC_WRITE_U16(datagram->address, configured_address);
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT FPWR datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_fpwr(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t configured_address, /**< Configured station address. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to write. */
        )
{
    if (unlikely(configured_address == 0x0000))
        EC_WARN("Using configured station address 0x0000!\n");

    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_FPWR;
    EC_WRITE_U16(datagram->address, configured_address);
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT FPRW datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_fprw(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t configured_address, /**< Configured station address. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to write. */
        )
{
    if (unlikely(configured_address == 0x0000))
        EC_WARN("Using configured station address 0x0000!\n");

    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_FPRW;
    EC_WRITE_U16(datagram->address, configured_address);
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT FRMW datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_frmw(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t configured_address, /**< Configured station address. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to write. */
        )
{
    if (unlikely(configured_address == 0x0000))
        EC_WARN("Using configured station address 0x0000!\n");

    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_FRMW;
    EC_WRITE_U16(datagram->address, configured_address);
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT BRD datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_brd(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to read. */
        )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_BRD;
    EC_WRITE_U16(datagram->address, 0x0000);
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT BWR datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_bwr(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to write. */
        )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_BWR;
    EC_WRITE_U16(datagram->address, 0x0000);
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT BRW datagram.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_brw(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint16_t mem_address, /**< Physical memory address. */
        size_t data_size /**< Number of bytes to write. */
        )
{
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_BRW;
    EC_WRITE_U16(datagram->address, 0x0000);
    EC_WRITE_U16(datagram->address + 2, mem_address);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT LRD datagram.
 *
 * \attention It is assumed, that the external memory is at least \a data_size
 *            bytes large.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_lrd(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint32_t offset, /**< Logical address. */
        size_t data_size, /**< Number of bytes to read/write. */
        uint8_t *external_memory /**< Pointer to the memory to use. */
        )
{
    datagram->data = external_memory;
    datagram->data_origin = EC_ORIG_EXTERNAL;
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_LRD;
    EC_WRITE_U32(datagram->address, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT LWR datagram.
 *
 * \attention It is assumed, that the external memory is at least \a data_size
 *            bytes large.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_lwr(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint32_t offset, /**< Logical address. */
        size_t data_size, /**< Number of bytes to read/write. */
        uint8_t *external_memory /**< Pointer to the memory to use. */
        )
{
    datagram->data = external_memory;
    datagram->data_origin = EC_ORIG_EXTERNAL;
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_LWR;
    EC_WRITE_U32(datagram->address, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Initializes an EtherCAT LRW datagram.
 *
 * \attention It is assumed, that the external memory is at least \a data_size
 *            bytes large.
 *
 * \return 0 in case of success, else < 0
 */
int ec_datagram_lrw(
        ec_datagram_t *datagram, /**< EtherCAT datagram. */
        uint32_t offset, /**< Logical address. */
        size_t data_size, /**< Number of bytes to read/write. */
        uint8_t *external_memory /**< Pointer to the memory to use. */
        )
{
    datagram->data = external_memory;
    datagram->data_origin = EC_ORIG_EXTERNAL;
    EC_FUNC_HEADER;
    datagram->type = EC_DATAGRAM_LRW;
    EC_WRITE_U32(datagram->address, offset);
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/** Evaluates the working counter of a single-cast datagram.
 *
 * Outputs an error message.
 */
void ec_datagram_print_wc_error(
        const ec_datagram_t *datagram /**< EtherCAT datagram */
        )
{
    if (datagram->working_counter == 0)
        printk("No response.");
    else if (datagram->working_counter > 1)
        printk("%u slaves responded!", datagram->working_counter);
    else
        printk("Success.");
    printk("\n");
}

/*****************************************************************************/

/** Outputs datagram statistics at most every second.
 */
void ec_datagram_output_stats(
        ec_datagram_t *datagram
        )
{
    if (jiffies - datagram->stats_output_jiffies < HZ) {
        datagram->stats_output_jiffies = jiffies;
    
        if (unlikely(datagram->skip_count)) {
            EC_WARN("Datagram %x (%s) was SKIPPED %u time%s.\n",
                    (unsigned int) datagram, datagram->name,
                    datagram->skip_count,
                    datagram->skip_count == 1 ? "" : "s");
            datagram->skip_count = 0;
        }
    }
}

/*****************************************************************************/

/** Returns a string describing the datagram type.
 */
const char *ec_datagram_type_string(
        const ec_datagram_t *datagram /**< EtherCAT datagram. */
        )
{
    return type_strings[datagram->type];
}

/*****************************************************************************/
