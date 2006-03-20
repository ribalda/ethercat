/******************************************************************************
 *
 *  s l a v e . c
 *
 *  Methoden für einen EtherCAT-Slave.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>

#include "globals.h"
#include "slave.h"
#include "command.h"
#include "master.h"

/*****************************************************************************/

/**
   EtherCAT-Slave-Konstruktor.
*/

void ec_slave_init(ec_slave_t *slave, /**< EtherCAT-Slave */
                   ec_master_t *master /**< EtherCAT-Master */
                   )
{
    slave->master = master;
    slave->base_type = 0;
    slave->base_revision = 0;
    slave->base_build = 0;
    slave->base_fmmu_count = 0;
    slave->base_sync_count = 0;
    slave->ring_position = 0;
    slave->station_address = 0;
    slave->sii_alias = 0;
    slave->sii_vendor_id = 0;
    slave->sii_product_code = 0;
    slave->sii_revision_number = 0;
    slave->sii_serial_number = 0;
    slave->type = NULL;
    slave->registered = 0;
    slave->fmmu_count = 0;
}

/*****************************************************************************/

/**
   EtherCAT-Slave-Destruktor.
*/

void ec_slave_clear(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    // Nichts freizugeben
}

/*****************************************************************************/

/**
   Liest alle benötigten Informationen aus einem Slave.

   \return 0 wenn alles ok, < 0 bei Fehler.
*/

int ec_slave_fetch(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    ec_command_t command;

    // Read base data
    ec_command_init_nprd(&command, slave->station_address, 0x0000, 6);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("Reading base datafrom slave %i failed!\n",
               slave->ring_position);
        return -1;
    }

    slave->base_type =       EC_READ_U8 (command.data);
    slave->base_revision =   EC_READ_U8 (command.data + 1);
    slave->base_build =      EC_READ_U16(command.data + 2);
    slave->base_fmmu_count = EC_READ_U8 (command.data + 4);
    slave->base_sync_count = EC_READ_U8 (command.data + 5);

    if (slave->base_fmmu_count > EC_MAX_FMMUS)
        slave->base_fmmu_count = EC_MAX_FMMUS;

    // Read identification from "Slave Information Interface" (SII)

    if (unlikely(ec_slave_sii_read(slave, 0x0004,
                                   (uint32_t *) &slave->sii_alias))) {
        EC_ERR("Could not read SII alias!\n");
        return -1;
    }

    if (unlikely(ec_slave_sii_read(slave, 0x0008, &slave->sii_vendor_id))) {
        EC_ERR("Could not read SII vendor id!\n");
        return -1;
    }

    if (unlikely(ec_slave_sii_read(slave, 0x000A, &slave->sii_product_code))) {
        EC_ERR("Could not read SII product code!\n");
        return -1;
    }

    if (unlikely(ec_slave_sii_read(slave, 0x000C,
                                   &slave->sii_revision_number))) {
        EC_ERR("Could not read SII revision number!\n");
        return -1;
    }

    if (unlikely(ec_slave_sii_read(slave, 0x000E,
                                   &slave->sii_serial_number))) {
        EC_ERR("Could not read SII serial number!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Liest Daten aus dem Slave-Information-Interface
   eines EtherCAT-Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_sii_read(ec_slave_t *slave,
                      /**< EtherCAT-Slave */
                      uint16_t offset,
                      /**< Adresse des zu lesenden SII-Registers */
                      uint32_t *target
                      /**< Zeiger auf einen 4 Byte großen Speicher zum Ablegen
                         der Daten */
                      )
{
    ec_command_t command;
    uint8_t data[10];
    cycles_t start, end, timeout;

    // Initiate read operation

    EC_WRITE_U8 (data,     0x00); // read-only access
    EC_WRITE_U8 (data + 1, 0x01); // request read operation
    EC_WRITE_U32(data + 2, offset);

    ec_command_init_npwr(&command, slave->station_address, 0x502, 6, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("SII-read failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Der Slave legt die Informationen des Slave-Information-Interface
    // in das Datenregister und löscht daraufhin ein Busy-Bit. Solange
    // den Status auslesen, bis das Bit weg ist.

    start = get_cycles();
    timeout = cpu_khz; // 1ms

    while (1)
    {
        udelay(10);

        ec_command_init_nprd(&command, slave->station_address, 0x502, 10);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Getting SII-read status failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (likely((EC_READ_U8(command.data + 1) & 0x81) == 0)) {
            *target = EC_READ_U32(command.data + 6);
            return 0;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("SSI-read. Slave %i timed out!\n", slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Schreibt Daten in das Slave-Information-Interface
   eines EtherCAT-Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_sii_write(ec_slave_t *slave,
                       /**< EtherCAT-Slave */
                       uint16_t offset,
                       /**< Adresse des zu lesenden SII-Registers */
                       uint16_t value
                       /**< Zu schreibender Wert */
                       )
{
    ec_command_t command;
    uint8_t data[8];
    cycles_t start, end, timeout;

    EC_INFO("SII-write (slave %i, offset 0x%04X, value 0x%04X)\n",
            slave->ring_position, offset, value);

    // Initiate write operation

    EC_WRITE_U8 (data,     0x01); // enable write access
    EC_WRITE_U8 (data + 1, 0x02); // request write operation
    EC_WRITE_U32(data + 2, offset);
    EC_WRITE_U16(data + 6, value);

    ec_command_init_npwr(&command, slave->station_address, 0x502, 8, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("SII-write failed on slave %i!\n", slave->ring_position);
        return -1;
    }

    // Der Slave legt die Informationen des Slave-Information-Interface
    // in das Datenregister und löscht daraufhin ein Busy-Bit. Solange
    // den Status auslesen, bis das Bit weg ist.

    start = get_cycles();
    timeout = cpu_khz; // 1ms

    while (1)
    {
        udelay(10);

        ec_command_init_nprd(&command, slave->station_address, 0x502, 2);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Getting SII-write status failed on slave %i!\n",
                   slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (likely((EC_READ_U8(command.data + 1) & 0x82) == 0)) {
            if (EC_READ_U8(command.data + 1) & 0x40) {
                EC_ERR("SII-write failed!\n");
                return -1;
            }
            else {
                EC_INFO("SII-write succeeded!\n");
                return 0;
            }
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("SSI-write: Slave %i timed out!\n", slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Bestätigt einen Fehler beim Zustandswechsel.

   \todo Funktioniert noch nicht...
*/

void ec_slave_state_ack(ec_slave_t *slave,
                        /**< Slave, dessen Zustand geändert werden soll */
                        uint8_t state
                        /**< Alter Zustand */
                        )
{
    ec_command_t command;
    uint8_t data[2];
    cycles_t start, end, timeout;

    EC_WRITE_U16(data, state | EC_ACK);

    ec_command_init_npwr(&command, slave->station_address, 0x0120, 2, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_WARN("State %02X acknowledge failed on slave %i!\n",
                state, slave->ring_position);
        return;
    }

    start = get_cycles();
    timeout = cpu_khz; // 1ms

    while (1)
    {
        udelay(100); // Dem Slave etwas Zeit lassen...

        ec_command_init_nprd(&command, slave->station_address, 0x0130, 2);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_WARN("State %02X acknowledge checking failed on slave %i!\n",
                    state, slave->ring_position);
            return;
        }

        end = get_cycles();

        if (unlikely(EC_READ_U8(command.data) != state)) {
            EC_WARN("Could not acknowledge state %02X on slave %i (code"
                    " %02X)!\n", state, slave->ring_position,
                    EC_READ_U8(command.data));
            return;
        }

        if (likely(EC_READ_U8(command.data) == state)) {
            EC_INFO("Acknowleged state %02X on slave %i.\n", state,
                    slave->ring_position);
            return;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_WARN("Could not check state acknowledgement %02X of slave %i -"
                    " Timeout while checking!\n", state, slave->ring_position);
            return;
        }
    }
}

/*****************************************************************************/

/**
   Ändert den Zustand eines Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_state_change(ec_slave_t *slave,
                          /**< Slave, dessen Zustand geändert werden soll */
                          uint8_t state
                          /**< Neuer Zustand */
                          )
{
    ec_command_t command;
    uint8_t data[2];
    cycles_t start, end, timeout;

    EC_WRITE_U16(data, state);

    ec_command_init_npwr(&command, slave->station_address, 0x0120, 2, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_ERR("Failed to set state %02X on slave %i!\n",
               state, slave->ring_position);
        return -1;
    }

    start = get_cycles();
    timeout = cpu_khz; // 1ms

    while (1)
    {
        udelay(100); // Dem Slave etwas Zeit lassen...

        ec_command_init_nprd(&command, slave->station_address, 0x0130, 2);
        if (unlikely(ec_master_simple_io(slave->master, &command))) {
            EC_ERR("Failed to check state %02X on slave %i!\n",
                   state, slave->ring_position);
            return -1;
        }

        end = get_cycles();

        if (unlikely(EC_READ_U8(command.data) & 0x10)) { // State change error
            EC_ERR("Could not set state %02X - Slave %i refused state change"
                   " (code %02X)!\n", state, slave->ring_position,
                   EC_READ_U8(command.data));
            ec_slave_state_ack(slave, EC_READ_U8(command.data) & 0x0F);
            return -1;
        }

        if (likely(EC_READ_U8(command.data) == (state & 0x0F))) {
            // State change successful
            return 0;
        }

        if (unlikely((end - start) >= timeout)) {
            EC_ERR("Could not check state %02X of slave %i - Timeout!\n",
                   state, slave->ring_position);
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Merkt eine FMMU-Konfiguration vor.

   Die FMMU wird so konfiguriert, dass sie den gesamten Datenbereich des
   entsprechenden Sync-Managers abdeckt. Für jede Domäne werden separate
   FMMUs konfiguriert.

   Wenn die entsprechende FMMU bereits konfiguriert ist, wird dies als
   Erfolg zurückgegeben.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_set_fmmu(ec_slave_t *slave, /**< EtherCAT-Slave */
                      const ec_domain_t *domain, /**< Domäne */
                      const ec_sync_t *sync  /**< Sync-Manager */
                      )
{
    unsigned int i;

    // FMMU schon vorgemerkt?
    for (i = 0; i < slave->fmmu_count; i++)
        if (slave->fmmus[i].domain == domain && slave->fmmus[i].sync == sync)
            return 0;

    // Neue FMMU reservieren...

    if (slave->fmmu_count >= slave->base_fmmu_count) {
        EC_ERR("Slave %i FMMU limit reached!\n", slave->ring_position);
        return -1;
    }

    slave->fmmus[slave->fmmu_count].domain = domain;
    slave->fmmus[slave->fmmu_count].sync = sync;
    slave->fmmus[slave->fmmu_count].logical_start_address = 0;
    slave->fmmu_count++;
    slave->registered = 1;

    return 0;
}

/*****************************************************************************/

/**
   Gibt alle Informationen über einen EtherCAT-Slave aus.
*/

void ec_slave_print(const ec_slave_t *slave /**< EtherCAT-Slave */)
{
    EC_INFO("--- EtherCAT slave information ---\n");

    if (slave->type) {
        EC_INFO("  Vendor \"%s\", Product \"%s\": %s\n",
                slave->type->vendor_name, slave->type->product_name,
                slave->type->description);
    }
    else {
        EC_INFO("  *** This slave has no type information! ***\n");
    }

    EC_INFO("  Ring position: %i, Station address: 0x%04X\n",
            slave->ring_position, slave->station_address);

    EC_INFO("  Base information:\n");
    EC_INFO("    Type %u, Revision %i, Build %i\n",
            slave->base_type, slave->base_revision, slave->base_build);
    EC_INFO("    Supported FMMUs: %i, Sync managers: %i\n",
            slave->base_fmmu_count, slave->base_sync_count);

    EC_INFO("  Slave information interface:\n");
    EC_INFO("    Configured station alias: 0x%04X (%i)\n", slave->sii_alias,
            slave->sii_alias);
    EC_INFO("    Vendor-ID: 0x%08X, Product code: 0x%08X\n",
            slave->sii_vendor_id, slave->sii_product_code);
    EC_INFO("    Revision number: 0x%08X, Serial number: 0x%08X\n",
            slave->sii_revision_number, slave->sii_serial_number);
}

/*****************************************************************************/

/**
   Gibt die Zählerstände der CRC-Fault-Counter aus und setzt diese zurück.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_slave_check_crc(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    ec_command_t command;
    uint8_t data[4];

    ec_command_init_nprd(&command, slave->station_address, 0x0300, 4);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_WARN("Reading CRC fault counters failed on slave %i!\n",
                slave->ring_position);
        return -1;
    }

    // No CRC faults.
    if (!EC_READ_U16(command.data) && !EC_READ_U16(command.data + 2)) return 0;

    EC_WARN("CRC faults on slave %i. A: %i, B: %i\n", slave->ring_position,
            EC_READ_U16(command.data), EC_READ_U16(command.data + 2));

    // Reset CRC counters
    EC_WRITE_U16(data,     0x0000);
    EC_WRITE_U16(data + 2, 0x0000);
    ec_command_init_npwr(&command, slave->station_address, 0x0300, 4, data);
    if (unlikely(ec_master_simple_io(slave->master, &command))) {
        EC_WARN("Resetting CRC fault counters failed on slave %i!\n",
                slave->ring_position);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/

