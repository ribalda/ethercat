/******************************************************************************
 *
 *  e t h e r n e t . c
 *
 *  Ethernet-over-EtherCAT (EoE)
 *
 *  $Id$
 *
 *****************************************************************************/

#include "../include/ecrt.h"
#include "globals.h"
#include "master.h"
#include "slave.h"
#include "mailbox.h"
#include "ethernet.h"

/*****************************************************************************/

void ec_eoe_init(ec_eoe_t *eoe, ec_slave_t *slave)
{
    eoe->slave = slave;
    eoe->rx_state = EC_EOE_IDLE;
}

/*****************************************************************************/

void ec_eoe_clear(ec_eoe_t *eoe)
{
}

/*****************************************************************************/

void ec_eoe_run(ec_eoe_t *eoe)
{
    uint8_t *data;
    ec_master_t *master;
    size_t rec_size;

    master = eoe->slave->master;

    if (eoe->rx_state == EC_EOE_IDLE) {
        ec_slave_mbox_prepare_check(eoe->slave);
        ec_master_queue_command(master, &eoe->slave->mbox_command);
        eoe->rx_state = EC_EOE_CHECKING;
        return;
    }

    if (eoe->rx_state == EC_EOE_CHECKING) {
        if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
            master->stats.eoe_errors++;
            eoe->rx_state = EC_EOE_IDLE;
            return;
        }
        if (!ec_slave_mbox_check(eoe->slave)) {
            eoe->rx_state = EC_EOE_IDLE;
            return;
        }
        ec_slave_mbox_prepare_fetch(eoe->slave);
        ec_master_queue_command(master, &eoe->slave->mbox_command);
        eoe->rx_state = EC_EOE_FETCHING;
        return;
    }

    if (eoe->rx_state == EC_EOE_FETCHING) {
        EC_DBG("EOE fetching\n");
        if (eoe->slave->mbox_command.state != EC_CMD_RECEIVED) {
            master->stats.eoe_errors++;
            eoe->rx_state = EC_EOE_IDLE;
            return;
        }
        if (!(data = ec_slave_mbox_fetch(eoe->slave, 0x02, &rec_size))) {
            master->stats.eoe_errors++;
            eoe->rx_state = EC_EOE_IDLE;
            return;
        }
        EC_DBG("EOE received: %i\n", rec_size);
        eoe->rx_state = EC_EOE_IDLE;
        return;
    }
}

/*****************************************************************************/

void ec_eoe_print(const ec_eoe_t *eoe)
{
    EC_INFO("  EoE slave %i\n", eoe->slave->ring_position);
    EC_INFO("    RX State %i\n", eoe->rx_state);
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
