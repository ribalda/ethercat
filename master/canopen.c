/******************************************************************************
 *
 *  c a n o p e n . c
 *
 *  CANopen over EtherCAT
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/delay.h>

#include "master.h"

/*****************************************************************************/

// Prototypen

/*****************************************************************************/

/**
   Schreibt ein CANopen-SDO (service data object).
 */

int EtherCAT_rt_canopen_sdo_write(
    ec_slave_t *slave, /**< EtherCAT-Slave */
    unsigned int sdo_index, /**< SDO-Index */
    unsigned char sdo_subindex, /**< SDO-Subindex */
    unsigned int value, /**< Neuer Wert */
    unsigned int size /**< Größe des Datenfeldes */
    )
{
    unsigned char data[0xF6];
    ec_frame_t frame;
    unsigned int tries_left, i;
    ec_master_t *master;

    memset(data, 0x00, 0xF6);

    master = slave->master;

    if (size == 0 || size > 4) {
        printk(KERN_ERR "EtherCAT: Illegal SDO data size: %i!\n", size);
        return -1;
    }

    data[0] = 0x0A; // Length of the Mailbox service data
    data[1] = 0x00;
    data[2] = slave->station_address & 0xFF; // Station address
    data[3] = (slave->station_address >> 8) & 0xFF;
    data[4] = 0x00; // Channel & priority
    data[5] = 0x03; // CANopen over EtherCAT
    data[6] = 0x00; // Number(7-0)
    data[7] = 0x2 << 4; // Number(8) & Service = SDO Request (0x02)
    data[8] = 0x01 // Size specified
        | (0x1 << 1) // Transfer type = Expedited
        | ((4 - size) << 2) // Data Set Size
        | (0x1 << 5); // Command specifier = Initiate download request (0x01)
    data[9] = sdo_index & 0xFF;
    data[10] = (sdo_index >> 8) & 0xFF;
    data[11] = sdo_subindex;

    for (i = 0; i < size; i++) {
        data[12 + i] = value & 0xFF;
        value >>= 8;
    }

    ec_frame_init_npwr(&frame, master, slave->station_address, 0x1800, 0xF6,
                       data);

    if (unlikely(ec_frame_send_receive(&frame) < 0)) return -1;

    if (unlikely(frame.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Mailbox send - Slave %i did not respond!\n",
               slave->ring_position);
        return -1;
    }

    // Read "written bit" of Sync-Manager

    tries_left = 10;
    while (tries_left)
    {
        ec_frame_init_nprd(&frame, master, slave->station_address, 0x808, 8);

        if (unlikely(ec_frame_send_receive(&frame) < 0)) return -1;

        if (unlikely(frame.working_counter != 1)) {
            printk(KERN_ERR "EtherCAT: Mailbox check - Slave %i did not"
                   " respond!\n", slave->ring_position);
            return -1;
        }

        if (frame.data[5] & 8) { // Written bit is high
            break;
        }

        udelay(1000);
        tries_left--;
    }

    if (!tries_left) {
        printk(KERN_ERR "EtherCAT: Mailbox check - Slave %i timed out.\n",
               slave->ring_position);
        return -1;
    }

    ec_frame_init_nprd(&frame, master, slave->station_address, 0x18F6, 0xF6);

    if (unlikely(ec_frame_send_receive(&frame) < 0)) return -1;

    if (unlikely(frame.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Mailbox receive - Slave %i did not"
               " respond!\n", slave->ring_position);
        return -1;
    }

    if (frame.data[5] != 0x03 // COE
        || (frame.data[7] >> 4) != 0x03 // SDO response
        || (frame.data[8] >> 5) != 0x03 // Initiate download response
        || (frame.data[9] != (sdo_index & 0xFF)) // Index
        || (frame.data[10] != ((sdo_index >> 8) & 0xFF))
        || (frame.data[11] != sdo_subindex)) // Subindex
    {
        printk(KERN_ERR "EtherCAT: Illegal mailbox response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_rt_canopen_sdo_write);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
