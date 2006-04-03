/******************************************************************************
 *
 *  m a i l b o x . c
 *
 *  Mailbox-Funktionen
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/slab.h>
#include <linux/delay.h>

#include "mailbox.h"
#include "command.h"
#include "master.h"

/*****************************************************************************/

/**
   Bereitet ein Mailbox-Send-Kommando vor.
 */

uint8_t *ec_slave_mbox_prepare_send(ec_slave_t *slave, /**< Slave */
                                    uint8_t type, /**< Mailbox-Protokoll */
                                    size_t size /**< Datengröße */
                                    )
{
    ec_command_t *command = &slave->mbox_command;
    size_t total_size;

    if (unlikely(!slave->sii_mailbox_protocols)) {
        EC_ERR("Slave %i does not support mailbox communication!\n",
               slave->ring_position);
        return NULL;
    }

    total_size = size + 6;
    if (unlikely(total_size > slave->sii_rx_mailbox_size)) {
        EC_ERR("Data size does not fit in mailbox!\n");
        return NULL;
    }

    if (ec_command_npwr(command, slave->station_address,
                        slave->sii_rx_mailbox_offset,
                        slave->sii_rx_mailbox_size))
        return NULL;

    EC_WRITE_U16(command->data,     size); // Mailbox service data length
    EC_WRITE_U16(command->data + 2, slave->station_address); // Station address
    EC_WRITE_U8 (command->data + 4, 0x00); // Channel & priority
    EC_WRITE_U8 (command->data + 5, type); // Underlying protocol type

    return command->data + 6;
}

/*****************************************************************************/

/**
   Bereitet ein Kommando zum Abfragen des Mailbox-Zustandes vor.
 */

int ec_slave_mbox_prepare_check(ec_slave_t *slave /**< Slave */)
{
    ec_command_t *command = &slave->mbox_command;

    // FIXME: Zweiter Sync-Manager nicht immer TX-Mailbox?
    if (ec_command_nprd(command, slave->station_address, 0x808, 8))
        return -1;

    return 0;
}

/*****************************************************************************/

/**
   Liest den Mailbox-Zustand aus einem empfangenen Kommando.
 */

int ec_slave_mbox_check(const ec_slave_t *slave /**< Slave */)
{
    return EC_READ_U8(slave->mbox_command.data + 5) & 8 ? 1 : 0;
}

/*****************************************************************************/

/**
   Bereitet ein Kommando zum Laden von Daten von der Mailbox vor.
 */

int ec_slave_mbox_prepare_fetch(ec_slave_t *slave /**< Slave */)
{
    ec_command_t *command = &slave->mbox_command;

    if (ec_command_nprd(command, slave->station_address,
                        slave->sii_tx_mailbox_offset,
                        slave->sii_tx_mailbox_size)) return -1;
    return 0;
}

/*****************************************************************************/

/**
   Verarbeitet empfangene Mailbox-Daten.
 */

uint8_t *ec_slave_mbox_fetch(ec_slave_t *slave, /**< Slave */
                             uint8_t type, /**< Protokoll */
                             size_t *size /**< Größe der empfangenen
                                             Daten */
                             )
{
    ec_command_t *command = &slave->mbox_command;
    size_t data_size;

    if ((EC_READ_U8(command->data + 5) & 0x0F) != type) {
        EC_ERR("Unexpected mailbox protocol 0x%02X (exp.: 0x%02X) at"
               " slave %i!\n", EC_READ_U8(command->data + 5), type,
               slave->ring_position);
        return NULL;
    }

    if ((data_size = EC_READ_U16(command->data)) >
        slave->sii_tx_mailbox_size - 6) {
        EC_ERR("Currupt mailbox response detected!\n");
        return NULL;
    }

    *size = data_size;
    return command->data + 6;
}

/*****************************************************************************/

/**
   Sendet und wartet auf den Empfang eines Mailbox-Kommandos.
 */

uint8_t *ec_slave_mbox_simple_io(ec_slave_t *slave, /**< Slave */
                                 size_t *size /**< Größe der gelesenen
                                                 Daten */
                                 )
{
    uint8_t type;
    ec_command_t *command;

    command = &slave->mbox_command;
    type = EC_READ_U8(command->data + 5);

    if (unlikely(ec_master_simple_io(slave->master, command))) {
        EC_ERR("Mailbox checking failed on slave %i!\n",
               slave->ring_position);
        return NULL;
    }

    return ec_slave_mbox_simple_receive(slave, type, size);
}

/*****************************************************************************/

/**
   Wartet auf den Empfang eines Mailbox-Kommandos.
 */

uint8_t *ec_slave_mbox_simple_receive(ec_slave_t *slave, /**< Slave */
                                      uint8_t type, /**< Protokoll */
                                      size_t *size /**< Größe der gelesenen
                                                      Daten */
                                      )
{
    cycles_t start, end, timeout;
    ec_command_t *command;

    command = &slave->mbox_command;
    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        if (ec_slave_mbox_prepare_check(slave)) return NULL;
        if (unlikely(ec_master_simple_io(slave->master, command))) {
            EC_ERR("Mailbox checking failed on slave %i!\n",
                   slave->ring_position);
            return NULL;
        }

        end = get_cycles();

        if (ec_slave_mbox_check(slave))
            break; // Proceed with receiving data

        if ((end - start) >= timeout) {
            EC_ERR("Mailbox check - Slave %i timed out.\n",
                   slave->ring_position);
            return NULL;
        }

        udelay(100);
    }

    if (ec_slave_mbox_prepare_fetch(slave)) return NULL;
    if (unlikely(ec_master_simple_io(slave->master, command))) {
        EC_ERR("Mailbox receiving failed on slave %i!\n",
               slave->ring_position);
        return NULL;
    }

    if (unlikely(slave->master->debug_level) > 1)
        EC_DBG("Mailbox receive took %ius.\n", ((u32) (end - start) * 1000
                                                / cpu_khz));

    return ec_slave_mbox_fetch(slave, type, size);
}

/*****************************************************************************/

#if 0
/**
   Sendet ein Mailbox-Kommando.
 */

int ec_slave_mbox_send(ec_slave_t *slave, /**< EtherCAT-Slave */
                       uint8_t type, /**< Unterliegendes Protokoll */
                       const uint8_t *prot_data, /**< Protokoll-Daten */
                       size_t size /**< Datengröße */
                       )
{
    uint8_t *data;
    ec_command_t command;


    }

    if (!(data = kmalloc(slave->sii_rx_mailbox_size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %i bytes of memory for mailbox data!\n",
               slave->sii_rx_mailbox_size);
        return -1;
    }

    memset(data, 0x00, slave->sii_rx_mailbox_size);
    EC_WRITE_U16(data,      size); // Length of the Mailbox service data
    EC_WRITE_U16(data + 2,  slave->station_address); // Station address
    EC_WRITE_U8 (data + 4,  0x00); // Channel & priority
    EC_WRITE_U8 (data + 5,  type); // Underlying protocol type
    memcpy(data + 6, prot_data, size);

    ec_command_init_npwr(&command, slave->station_address,
                         slave->sii_rx_mailbox_offset,
                         slave->sii_rx_mailbox_size, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("Mailbox sending failed on slave %i!\n", slave->ring_position);
        kfree(data);
        return -1;
    }

    kfree(data);
    return 0;
}

/*****************************************************************************/

/**
   Sendet ein Mailbox-Kommando.
 */

int ec_slave_mailbox_receive(ec_slave_t *slave, /**< EtherCAT-Slave */
                             uint8_t type, /**< Unterliegendes Protokoll */
                             uint8_t *prot_data, /**< Protokoll-Daten */
                             size_t *size /**< Datengröße des Puffers, später
                                             Größe der gelesenen Daten */
                             )
{
    ec_command_t command;
    size_t data_size;
    cycles_t start, end, timeout;

    // Read "written bit" of Sync-Manager
    start = get_cycles();
    timeout = (cycles_t) 100 * cpu_khz; // 100ms

    while (1)
    {
        // FIXME: Zweiter Sync-Manager nicht immer TX-Mailbox?
        ec_command_init_nprd(&command, slave->station_address, 0x808, 8);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Mailbox checking failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (EC_READ_U8(command.data + 5) & 8)
            break; // Proceed with received data

        if ((end - start) >= timeout) {
            EC_ERR("Mailbox check - Slave %i timed out.\n",
                   slave->ring_position);
            return -1;
        }

        udelay(100);
    }

    ec_command_init_nprd(&command, slave->station_address,
                         slave->sii_tx_mailbox_offset,
                         slave->sii_tx_mailbox_size);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("Mailbox receiving failed on slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if ((EC_READ_U8(command.data + 5) & 0x0F) != type) {
        EC_ERR("Unexpected mailbox protocol 0x%02X (exp.: 0x%02X) at"
               " slave %i!\n", EC_READ_U8(command.data + 5), type,
               slave->ring_position);
        return -1;
    }

    if (unlikely(slave->master->debug_level) > 1)
        EC_DBG("Mailbox receive took %ius.\n", ((u32) (end - start) * 1000
                                                / cpu_khz));

    if ((data_size = EC_READ_U16(command.data)) > *size) {
        EC_ERR("Mailbox service data does not fit into buffer (%i > %i).\n",
               data_size, *size);
        return -1;
    }

    if (data_size > slave->sii_tx_mailbox_size - 6) {
        EC_ERR("Currupt mailbox response detected!\n");
        return -1;
    }

    memcpy(prot_data, command.data + 6, data_size);
    *size = data_size;
    return 0;
}

/*****************************************************************************/

uint8_t *ec_slave_init_mbox_send_cmd(ec_slave_t *slave, /**< EtherCAT-Slave */
                                     ec_command_t *command, /**< Kommando */
                                     uint8_t type, /**< Protokolltyp */
                                     size_t size /**< Datengröße */
                                     )
{
    size_t total_size;
    uint8_t *data;

    if (unlikely(!slave->sii_mailbox_protocols)) {
        EC_ERR("Slave %i does not support mailbox communication!\n",
               slave->ring_position);
        return NULL;
    }

    total_size = size + 6;
    if (unlikely(total_size > slave->sii_rx_mailbox_size)) {
        EC_ERR("Data size does not fit into mailbox of slave %i!\n",
               slave->ring_position);
        return NULL;
    }

    data = command->data;

    memset(data, 0x00, slave->sii_rx_mailbox_size);
    EC_WRITE_U16(data,     size); // Length of the Mailbox service data
    EC_WRITE_U16(data + 2, slave->station_address); // Station address
    EC_WRITE_U8 (data + 4, 0x00); // Channel & priority
    EC_WRITE_U8 (data + 5, type); // Underlying protocol type

    return data + 6;
}
#endif

/*****************************************************************************/
