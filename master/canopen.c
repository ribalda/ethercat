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

int EtherCAT_rt_canopen_sdo_write(ec_master_t *master, ec_slave_t *slave,
                                  unsigned int sdo_index,
                                  unsigned char sdo_subindex,
                                  unsigned int value, unsigned int size)
{
    unsigned char data[0xF6];
    ec_command_t cmd;
    unsigned int tries_left, i;

    for (i = 0; i < 0xF6; i++) data[i] = 0x00;

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

    ec_command_write(&cmd, slave->station_address, 0x1800, 0xF6, data);

    if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
        return -1;

    if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Mailbox send - Slave %i did not respond!\n",
               slave->ring_position * (-1));
        return -1;
    }

    // Read "written bit" of Sync-Manager

    tries_left = 10;
    while (tries_left)
    {
        ec_command_read(&cmd, slave->station_address, 0x808, 8);

        if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
            return -1;

        if (unlikely(cmd.working_counter != 1)) {
            printk(KERN_ERR "EtherCAT: Mailbox check - Slave %i did not"
                   " respond!\n", slave->ring_position * (-1));
            return -1;
        }

        if (cmd.data[5] & 8) { // Written bit is high
            break;
        }

        udelay(1000);
        tries_left--;
    }

    if (!tries_left) {
        printk(KERN_ERR "EtherCAT: Mailbox check - Slave %i timed out.\n",
               slave->ring_position * (-1));
        return -1;
    }

    ec_command_read(&cmd, slave->station_address, 0x18F6, 0xF6);

    if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
        return -1;

    if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Mailbox receive - Slave %i did not"
               " respond!\n", slave->ring_position * (-1));
        return -1;
    }

    if (cmd.data[5] != 0x03 // COE
        || (cmd.data[7] >> 4) != 0x03 // SDO response
        || (cmd.data[8] >> 5) != 0x03 // Initiate download response
        || (cmd.data[9] != (sdo_index & 0xFF)) // Index
        || (cmd.data[10] != ((sdo_index >> 8) & 0xFF))
        || (cmd.data[11] != sdo_subindex)) // Subindex
    {
        printk(KERN_ERR "EtherCAT: Illegal mailbox response at slave %i!\n",
               slave->ring_position * (-1));
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
