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
    domain->data = NULL;
    domain->data_size = 0;
    domain->commands = NULL;
    domain->command_count = 0;
    domain->base_address = 0;
    domain->response_count = 0xFFFFFFFF;

    INIT_LIST_HEAD(&domain->field_regs);
}

/*****************************************************************************/

/**
   Destruktor einer EtherCAT-Domäne.
*/

void ec_domain_clear(ec_domain_t *domain /**< Domäne */)
{
    if (domain->data) kfree(domain->data);
    if (domain->commands) kfree(domain->commands);

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

    if (!(field_reg = (ec_field_reg_t *) kmalloc(sizeof(ec_field_reg_t),
                                                 GFP_KERNEL))) {
        EC_ERR("Failed to allocate field registration.\n");
        return -1;
    }

    if (ec_slave_set_fmmu(slave, domain, sync)) {
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
    unsigned int i, j, found;
    uint32_t data_offset;

    if (domain->data) {
        EC_ERR("Domain already allocated!\n");
        return -1;
    }

    domain->base_address = base_address;

    // Größe der Prozessdaten berechnen
    // und logische Adressen der FMMUs setzen
    domain->data_size = 0;
    for (i = 0; i < domain->master->slave_count; i++) {
        slave = &domain->master->slaves[i];
        for (j = 0; j < slave->fmmu_count; j++) {
            fmmu = &slave->fmmus[j];
            if (fmmu->domain == domain) {
                fmmu->logical_start_address = base_address + domain->data_size;
                domain->data_size += fmmu->sync->size;
            }
        }
    }

    if (!domain->data_size) {
        EC_WARN("Domain 0x%08X contains no data!\n", (u32) domain);
        ec_domain_clear_field_regs(domain);
        return 0;
    }

    // Prozessdaten allozieren
    if (!(domain->data = kmalloc(domain->data_size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate domain data!\n");
        return -1;
    }

    // Prozessdaten mit Nullen vorbelegen
    memset(domain->data, 0x00, domain->data_size);

    // Alle Prozessdatenzeiger setzen
    list_for_each_entry(field_reg, &domain->field_regs, list) {
        found = 0;
        for (i = 0; i < field_reg->slave->fmmu_count; i++) {
            fmmu = &field_reg->slave->fmmus[i];
            if (fmmu->domain == domain && fmmu->sync == field_reg->sync) {
                data_offset = fmmu->logical_start_address - base_address
                    + field_reg->field_offset;
                *field_reg->data_ptr = domain->data + data_offset;
                found = 1;
                break;
            }
        }

        if (!found) { // Sollte nie passieren
            EC_ERR("FMMU not found. Please report!\n");
            return -1;
        }
    }

    // Kommando-Array erzeugen
    domain->command_count = domain->data_size / EC_MAX_DATA_SIZE + 1;
    if (!(domain->commands = (ec_command_t *) kmalloc(sizeof(ec_command_t)
                                                      * domain->command_count,
                                                      GFP_KERNEL))) {
        EC_ERR("Failed to allocate domain command array!\n");
        return -1;
    }

    EC_INFO("Domain %X - Allocated %i bytes in %i command(s)\n",
            (u32) domain, domain->data_size, domain->command_count);

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

   \return Zeiger auf den Slave bei Erfolg, sonst NULL
*/

ec_slave_t *ecrt_domain_register_field(ec_domain_t *domain,
                                       /**< Domäne */
                                       const char *address,
                                       /**< ASCII-Addresse des Slaves,
                                          siehe ec_master_slave_address() */
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

    if (!field_count) {
        EC_ERR("field_count may not be 0!\n");
        return NULL;
    }

    master = domain->master;

    // Adresse übersetzen
    if (!(slave = ec_master_slave_address(master, address))) return NULL;

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
                                    ec_field_init_t *fields
                                    /**< Array mit Datenfeldern */
                                    )
{
    ec_field_init_t *field;

    for (field = fields; field->data_ptr; field++)
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
    unsigned int i;
    size_t size;
    off_t offset;

    offset = 0;
    for (i = 0; i < domain->command_count; i++) {
        size = domain->data_size - offset;
        if (size > EC_MAX_DATA_SIZE) size = EC_MAX_DATA_SIZE;
        ec_command_init_lrw(domain->commands + i,
                            domain->base_address + offset, size,
                            domain->data + offset);
        ec_master_queue_command(domain->master, domain->commands + i);
        offset += size;
    }
}

/*****************************************************************************/

/**
   Verarbeitet empfangene Prozessdaten.
*/

void ecrt_domain_process(ec_domain_t *domain /**< Domäne */)
{
    unsigned int working_counter_sum, i;
    ec_command_t *command;
    size_t size;
    off_t offset;

    working_counter_sum = 0;

    offset = 0;
    for (i = 0; i < domain->command_count; i++) {
        command = domain->commands + i;
        size = domain->data_size - offset;
        if (size > EC_MAX_DATA_SIZE) size = EC_MAX_DATA_SIZE;
        if (command->state == EC_CMD_RECEIVED) {
            // Daten vom Kommando- in den Prozessdatenspeicher kopieren
            memcpy(domain->data + offset, command->data, size);
            working_counter_sum += command->working_counter;
        }
        else if (unlikely(domain->master->debug_level)) {
            EC_DBG("process data command not received!\n");
        }
        offset += size;
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
    unsigned int i;
    ec_command_t *command;

    for (i = 0; i < domain->command_count; i++) {
        command = domain->commands + i;
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
