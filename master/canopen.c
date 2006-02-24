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

#include "../include/EtherCAT_si.h"
#include "master.h"

/*****************************************************************************/

// Prototypen

/*****************************************************************************/

/**
   Schreibt ein CANopen-SDO (service data object).
 */

int EtherCAT_rt_canopen_sdo_write(ec_slave_t *slave, /**< EtherCAT-Slave */
                                  uint16_t sdo_index, /**< SDO-Index */
                                  uint8_t sdo_subindex, /**< SDO-Subindex */
                                  uint32_t value, /**< Neuer Wert */
                                  size_t size /**< Größe des Datenfeldes */
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

    EC_WRITE_U16(data,      0x000A); // Length of the Mailbox service data
    EC_WRITE_U16(data + 2,  slave->station_address); // Station address
    EC_WRITE_U8 (data + 4,  0x00); // Channel & priority
    EC_WRITE_U8 (data + 5,  0x03); // CANopen over EtherCAT
    EC_WRITE_U16(data + 6,  0x2000); // Number & Service
    EC_WRITE_U8 (data + 8,  0x13 | ((4 - size) << 2)); // Spec., exp., init.
    EC_WRITE_U16(data + 9,  sdo_index);
    EC_WRITE_U8 (data + 11, sdo_subindex);

    for (i = 0; i < size; i++) {
        EC_WRITE_U8(data + 12 + i, value & 0xFF);
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

        if (EC_READ_U8(frame.data + 5) & 8) { // Written bit is high
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

    if (EC_READ_U8 (frame.data + 5) != 0x03 || // COE
        EC_READ_U16(frame.data + 6) != 0x3000 || // SDO response
        EC_READ_U8 (frame.data + 8) >> 5 != 0x03 || // Download response
        EC_READ_U16(frame.data + 9) != sdo_index || // Index
        EC_READ_U8 (frame.data + 11) != sdo_subindex) // Subindex
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
