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

/*****************************************************************************/

/**
   Konstruktor einer EtherCAT-Domäne.
*/

void ec_domain_init(ec_domain_t *domain, /**< Domäne */
                    ec_master_t *master, /**< Zugehöriger Master */
                    ec_domain_mode_t mode, /**< Synchron/Asynchron */
                    unsigned int timeout_us /**< Timeout in Mikrosekunden */
                    )
{
    domain->master = master;
    domain->mode = mode;
    domain->timeout_us = timeout_us;

    domain->data = NULL;
    domain->data_size = 0;
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
    ec_field_reg_t *field_reg, *next;

    if (domain->data) {
        kfree(domain->data);
        domain->data = NULL;
    }

    // Liste der registrierten Datenfelder löschen
    list_for_each_entry_safe(field_reg, next, &domain->field_regs, list) {
        kfree(field_reg);
    }
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
   Erzeugt eine Domäne.

   Reserviert den Speicher einer Domäne, berechnet die logischen Adressen der
   FMMUs und setzt die Prozessdatenzeiger der registrierten Felder.

   \return 0 bei Erfolg, < 0 bei Fehler
*/

int ec_domain_alloc(ec_domain_t *domain, /**< Domäne */
                    uint32_t base_address /**< Logische Basisadresse */
                    )
{
    ec_field_reg_t *field_reg, *next;
    ec_slave_t *slave;
    ec_fmmu_t *fmmu;
    unsigned int i, j, found, data_offset;

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
    }
    else {
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
    }

    // Registrierungsliste wird jetzt nicht mehr gebraucht.
    list_for_each_entry_safe(field_reg, next, &domain->field_regs, list) {
        kfree(field_reg);
    }
    INIT_LIST_HEAD(&domain->field_regs); // wichtig!

    return 0;
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

ec_slave_t *EtherCAT_rt_register_slave_field(
    ec_domain_t *domain, /**< Domäne */
    const char *address, /**< ASCII-Addresse des Slaves, siehe ec_address() */
    const char *vendor_name, /**< Herstellername */
    const char *product_name, /**< Produktname */
    void **data_ptr, /**< Adresse des Zeigers auf die Prozessdaten */
    ec_field_type_t field_type, /**< Typ des Datenfeldes */
    unsigned int field_index, /**< Gibt an, ab welchem Feld mit Typ
                                 \a field_type gezählt werden soll. */
    unsigned int field_count /**< Anzahl Felder des selben Typs */
    )
{
    ec_slave_t *slave;
    const ec_slave_type_t *type;
    ec_master_t *master;
    const ec_sync_t *sync;
    const ec_field_t *field;
    unsigned int field_idx, i, j;
    uint32_t field_offset;

    if (!field_count) {
        EC_ERR("field_count may not be 0!\n");
        return NULL;
    }

    master = domain->master;

    // Adresse übersetzen
    if ((slave = ec_address(master, address)) == NULL) return NULL;

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

    field_idx = 0;
    for (i = 0; type->sync_managers[i]; i++) {
        sync = type->sync_managers[i];
        field_offset = 0;
        for (j = 0; sync->fields[j]; j++) {
            field = sync->fields[j];
            if (field->type == field_type) {
                if (field_idx == field_index) {
                    ec_domain_reg_field(domain, slave, sync, field_offset,
                                        data_ptr++);
                    if (!(--field_count)) return slave;
                }
                field_idx++;
            }
            field_offset += field->size;
        }
    }

    EC_ERR("Slave %i (\"%s %s\") registration mismatch: Type %i, index %i,"
           " count %i.\n", slave->ring_position, vendor_name, product_name,
           field_type, field_index, field_count);
    return NULL;
}

/*****************************************************************************/

/**
   Sendet und empfängt Prozessdaten der angegebenen Domäne.

   \return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_rt_domain_xio(ec_domain_t *domain /**< Domäne */)
{
    unsigned int offset, size, working_counter_sum;
    unsigned long start_ticks, end_ticks, timeout_ticks;
    ec_master_t *master;
    ec_frame_t *frame;

    master = domain->master;
    frame = &domain->frame;
    working_counter_sum = 0;

    ec_cyclic_output(master);

    rdtscl(start_ticks); // Sendezeit nehmen
    timeout_ticks = domain->timeout_us * cpu_khz / 1000;

    offset = 0;
    while (offset < domain->data_size)
    {
        size = domain->data_size - offset;
        if (size > EC_MAX_DATA_SIZE) size = EC_MAX_DATA_SIZE;

        ec_frame_init_lrw(frame, master, domain->base_address + offset, size,
                          domain->data + offset);

        if (unlikely(ec_frame_send(frame) < 0)) {
            master->device.state = EC_DEVICE_STATE_READY;
            master->frames_lost++;
            ec_cyclic_output(master);

            // Falls Link down...
            ec_device_call_isr(&master->device);

            return -1;
        }

        // Warten
        do {
            ec_device_call_isr(&master->device);
            rdtscl(end_ticks); // Empfangszeit nehmen
        }
        while (unlikely(master->device.state == EC_DEVICE_STATE_SENT
                        && end_ticks - start_ticks < timeout_ticks));

        master->bus_time = (end_ticks - start_ticks) * 1000 / cpu_khz;

        if (unlikely(end_ticks - start_ticks >= timeout_ticks)) {
            if (master->device.state == EC_DEVICE_STATE_RECEIVED) {
                master->frames_delayed++;
                ec_cyclic_output(master);
            }
            else {
                master->device.state = EC_DEVICE_STATE_READY;
                master->frames_lost++;
                ec_cyclic_output(master);
                return -1;
            }
        }

        if (unlikely(ec_frame_receive(frame) < 0)) {
            EC_ERR("Receiving process data failed!\n");
            return -1;
        }

        working_counter_sum += frame->working_counter;

        // Daten vom Rahmen in den Prozessdatenspeicher kopieren
        memcpy(domain->data + offset, frame->data, size);

        offset += size;
    }

    if (working_counter_sum != domain->response_count) {
        domain->response_count = working_counter_sum;
        EC_INFO("Domain %08X state change - %i slaves responding.\n",
                (u32) domain, working_counter_sum);
    }

    return 0;
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_rt_register_slave_field);
EXPORT_SYMBOL(EtherCAT_rt_domain_xio);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
