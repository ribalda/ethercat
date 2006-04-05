/******************************************************************************
 *
 *  d o m a i n . c
 *
 *  Methoden für Gruppen von EtherCAT-Slaves.
 *
 *  $Id$
 *
 *****************************************************************************/

#include "globals.h"
#include "domain.h"
#include "master.h"

void ec_domain_clear_field_regs(ec_domain_t *);

/*****************************************************************************/

/**
   Konstruktor einer EtherCAT-Domäne.
*/

void ec_domain_init(ec_domain_t *domain, /**< Domäne */
                    ec_master_t *master /**< Zugehöriger Master */
                    )
{
    domain->master = master;
    domain->data_size = 0;
    domain->base_address = 0;
    domain->response_count = 0xFFFFFFFF;

    INIT_LIST_HEAD(&domain->field_regs);
    INIT_LIST_HEAD(&domain->commands);
}

/*****************************************************************************/

/**
   Destruktor einer EtherCAT-Domäne.
*/

void ec_domain_clear(ec_domain_t *domain /**< Domäne */)
{
    ec_command_t *command, *next;

    list_for_each_entry_safe(command, next, &domain->commands, list) {
        ec_command_clear(command);
        kfree(command);
    }

    ec_domain_clear_field_regs(domain);
}

/*****************************************************************************/

/**
   Registriert ein Feld in einer Domäne.

   \return 0 bei Erfolg, < 0 bei Fehler
*/

int ec_domain_reg_field(ec_domain_t *domain, /**< Domäne */
                        ec_slave_t *slave, /**< Slave */
                        const ec_sync_t *sync, /**< Sync-Manager */
                        uint32_t field_offset, /**< Datenfeld-Offset */
                        void **data_ptr /**< Adresse des Prozessdatenzeigers */
                        )
{
    ec_field_reg_t *field_reg;

    if (!(field_reg =
          (ec_field_reg_t *) kmalloc(sizeof(ec_field_reg_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate field registration.\n");
        return -1;
    }

    if (ec_slave_prepare_fmmu(slave, domain, sync)) {
        EC_ERR("FMMU configuration failed.\n");
        kfree(field_reg);
        return -1;
    }

    field_reg->slave = slave;
    field_reg->sync = sync;
    field_reg->field_offset = field_offset;
    field_reg->data_ptr = data_ptr;

    list_add_tail(&field_reg->list, &domain->field_regs);
    return 0;
}

/*****************************************************************************/

/**
   Gibt die Liste der registrierten Datenfelder frei.
*/

void ec_domain_clear_field_regs(ec_domain_t *domain)
{
    ec_field_reg_t *field_reg, *next;

    list_for_each_entry_safe(field_reg, next, &domain->field_regs, list) {
        list_del(&field_reg->list);
        kfree(field_reg);
    }
}

/*****************************************************************************/

/**
   Alloziert ein Prozessdatenkommando und fügt es in die Liste ein.
*/

int ec_domain_add_command(ec_domain_t *domain, /**< Domäne */
                          uint32_t offset, /**< Logisches Offset */
                          size_t data_size /**< Größe der Kommando-Daten */
                          )
{
    ec_command_t *command;

    if (!(command = kmalloc(sizeof(ec_command_t), GFP_KERNEL))) {
        EC_ERR("Failed to allocate domain command!\n");
        return -1;
    }

    ec_command_init(command);

    if (ec_command_lrw(command, offset, data_size)) {
        kfree(command);
        return -1;
    }

    list_add_tail(&command->list, &domain->commands);
    return 0;
}

/*****************************************************************************/

/**
   Erzeugt eine Domäne.

   Reserviert den Speicher einer Domäne, berechnet die logischen Adressen der
   FMMUs und setzt die Prozessdatenzeiger der registrierten Felder.

   \return 0 bei Erfolg, < 0 bei Fehler
*/

int ec_domain_alloc(ec_domain_t *domain, /**< Domäne */
                    uint32_t base_address /**< Logische Basisadresse */
                    )
{
    ec_field_reg_t *field_reg;
    ec_slave_t *slave;
    ec_fmmu_t *fmmu;
    unsigned int i, j, cmd_count;
    uint32_t field_off, field_off_cmd;
    uint32_t cmd_offset;
    size_t cmd_data_size;
    ec_command_t *command;

    domain->base_address = base_address;

    // Größe der Prozessdaten berechnen und Kommandos allozieren
    domain->data_size = 0;
    cmd_offset = base_address;
    cmd_data_size = 0;
    cmd_count = 0;
    for (i = 0; i < domain->master->slave_count; i++) {
        slave = &domain->master->slaves[i];
        for (j = 0; j < slave->fmmu_count; j++) {
            fmmu = &slave->fmmus[j];
            if (fmmu->domain == domain) {
                fmmu->logical_start_address = base_address + domain->data_size;
                domain->data_size += fmmu->sync->size;
                if (cmd_data_size + fmmu->sync->size > EC_MAX_DATA_SIZE) {
                    if (ec_domain_add_command(domain, cmd_offset,
                                              cmd_data_size)) return -1;
                    cmd_offset += cmd_data_size;
                    cmd_data_size = 0;
                    cmd_count++;
                }
                cmd_data_size += fmmu->sync->size;
            }
        }
    }

    // Letztes Kommando allozieren
    if (cmd_data_size) {
        if (ec_domain_add_command(domain, cmd_offset, cmd_data_size))
            return -1;
        cmd_count++;
    }

    if (!cmd_count) {
        EC_WARN("Domain 0x%08X contains no data!\n", (u32) domain);
        ec_domain_clear_field_regs(domain);
        return 0;
    }

    // Alle Prozessdatenzeiger setzen
    list_for_each_entry(field_reg, &domain->field_regs, list) {
        for (i = 0; i < field_reg->slave->fmmu_count; i++) {
            fmmu = &field_reg->slave->fmmus[i];
            if (fmmu->domain == domain && fmmu->sync == field_reg->sync) {
                field_off = fmmu->logical_start_address +
                    field_reg->field_offset;
                // Kommando suchen
                list_for_each_entry(command, &domain->commands, list) {
                    field_off_cmd = field_off - command->address.logical;
                    if (field_off >= command->address.logical &&
                        field_off_cmd < command->mem_size) {
                        *field_reg->data_ptr = command->data + field_off_cmd;
                    }
                }
                if (!field_reg->data_ptr) {
                    EC_ERR("Failed to assign data pointer!\n");
                    return -1;
                }
                break;
            }
        }
    }

    EC_INFO("Domain %X - Allocated %i bytes in %i command(s)\n",
            (u32) domain, domain->data_size, cmd_count);

    ec_domain_clear_field_regs(domain);

    return 0;
}

/*****************************************************************************/

/**
   Gibt die Anzahl der antwortenden Slaves aus.
*/

void ec_domain_response_count(ec_domain_t *domain, /**< Domäne */
                              unsigned int count /**< Neue Anzahl */
                              )
{
    if (count != domain->response_count) {
        domain->response_count = count;
        EC_INFO("Domain %08X state change - %i slave%s responding.\n",
                (u32) domain, count, count == 1 ? "" : "s");
    }
}

/******************************************************************************
 *
 * Echtzeitschnittstelle
 *
 *****************************************************************************/

/**
   Registriert ein Datenfeld innerhalb einer Domäne.

   - Ist \a data_ptr NULL, so wird der Slave nur auf den Typ überprüft.
   - Wenn \a field_count 0 ist, wird angenommen, dass 1 Feld registriert werden
     soll.
   - Wenn \a field_count größer als 1 ist, wird angenommen, dass \a data_ptr
     auf ein entsprechend großes Array zeigt.

   \return Zeiger auf den Slave bei Erfolg, sonst NULL
*/

ec_slave_t *ecrt_domain_register_field(ec_domain_t *domain,
                                       /**< Domäne */
                                       const char *address,
                                       /**< ASCII-Addresse des Slaves,
                                          siehe ecrt_master_get_slave() */
                                       const char *vendor_name,
                                       /**< Herstellername */
                                       const char *product_name,
                                       /**< Produktname */
                                       void **data_ptr,
                                       /**< Adresse des Zeigers auf die
                                          Prozessdaten */
                                       const char *field_name,
                                       /**< Name des Datenfeldes */
                                       unsigned int field_index,
                                       /**< Gibt an, ab welchem Feld mit
                                          Typ \a field_type gezählt
                                          werden soll. */
                                       unsigned int field_count
                                       /**< Anzahl Felder selben Typs */
                                       )
{
    ec_slave_t *slave;
    const ec_slave_type_t *type;
    ec_master_t *master;
    const ec_sync_t *sync;
    const ec_field_t *field;
    unsigned int field_counter, i, j, orig_field_index, orig_field_count;
    uint32_t field_offset;

    master = domain->master;

    // Adresse übersetzen
    if (!(slave = ecrt_master_get_slave(master, address))) return NULL;

    if (!(type = slave->type)) {
        EC_ERR("Slave \"%s\" (position %i) has unknown type!\n", address,
               slave->ring_position);
        return NULL;
    }

    if (strcmp(vendor_name, type->vendor_name) ||
        strcmp(product_name, type->product_name)) {
        EC_ERR("Invalid slave type at position %i - Requested: \"%s %s\","
               " found: \"%s %s\".\n", slave->ring_position, vendor_name,
               product_name, type->vendor_name, type->product_name);
        return NULL;
    }

    if (!data_ptr) {
        // Wenn data_ptr NULL, Slave als registriert ansehen (nicht warnen).
        slave->registered = 1;
    }

    if (!field_count) field_count = 1;
    orig_field_index = field_index;
    orig_field_count = field_count;

    field_counter = 0;
    for (i = 0; type->sync_managers[i]; i++) {
        sync = type->sync_managers[i];
        field_offset = 0;
        for (j = 0; sync->fields[j]; j++) {
            field = sync->fields[j];
            if (!strcmp(field->name, field_name)) {
                if (field_counter++ == field_index) {
                    if (data_ptr)
                        ec_domain_reg_field(domain, slave, sync, field_offset,
                                            data_ptr++);
                    if (!(--field_count)) return slave;
                    field_index++;
                }
            }
            field_offset += field->size;
        }
    }

    EC_ERR("Slave %i (\"%s %s\") registration mismatch: Field \"%s\","
           " index %i, count %i.\n", slave->ring_position, vendor_name,
           product_name, field_name, orig_field_index, orig_field_count);
    return NULL;
}

/*****************************************************************************/

/**
   Registriert eine ganze Liste von Datenfeldern innerhalb einer Domäne.

   Achtung: Die Liste muss mit einer NULL-Struktur ({}) abgeschlossen sein!

   \return 0 bei Erfolg, sonst < 0
*/

int ecrt_domain_register_field_list(ec_domain_t *domain,
                                    /**< Domäne */
                                    const ec_field_init_t *fields
                                    /**< Array mit Datenfeldern */
                                    )
{
    const ec_field_init_t *field;

    for (field = fields; field->slave_address; field++)
        if (!ecrt_domain_register_field(domain, field->slave_address,
                                        field->vendor_name,
                                        field->product_name, field->data_ptr,
                                        field->field_name, field->field_index,
                                        field->field_count))
            return -1;

    return 0;
}

/*****************************************************************************/

/**
   Setzt Prozessdaten-Kommandos in die Warteschlange des Masters.
*/

void ecrt_domain_queue(ec_domain_t *domain /**< Domäne */)
{
    ec_command_t *command;

    list_for_each_entry(command, &domain->commands, list) {
        ec_master_queue_command(domain->master, command);
    }
}

/*****************************************************************************/

/**
   Verarbeitet empfangene Prozessdaten.
*/

void ecrt_domain_process(ec_domain_t *domain /**< Domäne */)
{
    unsigned int working_counter_sum;
    ec_command_t *command;

    working_counter_sum = 0;

    list_for_each_entry(command, &domain->commands, list) {
        if (command->state == EC_CMD_RECEIVED) {
            working_counter_sum += command->working_counter;
        }
    }

    ec_domain_response_count(domain, working_counter_sum);
}

/*****************************************************************************/

/**
   Gibt den Status einer Domäne zurück.

   \return 0 wenn alle Kommandos empfangen wurden, sonst -1.
*/

int ecrt_domain_state(ec_domain_t *domain /**< Domäne */)
{
    ec_command_t *command;

    list_for_each_entry(command, &domain->commands, list) {
        if (command->state != EC_CMD_RECEIVED) return -1;
    }

    return 0;
}

/*****************************************************************************/

EXPORT_SYMBOL(ecrt_domain_register_field);
EXPORT_SYMBOL(ecrt_domain_register_field_list);
EXPORT_SYMBOL(ecrt_domain_queue);
EXPORT_SYMBOL(ecrt_domain_process);
EXPORT_SYMBOL(ecrt_domain_state);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
