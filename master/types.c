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
   EtherCAT slave descriptions.
   \cond
*/

/*****************************************************************************/

#include <linux/module.h>

#include "globals.h"
#include "types.h"

/*****************************************************************************/

const ec_sync_t mailbox_sm0 = {0x1800, 246, 0x26, {NULL}};
const ec_sync_t mailbox_sm1 = {0x18F6, 246, 0x22, {NULL}};

/******************************************************************************
 *  slave objects
 *****************************************************************************/

const ec_slave_type_t Beckhoff_EK1100 = {
    "Beckhoff", "EK1100", "Bus Coupler", EC_TYPE_BUS_COUPLER,
    {NULL} // no sync managers
};

/*****************************************************************************/

const ec_slave_type_t Beckhoff_EK1110 = {
    "Beckhoff", "EK1110", "Extension terminal", EC_TYPE_NORMAL,
    {NULL} // no sync managers
};

/*****************************************************************************/

const ec_field_t bk1120_in = {"Inputs", 0}; // variable size

const ec_sync_t bk1120_sm0 = {0x1C00, 264, 0x26, {NULL}};
const ec_sync_t bk1120_sm1 = {0x1E00, 264, 0x22, {NULL}};

const ec_sync_t bk1120_sm2 = { // outputs
    0x1000, 0, 0x24, // variable size
    {NULL}
};

const ec_sync_t bk1120_sm3 = { // inputs
    0x1600, 0, 0x00, // variable size
    {&bk1120_in, NULL}
};

const ec_slave_type_t Beckhoff_BK1120 = {
    "Beckhoff", "BK1120", "KBUS Coupler", EC_TYPE_NORMAL,
    {&bk1120_sm0, &bk1120_sm1, &bk1120_sm2, &bk1120_sm3, NULL}
};

/*****************************************************************************/

const ec_field_t el1014_in = {"InputValue", 1};

const ec_sync_t el1014_sm0 = { // inputs
    0x1000, 1, 0x00,
    {&el1014_in, NULL}
};

const ec_slave_type_t Beckhoff_EL1014 = {
    "Beckhoff", "EL1014", "4x Digital Input", EC_TYPE_NORMAL,
    {&el1014_sm0, NULL}
};

/*****************************************************************************/

const ec_field_t el20XX_out = {"OutputValue", 1};

const ec_sync_t el20XX_sm0 = {
    0x0F00, 1, 0x46,
    {&el20XX_out, NULL}
};

const ec_slave_type_t Beckhoff_EL2004 = {
    "Beckhoff", "EL2004", "4x Digital Output", EC_TYPE_NORMAL,
    {&el20XX_sm0, NULL}
};

const ec_slave_type_t Beckhoff_EL2032 = {
    "Beckhoff", "EL2032", "2x Digital Output (2A)", EC_TYPE_NORMAL,
    {&el20XX_sm0, NULL}
};

/*****************************************************************************/

const ec_field_t el31X2_st1 = {"Status",     1};
const ec_field_t el31X2_ip1 = {"InputValue", 2};
const ec_field_t el31X2_st2 = {"Status",     1};
const ec_field_t el31X2_ip2 = {"InputValue", 2};

const ec_sync_t el31X2_sm2 = {0x1000, 4, 0x24, {NULL}};

const ec_sync_t el31X2_sm3 = {
    0x1100, 6, 0x20,
    {&el31X2_st1, &el31X2_ip1, &el31X2_st2, &el31X2_ip2, NULL}
};

const ec_slave_type_t Beckhoff_EL3102 = {
    "Beckhoff", "EL3102", "2x Analog Input diff.", EC_TYPE_NORMAL,
    {&mailbox_sm0, &mailbox_sm1, &el31X2_sm2, &el31X2_sm3, NULL}
};

const ec_slave_type_t Beckhoff_EL3162 = {
    "Beckhoff", "EL3162", "2x Analog Input", EC_TYPE_NORMAL,
    {&mailbox_sm0, &mailbox_sm1, &el31X2_sm2, &el31X2_sm3, NULL}
};

/*****************************************************************************/

const ec_field_t el41X2_op = {"OutputValue", 2};

const ec_sync_t el41X2_sm2 = {
    0x1000, 4, 0x24,
    {&el41X2_op, &el41X2_op, NULL}
};

const ec_slave_type_t Beckhoff_EL4102 = {
    "Beckhoff", "EL4102", "2x Analog Output", EC_TYPE_NORMAL,
    {&mailbox_sm0, &mailbox_sm1, &el41X2_sm2, NULL}
};

const ec_slave_type_t Beckhoff_EL4132 = {
    "Beckhoff", "EL4132", "2x Analog Output diff.", EC_TYPE_NORMAL,
    {&mailbox_sm0, &mailbox_sm1, &el41X2_sm2, NULL}
};

/*****************************************************************************/

const ec_field_t el5001_st = {"Status",     1};
const ec_field_t el5001_ip = {"InputValue", 4};

const ec_sync_t el5001_sm2 = {
    0x1000, 4, 0x24,
    {NULL}
};

const ec_sync_t el5001_sm3 = {
    0x1100, 5, 0x20,
    {&el5001_st, &el5001_ip, NULL}
};

const ec_slave_type_t Beckhoff_EL5001 = {
    "Beckhoff", "EL5001", "SSI-Interface", EC_TYPE_NORMAL,
    {&mailbox_sm0, &mailbox_sm1, &el5001_sm2, &el5001_sm3, NULL}
};

/*****************************************************************************/

const ec_field_t el5101_ct = {"Control",     1};
const ec_field_t el5101_op = {"OutputValue", 2};
const ec_field_t el5101_st = {"Status",      1};
const ec_field_t el5101_ip = {"InputValue",  2};
const ec_field_t el5101_la = {"LatchValue",  2};

const ec_sync_t el5101_sm2 = {
    0x1000, 3, 0x24,
    {&el5101_ct, &el5101_op, NULL}
};

const ec_sync_t el5101_sm3 = {
    0x1100, 5, 0x20,
    {&el5101_st, &el5101_ip, &el5101_la, NULL}
};

const ec_slave_type_t Beckhoff_EL5101 = {
    "Beckhoff", "EL5101", "Incremental Encoder Interface", EC_TYPE_NORMAL,
    {&mailbox_sm0, &mailbox_sm1, &el5101_sm2, &el5101_sm3, NULL}
};

/*****************************************************************************/

const ec_sync_t el6601_sm0 = {0x1800, 522, 0x26, {NULL}};
const ec_sync_t el6601_sm1 = {0x1C00, 522, 0x22, {NULL}};

const ec_slave_type_t Beckhoff_EL6601 = {
    "Beckhoff", "EL6601", "1-Port Ethernet Switch Terminal", EC_TYPE_EOE,
    {&el6601_sm0, &el6601_sm1, NULL, NULL, NULL}
};

/*****************************************************************************/

const ec_field_t trlinenc2_st = {"Status",     1};
const ec_field_t trlinenc2_ip = {"InputValue", 4};

const ec_sync_t trlinenc2_sm0 = {0x1800, 192, 0x26, {NULL}};
const ec_sync_t trlinenc2_sm1 = {0x1C00, 192, 0x22, {NULL}};
const ec_sync_t trlinenc2_sm2 = {0x1000,   4, 0x24, {NULL}};

const ec_sync_t trlinenc2_sm3 = {
    0x1100, 5, 0x20,
    {&trlinenc2_st, &trlinenc2_ip, NULL}
};

const ec_slave_type_t TR_Electronic_LinEnc2 = {
    "TR-Electronic", "LinEnc2", "SSI-Encoder", EC_TYPE_NORMAL,
    {&trlinenc2_sm0, &trlinenc2_sm1, &trlinenc2_sm2, &trlinenc2_sm3, NULL}
};

/** \endcond */

/*****************************************************************************/

/**
   Mapping between vendor IDs and product codes <=> slave objects.
*/

ec_slave_ident_t slave_idents[] = {
    {0x00000002, 0x03F63052, &Beckhoff_EL1014},
    {0x00000002, 0x044C2C52, &Beckhoff_EK1100},
    {0x00000002, 0x04562C52, &Beckhoff_EK1110},
    {0x00000002, 0x04602C22, &Beckhoff_BK1120},
    {0x00000002, 0x07D43052, &Beckhoff_EL2004},
    {0x00000002, 0x07F03052, &Beckhoff_EL2032},
    {0x00000002, 0x0C1E3052, &Beckhoff_EL3102},
    {0x00000002, 0x0C5A3052, &Beckhoff_EL3162},
    {0x00000002, 0x10063052, &Beckhoff_EL4102},
    {0x00000002, 0x10243052, &Beckhoff_EL4132},
    {0x00000002, 0x13893052, &Beckhoff_EL5001},
    {0x00000002, 0x13ED3052, &Beckhoff_EL5101},
    {0x00000002, 0x19C93052, &Beckhoff_EL6601},
    {0x000000D4, 0x00000017, &TR_Electronic_LinEnc2},
    {}
};

/*****************************************************************************/
