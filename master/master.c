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

#include "../include/ecrt.h"
#include "globals.h"
#include "master.h"
#include "slave.h"
#include "types.h"
#include "device.h"
#include "command.h"
#include "ethernet.h"

/*****************************************************************************/

static struct attribute attr_slave_count = {
    .name = "slave_count",
    .owner = THIS_MODULE,
    .mode = S_IRUGO
};

static struct attribute *ec_def_attrs[] = {
    &attr_slave_count,
    NULL,
};

static struct sysfs_ops ec_sysfs_ops = {
    .show = &ec_show_master_attribute,
    .store = NULL
};

static struct kobj_type ktype_ec_master = {
    .release = ec_master_clear,
    .sysfs_ops = &ec_sysfs_ops,
    .default_attrs = ec_def_attrs
};

/*****************************************************************************/

/**
   Konstruktor des EtherCAT-Masters.
*/

int ec_master_init(ec_master_t *master, /**< EtherCAT-Master */
                   unsigned int index /**< Master-Index */
                   )
{
    EC_INFO("Initializing master %i.\n", index);

    master->index = index;
    master->slaves = NULL;
    master->device = NULL;
    master->reserved = 0;

    INIT_LIST_HEAD(&master->command_queue);
    INIT_LIST_HEAD(&master->domains);
    INIT_LIST_HEAD(&master->eoe_slaves);

    // Init kobject and add it to the hierarchy
    memset(&master->kobj, 0x00, sizeof(struct kobject));
    kobject_init(&master->kobj);
    master->kobj.ktype = &ktype_ec_master;
    if (kobject_set_name(&master->kobj, "ethercat%i", index)) {
        EC_ERR("Failed to set kobj name.\n");
        kobject_put(&master->kobj);
        return -1;
    }

    ec_command_init(&master->simple_command);
    ec_command_init(&master->watch_command);

    ec_master_reset(master);
    return 0;
}

/*****************************************************************************/

/**
   Destruktor eines EtherCAT-Masters.

   Entfernt alle Kommandos aus der Liste, löscht den Zeiger
   auf das Slave-Array und gibt die Prozessdaten frei.
*/

void ec_master_clear(struct kobject *kobj /**< KObject des Masters */)
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);

    EC_INFO("Clearing master %i...\n", master->index);

    ec_master_reset(master);

    if (master->device) {
        ec_device_clear(master->device);
        kfree(master->device);
    }

    ec_command_clear(&master->simple_command);
    ec_command_clear(&master->watch_command);

    EC_INFO("Master %i cleared.\n", master->index);
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
    ec_command_t *command, *next_c;
    ec_domain_t *domain, *next_d;
    ec_eoe_t *eoe, *next_eoe;

    // Alle Slaves entfernen
    if (master->slaves) {
        for (i = 0; i < master->slave_count; i++)
            ec_slave_clear(master->slaves + i);
        kfree(master->slaves);
        master->slaves = NULL;
    }
    master->slave_count = 0;

    // Kommando-Warteschlange leeren
    list_for_each_entry_safe(command, next_c, &master->command_queue, queue) {
        command->state = EC_CMD_ERROR;
        list_del_init(&command->queue);
    }

    // Domain-Liste leeren
    list_for_each_entry_safe(domain, next_d, &master->domains, list) {
        list_del(&domain->list);
        kobject_del(&domain->kobj);
        kobject_put(&domain->kobj);
    }

    // EOE-Liste leeren
    list_for_each_entry_safe(eoe, next_eoe, &master->eoe_slaves, list) {
        list_del(&eoe->list);
        ec_eoe_clear(eoe);
        kfree(eoe);
    }

    master->command_index = 0;
    master->debug_level = 0;
    master->timeout = 500; // 500us

    master->slaves_responding = 0;
    master->slave_states = EC_SLAVE_STATE_UNKNOWN;

    master->stats.timeouts = 0;
    master->stats.delayed = 0;
    master->stats.corrupted = 0;
    master->stats.unmatched = 0;
    master->stats.eoe_errors = 0;
    master->stats.t_last = 0;
}

/*****************************************************************************/

/**
   Stellt ein Kommando in die Warteschlange.
*/

void ec_master_queue_command(ec_master_t *master, /**< EtherCAT-Master */
                             ec_command_t *command /**< Kommando */
                             )
{
    ec_command_t *queued_command;

    // Ist das Kommando schon in der Warteschlange?
    list_for_each_entry(queued_command, &master->command_queue, queue) {
        if (queued_command == command) {
            command->state = EC_CMD_QUEUED;
            if (unlikely(master->debug_level))
                EC_WARN("command already queued.\n");
            return;
        }
    }

    list_add_tail(&command->queue, &master->command_queue);
    command->state = EC_CMD_QUEUED;
}

/*****************************************************************************/

/**
   Sendet die Kommandos in der Warteschlange.

   \return 0 bei Erfolg, sonst < 0
*/

void ec_master_send_commands(ec_master_t *master /**< EtherCAT-Master */)
{
    ec_command_t *command;
    size_t command_size;
    uint8_t *frame_data, *cur_data;
    void *follows_word;
    cycles_t start = 0, end;
    unsigned int frame_count, more_commands_waiting;

    frame_count = 0;

    if (unlikely(master->debug_level > 0)) {
        EC_DBG("ec_master_send_commands\n");
        start = get_cycles();
    }

    do {
        // Zeiger auf Socket-Buffer holen
        frame_data = ec_device_tx_data(master->device);
        cur_data = frame_data + EC_FRAME_HEADER_SIZE;
        follows_word = NULL;
        more_commands_waiting = 0;

        // Aktuellen Frame mit Kommandos füllen
        list_for_each_entry(command, &master->command_queue, queue) {
            if (command->state != EC_CMD_QUEUED) continue;

            // Passt das aktuelle Kommando noch in den aktuellen Rahmen?
            command_size = EC_COMMAND_HEADER_SIZE + command->data_size
                + EC_COMMAND_FOOTER_SIZE;
            if (cur_data - frame_data + command_size > EC_MAX_FRAME_SIZE) {
                more_commands_waiting = 1;
                break;
            }

            command->state = EC_CMD_SENT;
            command->index = master->command_index++;

            if (unlikely(master->debug_level > 0))
                EC_DBG("adding command 0x%02X\n", command->index);

            // Command-Following-Flag im letzten Kommando setzen
            if (follows_word)
                EC_WRITE_U16(follows_word, EC_READ_U16(follows_word) | 0x8000);

            // EtherCAT command header
            EC_WRITE_U8 (cur_data,     command->type);
            EC_WRITE_U8 (cur_data + 1, command->index);
            EC_WRITE_U32(cur_data + 2, command->address.logical);
            EC_WRITE_U16(cur_data + 6, command->data_size & 0x7FF);
            EC_WRITE_U16(cur_data + 8, 0x0000);
            follows_word = cur_data + 6;
            cur_data += EC_COMMAND_HEADER_SIZE;

            // EtherCAT command data
            memcpy(cur_data, command->data, command->data_size);
            cur_data += command->data_size;

            // EtherCAT command footer
            EC_WRITE_U16(cur_data, 0x0000); // Working counter
            cur_data += EC_COMMAND_FOOTER_SIZE;
        }

        if (cur_data - frame_data == EC_FRAME_HEADER_SIZE) {
            if (unlikely(master->debug_level > 0))
                EC_DBG("nothing to send.\n");
            break;
        }

        // EtherCAT frame header
        EC_WRITE_U16(frame_data, ((cur_data - frame_data
                                   - EC_FRAME_HEADER_SIZE) & 0x7FF) | 0x1000);

        // Rahmen auffüllen
        while (cur_data - frame_data < EC_MIN_FRAME_SIZE)
            EC_WRITE_U8(cur_data++, 0x00);

        if (unlikely(master->debug_level > 0))
            EC_DBG("frame size: %i\n", cur_data - frame_data);

        // Send frame
        ec_device_send(master->device, cur_data - frame_data);
        frame_count++;
    }
    while (more_commands_waiting);

    if (unlikely(master->debug_level > 0)) {
        end = get_cycles();
        EC_DBG("ec_master_send_commands sent %i frames in %ius.\n",
               frame_count, (u32) (end - start) * 1000 / cpu_khz);
    }
}

/*****************************************************************************/

/**
   Processes a received frame.

   This function is called by the network driver for every received frame.

   \return 0 bei Erfolg, sonst < 0
*/

void ec_master_receive(ec_master_t *master, /**< EtherCAT-Master */
                      const uint8_t *frame_data, /**< Empfangene Daten */
                      size_t size /**< Anzahl empfangene Datenbytes */
                      )
{
    size_t frame_size, data_size;
    uint8_t command_type, command_index;
    unsigned int cmd_follows, matched;
    const uint8_t *cur_data;
    ec_command_t *command;

    if (unlikely(size < EC_FRAME_HEADER_SIZE)) {
        master->stats.corrupted++;
        ec_master_output_stats(master);
        return;
    }

    cur_data = frame_data;

    // Länge des gesamten Frames prüfen
    frame_size = EC_READ_U16(cur_data) & 0x07FF;
    cur_data += EC_FRAME_HEADER_SIZE;

    if (unlikely(frame_size > size)) {
        master->stats.corrupted++;
        ec_master_output_stats(master);
        return;
    }

    cmd_follows = 1;
    while (cmd_follows) {
        // Kommando-Header auswerten
        command_type  = EC_READ_U8 (cur_data);
        command_index = EC_READ_U8 (cur_data + 1);
        data_size     = EC_READ_U16(cur_data + 6) & 0x07FF;
        cmd_follows   = EC_READ_U16(cur_data + 6) & 0x8000;
        cur_data += EC_COMMAND_HEADER_SIZE;

        if (unlikely(cur_data - frame_data
                     + data_size + EC_COMMAND_FOOTER_SIZE > size)) {
            master->stats.corrupted++;
            ec_master_output_stats(master);
            return;
        }

        // Suche passendes Kommando in der Liste
        matched = 0;
        list_for_each_entry(command, &master->command_queue, queue) {
            if (command->state == EC_CMD_SENT
                && command->type == command_type
                && command->index == command_index
                && command->data_size == data_size) {
                matched = 1;
                break;
            }
        }

        // Kein passendes Kommando in der Liste gefunden
        if (!matched) {
            master->stats.unmatched++;
            ec_master_output_stats(master);
            cur_data += data_size + EC_COMMAND_FOOTER_SIZE;
            continue;
        }

        // Empfangene Daten in Kommando-Datenspeicher kopieren
        memcpy(command->data, cur_data, data_size);
        cur_data += data_size;

        // Working-Counter setzen
        command->working_counter = EC_READ_U16(cur_data);
        cur_data += EC_COMMAND_FOOTER_SIZE;

        // Kommando aus der Warteschlange entfernen
        command->state = EC_CMD_RECEIVED;
        list_del_init(&command->queue);
    }
}

/*****************************************************************************/

/**
   Sendet ein einzelnes Kommando und wartet auf den Empfang.

   Wenn der Slave nicht antwortet, wird das Kommando
   nochmals gesendet.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_master_simple_io(ec_master_t *master, /**< EtherCAT-Master */
                        ec_command_t *command /**< Kommando */
                        )
{
    unsigned int response_tries_left;

    response_tries_left = 10;

    while (1)
    {
        ec_master_queue_command(master, command);
        ecrt_master_sync_io(master);

        if (command->state == EC_CMD_RECEIVED) {
            if (likely(command->working_counter))
                return 0;
        }
        else if (command->state == EC_CMD_TIMEOUT) {
            EC_ERR("Simple-IO TIMEOUT!\n");
            return -1;
        }
        else if (command->state == EC_CMD_ERROR) {
            EC_ERR("Simple-IO command error!\n");
            return -1;
        }

        // Keine direkte Antwort. Dem Slave Zeit lassen...
        udelay(100);

        if (unlikely(--response_tries_left)) {
            EC_ERR("No response in simple-IO!\n");
            return -1;
        }
    }
}

/*****************************************************************************/

/**
   Durchsucht den EtherCAT-Bus nach Slaves.

   Erstellt ein Array mit allen Slave-Informationen die für den
   weiteren Betrieb notwendig sind.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_master_bus_scan(ec_master_t *master /**< EtherCAT-Master */)
{
    ec_slave_t *slave;
    ec_slave_ident_t *ident;
    unsigned int i;
    ec_command_t *command;
    ec_eoe_t *eoe;

    if (master->slaves || master->slave_count) {
        EC_ERR("Slave scan already done!\n");
        return -1;
    }

    command = &master->simple_command;

    // Determine number of slaves on bus
    if (ec_command_brd(command, 0x0000, 4)) return -1;
    if (unlikely(ec_master_simple_io(master, command))) return -1;
    master->slave_count = command->working_counter;
    EC_INFO("Found %i slaves on bus.\n", master->slave_count);

    if (!master->slave_count) return 0;

    if (!(master->slaves = (ec_slave_t *)
          kmalloc(master->slave_count * sizeof(ec_slave_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate slaves!\n");
        return -1;
    }

    // Init slaves
    for (i = 0; i < master->slave_count; i++) {
        slave = master->slaves + i;
        ec_slave_init(slave, master);
        slave->ring_position = i;
        slave->station_address = i + 1;
    }

    // For every slave on the bus
    for (i = 0; i < master->slave_count; i++) {
        slave = master->slaves + i;

        // Write station address
        if (ec_command_apwr(command, slave->ring_position,
                            0x0010, sizeof(uint16_t))) return -1;
        EC_WRITE_U16(command->data, slave->station_address);
        if (unlikely(ec_master_simple_io(master, command))) {
            EC_ERR("Writing station address failed on slave %i!\n", i);
            return -1;
        }

        // Fetch all slave information
        if (ec_slave_fetch(slave)) return -1;

        // Search for identification in "database"
        ident = slave_idents;
        while (ident->type) {
            if (unlikely(ident->vendor_id == slave->sii_vendor_id
                         && ident->product_code == slave->sii_product_code)) {
                slave->type = ident->type;
                break;
            }
            ident++;
        }

        if (!slave->type)
            EC_WARN("Unknown slave device (vendor 0x%08X, code 0x%08X) at"
                    " position %i.\n", slave->sii_vendor_id,
                    slave->sii_product_code, i);

        // Does the slave support EoE?
        if (slave->sii_mailbox_protocols & EC_MBOX_EOE) {
            if (!(eoe = (ec_eoe_t *) kmalloc(sizeof(ec_eoe_t), GFP_KERNEL))) {
                EC_ERR("Failed to allocate EoE-Object.\n");
                return -1;
            }

            ec_eoe_init(eoe, slave);
            list_add_tail(&eoe->list, &master->eoe_slaves);
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Statistik-Ausgaben während des zyklischen Betriebs.

   Diese Funktion sorgt dafür, dass Statistiken während des zyklischen
   Betriebs bei Bedarf, aber nicht zu oft ausgegeben werden.

   Die Ausgabe erfolgt gesammelt höchstens einmal pro Sekunde.
*/

void ec_master_output_stats(ec_master_t *master /**< EtherCAT-Master */)
{
    cycles_t t_now = get_cycles();

    if (unlikely((u32) (t_now - master->stats.t_last) / cpu_khz > 1000)) {
        if (master->stats.timeouts) {
            EC_WARN("%i commands TIMED OUT!\n", master->stats.timeouts);
            master->stats.timeouts = 0;
        }
        if (master->stats.delayed) {
            EC_WARN("%i frame(s) DELAYED!\n", master->stats.delayed);
            master->stats.delayed = 0;
        }
        if (master->stats.corrupted) {
            EC_WARN("%i frame(s) CORRUPTED!\n", master->stats.corrupted);
            master->stats.corrupted = 0;
        }
        if (master->stats.unmatched) {
            EC_WARN("%i command(s) UNMATCHED!\n", master->stats.unmatched);
            master->stats.unmatched = 0;
        }
        if (master->stats.eoe_errors) {
            EC_WARN("%i EOE ERROR(S)!\n", master->stats.eoe_errors);
            master->stats.eoe_errors = 0;
        }
        master->stats.t_last = t_now;
    }
}

/*****************************************************************************/

/**
   Wandelt eine ASCII-kodierte Bus-Adresse in einen Slave-Zeiger.

   Gültige Adress-Strings sind Folgende:
   - \a "X" = der X. Slave im Bus,
   - \a "X:Y" = der Y. Slave hinter dem X. Buskoppler,
   - \a "#X" = der Slave mit dem Alias X,
   - \a "#X:Y" = der Y. Slave hinter dem Buskoppler mit dem Alias X.

   X und Y fangen immer bei 0 an und können auch hexadezimal oder oktal
   angegeben werden (mit entsprechendem Prefix).

   \return Zeiger auf Slave bei Erfolg, sonst NULL
*/

ec_slave_t *ecrt_master_get_slave(const ec_master_t *master, /**< Master */
                                  const char *address /**< Address-String */
                                  )
{
    unsigned long first, second;
    char *remainder, *remainder2;
    unsigned int i, alias_requested, alias_slave_index, alias_found;
    int coupler_idx, slave_idx;
    ec_slave_t *slave;

    if (!address || address[0] == 0) return NULL;

    alias_requested = 0;
    alias_slave_index = 0;
    if (address[0] == '#') {
        alias_requested = 1;
        address++;
    }

    first = simple_strtoul(address, &remainder, 0);
    if (remainder == address) {
        EC_ERR("Slave address \"%s\" - First number empty!\n", address);
        return NULL;
    }

    if (alias_requested) {
        alias_found = 0;
        for (i = 0; i < master->slave_count; i++) {
            if (master->slaves[i].sii_alias == first) {
                alias_slave_index = i;
                alias_found = 1;
                break;
            }
        }
        if (!alias_found) {
            EC_ERR("Slave address \"%s\" - Alias not found!\n", address);
            return NULL;
        }
    }

    if (!remainder[0]) { // absolute position
        if (alias_requested) {
            return master->slaves + alias_slave_index;
        }
        else {
            if (first < master->slave_count) {
                return master->slaves + first;
            }
            EC_ERR("Slave address \"%s\" - Absolute position invalid!\n",
                   address);
        }
    }
    else if (remainder[0] == ':') { // field position
        remainder++;
        second = simple_strtoul(remainder, &remainder2, 0);

        if (remainder2 == remainder) {
            EC_ERR("Slave address \"%s\" - Second number empty!\n", address);
            return NULL;
        }

        if (remainder2[0]) {
            EC_ERR("Slave address \"%s\" - Invalid trailer!\n", address);
            return NULL;
        }

        if (alias_requested) {
            for (i = alias_slave_index + 1; i < master->slave_count; i++) {
                slave = master->slaves + i;
                if (!slave->type ||
                    slave->type->special == EC_TYPE_BUS_COUPLER) break;
                if (i - alias_slave_index == second) return slave;
            }
            EC_ERR("Slave address \"%s\" - Bus coupler %i has no %lu. slave"
                   " following!\n", address,
                   (master->slaves + alias_slave_index)->ring_position,
                   second);
            return NULL;
        }
        else {
            coupler_idx = -1;
            slave_idx = 0;
            for (i = 0; i < master->slave_count; i++, slave_idx++) {
                slave = master->slaves + i;
                if (!slave->type) continue;
                if (slave->type->special == EC_TYPE_BUS_COUPLER) {
                    coupler_idx++;
                    slave_idx = 0;
                }
                if (coupler_idx == first && slave_idx == second) return slave;
            }
        }
    }
    else
        EC_ERR("Slave address \"%s\" - Invalid format!\n", address);

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
   Initialisiert eine Sync-Manager-Konfigurationsseite mit EEPROM-Daten.

   Der mit \a data referenzierte Speicher muss mindestens EC_SYNC_SIZE Bytes
   groß sein.
*/

void ec_eeprom_sync_config(const ec_eeprom_sync_t *sync, /**< Sync-Manager */
                           uint8_t *data /**> Zeiger auf Konfig.-Speicher */
                           )
{
    EC_WRITE_U16(data,     sync->physical_start_address);
    EC_WRITE_U16(data + 2, sync->length);
    EC_WRITE_U8 (data + 4, sync->control_register);
    EC_WRITE_U8 (data + 5, 0x00); // status byte (read only)
    EC_WRITE_U16(data + 6, sync->enable ? 0x0001 : 0x0000); // enable
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

/*****************************************************************************/

/**
   Formatiert Attribut-Daten für lesenden Zugriff im SysFS

   \return Anzahl Bytes im Speicher
*/

ssize_t ec_show_master_attribute(struct kobject *kobj, /**< KObject */
                                 struct attribute *attr, /**< Attribut */
                                 char *buffer /**< Speicher für die Daten */
                                 )
{
    ec_master_t *master = container_of(kobj, ec_master_t, kobj);

    if (attr == &attr_slave_count) {
        return sprintf(buffer, "%i\n", master->slave_count);
    }

    return 0;
}

/*****************************************************************************/

/**
   Gibt Überwachungsinformationen aus.
*/

void ec_master_process_watch_command(ec_master_t *master
                                     /**< EtherCAT-Master */
                                     )
{
    unsigned int first;

    if (unlikely(master->watch_command.state == EC_CMD_INIT)) return;

    first = 1;

    if (master->watch_command.working_counter != master->slaves_responding ||
        master->watch_command.data[0] != master->slave_states)
    {
        master->slaves_responding = master->watch_command.working_counter;
        master->slave_states = master->watch_command.data[0];

        EC_INFO("%i slave%s responding (", master->slaves_responding,
                master->slaves_responding == 1 ? "" : "s");

        if (master->slave_states & EC_SLAVE_STATE_INIT) {
            printk("INIT");
            first = 0;
        }
        if (master->slave_states & EC_SLAVE_STATE_PREOP) {
            if (!first) printk(", ");
            printk("PREOP");
            first = 0;
        }
        if (master->slave_states & EC_SLAVE_STATE_SAVEOP) {
            if (!first) printk(", ");
            printk("SAVEOP");
            first = 0;
        }
        if (master->slave_states & EC_SLAVE_STATE_OP) {
            if (!first) printk(", ");
            printk("OP");
        }
        printk(")\n");
    }
}

/******************************************************************************
 *
 * Echtzeitschnittstelle
 *
 *****************************************************************************/

/**
   Erstellt eine neue Domäne.

   \return Zeiger auf die Domäne bei Erfolg, sonst NULL.
*/

ec_domain_t *ecrt_master_create_domain(ec_master_t *master /**< Master */)
{
    ec_domain_t *domain, *last_domain;
    unsigned int index;

    if (!(domain = (ec_domain_t *) kmalloc(sizeof(ec_domain_t), GFP_KERNEL))) {
        EC_ERR("Error allocating domain memory!\n");
        goto out_return;
    }

    if (list_empty(&master->domains)) index = 0;
    else {
        last_domain = list_entry(master->domains.prev, ec_domain_t, list);
        index = last_domain->index + 1;
    }

    if (ec_domain_init(domain, master, index)) {
        EC_ERR("Failed to init domain.\n");
        goto out_return;
    }

    if (kobject_add(&domain->kobj)) {
        EC_ERR("Failed to add domain kobject.\n");
        goto out_put;
    }

    list_add_tail(&domain->list, &master->domains);
    return domain;

 out_put:
    kobject_put(&domain->kobj);
 out_return:
    return NULL;
}

/*****************************************************************************/

/**
   Konfiguriert alle Slaves und setzt den Operational-Zustand.

   Führt die komplette Konfiguration und Aktivierunge aller registrierten
   Slaves durch. Setzt Sync-Manager und FMMUs, führt die entsprechenden
   Zustandsübergänge durch, bis der Slave betriebsbereit ist.

   \return 0 bei Erfolg, sonst < 0
*/

int ecrt_master_activate(ec_master_t *master /**< EtherCAT-Master */)
{
    unsigned int i, j;
    ec_slave_t *slave;
    ec_command_t *command;
    const ec_sync_t *sync;
    const ec_slave_type_t *type;
    const ec_fmmu_t *fmmu;
    uint32_t domain_offset;
    ec_domain_t *domain;
    ec_eeprom_sync_t *eeprom_sync, mbox_sync;

    command = &master->simple_command;

    if (ec_command_brd(&master->watch_command, 0x130, 2)) {
        EC_ERR("Failed to allocate watchdog command!\n");
        return -1;
    }

    // Domains erstellen
    domain_offset = 0;
    list_for_each_entry(domain, &master->domains, list) {
        if (ec_domain_alloc(domain, domain_offset)) {
            EC_ERR("Failed to allocate domain %X!\n", (u32) domain);
            return -1;
        }
        domain_offset += domain->data_size;
    }

    // Slaves aktivieren
    for (i = 0; i < master->slave_count; i++) {
        slave = master->slaves + i;

        // Change state to INIT
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_INIT)))
            return -1;

        // Check if slave was registered...
        type = slave->type;
        if (!type) EC_WARN("Slave %i has unknown type!\n", i);

        // Check and reset CRC fault counters
        ec_slave_check_crc(slave);

        // Resetting FMMUs
        if (slave->base_fmmu_count) {
            if (ec_command_npwr(command, slave->station_address, 0x0600,
                                EC_FMMU_SIZE * slave->base_fmmu_count))
                return -1;
            memset(command->data, 0x00, EC_FMMU_SIZE * slave->base_fmmu_count);
            if (unlikely(ec_master_simple_io(master, command))) {
                EC_ERR("Resetting FMMUs failed on slave %i!\n",
                       slave->ring_position);
                return -1;
            }
        }

        // Resetting Sync Manager channels
        if (slave->base_sync_count) {
            if (ec_command_npwr(command, slave->station_address, 0x0800,
                                EC_SYNC_SIZE * slave->base_sync_count))
                return -1;
            memset(command->data, 0x00, EC_SYNC_SIZE * slave->base_sync_count);
            if (unlikely(ec_master_simple_io(master, command))) {
                EC_ERR("Resetting sync managers failed on slave %i!\n",
                       slave->ring_position);
                return -1;
            }
        }

        // Set Sync Managers

        // Known slave type, take type's SM information
        if (type) {
            for (j = 0; type->sync_managers[j] && j < EC_MAX_SYNC; j++) {
                sync = type->sync_managers[j];
                if (ec_command_npwr(command, slave->station_address,
                                    0x0800 + j * EC_SYNC_SIZE, EC_SYNC_SIZE))
                    return -1;
                ec_sync_config(sync, command->data);
                if (unlikely(ec_master_simple_io(master, command))) {
                    EC_ERR("Setting sync manager %i failed on slave %i!\n",
                           j, slave->ring_position);
                    return -1;
                }
            }
        }

        // Unknown slave type, but has mailbox
        else if (slave->sii_mailbox_protocols) {

            // Does the device supply SM configurations in its EEPROM?
	    if (!list_empty(&slave->eeprom_syncs)) {
		list_for_each_entry(eeprom_sync, &slave->eeprom_syncs, list) {
		    EC_INFO("Sync manager %i...\n", eeprom_sync->index);
		    if (ec_command_npwr(command, slave->station_address,
					0x800 + eeprom_sync->index *
					EC_SYNC_SIZE,
					EC_SYNC_SIZE)) return -1;
		    ec_eeprom_sync_config(eeprom_sync, command->data);
		    if (unlikely(ec_master_simple_io(master, command))) {
			EC_ERR("Setting sync manager %i failed on slave %i!\n",
			       eeprom_sync->index, slave->ring_position);
			return -1;
		    }
		}
            }

            // No sync manager information; guess mailbox settings
	    else {
		mbox_sync.physical_start_address =
                    slave->sii_rx_mailbox_offset;
		mbox_sync.length = slave->sii_rx_mailbox_size;
		mbox_sync.control_register = 0x26;
		mbox_sync.enable = 1;
		if (ec_command_npwr(command, slave->station_address,
				    0x800,EC_SYNC_SIZE)) return -1;
		ec_eeprom_sync_config(&mbox_sync, command->data);
		if (unlikely(ec_master_simple_io(master, command))) {
		    EC_ERR("Setting sync manager 0 failed on slave %i!\n",
			   slave->ring_position);
		    return -1;
		}

		mbox_sync.physical_start_address =
                    slave->sii_tx_mailbox_offset;
		mbox_sync.length = slave->sii_tx_mailbox_size;
		mbox_sync.control_register = 0x22;
		mbox_sync.enable = 1;
		if (ec_command_npwr(command, slave->station_address,
				    0x808, EC_SYNC_SIZE)) return -1;
		ec_eeprom_sync_config(&mbox_sync, command->data);
		if (unlikely(ec_master_simple_io(master, command))) {
		    EC_ERR("Setting sync manager 1 failed on slave %i!\n",
			   slave->ring_position);
		    return -1;
		}
	    }
	    EC_INFO("Mailbox configured for unknown slave %i\n",
		    slave->ring_position);
        }

        // Change state to PREOP
        if (unlikely(ec_slave_state_change(slave, EC_SLAVE_STATE_PREOP)))
            return -1;

        // Stop activation here for slaves without type
        if (!type) continue;

        // Slaves that are not registered are only brought into PREOP
        // state -> nice blinking and mailbox communication possible
        if (!slave->registered && !slave->type->special) {
            EC_WARN("Slave %i was not registered!\n", slave->ring_position);
            continue;
        }

        // Set FMMUs
        for (j = 0; j < slave->fmmu_count; j++) {
            fmmu = &slave->fmmus[j];
            if (ec_command_npwr(command, slave->station_address,
                                0x0600 + j * EC_FMMU_SIZE, EC_FMMU_SIZE))
                return -1;
            ec_fmmu_config(fmmu, command->data);
            if (unlikely(ec_master_simple_io(master, command))) {
                EC_ERR("Setting FMMU %i failed on slave %i!\n",
                       j, slave->ring_position);
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

    master->slaves_responding = 0;
    master->slave_states = EC_SLAVE_STATE_INIT;

    return 0;
}

/*****************************************************************************/

/**
   Setzt alle Slaves zurück in den Init-Zustand.
*/

void ecrt_master_deactivate(ec_master_t *master /**< EtherCAT-Master */)
{
    ec_slave_t *slave;
    unsigned int i;

    for (i = 0; i < master->slave_count; i++)
    {
        slave = master->slaves + i;
        ec_slave_check_crc(slave);
        ec_slave_state_change(slave, EC_SLAVE_STATE_INIT);
    }
}


/*****************************************************************************/

/**
   Lädt die SDO-Dictionaries aller Slaves.

   Slaves, die kein CoE unterstützen, werden ausgelassen.

   \return 0 wenn alles ok, sonst < 0
*/

int ecrt_master_fetch_sdo_lists(ec_master_t *master /**< EtherCAT-Master */)
{
    ec_slave_t *slave;
    unsigned int i;

    for (i = 0; i < master->slave_count; i++) {
        slave = master->slaves + i;
        if (slave->sii_mailbox_protocols & EC_MBOX_COE) {
            if (unlikely(ec_slave_fetch_sdo_list(slave))) {
                EC_ERR("Failed to fetch SDO list on slave %i!\n",
                       slave->ring_position);
                return -1;
            }
        }
    }

    return 0;
}

/*****************************************************************************/

/**
   Sendet und empfängt Kommandos synchron.
*/

void ecrt_master_sync_io(ec_master_t *master)
{
    ec_command_t *command, *n;
    unsigned int commands_sent;
    cycles_t t_start, t_end, t_timeout;

    // Kommandos senden
    ecrt_master_async_send(master);

    t_start = get_cycles(); // Sendezeit nehmen
    t_timeout = (cycles_t) master->timeout * (cpu_khz / 1000);

    while (1) // Aktiv auf Empfang warten
    {
        ec_device_call_isr(master->device);

        t_end = get_cycles(); // Aktuelle Zeit nehmen
        if (t_end - t_start >= t_timeout) break; // Timeout

        commands_sent = 0;
        list_for_each_entry_safe(command, n, &master->command_queue, queue) {
            if (command->state == EC_CMD_RECEIVED)
                list_del_init(&command->queue);
            else if (command->state == EC_CMD_SENT)
                commands_sent++;
        }

        if (!commands_sent) break;
    }

    // Zeit abgelaufen. Alle verbleibenden Kommandos entfernen.
    list_for_each_entry_safe(command, n, &master->command_queue, queue) {
        switch (command->state) {
            case EC_CMD_SENT:
            case EC_CMD_QUEUED:
                command->state = EC_CMD_TIMEOUT;
                master->stats.timeouts++;
                ec_master_output_stats(master);
                break;
            case EC_CMD_RECEIVED:
                master->stats.delayed++;
                ec_master_output_stats(master);
                break;
            default:
                break;
        }
        list_del_init(&command->queue);
    }
}

/*****************************************************************************/

/**
   Sendet Kommandos asynchron.
*/

void ecrt_master_async_send(ec_master_t *master)
{
    ec_command_t *command, *n;

    if (unlikely(!master->device->link_state)) {
        // Link DOWN, keines der Kommandos kann gesendet werden.
        list_for_each_entry_safe(command, n, &master->command_queue, queue) {
            command->state = EC_CMD_ERROR;
            list_del_init(&command->queue);
        }

        // Device-Zustand abfragen
        ec_device_call_isr(master->device);
        return;
    }

    // Rahmen senden
    ec_master_send_commands(master);
}

/*****************************************************************************/

/**
   Empfängt Kommandos asynchron.
*/

void ecrt_master_async_receive(ec_master_t *master)
{
    ec_command_t *command, *next;

    ec_device_call_isr(master->device);

    // Alle empfangenen Kommandos aus der Liste entfernen
    list_for_each_entry_safe(command, next, &master->command_queue, queue)
        if (command->state == EC_CMD_RECEIVED) list_del_init(&command->queue);

    // Alle verbleibenden Kommandos entfernen.
    list_for_each_entry_safe(command, next, &master->command_queue, queue) {
        switch (command->state) {
            case EC_CMD_SENT:
            case EC_CMD_QUEUED:
                command->state = EC_CMD_TIMEOUT;
                master->stats.timeouts++;
                ec_master_output_stats(master);
                break;
            default:
                break;
        }
        list_del_init(&command->queue);
    }
}

/*****************************************************************************/

/**
   Bereitet Synchronen Datenverkehr vor.

   Fürgt einmal die Kommandos aller Domains zur Warteschlange hinzu, sendet
   diese ab und wartet so lange, bis diese anschließend problemlos empfangen
   werden können.
*/

void ecrt_master_prepare_async_io(ec_master_t *master)
{
    ec_domain_t *domain;
    cycles_t t_start, t_end, t_timeout;

    // Kommandos aller Domains in die Warteschlange setzen
    list_for_each_entry(domain, &master->domains, list)
        ecrt_domain_queue(domain);

    ecrt_master_async_send(master);

    t_start = get_cycles(); // Sendezeit nehmen
    t_timeout = (cycles_t) master->timeout * (cpu_khz / 1000);

    // Aktiv warten!
    while (1) {
        t_end = get_cycles();
        if (t_end - t_start >= t_timeout) break;
    }
}

/*****************************************************************************/

/**
   Führt Routinen im zyklischen Betrieb aus.
*/

void ecrt_master_run(ec_master_t *master /**< EtherCAT-Master */)
{
    // Statistiken ausgeben
    ec_master_output_stats(master);

    // Watchdog-Kommando
    ec_master_process_watch_command(master);
    ec_master_queue_command(master, &master->watch_command);

    // Ethernet-over-EtherCAT
    ec_master_run_eoe(master);
}

/*****************************************************************************/

/**
   Setzt die Debug-Ebene des Masters.

   Folgende Debug-Level sind definiert:

   - 1: Nur Positionsmarken in bestimmten Funktionen
   - 2: Komplette Frame-Inhalte
*/

void ecrt_master_debug(ec_master_t *master, /**< EtherCAT-Master */
                       int level /**< Debug-Level */
                       )
{
    if (level != master->debug_level) {
        master->debug_level = level;
        EC_INFO("Master debug level set to %i.\n", level);
    }
}

/*****************************************************************************/

/**
   Gibt alle Informationen zum Master aus.

   Verbosity:
     0 - Nur Slavetypen und Adressen
     1 - mit EEPROM-Informationen
    >1 - mit SDO-Dictionaries
*/

void ecrt_master_print(const ec_master_t *master, /**< EtherCAT-Master */
                       unsigned int verbosity /**< Geschwätzigkeit */
                       )
{
    unsigned int i;
    ec_eoe_t *eoe;

    EC_INFO("*** Begin master information ***\n");
    if (master->slave_count) {
        EC_INFO("Slave list:\n");
        for (i = 0; i < master->slave_count; i++)
            ec_slave_print(&master->slaves[i], verbosity);
    }
    if (!list_empty(&master->eoe_slaves)) {
        EC_INFO("Ethernet-over-EtherCAT (EoE) objects:\n");
        list_for_each_entry(eoe, &master->eoe_slaves, list) {
            ec_eoe_print(eoe);
        }
    }
    EC_INFO("*** End master information ***\n");
}

/*****************************************************************************/

void ec_master_run_eoe(ec_master_t *master /**< EtherCAT-Master */)
{
    ec_eoe_t *eoe;

    list_for_each_entry(eoe, &master->eoe_slaves, list) {
        ec_eoe_run(eoe);
    }
}

/*****************************************************************************/

EXPORT_SYMBOL(ecrt_master_create_domain);
EXPORT_SYMBOL(ecrt_master_activate);
EXPORT_SYMBOL(ecrt_master_deactivate);
EXPORT_SYMBOL(ecrt_master_fetch_sdo_lists);
EXPORT_SYMBOL(ecrt_master_prepare_async_io);
EXPORT_SYMBOL(ecrt_master_sync_io);
EXPORT_SYMBOL(ecrt_master_async_send);
EXPORT_SYMBOL(ecrt_master_async_receive);
EXPORT_SYMBOL(ecrt_master_run);
EXPORT_SYMBOL(ecrt_master_debug);
EXPORT_SYMBOL(ecrt_master_print);
EXPORT_SYMBOL(ecrt_master_get_slave);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
