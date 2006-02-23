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
#include "frame.h"

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
*/

int ec_slave_fetch(ec_slave_t *slave /**< EtherCAT-Slave */)
{
    ec_frame_t frame;

    // Read base data
    ec_frame_init_nprd(&frame, slave->master, slave->station_address,
                       0x0000, 6);

    if (unlikely(ec_frame_send_receive(&frame))) return -1;

    if (unlikely(frame.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Slave %i did not respond while reading base"
               " data!\n", slave->ring_position);
        return -1;
    }

    slave->base_type = frame.data[0];
    slave->base_revision = frame.data[1];
    slave->base_build = frame.data[2] | (frame.data[3] << 8);
    slave->base_fmmu_count = frame.data[4];
    slave->base_sync_count = frame.data[5];

    if (slave->base_fmmu_count > EC_MAX_FMMUS)
        slave->base_fmmu_count = EC_MAX_FMMUS;

    // Read identification from "Slave Information Interface" (SII)

    if (unlikely(ec_slave_sii_read(slave, 0x0008, &slave->sii_vendor_id))) {
        printk(KERN_ERR "EtherCAT: Could not read SII vendor id!\n");
        return -1;
    }

    if (unlikely(ec_slave_sii_read(slave, 0x000A, &slave->sii_product_code))) {
        printk(KERN_ERR "EtherCAT: Could not read SII product code!\n");
        return -1;
    }

    if (unlikely(ec_slave_sii_read(slave, 0x000C,
                                   &slave->sii_revision_number))) {
        printk(KERN_ERR "EtherCAT: Could not read SII revision number!\n");
        return -1;
    }

    if (unlikely(ec_slave_sii_read(slave, 0x000E,
                                   &slave->sii_serial_number))) {
        printk(KERN_ERR "EtherCAT: Could not read SII serial number!\n");
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
                      unsigned short int offset,
                      /**< Adresse des zu lesenden SII-Registers */
                      unsigned int *target
                      /**< Zeiger auf einen 4 Byte großen Speicher zum Ablegen
                         der Daten */
                      )
{
    ec_frame_t frame;
    unsigned char data[10];
    unsigned int tries_left;

    // Initiate read operation

    data[0] = 0x00;
    data[1] = 0x01;
    data[2] = offset & 0xFF;
    data[3] = (offset & 0xFF00) >> 8;
    data[4] = 0x00;
    data[5] = 0x00;

    ec_frame_init_npwr(&frame, slave->master, slave->station_address, 0x502, 6,
                       data);

    if (unlikely(ec_frame_send_receive(&frame))) return -1;

    if (unlikely(frame.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: SII-read - Slave %i did not respond!\n",
               slave->ring_position);
        return -1;
    }

    // Der Slave legt die Informationen des Slave-Information-Interface
    // in das Datenregister und löscht daraufhin ein Busy-Bit. Solange
    // den Status auslesen, bis das Bit weg ist.

    tries_left = 100;
    while (likely(tries_left))
    {
        udelay(10);

        ec_frame_init_nprd(&frame, slave->master, slave->station_address, 0x502,
                           10);

        if (unlikely(ec_frame_send_receive(&frame))) return -1;

        if (unlikely(frame.working_counter != 1)) {
            printk(KERN_ERR "EtherCAT: SII-read status -"
                   " Slave %i did not respond!\n", slave->ring_position);
            return -1;
        }

        if (likely((frame.data[1] & 0x81) == 0)) {
            memcpy(target, frame.data + 6, 4);
            break;
        }

        tries_left--;
    }

    if (unlikely(!tries_left)) {
        printk(KERN_WARNING "EtherCAT: SSI-read. Slave %i timed out!\n",
               slave->ring_position);
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Bestätigt einen Fehler beim Zustandswechsel.

   FIXME Funktioniert noch nicht...
*/

void ec_slave_state_ack(ec_slave_t *slave,
                        /**< Slave, dessen Zustand geändert werden soll */
                        uint8_t state
                        /**< Alter Zustand */
                        )
{
    ec_frame_t frame;
    unsigned char data[2];
    unsigned int tries_left;

    data[0] = state | EC_ACK;
    data[1] = 0x00;

    ec_frame_init_npwr(&frame, slave->master, slave->station_address, 0x0120,
                       2, data);

    if (unlikely(ec_frame_send_receive(&frame) != 0)) {
        printk(KERN_ERR "EtherCAT: Could no acknowledge state %02X - Unable to"
               " send!\n", state);
        return;
    }

    if (unlikely(frame.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Could not acknowledge state %02X - Slave"
               " %i did not respond!\n", state, slave->ring_position);
        return;
    }

    tries_left = 100;
    while (likely(tries_left))
    {
        udelay(10);

        ec_frame_init_nprd(&frame, slave->master, slave->station_address,
                           0x0130, 2);

        if (unlikely(ec_frame_send_receive(&frame) != 0)) {
            printk(KERN_ERR "EtherCAT: Could not check state acknowledgement"
                   " %02X - Unable to send!\n", state);
            return;
        }

        if (unlikely(frame.working_counter != 1)) {
            printk(KERN_ERR "EtherCAT: Could not check state acknowledgement"
                   " %02X - Slave %i did not respond!\n", state,
                   slave->ring_position);
            return;
        }

        if (unlikely(frame.data[0] != state)) {
            printk(KERN_ERR "EtherCAT: Could not acknowledge state %02X on"
                   " slave %i (code %02X)!\n", state, slave->ring_position,
                   frame.data[0]);
            return;
        }

        if (likely(frame.data[0] == state)) {
            printk(KERN_INFO "EtherCAT: Acknowleged state %02X on slave %i.\n",
                   state, slave->ring_position);
            return;
        }

        tries_left--;
    }

    if (unlikely(!tries_left)) {
        printk(KERN_ERR "EtherCAT: Could not check state acknowledgement %02X"
               " of slave %i - Timeout while checking!\n", state,
               slave->ring_position);
        return;
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
    ec_frame_t frame;
    unsigned char data[2];
    unsigned int tries_left;

    data[0] = state;
    data[1] = 0x00;

    ec_frame_init_npwr(&frame, slave->master, slave->station_address, 0x0120,
                       2, data);

    if (unlikely(ec_frame_send_receive(&frame) != 0)) {
        printk(KERN_ERR "EtherCAT: Could not set state %02X - Unable to"
               " send!\n", state);
        return -1;
    }

    if (unlikely(frame.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Could not set state %02X - Slave %i did not"
               " respond!\n", state, slave->ring_position);
        return -1;
    }

    tries_left = 100;
    while (likely(tries_left))
    {
        udelay(10);

        ec_frame_init_nprd(&frame, slave->master, slave->station_address,
                           0x0130, 2);

        if (unlikely(ec_frame_send_receive(&frame) != 0)) {
            printk(KERN_ERR "EtherCAT: Could not check state %02X - Unable to"
                   " send!\n", state);
            return -1;
        }

        if (unlikely(frame.working_counter != 1)) {
            printk(KERN_ERR "EtherCAT: Could not check state %02X - Slave %i"
                   " did not respond!\n", state, slave->ring_position);
            return -1;
        }

        if (unlikely(frame.data[0] & 0x10)) { // State change error
            printk(KERN_ERR "EtherCAT: Could not set state %02X - Slave %i"
                   " refused state change (code %02X)!\n", state,
                   slave->ring_position, frame.data[0]);
            ec_slave_state_ack(slave, frame.data[0] & 0x0F);
            return -1;
        }

        if (likely(frame.data[0] == (state & 0x0F))) {
            // State change successful
            break;
        }

        tries_left--;
    }

    if (unlikely(!tries_left)) {
        printk(KERN_ERR "EtherCAT: Could not check state %02X of slave %i -"
               " Timeout while checking!\n", state,
               slave->ring_position);
        return -1;
    }

    return 0;
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

    if (slave->fmmu_count >= slave->base_fmmu_count) {
        printk(KERN_ERR "EtherCAT: Slave %i supports only %i FMMUs.\n",
               slave->ring_position, slave->base_fmmu_count);
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
    printk(KERN_INFO "--- EtherCAT slave information ---\n");

    if (slave->type) {
        printk(KERN_INFO "  Vendor \"%s\", Product \"%s\": %s\n",
               slave->type->vendor_name, slave->type->product_name,
               slave->type->description);
    }
    else {
        printk(KERN_INFO "  *** This slave has no type information! ***\n");
    }

    printk(KERN_INFO "  Ring position: %i, Station address: 0x%04X\n",
           slave->ring_position, slave->station_address);

    printk(KERN_INFO "  Base information:\n");
    printk(KERN_INFO "    Type %u, Revision %i, Build %i\n",
           slave->base_type, slave->base_revision, slave->base_build);
    printk(KERN_INFO "    Supported FMMUs: %i, Sync managers: %i\n",
           slave->base_fmmu_count, slave->base_sync_count);

    printk(KERN_INFO "  Slave information interface:\n");
    printk(KERN_INFO "    Vendor-ID: 0x%08X, Product code: 0x%08X\n",
           slave->sii_vendor_id, slave->sii_product_code);
    printk(KERN_INFO "    Revision number: 0x%08X, Serial number: 0x%08X\n",
           slave->sii_revision_number, slave->sii_serial_number);
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
