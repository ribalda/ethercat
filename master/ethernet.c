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
#if 0
    unsigned int i;
    uint8_t fragment_number;
    uint8_t complete_size;
    uint8_t frame_number;
    uint8_t last_fragment;
#endif

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

#if 0
        fragment_number = EC_READ_U16(data + 2) & 0x003F;
        complete_size = (EC_READ_U16(data + 2) >> 6) & 0x003F;
        frame_number = (EC_READ_U16(data + 2) >> 12) & 0x0003;
        last_fragment = (EC_READ_U16(data + 2) >> 15) & 0x0001;

        EC_DBG("EOE %s received, fragment: %i, complete size: %i (0x%02X),"
               " frame %i%s\n",
               fragment_number ? "fragment" : "initiate", fragment_number,
               (complete_size - 31) / 32, complete_size, frame_number,
               last_fragment ? ", last fragment" : "");
        EC_DBG("");
        for (i = 0; i < rec_size - 2; i++) {
            printk("%02X ", data[i + 2]);
            if ((i + 1) % 16 == 0) {
                printk("\n");
                EC_DBG("");
            }
        }
        printk("\n");
#endif

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
