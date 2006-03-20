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

#include "master.h"

/*****************************************************************************/

/**
   SDO Abort Code Messages
*/

typedef struct
{
    uint32_t code;
    const char *message;
}
ec_sdo_abort_message_t;

const ec_sdo_abort_message_t sdo_abort_messages[];

/*****************************************************************************/

/**
   Schreibt ein CANopen-SDO (service data object).
 */

int ecrt_slave_sdo_write(ec_slave_t *slave, /**< EtherCAT-Slave */
                         uint16_t sdo_index, /**< SDO-Index */
                         uint8_t sdo_subindex, /**< SDO-Subindex */
                         uint32_t value, /**< Neuer Wert */
                         size_t size /**< Größe des Datenfeldes */
                         )
{
    uint8_t data[0xF6];
    ec_command_t command;
    unsigned int i;
    ec_master_t *master;
    cycles_t start, end, timeout;
    uint32_t abort_code;
    const ec_sdo_abort_message_t *abort_msg;

    memset(data, 0x00, 0xF6);

    master = slave->master;

    if (size == 0 || size > 4) {
        EC_ERR("Invalid SDO data size: %i!\n", size);
        return -1;
    }

    EC_WRITE_U16(data,      0x000A); // Length of the Mailbox service data
    EC_WRITE_U16(data + 2,  slave->station_address); // Station address
    EC_WRITE_U8 (data + 4,  0x00); // Channel & priority
    EC_WRITE_U8 (data + 5,  0x03); // CANopen over EtherCAT
    EC_WRITE_U16(data + 6,  0x02 << 12); // Number (0), Service (SDO request)
    EC_WRITE_U8 (data + 8,  0x23 | ((4 - size) << 2)); // Spec., exp., init.
    EC_WRITE_U16(data + 9,  sdo_index);
    EC_WRITE_U8 (data + 11, sdo_subindex);

    for (i = 0; i < size; i++) {
        EC_WRITE_U8(data + 12 + i, value & 0xFF);
        value >>= 8;
    }

    ec_command_init_npwr(&command, slave->station_address, 0x1800, 0xF6, data);
    if (unlikely(ec_master_simple_io(master, &command))) {
        EC_ERR("Mailbox sending failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Read "written bit" of Sync-Manager
    start = get_cycles();
    timeout = (cycles_t) 10 * cpu_khz; // 10ms

    while (1)
    {
        udelay(100);

        ec_command_init_nprd(&command, slave->station_address, 0x808, 8);
        if (unlikely(ec_master_simple_io(master, &command))) {
            EC_ERR("Mailbox checking failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (EC_READ_U8(command.data + 5) & 8) break; // Written bit is high

        if ((end - start) >= timeout) {
            EC_ERR("Mailbox check - Slave %i timed out.\n",
                   slave->ring_position);
            return -1;
        }
    }

    if (unlikely(slave->master->debug_level) > 1)
        EC_DBG("SDO download took %ius.\n", ((u32) (end - start) * 1000
                                             / cpu_khz));

    ec_command_init_nprd(&command, slave->station_address, 0x18F6, 0xF6);
    if (unlikely(ec_master_simple_io(master, &command))) {
        EC_ERR("Mailbox receiving failed on slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if (EC_READ_U8 (command.data + 5) != 0x03) { // nicht CoE
        EC_ERR("Invalid mailbox response (non-CoE) at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if (EC_READ_U16(command.data + 6) >> 12 == 0x02 && // SDO request
        EC_READ_U8 (command.data + 8) >> 5 == 0x04) { // Abort SDO transf. req.
        EC_ERR("SDO download of 0x%04X:%X (value %X, size %X) aborted on slave"
               " %i.\n", sdo_index, sdo_subindex, value, size,
               slave->ring_position);
        abort_code = EC_READ_U32(command.data + 12);
        for (abort_msg = sdo_abort_messages; abort_msg->code; abort_msg++) {
            if (abort_msg->code == abort_code) {
                EC_ERR("SDO abort message 0x%08X: \"%s\".\n",
                       abort_msg->code, abort_msg->message);
                return -1;
            }
        }
        EC_ERR("Unknown SDO abort code 0x%08X.\n", abort_code);
        return -1;
    }

    if (EC_READ_U16(command.data + 6) >> 12 != 0x03 || // SDO response
        EC_READ_U8 (command.data + 8) >> 5 != 0x03 || // Download response
        EC_READ_U16(command.data + 9) != sdo_index || // Index
        EC_READ_U8 (command.data + 11) != sdo_subindex) // Subindex
    {
        EC_ERR("Invalid SDO download response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Liest ein CANopen-SDO (service data object).
 */

int ecrt_slave_sdo_read(ec_slave_t *slave, /**< EtherCAT-Slave */
                        uint16_t sdo_index, /**< SDO-Index */
                        uint8_t sdo_subindex, /**< SDO-Subindex */
                        uint32_t *value /**< Speicher für gel. Wert */
                        )
{
    uint8_t data[0xF6];
    ec_command_t command;
    ec_master_t *master;
    cycles_t start, end, timeout;
    uint32_t abort_code;
    const ec_sdo_abort_message_t *abort_msg;

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

    ec_command_init_npwr(&command, slave->station_address, 0x1800, 0xF6, data);
    if (unlikely(ec_master_simple_io(master, &command))) {
        EC_ERR("Mailbox sending failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Read "written bit" of Sync-Manager

    start = get_cycles();
    timeout = cpu_khz; // 1ms

    while (1)
    {
        udelay(10);

        ec_command_init_nprd(&command, slave->station_address, 0x808, 8);
        if (unlikely(ec_master_simple_io(master, &command))) {
            EC_ERR("Mailbox checking failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (EC_READ_U8(command.data + 5) & 8) { // Written bit is high
            break;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("Mailbox check on slave %i timed out.\n",
                   slave->ring_position);
            return -1;
        }
    }

    ec_command_init_nprd(&command, slave->station_address, 0x18F6, 0xF6);
    if (unlikely(ec_master_simple_io(master, &command))) {
        EC_ERR("Mailbox receiving failed on slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if (EC_READ_U8 (command.data + 5) != 0x03) { // nicht CoE
        EC_ERR("Invalid mailbox response (non-CoE) at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if (EC_READ_U16(command.data + 6) >> 12 == 0x02 && // SDO request
        EC_READ_U8 (command.data + 8) >> 5 == 0x04) { // Abort SDO transf. req.
        EC_ERR("SDO upload of 0x%04X:%X aborted on slave %i.\n",
               sdo_index, sdo_subindex, slave->ring_position);
        abort_code = EC_READ_U32(command.data + 12);
        for (abort_msg = sdo_abort_messages; abort_msg->code; abort_msg++) {
            if (abort_msg->code == abort_code) {
                EC_ERR("SDO abort message 0x%08X: \"%s\".\n",
                       abort_msg->code, abort_msg->message);
                return -1;
            }
        }
        EC_ERR("Unknown SDO abort code 0x%08X.\n", abort_code);
        return -1;
    }

    if (EC_READ_U16(command.data + 6) >> 12 != 0x03 || // SDO response
        EC_READ_U8 (command.data + 8) >> 5 != 0x02 || // Upload response
        EC_READ_U16(command.data + 9) != sdo_index || // Index
        EC_READ_U8 (command.data + 11) != sdo_subindex) // Subindex
    {
        EC_ERR("Invalid SDO upload response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    *value = EC_READ_U32(command.data + 12);

    return 0;
}

/*****************************************************************************/

/**
   Schweibt ein CANopen-SDO (Variante mit Angabe des Masters und der Adresse).

   Siehe ecrt_slave_sdo_write()

   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_master_sdo_write(ec_master_t *master,
                          /**< EtherCAT-Master */
                          const char *addr,
                          /**< Addresse, siehe ec_master_slave_address() */
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
    if (!(slave = ec_master_slave_address(master, addr))) return -1;
    return ecrt_slave_sdo_write(slave, index, subindex, value, size);
}

/*****************************************************************************/

/**
   Liest ein CANopen-SDO (Variante mit Angabe des Masters und der Adresse).

   Siehe ecrt_slave_sdo_read()

   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_master_sdo_read(ec_master_t *master,
                         /**< EtherCAT-Slave */
                         const char *addr,
                         /**< Addresse, siehe ec_master_slave_address() */
                         uint16_t index,
                         /**< SDO-Index */
                         uint8_t subindex,
                         /**< SDO-Subindex */
                         uint32_t *value
                         /**< Speicher für gel. Wert */
                         )
{
    ec_slave_t *slave;
    if (!(slave = ec_master_slave_address(master, addr))) return -1;
    return ecrt_slave_sdo_read(slave, index, subindex, value);
}

/*****************************************************************************/

const ec_sdo_abort_message_t sdo_abort_messages[] = {
    {0x05030000, "Toggle bit not changed"},
    {0x05040000, "SDO protocol timeout"},
    {0x05040001, "Client/Server command specifier not valid or unknown"},
    {0x05040005, "Out of memory"},
    {0x06010000, "Unsupported access to an object"},
    {0x06010001, "Attempt to read a write-only object"},
    {0x06010002, "Attempt to write a read-only object"},
    {0x06020000, "This object does not exist in the object directory"},
    {0x06040041, "The object cannot be mapped into the PDO"},
    {0x06040042, "The number and length of the objects to be mapped would"
     " exceed the PDO length"},
    {0x06040043, "General parameter incompatibility reason"},
    {0x06040047, "Gerneral internal incompatibility in device"},
    {0x06060000, "Access failure due to a hardware error"},
    {0x06070010, "Data type does not match, length of service parameter does"
     " not match"},
    {0x06070012, "Data type does not match, length of service parameter too"
     " high"},
    {0x06070013, "Data type does not match, length of service parameter too"
     " low"},
    {0x06090011, "Subindex does not exist"},
    {0x06090030, "Value range of parameter exceeded"},
    {0x06090031, "Value of parameter written too high"},
    {0x06090032, "Value of parameter written too low"},
    {0x06090036, "Maximum value is less than minimum value"},
    {0x08000000, "General error"},
    {0x08000020, "Data cannot be transferred or stored to the application"},
    {0x08000021, "Data cannot be transferred or stored to the application"
     " because of local control"},
    {0x08000022, "Data cannot be transferred or stored to the application"
     " because of the present device state"},
    {0x08000023, "Object dictionary dynamic generation fails or no object"
     " dictionary is present"},
    {}
};

/*****************************************************************************/

EXPORT_SYMBOL(ecrt_slave_sdo_write);
EXPORT_SYMBOL(ecrt_slave_sdo_read);
EXPORT_SYMBOL(ecrt_master_sdo_write);
EXPORT_SYMBOL(ecrt_master_sdo_read);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
