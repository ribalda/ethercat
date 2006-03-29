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

void ec_canopen_abort_msg(uint32_t);
int ec_slave_fetch_sdo_descriptions(ec_slave_t *);
int ec_slave_fetch_sdo_entries(ec_slave_t *, ec_sdo_t *, uint8_t);

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
   Liest 32 Bit eines CANopen-SDOs im Expedited-Modus aus einem Slave.
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ec_slave_sdo_read_exp(ec_slave_t *slave, /**< EtherCAT-Slave */
                          uint16_t sdo_index, /**< SDO-Index */
                          uint8_t sdo_subindex, /**< SDO-Subindex */
                          uint8_t *target /**< Speicher für 4 Bytes */
                          )
{
    uint8_t data[0x20];
    size_t rec_size;

    EC_WRITE_U16(data, 0x2 << 12); // SDO request
    EC_WRITE_U8 (data + 2, (0x1 << 1 // expedited transfer
                            | 0x2 << 5));  // initiate upload request
    EC_WRITE_U16(data + 3, sdo_index);
    EC_WRITE_U8 (data + 5, sdo_subindex);

    if (ec_slave_mailbox_send(slave, 0x03, data, 6)) return -1;

    rec_size = 0x20;
    if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size)) return -1;

    if (EC_READ_U16(data) >> 12 == 0x2 && // SDO request
        EC_READ_U8 (data + 2) >> 5 == 0x4) { // Abort SDO transfer request
        EC_ERR("SDO upload of 0x%04X:%X aborted on slave %i.\n",
               sdo_index, sdo_subindex, slave->ring_position);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        return -1;
    }

    if (EC_READ_U16(data) >> 12 != 0x3 || // SDO response
        EC_READ_U8 (data + 2) >> 5 != 0x2 || // Upload response
        EC_READ_U16(data + 3) != sdo_index || // Index
        EC_READ_U8 (data + 5) != sdo_subindex) { // Subindex
        EC_ERR("Invalid SDO upload response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    memcpy(target, data + 6, 4);
    return 0;
}

/*****************************************************************************/

/**
   Beschreibt ein CANopen-SDO eines Slaves im Expedited-Modus.
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ec_slave_sdo_write_exp(ec_slave_t *slave, /**< EtherCAT-Slave */
                           uint16_t sdo_index, /**< SDO-Index */
                           uint8_t sdo_subindex, /**< SDO-Subindex */
                           const uint8_t *sdo_data, /**< Neuer Wert */
                           size_t size
                           )
{
    uint8_t data[0x0A];
    size_t rec_size;

    if (size == 0 || size > 4) {
        EC_ERR("Invalid data size!\n");
        return -1;
    }

    EC_WRITE_U16(data, 0x2 << 12); // SDO request
    EC_WRITE_U8 (data + 2, (0x1 // size specified
                            | 0x1 << 1 // expedited transfer
                            | (4 - size) << 2 // data set size
                            | 0x1 << 5)); // initiate download request
    EC_WRITE_U16(data + 3, sdo_index);
    EC_WRITE_U8 (data + 5, sdo_subindex);

    memcpy(data + 6, sdo_data, size);
    if (size < 4) memset(data + 6 + size, 0x00, 4 - size);

    // Mailox senden und empfangen
    if (ec_slave_mailbox_send(slave, 0x03, data, 0x0A)) return -1;

    rec_size = 0x0A;
    if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size)) return -1;

    if (EC_READ_U16(data) >> 12 == 0x2 && // SDO request
        EC_READ_U8 (data + 2) >> 5 == 0x4) { // Abort SDO transfer request
        EC_ERR("SDO download of 0x%04X:%X (%i bytes) aborted on!"
               " slave %i.\n", sdo_index, sdo_subindex, size,
               slave->ring_position);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        return -1;
    }

    if (EC_READ_U16(data) >> 12 != 0x3 || // SDO response
        EC_READ_U8 (data + 2) >> 5 != 0x3 || // Download response
        EC_READ_U16(data + 3) != sdo_index || // Index
        EC_READ_U8 (data + 5) != sdo_subindex) // Subindex
    {
        EC_ERR("Invalid SDO download response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Liest ein CANopen-SDO im "Normal-Modus" aus einem Slave.
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_slave_sdo_read(ec_slave_t *slave, /**< EtherCAT-Slave */
                        uint16_t sdo_index, /**< SDO-Index */
                        uint8_t sdo_subindex, /**< SDO-Subindex */
                        uint8_t *target, /**< Speicher für gel. Wert */
                        size_t *size /**< Größe des Speichers */
                        )
{
    uint8_t data[0x20];
    size_t rec_size, data_size;
    uint32_t complete_size;

    EC_WRITE_U16(data, 0x2 << 12); // SDO request
    EC_WRITE_U8 (data + 2, 0x2 << 5); // initiate upload request
    EC_WRITE_U16(data + 3, sdo_index);
    EC_WRITE_U8 (data + 5, sdo_subindex);

    if (ec_slave_mailbox_send(slave, 0x03, data, 6)) return -1;

    rec_size = 0x20;
    if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size)) return -1;

    if (EC_READ_U16(data) >> 12 == 0x2 && // SDO request
        EC_READ_U8 (data + 2) >> 5 == 0x4) { // Abort SDO transfer request
        EC_ERR("SDO upload of 0x%04X:%X aborted on slave %i.\n",
               sdo_index, sdo_subindex, slave->ring_position);
        ec_canopen_abort_msg(EC_READ_U32(data + 6));
        return -1;
    }

    if (EC_READ_U16(data) >> 12 != 0x3 || // SDO response
        EC_READ_U8 (data + 2) >> 5 != 0x2 || // Initiate upload response
        EC_READ_U16(data + 3) != sdo_index || // Index
        EC_READ_U8 (data + 5) != sdo_subindex) { // Subindex
        EC_ERR("Invalid SDO upload response at slave %i!\n",
               slave->ring_position);
        return -1;
    }

    if (rec_size < 10) {
        EC_ERR("Received currupted SDO upload response!\n");
        return -1;
    }

    if ((complete_size = EC_READ_U32(data + 6)) > *size) {
        EC_ERR("SDO data does not fit into buffer (%i / %i)!\n",
               complete_size, *size);
        return -1;
    }

    data_size = rec_size - 10;

    if (data_size == complete_size) {
        memcpy(target, data + 10, data_size);
    }
    else {
        EC_ERR("SDO data incomplete.\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Lädt das gesamte SDO-Dictionary aus einem Slave.
   \return 0, wenn alles ok, sonst < 0
*/

int ec_slave_fetch_sdo_list(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    uint8_t data[0xF0];
    size_t rec_size;
    unsigned int i, sdo_count;
    ec_sdo_t *sdo;
    uint16_t sdo_index;

    EC_WRITE_U16(data, 0x8 << 12); // SDO information
    EC_WRITE_U8 (data + 2, 0x01); // Get OD List Request
    EC_WRITE_U8 (data + 3, 0x00);
    EC_WRITE_U16(data + 4, 0x0000);
    EC_WRITE_U16(data + 6, 0x0001); // Deliver all SDOs!

    if (ec_slave_mailbox_send(slave, 0x03, data, 8)) return -1;

    do {
        rec_size = 0xF0;
        if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size)) return -1;

        if (EC_READ_U16(data) >> 12 == 0x8 && // SDO information
            (EC_READ_U8(data + 2) & 0x7F) == 0x07) { // Error response
            EC_ERR("SDO information error response at slave %i!\n",
                   slave->ring_position);
            ec_canopen_abort_msg(EC_READ_U32(data + 6));
            return -1;
        }

        if (EC_READ_U16(data) >> 12 != 0x8 || // SDO information
            (EC_READ_U8 (data + 2) & 0x7F) != 0x02) { // Get OD List response
            EC_ERR("Invalid SDO list response at slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        if (rec_size < 8) {
            EC_ERR("Invalid data size!\n");
            return -1;
        }

        sdo_count = (rec_size - 8) / 2;
        for (i = 0; i < sdo_count; i++) {
            sdo_index = EC_READ_U16(data + 8 + i * 2);
            if (!sdo_index) continue; // Manchmal ist der Index 0... ???

            if (!(sdo = (ec_sdo_t *) kmalloc(sizeof(ec_sdo_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate memory for SDO!\n");
                return -1;
            }
            sdo->index = sdo_index;
            sdo->name = NULL;
            list_add_tail(&sdo->list, &slave->sdo_dictionary);
        }
    } while (EC_READ_U8(data + 2) & 0x80);

    // Alle Beschreibungen laden
    if (ec_slave_fetch_sdo_descriptions(slave)) return -1;

    return 0;
}

/*****************************************************************************/

/**
   Holt die Beschreibungen zu allen bereits bekannten SDOs.
   \return 0, wenn alles ok, sonst < 0
*/

int ec_slave_fetch_sdo_descriptions(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    uint8_t data[0xF0];
    size_t rec_size, name_size;
    ec_sdo_t *sdo;

    list_for_each_entry(sdo, &slave->sdo_dictionary, list) {
        EC_WRITE_U16(data, 0x8 << 12); // SDO information
        EC_WRITE_U8 (data + 2, 0x03); // Get object description request
        EC_WRITE_U8 (data + 3, 0x00);
        EC_WRITE_U16(data + 4, 0x0000);
        EC_WRITE_U16(data + 6, sdo->index); // SDO index

        if (ec_slave_mailbox_send(slave, 0x03, data, 8)) return -1;

        rec_size = 0xF0;
        if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size))
            return -1;

        if (EC_READ_U16(data) >> 12 == 0x8 && // SDO information
            (EC_READ_U8 (data + 2) & 0x7F) == 0x07) { // Error response
            EC_ERR("SDO information error response at slave %i while"
                   " fetching SDO 0x%04X!\n", slave->ring_position,
                   sdo->index);
            ec_canopen_abort_msg(EC_READ_U32(data + 6));
            return -1;
        }

        if (EC_READ_U16(data) >> 12 != 0x8 || // SDO information
            (EC_READ_U8 (data + 2) & 0x7F) != 0x04 || // Object desc. response
            EC_READ_U16(data + 6) != sdo->index) { // SDO index
            EC_ERR("Invalid object description response at slave %i while"
                   " fetching SDO 0x%04X!\n", slave->ring_position,
                   sdo->index);
            return -1;
        }

        if (rec_size < 12) {
            EC_ERR("Invalid data size!\n");
            return -1;
        }

        sdo->type = EC_READ_U16(data + 8);
        sdo->features = EC_READ_U8(data + 11);
        INIT_LIST_HEAD(&sdo->entries);

        name_size = rec_size - 12;
        if (name_size) {
            if (!(sdo->name = kmalloc(name_size + 1, GFP_KERNEL))) {
                EC_ERR("Failed to allocate SDO name!\n");
                return -1;
            }

            memcpy(sdo->name, data + 12, name_size);
            sdo->name[name_size] = 0;
        }

        if (EC_READ_U8(data + 2) & 0x80) {
            EC_ERR("Fragment follows (not implemented)!\n");
            return -1;
        }

        // Alle Entries (Subindizes) laden
        if (ec_slave_fetch_sdo_entries(slave, sdo, EC_READ_U8(data + 10)))
            return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Lädt alle Entries (Subindizes) zu einem SDO.
   \return 0, wenn alles ok, sonst < 0
*/

int ec_slave_fetch_sdo_entries(ec_slave_t *slave, /**< EtherCAT-Slave */
                               ec_sdo_t *sdo, /**< SDO */
                               uint8_t subindices /**< Anzahl Subindizes */
                               )
{
    uint8_t data[0xF0];
    size_t rec_size, data_size;
    uint8_t i;
    ec_sdo_entry_t *entry;

    for (i = 1; i <= subindices; i++) {
        EC_WRITE_U16(data, 0x8 << 12); // SDO information
        EC_WRITE_U8 (data + 2, 0x05); // Get entry description request
        EC_WRITE_U8 (data + 3, 0x00);
        EC_WRITE_U16(data + 4, 0x0000);
        EC_WRITE_U16(data + 6, sdo->index); // SDO index
        EC_WRITE_U8 (data + 8, i); // SDO subindex
        EC_WRITE_U8 (data + 9, 0x00); // value info (no values)

        if (ec_slave_mailbox_send(slave, 0x03, data, 10)) return -1;

        rec_size = 0xF0;
        if (ec_slave_mailbox_receive(slave, 0x03, data, &rec_size))
            return -1;

        if (EC_READ_U16(data) >> 12 == 0x8 && // SDO information
            (EC_READ_U8 (data + 2) & 0x7F) == 0x07) { // Error response
            EC_ERR("SDO information error response at slave %i while"
                   " fetching SDO entry 0x%04X:%i!\n", slave->ring_position,
                   sdo->index, i);
            ec_canopen_abort_msg(EC_READ_U32(data + 6));
            return -1;
        }

        if (EC_READ_U16(data) >> 12 != 0x8 || // SDO information
            (EC_READ_U8(data + 2) & 0x7F) != 0x06 || // entry descr. response
            EC_READ_U16(data + 6) != sdo->index || // SDO index
            EC_READ_U8(data + 8) != i) { // SDO subindex
            EC_ERR("Invalid entry description response at slave %i while"
                   " fetching SDO 0x%04X:%i!\n", slave->ring_position,
                   sdo->index, i);
            return -1;
        }

        if (rec_size < 16) {
            EC_ERR("Invalid data size!\n");
            return -1;
        }

        if (!EC_READ_U16(data + 12)) // bit length = 0
            continue;

        data_size = rec_size - 16;

        if (!(entry = (ec_sdo_entry_t *)
              kmalloc(sizeof(ec_sdo_entry_t) + data_size + 1, GFP_KERNEL))) {
            EC_ERR("Failed to allocate entry memory!\n");
            return -1;
        }

        entry->subindex = i;
        entry->data_type = EC_READ_U16(data + 10);
        entry->bit_length = EC_READ_U16(data + 12);
        if (!data_size) {
            entry->name = NULL;
        }
        else {
            entry->name = (uint8_t *) entry + sizeof(ec_sdo_entry_t);
            memcpy(entry->name, data + 16, data_size);
            entry->name[data_size] = 0;
        }

        list_add_tail(&entry->list, &sdo->entries);
    }

    return 0;
}

/*****************************************************************************/

/**
   Gibt eine SDO-Abort-Meldung aus.
*/

void ec_canopen_abort_msg(uint32_t abort_code)
{
    const ec_sdo_abort_message_t *abort_msg;

    for (abort_msg = sdo_abort_messages; abort_msg->code; abort_msg++) {
        if (abort_msg->code == abort_code) {
            EC_ERR("SDO abort message 0x%08X: \"%s\".\n",
                   abort_msg->code, abort_msg->message);
            return;
        }
    }
    EC_ERR("Unknown SDO abort code 0x%08X.\n", abort_code);
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
// Echtzeitschnittstelle

/*****************************************************************************/

/**
   Liest ein 8-Bit CANopen-SDO im Expedited-Modus aus einem Slave.
   Siehe ec_slave_sdo_read_exp()
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_slave_sdo_read_exp8(ec_slave_t *slave, /**< EtherCAT-Slave */
                             uint16_t sdo_index, /**< SDO-Index */
                             uint8_t sdo_subindex, /**< SDO-Subindex */
                             uint8_t *target /**< Speicher für gel. Wert */
                             )
{
    uint8_t data[4];
    if (ec_slave_sdo_read_exp(slave, sdo_index, sdo_subindex, data)) return -1;
    *target = EC_READ_U8(data);
    return 0;
}

/*****************************************************************************/

/**
   Liest ein 16-Bit CANopen-SDO im Expedited-Modus aus einem Slave.
   Siehe ec_slave_sdo_read_exp()
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_slave_sdo_read_exp16(ec_slave_t *slave, /**< EtherCAT-Slave */
                              uint16_t sdo_index, /**< SDO-Index */
                              uint8_t sdo_subindex, /**< SDO-Subindex */
                              uint16_t *target /**< Speicher für gel. Wert */
                              )
{
    uint8_t data[4];
    if (ec_slave_sdo_read_exp(slave, sdo_index, sdo_subindex, data)) return -1;
    *target = EC_READ_U16(data);
    return 0;
}

/*****************************************************************************/

/**
   Liest ein 32-Bit CANopen-SDO im Expedited-Modus aus einem Slave.
   Siehe ec_slave_sdo_read_exp()
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_slave_sdo_read_exp32(ec_slave_t *slave, /**< EtherCAT-Slave */
                              uint16_t sdo_index, /**< SDO-Index */
                              uint8_t sdo_subindex, /**< SDO-Subindex */
                              uint32_t *target /**< Speicher für gel. Wert */
                              )
{
    uint8_t data[4];
    if (ec_slave_sdo_read_exp(slave, sdo_index, sdo_subindex, data)) return -1;
    *target = EC_READ_U32(data);
    return 0;
}

/*****************************************************************************/

/**
   Beschreibt ein 8-Bit CANopen-SDO eines Slaves im Expedited-Modus.
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_slave_sdo_write_exp8(ec_slave_t *slave, /**< EtherCAT-Slave */
                              uint16_t sdo_index, /**< SDO-Index */
                              uint8_t sdo_subindex, /**< SDO-Subindex */
                              uint8_t value /**< Neuer Wert */
                              )
{
    return ec_slave_sdo_write_exp(slave, sdo_index, sdo_subindex, &value, 1);
}

/*****************************************************************************/

/**
   Beschreibt ein 16-Bit CANopen-SDO eines Slaves im Expedited-Modus.
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_slave_sdo_write_exp16(ec_slave_t *slave, /**< EtherCAT-Slave */
                               uint16_t sdo_index, /**< SDO-Index */
                               uint8_t sdo_subindex, /**< SDO-Subindex */
                               uint16_t value /**< Neuer Wert */
                               )
{
    uint8_t data[2];
    EC_WRITE_U16(data, value);
    return ec_slave_sdo_write_exp(slave, sdo_index, sdo_subindex, data, 2);
}

/*****************************************************************************/

/**
   Beschreibt ein 32-Bit CANopen-SDO eines Slaves im Expedited-Modus.
   \return 0 wenn alles ok, < 0 bei Fehler
 */

int ecrt_slave_sdo_write_exp32(ec_slave_t *slave, /**< EtherCAT-Slave */
                               uint16_t sdo_index, /**< SDO-Index */
                               uint8_t sdo_subindex, /**< SDO-Subindex */
                               uint32_t value /**< Neuer Wert */
                               )
{
    uint8_t data[4];
    EC_WRITE_U32(data, value);
    return ec_slave_sdo_write_exp(slave, sdo_index, sdo_subindex, data, 4);
}

/*****************************************************************************/

EXPORT_SYMBOL(ecrt_slave_sdo_read_exp8);
EXPORT_SYMBOL(ecrt_slave_sdo_read_exp16);
EXPORT_SYMBOL(ecrt_slave_sdo_read_exp32);
EXPORT_SYMBOL(ecrt_slave_sdo_write_exp8);
EXPORT_SYMBOL(ecrt_slave_sdo_write_exp16);
EXPORT_SYMBOL(ecrt_slave_sdo_write_exp32);
EXPORT_SYMBOL(ecrt_slave_sdo_read);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
