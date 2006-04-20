/******************************************************************************
 *
 *  e t h e r n e t . h
 *
 *  Ethernet-over-EtherCAT (EoE)
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/list.h>

#include "../include/ecrt.h"
#include "globals.h"
#include "slave.h"
#include "command.h"

/*****************************************************************************/

typedef enum
{
    EC_EOE_IDLE,
    EC_EOE_CHECKING,
    EC_EOE_FETCHING
}
ec_eoe_state_t;

/*****************************************************************************/

typedef struct
{
    struct list_head list;
    ec_slave_t *slave;
    ec_eoe_state_t rx_state;
}
ec_eoe_t;

/*****************************************************************************/

void ec_eoe_init(ec_eoe_t *, ec_slave_t *);
void ec_eoe_clear(ec_eoe_t *);
void ec_eoe_run(ec_eoe_t *);
void ec_eoe_print(const ec_eoe_t *);

/*****************************************************************************/
