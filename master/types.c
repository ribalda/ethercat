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

/* Konfigurationen der Sync-Manager */

unsigned char sm0_multi[] = {0x00, 0x18, 0xF6, 0x00, 0x26, 0x00, 0x01, 0x00};
unsigned char sm1_multi[] = {0xF6, 0x18, 0xF6, 0x00, 0x22, 0x00, 0x01, 0x00};

unsigned char sm0_1014[] = {0x00, 0x10, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00};

unsigned char sm0_2004[] = {0x00, 0x0F, 0x01, 0x00, 0x46, 0x00, 0x01, 0x00};

unsigned char sm2_31xx[] = {0x00, 0x10, 0x04, 0x00, 0x24, 0x00, 0x00, 0x00};
unsigned char sm3_31xx[] = {0x00, 0x11, 0x06, 0x00, 0x20, 0x00, 0x01, 0x00};

unsigned char sm2_41xx[] = {0x00, 0x10, 0x04, 0x00, 0x24, 0x00, 0x01, 0x00};

/* Konfigurationen der Memory-Management-Units */

unsigned char fmmu0_1014[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x07,
                              0x00, 0x10, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00};

unsigned char fmmu0_2004[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x07,
                              0x00, 0x0F, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00};

unsigned char fmmu0_31xx[] = {0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x07,
                              0x00, 0x11, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00};

unsigned char fmmu0_41xx[] = {0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x07,
                              0x00, 0x10, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00};

/*****************************************************************************/

/* Klemmen-Objekte */

ec_slave_type_t Beckhoff_EK1100[] =
{{
  "Beckhoff", "EK1100", "Bus Coupler",
  EC_NOSYNC_SLAVE, NULL, NULL, NULL, NULL, NULL, 0
}};

ec_slave_type_t Beckhoff_EK1110[] =
{{
  "Beckhoff", "EK1110", "Extension terminal",
  EC_NOSYNC_SLAVE, NULL, NULL, NULL, NULL, NULL, 0
}};

ec_slave_type_t Beckhoff_EL1014[] =
{{
  "Beckhoff", "EL1014", "4x Digital Input",
  EC_SIMPLE_SLAVE, sm0_1014, NULL, NULL, NULL, fmmu0_1014, 1
}};

ec_slave_type_t Beckhoff_EL2004[] =
{{
  "Beckhoff", "EL2004", "4x Digital Output",
  EC_SIMPLE_SLAVE, sm0_2004, NULL, NULL, NULL, fmmu0_2004, 1
}};

ec_slave_type_t Beckhoff_EL3102[] =
{{
  "Beckhoff", "EL3102", "2x Analog Input diff.",
  EC_MAILBOX_SLAVE, sm0_multi, sm1_multi, sm2_31xx, sm3_31xx, fmmu0_31xx, 6
}};

ec_slave_type_t Beckhoff_EL3162[] =
{{
  "Beckhoff", "EL3162", "2x Analog Input",
  EC_MAILBOX_SLAVE, sm0_multi, sm1_multi, sm2_31xx, sm3_31xx, fmmu0_31xx, 6
}};

ec_slave_type_t Beckhoff_EL4102[] =
{{
  "Beckhoff", "EL4102", "2x Analog Output",
  EC_MAILBOX_SLAVE, sm0_multi, sm1_multi, sm2_41xx, NULL, fmmu0_41xx, 4
}};

ec_slave_type_t Beckhoff_EL4132[] =
{{
  "Beckhoff", "EL4132", "2x Analog Output diff.",
  EC_MAILBOX_SLAVE, sm0_multi, sm1_multi, sm2_41xx, NULL, fmmu0_41xx, 4
}};

ec_slave_type_t Beckhoff_EL5001[] =
{{
  "Beckhoff", "EL5001", "SSI-Interface", // Noch nicht eingepflegt...
  EC_SIMPLE_SLAVE, NULL, NULL, NULL, NULL, NULL, 0
}};

/*****************************************************************************/

/**
   Beziehung zwischen Identifikationsnummern und Klemmen-Objekt.

   Diese Tabelle stellt die Beziehungen zwischen bestimmten Kombinationen
   aus Vendor-IDs und Product-Codes und der entsprechenden Klemme her.
   Neue Klemmen müssen hier eingetragen werden.
*/

ec_slave_ident_t slave_idents[] =
{
  {0x00000002, 0x03F63052, Beckhoff_EL1014},
  {0x00000002, 0x044C2C52, Beckhoff_EK1100},
  {0x00000002, 0x04562C52, Beckhoff_EK1110},
  {0x00000002, 0x07D43052, Beckhoff_EL2004},
  {0x00000002, 0x0C1E3052, Beckhoff_EL3102},
  {0x00000002, 0x0C5A3052, Beckhoff_EL3162},
  {0x00000002, 0x10063052, Beckhoff_EL4102},
  {0x00000002, 0x10243052, Beckhoff_EL4132},
  {0x00000002, 0x13893052, Beckhoff_EL5001}
};

unsigned int slave_ident_count = sizeof(slave_idents)
     / sizeof(ec_slave_ident_t);


/*****************************************************************************/
