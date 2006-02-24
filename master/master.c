/******************************************************************************
 *
 *  m a s t e r . c
 *
 *  Methoden für einen EtherCAT-Master.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "../include/EtherCAT_rt.h"
#include "../include/EtherCAT_si.h"
#include "globals.h"
#include "master.h"
#include "slave.h"
#include "types.h"
#include "device.h"
#include "frame.h"

/*****************************************************************************/

/**
   Konstruktor des EtherCAT-Masters.
*/

void ec_master_init(ec_master_t *master /**< EtherCAT-Master */)
{
    master->slaves = NULL;
    master->slave_count = 0;
    master->device_registered = 0;
    master->command_index = 0x00;
    master->domain_count = 0;
    master->debug_level = 0;
    master->bus_time = 0;
    master->frames_lost = 0;
    master->t_lost_output = 0;
}

/*****************************************************************************/

/**
   Destruktor eines EtherCAT-Masters.

   Entfernt alle Kommandos aus der Liste, löscht den Zeiger
   auf das Slave-Array und gibt die Prozessdaten frei.
*/

void ec_master_clear(ec_master_t *master /**< EtherCAT-Master */)
{
    ec_master_reset(master);
    ec_device_clear(&master->device);
}

/*****************************************************************************/

/**
   Setzt den Master zurück in den Ausgangszustand.

   Bei einem "release" sollte immer diese Funktion aufgerufen werden,
   da sonst Slave-Liste, Domains, etc. weiter existieren.
*/

void ec_master_reset(ec_master_t *master
                     /**< Zeiger auf den zurückzusetzenden Master */
                     )
{
    unsigned int i;

    ec_master_clear_slaves(master);

    for (i = 0; i < master->domain_count; i++) {
        ec_domain_clear(master->domains[i]);
        kfree(master->domains[i]);
    }
    master->domain_count = 0;

    master->command_index = 0;
    master->debug_level = 0;
    master->bus_time = 0;
    master->frames_lost = 0;
    master->t_lost_output = 0;
}

/*****************************************************************************/

/**
   Entfernt alle Slaves.
*/

void ec_master_clear_slaves(ec_master_t *master /**< EtherCAT-Master */)
{
    unsigned int i;

    if (master->slaves) {
        for (i = 0; i < master->slave_count; i++) {
            ec_slave_clear(master->slaves + i);
        }
        kfree(master->slaves);
        master->slaves = NULL;
    }
    master->slave_count = 0;
}

/*****************************************************************************/

/**
   Öffnet das EtherCAT-Geraet des Masters.

   \return 0, wenn alles o.k., < 0, wenn kein Gerät registriert wurde oder
   es nicht geoeffnet werden konnte.
*/

int ec_master_open(ec_master_t *master /**< Der EtherCAT-Master */)
{
    if (!master->device_registered) {
        printk(KERN_ERR "EtherCAT: No device registered!\n");
        return -1;
    }

    if (ec_device_open(&master->device) < 0) {
        printk(KERN_ERR "EtherCAT: Could not open device!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Schliesst das EtherCAT-Geraet, auf dem der Master arbeitet.
*/

void ec_master_close(ec_master_t *master /**< EtherCAT-Master */)
{
    if (!master->device_registered) {
        printk(KERN_WARNING "EtherCAT: Warning -"
               " Trying to close an unregistered device!\n");
        return;
    }

    if (ec_device_close(&master->device) < 0) {
        printk(KERN_WARNING "EtherCAT: Warning - Could not close device!\n");
    }
}

/*****************************************************************************/

/**
   Durchsucht den Bus nach Slaves.

   @return 0 bei Erfolg, sonst < 0
*/

int ec_scan_for_slaves(ec_master_t *master /**< EtherCAT-Master */)
{
    ec_frame_t frame;
    ec_slave_t *slave;
    ec_slave_ident_t *ident;
    unsigned int i;
    unsigned char data[2];

    if (master->slaves || master->slave_count)
        printk(KERN_WARNING "EtherCAT: Slave scan already done!\n");
    ec_master_clear_slaves(master);

    // Determine number of slaves on bus

    ec_frame_init_brd(&frame, master, 0x0000, 4);
    if (unlikely(ec_frame_send_receive(&frame) < 0)) return -1;

    master->slave_count = frame.working_counter;
    printk("EtherCAT: Found %i slaves on bus.\n", master->slave_count);

    if (!master->slave_count) return 0;

    if (!(master->slaves = (ec_slave_t *) kmalloc(master->slave_count
                                                      * sizeof(ec_slave_t),
                                                      GFP_KERNEL))) {
        printk(KERN_ERR "EtherCAT: Could not allocate memory for bus"
               " slaves!\n");
        return -1;
    }

    // Init slaves
    for (i = 0; i < master->slave_count; i++) {
        slave = master->slaves + i;
        ec_slave_init(slave, master);
        slave->ring_position = i;
        slave->station_address = i + 1;
    }

    // For every slave in the list
    for (i = 0; i < master->slave_count; i++)
    {
        slave = master->slaves + i;

        // Write station address
        EC_WRITE_U16(data, slave->station_address);

        ec_frame_init_apwr(&frame, master, slave->ring_position, 0x0010,
                           sizeof(uint16_t), data);

        if (unlikely(ec_frame_send_receive(&frame) < 0)) return -1;

        if (unlikely(frame.working_counter != 1)) {
            printk(KERN_ERR "EtherCAT: Slave %i did not repond while writing"
                   " station address!\n", i);
            return -1;
        }

        // Fetch all slave information
        if (ec_slave_fetch(slave)) return -1;

        // Search for identification in "database"
        ident = slave_idents;
        while (ident) {
            if (unlikely(ident->vendor_id == slave->sii_vendor_id
                         && ident->product_code == slave->sii_product_code)) {
                slave->type = ident->type;
                break;
            }
            ident++;
        }

        if (!slave->type) {
            printk(KERN_WARNING "EtherCAT: Unknown slave device (vendor"
                   " 0x%08X, code 0x%08X) at position %i.\n",
                   slave->sii_vendor_id, slave->sii_product_code, i);
            return 0;
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Gibt von Zeit zu Zeit die Anzahl verlorener Frames aus.
*/

void ec_output_lost_frames(ec_master_t *master /**< EtherCAT-Master */)
{
    unsigned long int t;

    if (master->frames_lost) {
        rdtscl(t);
        if ((t - master->t_lost_output) / cpu_khz > 1000) {
            printk(KERN_ERR "EtherCAT: %u frame(s) LOST!\n",
                   master->frames_lost);
            master->frames_lost = 0;
            master->t_lost_output = t;
        }
    }
}

/*****************************************************************************/

/**
   Wandelt eine ASCII-kodierte Bus-Adresse in einen Slave-Zeiger.

   Gültige Adress-Strings sind Folgende:

   - \a "X" = der X. Slave im Bus,
   - \a "X:Y" = der Y. Slave hinter dem X. Buskoppler,
   - \a "#X" = der Slave mit der SSID X,
   - \a "#X:Y" = der Y. Slave hinter dem Buskoppler mit der SSID X.

   \return Zeiger auf Slave bei Erfolg, sonst NULL
*/

ec_slave_t *ec_address(const ec_master_t *master,
                       /**< EtherCAT-Master */
                       const char *address
                       /**< Address-String */
                       )
{
    unsigned long first, second;
    char *remainder, *remainder2;
    unsigned int i;
    int coupler_idx, slave_idx;
    ec_slave_t *slave;

    if (!address || address[0] == 0) return NULL;

    if (address[0] == '#') {
        printk(KERN_ERR "EtherCAT: Bus ID \"%s\" - #<SSID> not implemented"
               " yet!\n", address);
        return NULL;
    }

    first = simple_strtoul(address, &remainder, 0);
    if (remainder == address) {
        printk(KERN_ERR "EtherCAT: Bus ID \"%s\" - First number empty!\n",
               address);
        return NULL;
    }

    if (!remainder[0]) { // absolute position
        if (first < master->slave_count) {
            return master->slaves + first;
        }

        printk(KERN_ERR "EtherCAT: Bus ID \"%s\" - Absolute position"
               " illegal!\n", address);
    }

    else if (remainder[0] == ':') { // field position

        remainder++;
        second = simple_strtoul(remainder, &remainder2, 0);

        if (remainder2 == remainder) {
            printk(KERN_ERR "EtherCAT: Bus ID \"%s\" - Sencond number"
                   " empty!\n", address);
            return NULL;
        }

        if (remainder2[0]) {
            printk(KERN_ERR "EtherCAT: Bus ID \"%s\" - Illegal trailer"
                   " (2)!\n", address);
            return NULL;
        }

        coupler_idx = -1;
        slave_idx = 0;
        for (i = 0; i < master->slave_count; i++, slave_idx++) {
            slave = master->slaves + i;
            if (!slave->type) continue;

            if (slave->type->bus_coupler) {
                coupler_idx++;
                slave_idx = 0;
            }

            if (coupler_idx == first && slave_idx == second) return slave;
        }
    }

    else {
        printk(KERN_ERR "EtherCAT: Bus ID \"%s\" - Illegal trailer!\n",
               address);
    }

    return NULL;
}

/*****************************************************************************/

/**
   Initialisiert eine Sync-Manager-Konfigurationsseite.

   Der mit \a data referenzierte Speicher muss mindestens EC_SYNC_SIZE Bytes
   groß sein.
*/

void ec_sync_config(const ec_sync_t *sync, /**< Sync-Manager */
                    uint8_t *data /**> Zeiger auf Konfigurationsspeicher */
                    )
{
    EC_WRITE_U16(data,     sync->physical_start_address);
    EC_WRITE_U16(data + 2, sync->size);
    EC_WRITE_U8 (data + 4, sync->control_byte);
    EC_WRITE_U8 (data + 5, 0x00); // status byte (read only)
    EC_WRITE_U16(data + 6, 0x0001); // enable
}

/*****************************************************************************/

/**
   Initialisiert eine FMMU-Konfigurationsseite.

   Der mit \a data referenzierte Speicher muss mindestens EC_FMMU_SIZE Bytes
   groß sein.
*/

void ec_fmmu_config(const ec_fmmu_t *fmmu, /**< Sync-Manager */
                    uint8_t *data /**> Zeiger auf Konfigurationsspeicher */
                    )
{
    EC_WRITE_U32(data,      fmmu->logical_start_address);
    EC_WRITE_U16(data + 4,  fmmu->sync->size);
    EC_WRITE_U8 (data + 6,  0x00); // Logical start bit
    EC_WRITE_U8 (data + 7,  0x07); // Logical end bit
    EC_WRITE_U16(data + 8,  fmmu->sync->physical_start_address);
    EC_WRITE_U8 (data + 10, 0x00); // Physical start bit
    EC_WRITE_U8 (data + 11, (fmmu->sync->control_byte & 0x04) ? 0x02 : 0x01);
    EC_WRITE_U16(data + 12, 0x0001); // Enable
    EC_WRITE_U16(data + 14, 0x0000); // res.
}

/******************************************************************************
 *
 * Echtzeitschnittstelle
 *
 *****************************************************************************/

/**
   Registriert eine neue Domäne.

   \return Zeiger auf die Domäne bei Erfolg, sonst NULL.
*/

ec_domain_t *EtherCAT_rt_master_register_domain(ec_master_t *master,
                                                /**< Domäne */
                                                ec_domain_mode_t mode,
                                                /**< Modus */
                                                unsigned int timeout_us
                                                /**< Timeout */
                                                )
{
    ec_domain_t *domain;

    if (master->domain_count >= EC_MASTER_MAX_DOMAINS) {
        printk(KERN_ERR "EtherCAT: Maximum number of domains reached!\n");
        return NULL;
    }

    if (!(domain = (ec_domain_t *) kmalloc(sizeof(ec_domain_t), GFP_KERNEL))) {
        printk(KERN_ERR "EthertCAT: Error allocating domain memory!\n");
        return NULL;
    }

    ec_domain_init(domain, master, mode, timeout_us);
    master->domains[master->domain_count] = domain;
    master->domain_count++;

    return domain;
}

/*****************************************************************************/

/**
   Konfiguriert alle Slaves und setzt den Operational-Zustand.

   Führt die komplette Konfiguration und Aktivierunge aller registrierten
   Slaves durch. Setzt Sync-Manager und FMMU's, führt die entsprechenden
   Zustandsübergänge durch, bis der Slave betriebsbereit ist.

   \return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_rt_master_activate(ec_master_t *master /**< EtherCAT-Master */)
{
    unsigned int i, j;
    ec_slave_t *slave;
    ec_frame_t frame;
    const ec_sync_t *sync;
    const ec_slave_type_t *type;
    const ec_fmmu_t *fmmu;
    uint8_t data[256];
    uint32_t domain_offset;

    // Domains erstellen
    domain_offset = 0;
    for (i = 0; i < master->domain_count; i++) {
        ec_domain_t *domain = master->domains[i];
        if (ec_domain_alloc(domain, domain_offset)) {
            printk(KERN_INFO "EtherCAT: Failed to allocate domain %i!\n", i);
            return -1;
        }
        printk(KERN_INFO "EtherCAT: Domain %i - Allocated %i bytes (%i"
               " Frame(s))\n", i, domain->data_size,
               domain->data_size / EC_MAX_FRAME_SIZE + 1);
        domain_offset += domain->data_size;
    }

    // Slaves aktivieren
    for (i = 0; i < master->slave_count; i++)
    {
        slave = master->slaves + i;

        // Change state to INIT
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_INIT)))
            return -1;

        // Check if slave was registered...
        if (!slave->type) {
            printk(KERN_INFO "EtherCAT: Slave %i has unknown type!\n", i);
            continue;
        }

        type = slave->type;

        // Check and reset CRC fault counters
        ec_slave_check_crc(slave);

        // Resetting FMMU's
        if (slave->base_fmmu_count) {
            memset(data, 0x00, EC_FMMU_SIZE * slave->base_fmmu_count);
            ec_frame_init_npwr(&frame, master, slave->station_address, 0x0600,
                               EC_FMMU_SIZE * slave->base_fmmu_count, data);
            if (unlikely(ec_frame_send_receive(&frame) < 0)) return -1;
            if (unlikely(frame.working_counter != 1)) {
                printk(KERN_ERR "EtherCAT: Resetting FMMUs - Slave %i did"
                       " not respond!\n", slave->ring_position);
                return -1;
            }
        }

        // Resetting Sync Manager channels
        if (slave->base_sync_count) {
            memset(data, 0x00, EC_SYNC_SIZE * slave->base_sync_count);
            ec_frame_init_npwr(&frame, master, slave->station_address, 0x0800,
                               EC_SYNC_SIZE * slave->base_sync_count, data);
            if (unlikely(ec_frame_send_receive(&frame) < 0)) return -1;
            if (unlikely(frame.working_counter != 1)) {
                printk(KERN_ERR "EtherCAT: Resetting SMs - Slave %i did not"
                       " respond!\n", slave->ring_position);
                return -1;
            }
        }

        // Set Sync Managers
        for (j = 0; type->sync_managers[j] && j < EC_MAX_SYNC; j++)
        {
            sync = type->sync_managers[j];

            ec_sync_config(sync, data);
            ec_frame_init_npwr(&frame, master, slave->station_address,
                               0x0800 + j * EC_SYNC_SIZE, EC_SYNC_SIZE, data);

            if (unlikely(ec_frame_send_receive(&frame))) return -1;

            if (unlikely(frame.working_counter != 1)) {
                printk(KERN_ERR "EtherCAT: Setting sync manager %i - Slave"
                       " %i did not respond!\n", j, slave->ring_position);
                return -1;
            }
        }

        // Change state to PREOP
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_PREOP)))
            return -1;

        // Slaves that are not registered are only brought into PREOP
        // state -> nice blinking and mailbox comm. possible
        if (!slave->registered && !slave->type->bus_coupler) {
            printk(KERN_WARNING "EtherCAT: Slave %i was not registered!\n",
                   slave->ring_position);
            continue;
        }

        // Set FMMUs
        for (j = 0; j < slave->fmmu_count; j++)
        {
            fmmu = &slave->fmmus[j];

            ec_fmmu_config(fmmu, data);
            ec_frame_init_npwr(&frame, master, slave->station_address,
                               0x0600 + j * EC_FMMU_SIZE, EC_FMMU_SIZE, data);

            if (unlikely(ec_frame_send_receive(&frame))) return -1;

            if (unlikely(frame.working_counter != 1)) {
                printk(KERN_ERR "EtherCAT: Setting FMMU %i - Slave %i did not"
                       " respond!\n", j, slave->ring_position);
                return -1;
            }
        }

        // Change state to SAVEOP
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_SAVEOP)))
            return -1;

        // Change state to OP
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_OP)))
            return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Setzt alle Slaves zurück in den Init-Zustand.

   \return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_rt_master_deactivate(ec_master_t *master /**< EtherCAT-Master */)
{
    ec_slave_t *slave;
    unsigned int i;

    for (i = 0; i < master->slave_count; i++)
    {
        slave = master->slaves + i;

        // CRC-Zählerstände ausgeben
        ec_slave_check_crc(slave);

        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_INIT) != 0))
            return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Setzt die Debug-Ebene des Masters.

   Folgende Debug-level sind definiert:

   - 1: Nur Positionsmarken in bestimmten Funktionen
   - 2: Komplette Frame-Inhalte
*/

void EtherCAT_rt_master_debug(ec_master_t *master,
                              /**< EtherCAT-Master */
                              int level
                              /**< Debug-Level */
                              )
{
    master->debug_level = level;

    printk(KERN_INFO "EtherCAT: Master debug level set to %i.\n", level);
}

/*****************************************************************************/

/**
   Gibt alle Informationen zum Master aus.
*/

void EtherCAT_rt_master_print(const ec_master_t *master
                              /**< EtherCAT-Master */
                              )
{
    unsigned int i;

    printk(KERN_INFO "EtherCAT: *** Begin master information ***\n");

    for (i = 0; i < master->slave_count; i++) {
        ec_slave_print(&master->slaves[i]);
    }

    printk(KERN_INFO "EtherCAT: *** End master information ***\n");
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_rt_master_register_domain);
EXPORT_SYMBOL(EtherCAT_rt_master_activate);
EXPORT_SYMBOL(EtherCAT_rt_master_deactivate);
EXPORT_SYMBOL(EtherCAT_rt_master_debug);
EXPORT_SYMBOL(EtherCAT_rt_master_print);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
