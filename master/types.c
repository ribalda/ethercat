/******************************************************************************
 *
 *  t y p e s . c
 *
 *  EtherCAT-Slave-Typen.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>

#include "globals.h"
#include "types.h"

/*****************************************************************************/

const ec_sync_t mailbox_sm0 = {0x1800, 246, 0x26, {NULL}};
const ec_sync_t mailbox_sm1 = {0x18F6, 246, 0x22, {NULL}};

/*****************************************************************************/

/* Klemmen-Objekte */

/*****************************************************************************/

const ec_slave_type_t Beckhoff_EK1100 = {
    "Beckhoff", "EK1100", "Bus Coupler", 1,
    {NULL} // Keine Sync-Manager
};

/*****************************************************************************/

const ec_slave_type_t Beckhoff_EK1110 = {
    "Beckhoff", "EK1110", "Extension terminal", 0,
    {NULL} // Keine Sync-Manager
};

/*****************************************************************************/

const ec_field_t el1014_in = {"InputValue", 1};

const ec_sync_t el1014_sm0 = { // Inputs
    0x1000, 1, 0x00,
    {&el1014_in, NULL}
};

const ec_slave_type_t Beckhoff_EL1014 = {
    "Beckhoff", "EL1014", "4x Digital Input", 0,
    {&el1014_sm0, NULL}
};

/*****************************************************************************/

const ec_field_t el20XX_out = {"OutputValue", 1};

const ec_sync_t el20XX_sm0 = {
    0x0F00, 1, 0x46,
    {&el20XX_out, NULL}
};

const ec_slave_type_t Beckhoff_EL2004 = {
    "Beckhoff", "EL2004", "4x Digital Output", 0,
    {&el20XX_sm0, NULL}
};

const ec_slave_type_t Beckhoff_EL2032 = {
    "Beckhoff", "EL2032", "2x Digital Output (2A)", 0,
    {&el20XX_sm0, NULL}
};

/*****************************************************************************/

const ec_field_t el31X2_st1 = {"Status",     1};
const ec_field_t el31X2_ip1 = {"InputValue", 2};
const ec_field_t el31X2_st2 = {"Status",     1};
const ec_field_t el31X2_ip2 = {"InputValue", 2};

const ec_sync_t el31X2_sm2 = {
    0x1000, 4, 0x24,
    {NULL}
};

const ec_sync_t el31X2_sm3 = {
    0x1100, 6, 0x20,
    {&el31X2_st1, &el31X2_ip1, &el31X2_st2, &el31X2_ip2, NULL}
};

const ec_slave_type_t Beckhoff_EL3102 = {
    "Beckhoff", "EL3102", "2x Analog Input diff.", 0,
    {&mailbox_sm0, &mailbox_sm1, &el31X2_sm2, &el31X2_sm3, NULL}
};

const ec_slave_type_t Beckhoff_EL3162 = {
    "Beckhoff", "EL3162", "2x Analog Input", 0,
    {&mailbox_sm0, &mailbox_sm1, &el31X2_sm2, &el31X2_sm3, NULL}
};

/*****************************************************************************/

const ec_field_t el41X2_op = {"OutputValue", 2};

const ec_sync_t el41X2_sm2 = {
    0x1000, 4, 0x24,
    {&el41X2_op, &el41X2_op, NULL}
};

const ec_slave_type_t Beckhoff_EL4102 = {
    "Beckhoff", "EL4102", "2x Analog Output", 0,
    {&mailbox_sm0, &mailbox_sm1, &el41X2_sm2, NULL}
};

const ec_slave_type_t Beckhoff_EL4132 = {
    "Beckhoff", "EL4132", "2x Analog Output diff.", 0,
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
    "Beckhoff", "EL5001", "SSI-Interface", 0,
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

const ec_slave_type_t Beckhoff_EL5101 =
{
    "Beckhoff", "EL5101", "Incremental Encoder Interface", 0,
    {&mailbox_sm0, &mailbox_sm1, &el5101_sm2, &el5101_sm3, NULL}
};

/*****************************************************************************/

/**
   Beziehung zwischen Identifikationsnummern und Klemmen-Objekt.

   Diese Tabelle stellt die Beziehungen zwischen bestimmten Kombinationen
   aus Vendor-IDs und Product-Codes und der entsprechenden Klemme her.
   Neue Klemmen müssen hier eingetragen werden.
*/

ec_slave_ident_t slave_idents[] =
{
    {0x00000002, 0x03F63052, &Beckhoff_EL1014},
    {0x00000002, 0x044C2C52, &Beckhoff_EK1100},
    {0x00000002, 0x04562C52, &Beckhoff_EK1110},
    {0x00000002, 0x07D43052, &Beckhoff_EL2004},
    {0x00000002, 0x07F03052, &Beckhoff_EL2032},
    {0x00000002, 0x0C1E3052, &Beckhoff_EL3102},
    {0x00000002, 0x0C5A3052, &Beckhoff_EL3162},
    {0x00000002, 0x10063052, &Beckhoff_EL4102},
    {0x00000002, 0x10243052, &Beckhoff_EL4132},
    {0x00000002, 0x13893052, &Beckhoff_EL5001},
    {0x00000002, 0x13ED3052, &Beckhoff_EL5101},
    {}
};

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
