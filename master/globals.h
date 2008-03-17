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

/** \file
 * Global definitions and macros.
 */

/*****************************************************************************/

#ifndef _EC_MASTER_GLOBALS_
#define _EC_MASTER_GLOBALS_

#include <linux/types.h>

#include "../globals.h"

/******************************************************************************
 * EtherCAT master
 *****************************************************************************/

/** Clock frequency for the EoE state machines. */
#define EC_EOE_FREQUENCY 1000

/** Datagram timeout in microseconds. */
#define EC_IO_TIMEOUT 500

/** Number of state machine retries on datagram timeout. */
#define EC_FSM_RETRIES 3

/** Seconds to wait before fetching Sdo dictionary
    after slave entered PREOP state. */
#define EC_WAIT_SDO_DICT 3

/** Minimum size of a buffer used with ec_state_string(). */
#define EC_STATE_STRING_SIZE 32

/** Maximum SII size in words, to avoid infinite reading. */
#define EC_MAX_SII_SIZE 1024

/******************************************************************************
 * EtherCAT protocol
 *****************************************************************************/

/** Size of an EtherCAT frame header. */
#define EC_FRAME_HEADER_SIZE 2

/** Size of an EtherCAT datagram header. */
#define EC_DATAGRAM_HEADER_SIZE 10

/** Size of an EtherCAT datagram footer. */
#define EC_DATAGRAM_FOOTER_SIZE 2

/** Size of the EtherCAT address field. */
#define EC_ADDR_LEN 4

/** Resulting maximum data size of a single datagram in a frame. */
#define EC_MAX_DATA_SIZE (ETH_DATA_LEN - EC_FRAME_HEADER_SIZE \
                          - EC_DATAGRAM_HEADER_SIZE - EC_DATAGRAM_FOOTER_SIZE)

/** Word offset of first SII category. */
#define EC_FIRST_SII_CATEGORY_OFFSET 0x40

/** Size of a sync manager configuration page. */
#define EC_SYNC_PAGE_SIZE 8

/** Maximum number of FMMUs per slave. */
#define EC_MAX_FMMUS 16

/** Size of an FMMU configuration page. */
#define EC_FMMU_PAGE_SIZE 16

/*****************************************************************************/

/** Convenience macro for printing EtherCAT-specific information to syslog.
 *
 * This will print the message in \a fmt with a prefixed "EtherCAT: ".
 *
 * \param fmt format string (like in printf())
 * \param args arguments (optional)
 */
#define EC_INFO(fmt, args...) \
    printk(KERN_INFO "EtherCAT: " fmt, ##args)

/** Convenience macro for printing EtherCAT-specific errors to syslog.
 *
 * This will print the message in \a fmt with a prefixed "EtherCAT ERROR: ".
 *
 * \param fmt format string (like in printf())
 * \param args arguments (optional)
 */
#define EC_ERR(fmt, args...) \
    printk(KERN_ERR "EtherCAT ERROR: " fmt, ##args)

/** Convenience macro for printing EtherCAT-specific warnings to syslog.
 *
 * This will print the message in \a fmt with a prefixed "EtherCAT WARNING: ".
 *
 * \param fmt format string (like in printf())
 * \param args arguments (optional)
 */
#define EC_WARN(fmt, args...) \
    printk(KERN_WARNING "EtherCAT WARNING: " fmt, ##args)

/** Convenience macro for printing EtherCAT debug messages to syslog.
 *
 * This will print the message in \a fmt with a prefixed "EtherCAT DEBUG: ".
 *
 * \param fmt format string (like in printf())
 * \param args arguments (optional)
 */
#define EC_DBG(fmt, args...) \
    printk(KERN_DEBUG "EtherCAT DEBUG: " fmt, ##args)

/** Convenience macro for defining read-only SysFS attributes.
 *
 * This results in creating a static variable called attr_\a NAME. The SysFS
 * file will be world-readable.
 *
 * \param NAME name of the attribute to create.
 */
#define EC_SYSFS_READ_ATTR(NAME) \
    static struct attribute attr_##NAME = { \
        .name = EC_STR(NAME), .owner = THIS_MODULE, .mode = S_IRUGO \
    }

/** Convenience macro for defining read-write SysFS attributes.
 *
 * This results in creating a static variable called attr_\a NAME. The SysFS
 * file will be word-readable plus owner-writable.
 *
 * \param NAME name of the attribute to create.
 */
#define EC_SYSFS_READ_WRITE_ATTR(NAME) \
    static struct attribute attr_##NAME = { \
        .name = EC_STR(NAME), .owner = THIS_MODULE, .mode = S_IRUGO | S_IWUSR \
    }

/*****************************************************************************/

extern char *ec_master_version_str;

/*****************************************************************************/

void ec_print_data(const uint8_t *, size_t);
void ec_print_data_diff(const uint8_t *, const uint8_t *, size_t);
size_t ec_state_string(uint8_t, char *);
ssize_t ec_mac_print(const uint8_t *, char *);
int ec_mac_is_zero(const uint8_t *);

/*****************************************************************************/

/** Code/Message pair.
 *
 * Some EtherCAT datagrams support reading a status code to display a certain
 * message. This type allows to map a code to a message string.
 */
typedef struct {
    uint32_t code; /**< Code. */
    const char *message; /**< Message belonging to \a code. */
} ec_code_msg_t;

/*****************************************************************************/

/** Generic request state.
 *
 * \attention If ever changing this, please be sure to adjust the \a
 * state_table in master/sdo_request.c.
 */
typedef enum {
    EC_REQUEST_INIT,
    EC_REQUEST_QUEUED,
    EC_REQUEST_BUSY,
    EC_REQUEST_SUCCESS,
    EC_REQUEST_FAILURE
} ec_request_state_t;

/*****************************************************************************/

/** Origin type.
 */
typedef enum {
    EC_ORIG_INTERNAL, /**< Internal. */
    EC_ORIG_EXTERNAL /**< External. */
} ec_origin_t;

/*****************************************************************************/

typedef struct ec_slave ec_slave_t; /**< \see ec_slave. */

/*****************************************************************************/

#endif
