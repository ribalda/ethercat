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
#include <linux/slab.h>
#include <linux/module.h>

#include "../include/EtherCAT_si.h"
#include "master.h"

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
        EC_ERR("Illegal SDO data size: %i!\n", size);
        return -1;
    }

    EC_WRITE_U16(data,      0x000A); // Length of the Mailbox service data
    EC_WRITE_U16(data + 2,  slave->station_address); // Station address
    EC_WRITE_U8 (data + 4,  0x00); // Channel & priority
    EC_WRITE_U8 (data + 5,  0x03); // CANopen over EtherCAT
    EC_WRITE_U16(data + 6,  0x2000); // Number (0), Service (SDO request)
    EC_WRITE_U8 (data + 8,  0x13 | ((4 - size) << 2)); // Spec., exp., init.
    EC_WRITE_U16(data + 9,  sdo_index);
    EC_WRITE_U8 (data + 11, sdo_subindex);

    for (i = 0; i < size; i++) {
        EC_WRITE_U8(data + 12 + i, value & 0xFF);
        value >>= 8;
    }

    ec_frame_init_npwr(&frame, master, slave->station_address,
                       0x1800, 0xF6, data);

    if (unlikely(ec_frame_send_receive(&frame))) {
        EC_ERR("Mailbox sending failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Read "written bit" of Sync-Manager

    tries_left = 10;
    while (tries_left)
    {
        ec_frame_init_nprd(&frame, master, slave->station_address, 0x808, 8);

        if (unlikely(ec_frame_send_receive(&frame) < 0)) {
            EC_ERR("Mailbox checking failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        if (EC_READ_U8(frame.data + 5) & 8) { // Written bit is high
            break;
        }

        udelay(1000);
        tries_left--;
    }

    if (!tries_left) {
        EC_ERR("Mailbox check - Slave %i timed out.\n", slave->ring_position);
        return -1;
    }

    ec_frame_init_nprd(&frame, master, slave->station_address, 0x18F6, 0xF6);

    if (unlikely(ec_frame_send_receive(&frame) < 0)) {
        EC_ERR("Mailbox receiving failed on slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if (EC_READ_U8 (frame.data + 5) != 0x03 || // COE
        EC_READ_U16(frame.data + 6) != 0x3000 || // SDO response
        EC_READ_U8 (frame.data + 8) >> 5 != 0x03 || // Download response
        EC_READ_U16(frame.data + 9) != sdo_index || // Index
        EC_READ_U8 (frame.data + 11) != sdo_subindex) // Subindex
    {
        EC_ERR("Illegal mailbox response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Liest ein CANopen-SDO (service data object).
 */

int EtherCAT_rt_canopen_sdo_read(ec_slave_t *slave, /**< EtherCAT-Slave */
                                 uint16_t sdo_index, /**< SDO-Index */
                                 uint8_t sdo_subindex, /**< SDO-Subindex */
                                 uint32_t *value /**< Speicher für gel. Wert */
                                 )
{
    unsigned char data[0xF6];
    ec_frame_t frame;
    unsigned int tries_left;
    ec_master_t *master;

    memset(data, 0x00, 0xF6);

    master = slave->master;

    EC_WRITE_U16(data,      0x0006); // Length of the Mailbox service data
    EC_WRITE_U16(data + 2,  slave->station_address); // Station address
    EC_WRITE_U8 (data + 4,  0x00); // Channel & priority
    EC_WRITE_U8 (data + 5,  0x03); // CANopen over EtherCAT
    EC_WRITE_U16(data + 6,  0x2000); // Number (0), Service (SDO request)
    EC_WRITE_U8 (data + 8,  0x1 << 1 | 0x2 << 5); // Exp., Upload request
    EC_WRITE_U16(data + 9,  sdo_index);
    EC_WRITE_U8 (data + 11, sdo_subindex);

    ec_frame_init_npwr(&frame, master, slave->station_address,
                       0x1800, 0xF6, data);

    if (unlikely(ec_frame_send_receive(&frame) < 0)) {
        EC_ERR("Mailbox sending failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Read "written bit" of Sync-Manager

    tries_left = 10;
    while (tries_left)
    {
        ec_frame_init_nprd(&frame, master, slave->station_address, 0x808, 8);

        if (unlikely(ec_frame_send_receive(&frame) < 0)) {
            EC_ERR("Mailbox checking failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        if (EC_READ_U8(frame.data + 5) & 8) { // Written bit is high
            break;
        }

        udelay(1000);
        tries_left--;
    }

    if (!tries_left) {
        EC_ERR("Mailbox check - Slave %i timed out.\n", slave->ring_position);
        return -1;
    }

    ec_frame_init_nprd(&frame, master, slave->station_address, 0x18F6, 0xF6);

    if (unlikely(ec_frame_send_receive(&frame) < 0)) {
        EC_ERR("Mailbox receiving failed on slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if (EC_READ_U8 (frame.data + 5) != 0x03 || // COE
        EC_READ_U16(frame.data + 6) != 0x3000 || // SDO response
        EC_READ_U8 (frame.data + 8) >> 5 != 0x02 || // Upload response
        EC_READ_U16(frame.data + 9) != sdo_index || // Index
        EC_READ_U8 (frame.data + 11) != sdo_subindex) // Subindex
    {
        EC_ERR("Illegal mailbox response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    *value = EC_READ_U32(frame.data + 12);

    return 0;
}

/*****************************************************************************/

/**
   Schweibt ein CANopen-SDO (Variante mit Angabe des Masters und der Adresse).

   Siehe EtherCAT_rt_canopen_sdo_write()

   \return 0 wenn alles ok, < 0 bei Fehler
 */

int EtherCAT_rt_canopen_sdo_addr_write(ec_master_t *master,
                                       /**< EtherCAT-Master */
                                       const char *addr,
                                       /**< Addresse, siehe ec_address() */
                                       uint16_t index,
                                       /**< SDO-Index */
                                       uint8_t subindex,
                                       /**< SDO-Subindex */
                                       uint32_t value,
                                       /**< Neuer Wert */
                                       size_t size
                                       /**< Größe des Datenfeldes */
                                       )
{
    ec_slave_t *slave;
    if (!(slave = ec_address(master, addr))) return -1;
    return EtherCAT_rt_canopen_sdo_write(slave, index, subindex, value, size);
}

/*****************************************************************************/

/**
   Liest ein CANopen-SDO (Variante mit Angabe des Masters und der Adresse).

   Siehe EtherCAT_rt_canopen_sdo_read()

   \return 0 wenn alles ok, < 0 bei Fehler
 */

int EtherCAT_rt_canopen_sdo_addr_read(ec_master_t *master,
                                      /**< EtherCAT-Slave */
                                      const char *addr,
                                      /**< Addresse, siehe ec_address() */
                                      uint16_t index,
                                      /**< SDO-Index */
                                      uint8_t subindex,
                                      /**< SDO-Subindex */
                                      uint32_t *value
                                      /**< Speicher für gel. Wert */
                                      )
{
    ec_slave_t *slave;
    if (!(slave = ec_address(master, addr))) return -1;
    return EtherCAT_rt_canopen_sdo_read(slave, index, subindex, value);
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_rt_canopen_sdo_write);
EXPORT_SYMBOL(EtherCAT_rt_canopen_sdo_read);
EXPORT_SYMBOL(EtherCAT_rt_canopen_sdo_addr_write);
EXPORT_SYMBOL(EtherCAT_rt_canopen_sdo_addr_read);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
