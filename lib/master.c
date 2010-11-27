/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT master userspace library.
 *
 *  The IgH EtherCAT master userspace library is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation; version 2.1
 *  of the License.
 *
 *  The IgH EtherCAT master userspace library is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with the IgH EtherCAT master userspace library. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "master.h"
#include "domain.h"
#include "slave_config.h"
#include "master/ioctl.h"

/*****************************************************************************/

int ecrt_master_reserve(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_REQUEST, NULL) == -1) {
        fprintf(stderr, "Failed to reserve master: %s\n",
                strerror(errno));
        return -1;
    }
    return 0;
}

/*****************************************************************************/

void ec_master_clear_config(ec_master_t *master)
{
    ec_domain_t *d, *next_d;
    ec_slave_config_t *c, *next_c;

    d = master->first_domain;
    while (d) {
        next_d = d->next;
        ec_domain_clear(d);
        d = next_d;
    }
    master->first_domain = NULL;

    c = master->first_config;
    while (c) {
        next_c = c->next;
        ec_slave_config_clear(c);
        c = next_c;
    }
    master->first_config = NULL;
}

/*****************************************************************************/

void ec_master_clear(ec_master_t *master)
{
    if (master->process_data)  {
        munmap(master->process_data, master->process_data_size);
    }

    ec_master_clear_config(master);

    if (master->fd != -1) {
        close(master->fd);
    }
}

/*****************************************************************************/

void ec_master_add_domain(ec_master_t *master, ec_domain_t *domain)
{
    if (master->first_domain) {
        ec_domain_t *d = master->first_domain;
        while (d->next) {
            d = d->next;
        }
        d->next = domain;
    } else {
        master->first_domain = domain;
    }
}

/*****************************************************************************/

ec_domain_t *ecrt_master_create_domain(ec_master_t *master)
{
    ec_domain_t *domain;
    int index;

    domain = malloc(sizeof(ec_domain_t));
    if (!domain) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }

    index = ioctl(master->fd, EC_IOCTL_CREATE_DOMAIN, NULL);
    if (index == -1) {
        fprintf(stderr, "Failed to create domain: %s\n", strerror(errno));
        free(domain);
        return 0;
    }

    domain->next = NULL;
    domain->index = (unsigned int) index;
    domain->master = master;
    domain->process_data = NULL;

    ec_master_add_domain(master, domain);

    return domain;
}

/*****************************************************************************/

void ec_master_add_slave_config(ec_master_t *master, ec_slave_config_t *sc)
{
    if (master->first_config) {
        ec_slave_config_t *c = master->first_config;
        while (c->next) {
            c = c->next;
        }
        c->next = sc;
    } else {
        master->first_config = sc;
    }
}

/*****************************************************************************/

ec_slave_config_t *ecrt_master_slave_config(ec_master_t *master,
        uint16_t alias, uint16_t position, uint32_t vendor_id,
        uint32_t product_code)
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc;
    int index;

    sc = malloc(sizeof(ec_slave_config_t));
    if (!sc) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }

    data.alias = alias;
    data.position = position;
    data.vendor_id = vendor_id;
    data.product_code = product_code;

    if (ioctl(master->fd, EC_IOCTL_CREATE_SLAVE_CONFIG, &data) == -1) {
        fprintf(stderr, "Failed to create slave config: %s\n",
                strerror(errno));
        free(sc);
        return 0;
    }

    sc->next = NULL;
    sc->master = master;
    sc->index = data.config_index;
    sc->alias = alias;
    sc->position = position;
    sc->first_sdo_request = NULL;
    sc->first_voe_handler = NULL;

    ec_master_add_slave_config(master, sc);

    return sc;
}

/*****************************************************************************/

int ecrt_master(ec_master_t* master, ec_master_info_t *master_info)
{
    ec_ioctl_master_t data;

    if (ioctl(master->fd, EC_IOCTL_MASTER, &data) < 0) {
        fprintf(stderr, "Failed to get master info: %s\n", strerror(errno));
        return -1;
    }

    master_info->slave_count = data.slave_count;
    master_info->link_up = data.devices[0].link_state;
    master_info->scan_busy = data.scan_busy;
    master_info->app_time = data.app_time;
    return 0;
}

/*****************************************************************************/

int ecrt_master_get_slave(ec_master_t *master, uint16_t slave_position,
        ec_slave_info_t *slave_info)
{
    ec_ioctl_slave_t data;
    int index, i;

    data.position = slave_position;

    if (ioctl(master->fd, EC_IOCTL_SLAVE, &data) == -1) {
        fprintf(stderr, "Failed to get slave info: %s\n", strerror(errno));
        return -1;
    }

    slave_info->position = data.position;
    slave_info->vendor_id = data.vendor_id;
    slave_info->product_code = data.product_code;
    slave_info->revision_number = data.revision_number;
    slave_info->serial_number = data.serial_number;
    slave_info->alias = data.alias;
    slave_info->current_on_ebus = data.current_on_ebus;
    for ( i = 0; i < EC_MAX_PORTS; i++ ) {
    	slave_info->ports[i].desc = data.ports[i].desc;
    	slave_info->ports[i].link.link_up = data.ports[i].link.link_up;
    	slave_info->ports[i].link.loop_closed = data.ports[i].link.loop_closed;
    	slave_info->ports[i].link.signal_detected = data.ports[i].link.signal_detected;
    	slave_info->ports[i].receive_time = data.ports[i].receive_time;
    	slave_info->ports[i].next_slave = data.ports[i].next_slave;
    	slave_info->ports[i].delay_to_next_dc = data.ports[i].delay_to_next_dc;
    }
    slave_info->al_state = data.al_state;
    slave_info->error_flag = data.error_flag;
    slave_info->sync_count = data.sync_count;
    slave_info->sdo_count = data.sdo_count;
    strncpy(slave_info->name, data.name, EC_MAX_STRING_LENGTH);
    return 0;
}

/*****************************************************************************/

int ecrt_master_get_sync_manager(ec_master_t *master, uint16_t slave_position,
        uint8_t sync_index, ec_sync_info_t *sync)
{
    ec_ioctl_slave_sync_t data;

    if (sync_index >= EC_MAX_SYNC_MANAGERS)
        return -ENOENT;

    memset(&data, 0x00, sizeof(ec_ioctl_slave_sync_t));
    data.slave_position = slave_position;
    data.sync_index = sync_index;

    if (ioctl(master->fd, EC_IOCTL_SLAVE_SYNC, &data) == -1) {
        fprintf(stderr, "Failed to get sync manager information: %s\n",
                strerror(errno));
        return -1; // FIXME
    }

    sync->index = sync_index;
    sync->dir = EC_READ_BIT(&data.control_register, 2) ?
        EC_DIR_OUTPUT : EC_DIR_INPUT;
    sync->n_pdos = data.pdo_count;
    sync->pdos = NULL;
    sync->watchdog_mode = EC_READ_BIT(&data.control_register, 6) ?
        EC_WD_ENABLE : EC_WD_DISABLE;

    return 0;
}

/*****************************************************************************/

int ecrt_master_get_pdo(ec_master_t *master, uint16_t slave_position,
        uint8_t sync_index, uint16_t pos, ec_pdo_info_t *pdo)
{
    ec_ioctl_slave_sync_pdo_t data;

    if (sync_index >= EC_MAX_SYNC_MANAGERS)
        return -ENOENT;

    memset(&data, 0x00, sizeof(ec_ioctl_slave_sync_pdo_t));
    data.slave_position = slave_position;
    data.sync_index = sync_index;
    data.pdo_pos = pos;

    if (ioctl(master->fd, EC_IOCTL_SLAVE_SYNC_PDO, &data) == -1) {
        fprintf(stderr, "Failed to get pdo information: %s\n",
                strerror(errno));
        return -1; // FIXME
    }

    pdo->index = data.index;
    pdo->n_entries = data.entry_count;
    pdo->entries = NULL;

    return 0;
}

/*****************************************************************************/

int ecrt_master_get_pdo_entry(ec_master_t *master, uint16_t slave_position,
        uint8_t sync_index, uint16_t pdo_pos, uint16_t entry_pos,
        ec_pdo_entry_info_t *entry)
{
    ec_ioctl_slave_sync_pdo_entry_t data;

    if (sync_index >= EC_MAX_SYNC_MANAGERS)
        return -ENOENT;

    memset(&data, 0x00, sizeof(ec_ioctl_slave_sync_pdo_entry_t));
    data.slave_position = slave_position;
    data.sync_index = sync_index;
    data.pdo_pos = pdo_pos;
    data.entry_pos = entry_pos;

    if (ioctl(master->fd, EC_IOCTL_SLAVE_SYNC_PDO_ENTRY, &data) == -1) {
        fprintf(stderr, "Failed to get pdo entry information: %s\n",
                strerror(errno));
        return -1; // FIXME
    }

    entry->index = data.index;
    entry->subindex = data.subindex;
    entry->bit_length = data.bit_length;

    return 0;
}

/*****************************************************************************/

int ecrt_master_sdo_download(ec_master_t *master, uint16_t slave_position,
        uint16_t index, uint8_t subindex, uint8_t *data,
        size_t data_size, uint32_t *abort_code)
{
    ec_ioctl_slave_sdo_download_t download;

    download.slave_position = slave_position;
    download.sdo_index = index;
    download.sdo_entry_subindex = subindex;
    download.data_size = data_size;
    download.data = data;

    if (ioctl(master->fd, EC_IOCTL_SLAVE_SDO_DOWNLOAD, &download) == -1) {
        if (errno == EIO && abort_code) {
            *abort_code = download.abort_code;
        }
        fprintf(stderr, "Failed to execute SDO download: %s\n",
            strerror(errno));
        return -1;
    }

    return 0;
}

/*****************************************************************************/

int ecrt_master_sdo_upload(ec_master_t *master, uint16_t slave_position,
        uint16_t index, uint8_t subindex, uint8_t *target,
        size_t target_size, size_t *result_size, uint32_t *abort_code)
{
    ec_ioctl_slave_sdo_upload_t upload;

    upload.slave_position = slave_position;
    upload.sdo_index = index;
    upload.sdo_entry_subindex = subindex;
    upload.target_size = target_size;
    upload.target = target;

    if (ioctl(master->fd, EC_IOCTL_SLAVE_SDO_UPLOAD, &upload) == -1) {
        if (errno == EIO && abort_code) {
            *abort_code = upload.abort_code;
        }
        fprintf(stderr, "Failed to execute SDO upload: %s\n",
                strerror(errno));
        return -1;
    }

    *result_size = upload.data_size;
    return 0;
}

/*****************************************************************************/

int ecrt_master_write_idn(ec_master_t *master, uint16_t slave_position,
        uint8_t drive_no, uint16_t idn, uint8_t *data, size_t data_size,
        uint16_t *error_code)
{
    ec_ioctl_slave_soe_write_t io;

    io.slave_position = slave_position;
    io.drive_no = drive_no;
    io.idn = idn;
    io.data_size = data_size;
    io.data = data;

    if (ioctl(master->fd, EC_IOCTL_SLAVE_SOE_WRITE, &io) == -1) {
        if (errno == EIO && error_code) {
            *error_code = io.error_code;
        }
        fprintf(stderr, "Failed to write IDN: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/*****************************************************************************/

int ecrt_master_read_idn(ec_master_t *master, uint16_t slave_position,
        uint8_t drive_no, uint16_t idn, uint8_t *target, size_t target_size,
        size_t *result_size, uint16_t *error_code)
{
    ec_ioctl_slave_soe_read_t io;

    io.slave_position = slave_position;
    io.drive_no = drive_no;
    io.idn = idn;
    io.mem_size = target_size;
    io.data = target;

    if (ioctl(master->fd, EC_IOCTL_SLAVE_SOE_READ, &io) == -1) {
        if (errno == EIO && error_code) {
            *error_code = io.error_code;
        }
        fprintf(stderr, "Failed to read IDN: %s\n", strerror(errno));
        return -1;
    }

    *result_size = io.data_size;
    return 0;
}

/*****************************************************************************/

int ecrt_master_activate(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_ACTIVATE,
                &master->process_data_size) == -1) {
        fprintf(stderr, "Failed to activate master: %s\n",
                strerror(errno));
        return -1; // FIXME
    }

    if (master->process_data_size) {
        master->process_data = mmap(0, master->process_data_size,
                PROT_READ | PROT_WRITE, MAP_SHARED, master->fd, 0);
        if (master->process_data == MAP_FAILED) {
            fprintf(stderr, "Failed to map process data: %s", strerror(errno));
            master->process_data = NULL;
            master->process_data_size = 0;
            return -1; // FIXME
        }

        // Access the mapped region to cause the initial page fault
        memset(master->process_data, 0, master->process_data_size);
    }

    return 0;
}

/*****************************************************************************/

void ecrt_master_deactivate(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_DEACTIVATE, NULL) == -1) {
        fprintf(stderr, "Failed to deactivate master: %s\n", strerror(errno));
        return;
    }

    ec_master_clear_config(master);
}

/*****************************************************************************/

int ecrt_master_set_send_interval(ec_master_t *master,size_t send_interval_us)
{
    if (ioctl(master->fd, EC_IOCTL_SET_SEND_INTERVAL,
                &send_interval_us) == -1) {
        fprintf(stderr, "Failed to set send interval: %s\n",
                strerror(errno));
        return -1; // FIXME
    }
    return 0;
}

/*****************************************************************************/

void ecrt_master_send(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_SEND, NULL) == -1) {
        fprintf(stderr, "Failed to send: %s\n", strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_receive(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_RECEIVE, NULL) == -1) {
        fprintf(stderr, "Failed to receive: %s\n", strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_state(const ec_master_t *master, ec_master_state_t *state)
{
    if (ioctl(master->fd, EC_IOCTL_MASTER_STATE, state) == -1) {
        fprintf(stderr, "Failed to get master state: %s\n", strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_configured_slaves_state(const ec_master_t *master,
                                         ec_master_state_t *state)
{
    if (ioctl(master->fd, EC_IOCTL_MASTER_SC_STATE, state) == -1) {
        fprintf(stderr, "Failed to get master state: %s\n", strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_application_time(ec_master_t *master, uint64_t app_time)
{
    ec_ioctl_app_time_t data;

    data.app_time = app_time;

    if (ioctl(master->fd, EC_IOCTL_APP_TIME, &data) == -1) {
        fprintf(stderr, "Failed to set application time: %s\n",
                strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_sync_reference_clock(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_SYNC_REF, NULL) == -1) {
        fprintf(stderr, "Failed to sync reference clock: %s\n",
                strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_sync_slave_clocks(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_SYNC_SLAVES, NULL) == -1) {
        fprintf(stderr, "Failed to sync slave clocks: %s\n", strerror(errno));
    }
}

/*****************************************************************************/

void ecrt_master_sync_monitor_queue(ec_master_t *master)
{
    if (ioctl(master->fd, EC_IOCTL_SYNC_MON_QUEUE, NULL) == -1) {
        fprintf(stderr, "Failed to queue sync monitor datagram: %s\n",
                strerror(errno));
    }
}

/*****************************************************************************/

uint32_t ecrt_master_sync_monitor_process(ec_master_t *master)
{
    uint32_t time_diff;

    if (ioctl(master->fd, EC_IOCTL_SYNC_MON_PROCESS, &time_diff) == -1) {
        time_diff = 0xffffffff;
        fprintf(stderr, "Failed to process sync monitor datagram: %s\n",
                strerror(errno));
    }

    return time_diff;
}

/*****************************************************************************/
