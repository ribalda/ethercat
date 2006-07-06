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
   Global definitions and macros.
*/

/*****************************************************************************/

#ifndef _EC_GLOBALS_
#define _EC_GLOBALS_

#include <linux/types.h>

/******************************************************************************
 *  EtherCAT master
 *****************************************************************************/

/** master main version */
#define EC_MASTER_VERSION_MAIN  1

/** master sub version (after the dot) */
#define EC_MASTER_VERSION_SUB   0

/** master extra version (just a string) */
#define EC_MASTER_VERSION_EXTRA "trunk"

/** maximum number of FMMUs per slave */
#define EC_MAX_FMMUS 16

/** size of the EoE tx queue */
#define EC_EOE_TX_QUEUE_SIZE 100

/** clock frequency for the EoE state machines */
#define EC_EOE_FREQUENCY 1000

/******************************************************************************
 *  EtherCAT protocol
 *****************************************************************************/

/** size of an EtherCAT frame header */
#define EC_FRAME_HEADER_SIZE 2

/** size of an EtherCAT datagram header */
#define EC_DATAGRAM_HEADER_SIZE 10

/** size of an EtherCAT datagram footer */
#define EC_DATAGRAM_FOOTER_SIZE 2

/** size of a sync manager configuration page */
#define EC_SYNC_SIZE 8

/** size of an FMMU configuration page */
#define EC_FMMU_SIZE 16

/** resulting maximum data size of a single datagram in a frame */
#define EC_MAX_DATA_SIZE (ETH_DATA_LEN - EC_FRAME_HEADER_SIZE \
                          - EC_DATAGRAM_HEADER_SIZE - EC_DATAGRAM_FOOTER_SIZE)

/*****************************************************************************/

/**
   Convenience macro for printing EtherCAT-specific information to syslog.
   This will print the message in \a fmt with a prefixed "EtherCAT: ".
   \param fmt format string (like in printf())
   \param args arguments (optional)
*/

#define EC_INFO(fmt, args...) \
    printk(KERN_INFO "EtherCAT: " fmt, ##args)

/**
   Convenience macro for printing EtherCAT-specific errors to syslog.
   This will print the message in \a fmt with a prefixed "EtherCAT ERROR: ".
   \param fmt format string (like in printf())
   \param args arguments (optional)
*/

#define EC_ERR(fmt, args...) \
    printk(KERN_ERR "EtherCAT ERROR: " fmt, ##args)

/**
   Convenience macro for printing EtherCAT-specific warnings to syslog.
   This will print the message in \a fmt with a prefixed "EtherCAT WARNING: ".
   \param fmt format string (like in printf())
   \param args arguments (optional)
*/

#define EC_WARN(fmt, args...) \
    printk(KERN_WARNING "EtherCAT WARNING: " fmt, ##args)

/**
   Convenience macro for printing EtherCAT debug messages to syslog.
   This will print the message in \a fmt with a prefixed "EtherCAT DEBUG: ".
   \param fmt format string (like in printf())
   \param args arguments (optional)
*/

#define EC_DBG(fmt, args...) \
    printk(KERN_DEBUG "EtherCAT DEBUG: " fmt, ##args)

/**
   Helper macro for EC_STR(), literates a macro argument.
   \param X argument to literate.
*/

#define EC_LIT(X) #X

/**
   Converts a macro argument to a string.
   \param X argument to stringify.
*/

#define EC_STR(X) EC_LIT(X)

/**
   Convenience macro for defining read-only SysFS attributes.
   This results in creating a static variable called attr_\a NAME. The SysFS
   file will be world-readable.
   \param NAME name of the attribute to create.
*/

#define EC_SYSFS_READ_ATTR(NAME) \
    static struct attribute attr_##NAME = { \
        .name = EC_STR(NAME), .owner = THIS_MODULE, .mode = S_IRUGO \
    }

/**
   Convenience macro for defining read-write SysFS attributes.
   This results in creating a static variable called attr_\a NAME. The SysFS
   file will be word-readable plus owner-writable.
   \param NAME name of the attribute to create.
*/

#define EC_SYSFS_READ_WRITE_ATTR(NAME) \
    static struct attribute attr_##NAME = { \
        .name = EC_STR(NAME), .owner = THIS_MODULE, .mode = S_IRUGO | S_IWUSR \
    }

/*****************************************************************************/

extern void ec_print_data(const uint8_t *, size_t);
extern void ec_print_data_diff(const uint8_t *, const uint8_t *, size_t);
extern void ec_print_states(uint8_t);

/*****************************************************************************/

/**
   Code - Message pair.
   Some EtherCAT datagrams support reading a status code to display a certain
   message. This type allows to map a code to a message string.
*/

typedef struct
{
    uint32_t code; /**< code */
    const char *message; /**< message belonging to \a code */
}
ec_code_msg_t;

/*****************************************************************************/

#endif

