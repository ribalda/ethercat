//---------------------------------------------------------------
//
//  e c _ s l a v e . c
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

#include <stdlib.h>

#include "ec_globals.h"
#include "ec_slave.h"

//---------------------------------------------------------------

void EtherCAT_slave_init(EtherCAT_slave_t *slave)
{
  slave->type = 0;
  slave->revision = 0;
  slave->build = 0;

  slave->ring_position = 0;
  slave->station_address = 0;

  slave->vendor_id = 0;
  slave->product_code = 0;
  slave->revision_number = 0;
  
  slave->desc = NULL;
  
  slave->logical_address0 = 0;

  slave->current_state = EC_STATE_UNKNOWN;
  slave->requested_state = EC_STATE_UNKNOWN;
}

//---------------------------------------------------------------

void EtherCAT_slave_clear(EtherCAT_slave_t *slave)
{
  // Nothing yet...
}

//---------------------------------------------------------------

void EtherCAT_slave_print(EtherCAT_slave_t *slave)
{
}

//---------------------------------------------------------------

unsigned char sm0_multi[] = {0x00, 0x18, 0xF6, 0x00, 0x26, 0x00, 0x01, 0x00};
unsigned char sm1_multi[] = {0xF6, 0x18, 0xF6, 0x00, 0x22, 0x00, 0x01, 0x00};

unsigned char sm0_1014[] = {0x00, 0x10, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00};

unsigned char sm0_2004[] = {0x00, 0x0F, 0x01, 0x00, 0x46, 0x00, 0x01, 0x00};

unsigned char sm2_31xx[] = {0x00, 0x10, 0x04, 0x00, 0x24, 0x00, 0x00, 0x00};
unsigned char sm3_31xx[] = {0x00, 0x11, 0x06, 0x00, 0x20, 0x00, 0x01, 0x00};

unsigned char sm2_4102[] = {0x00, 0x10, 0x04, 0x00, 0x24, 0x00, 0x01, 0x00};


unsigned char fmmu0_1014[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x07,
                              0x00, 0x10, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00};

unsigned char fmmu0_2004[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x07,
                              0x00, 0x0F, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00};

unsigned char fmmu0_31xx[] = {0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x07,
                              0x00, 0x11, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00};

unsigned char fmmu0_4102[] = {0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x07,
                              0x00, 0x10, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00};

//---------------------------------------------------------------

EtherCAT_slave_desc_t Beckhoff_EK1100[] = {"Beckhoff", "EK1100", "Bus Coupler",
                                           SIMPLE,
                                           NULL, NULL, NULL, NULL, // Noch nicht eingepflegt...
                                           NULL,
                                           0};

EtherCAT_slave_desc_t Beckhoff_EL1014[] = {"Beckhoff", "EL1014", "4x Digital Input",
                                           SIMPLE,
                                           sm0_1014, NULL, NULL, NULL,
                                           fmmu0_1014,
                                           1};

EtherCAT_slave_desc_t Beckhoff_EL2004[] = {"Beckhoff", "EL2004", "4x Digital Output",
                                           SIMPLE,
                                           sm0_2004, NULL, NULL, NULL,
                                           fmmu0_2004,
                                           1};

EtherCAT_slave_desc_t Beckhoff_EL3102[] = {"Beckhoff", "EL3102", "2x Analog Input Diff",
                                           MAILBOX,
                                           sm0_multi, sm1_multi, sm2_31xx, sm3_31xx,
                                           fmmu0_31xx,
                                           6};

EtherCAT_slave_desc_t Beckhoff_EL3162[] = {"Beckhoff", "EL3162", "2x Analog Input",
                                           MAILBOX,
                                           sm0_multi, sm1_multi, sm2_31xx, sm3_31xx,
                                           fmmu0_31xx,
                                           6};

EtherCAT_slave_desc_t Beckhoff_EL4102[] = {"Beckhoff", "EL4102", "2x Analog Output",
                                           MAILBOX,
                                           sm0_multi, sm1_multi, sm2_4102, NULL,
                                           fmmu0_4102,
                                           4};

EtherCAT_slave_desc_t Beckhoff_EL5001[] = {"Beckhoff", "EL5001", "SSI-Interface",
                                           SIMPLE,
                                           NULL, NULL, NULL, NULL, // Noch nicht eingepflegt...
                                           NULL,
                                           0};

//---------------------------------------------------------------

unsigned int slave_idents_count = 7;

struct slave_ident slave_idents[] = 
{
  {0x00000002, 0x03F63052, Beckhoff_EL1014},
  {0x00000002, 0x044C2C52, Beckhoff_EK1100},
  {0x00000002, 0x07D43052, Beckhoff_EL2004},
  {0x00000002, 0x0C1E3052, Beckhoff_EL3102},
  {0x00000002, 0x0C5A3052, Beckhoff_EL3162},
  {0x00000002, 0x10063052, Beckhoff_EL4102},
  {0x00000002, 0x13893052, Beckhoff_EL5001}
};

//---------------------------------------------------------------
