/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2012  Florian Pose, Ingenieurgemeinschaft IgH
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
 ****************************************************************************/

#include <unistd.h> /* close() */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "ioctl.h"
#include "master.h"
#include "domain.h"
#include "slave_config.h"

/****************************************************************************/

int ecrt_master_reserve(ec_master_t *master)
{
    int ret = ioctl(master->fd, EC_IOCTL_REQUEST, NULL);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to reserve master: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }
    return 0;
}

/****************************************************************************/

void ec_master_clear_config(ec_master_t *master)
{
    ec_domain_t *d, *next_d;
    ec_slave_config_t *c, *next_c;

    if (master->process_data)  {
        munmap(master->process_data, master->process_data_size);
        master->process_data = NULL;
    }

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

/****************************************************************************/

void ec_master_clear(ec_master_t *master)
{
    ec_master_clear_config(master);

    if (master->fd != -1) {
#if USE_RTDM
        rt_dev_close(master->fd);
#else
        close(master->fd);
#endif
    }
}

/****************************************************************************/

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

/****************************************************************************/

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
    if (EC_IOCTL_IS_ERROR(index)) {
        fprintf(stderr, "Failed to create domain: %s\n",
                strerror(EC_IOCTL_ERRNO(index)));
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

/****************************************************************************/

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

/****************************************************************************/

ec_slave_config_t *ecrt_master_slave_config(ec_master_t *master,
        uint16_t alias, uint16_t position, uint32_t vendor_id,
        uint32_t product_code)
{
    ec_ioctl_config_t data;
    ec_slave_config_t *sc;
    int ret;

    sc = malloc(sizeof(ec_slave_config_t));
    if (!sc) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }

    data.alias = alias;
    data.position = position;
    data.vendor_id = vendor_id;
    data.product_code = product_code;

    ret = ioctl(master->fd, EC_IOCTL_CREATE_SLAVE_CONFIG, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to create slave config: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        free(sc);
        return 0;
    }

    sc->next = NULL;
    sc->master = master;
    sc->index = data.config_index;
    sc->alias = alias;
    sc->position = position;
    sc->first_sdo_request = NULL;
    sc->first_reg_request = NULL;
    sc->first_voe_handler = NULL;

    ec_master_add_slave_config(master, sc);

    return sc;
}

/*****************************************************************************/

int ecrt_master_select_reference_clock(ec_master_t *master,
        ec_slave_config_t *sc)
{
    uint32_t config_index;
    int ret;

    if (sc) {
        config_index = sc->index;
    }
    else {
        config_index = 0xFFFFFFFF;
    }

    ret = ioctl(master->fd, EC_IOCTL_SELECT_REF_CLOCK, config_index);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to select reference clock: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    return 0;
}

/****************************************************************************/

int ecrt_master(ec_master_t *master, ec_master_info_t *master_info)
{
    ec_ioctl_master_t data;
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_MASTER, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get master info: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    master_info->slave_count = data.slave_count;
    master_info->link_up = data.devices[0].link_state;
    master_info->scan_busy = data.scan_busy;
    master_info->app_time = data.app_time;
    return 0;
}

/****************************************************************************/

int ecrt_master_get_slave(ec_master_t *master, uint16_t slave_position,
        ec_slave_info_t *slave_info)
{
    ec_ioctl_slave_t data;
    int ret, i;

    data.position = slave_position;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get slave info: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    slave_info->position = data.position;
    slave_info->vendor_id = data.vendor_id;
    slave_info->product_code = data.product_code;
    slave_info->revision_number = data.revision_number;
    slave_info->serial_number = data.serial_number;
    slave_info->alias = data.alias;
    slave_info->current_on_ebus = data.current_on_ebus;
    for (i = 0; i < EC_MAX_PORTS; i++) {
        slave_info->ports[i].desc = data.ports[i].desc;
        slave_info->ports[i].link.link_up = data.ports[i].link.link_up;
        slave_info->ports[i].link.loop_closed =
            data.ports[i].link.loop_closed;
        slave_info->ports[i].link.signal_detected =
            data.ports[i].link.signal_detected;
        slave_info->ports[i].receive_time = data.ports[i].receive_time;
        slave_info->ports[i].next_slave = data.ports[i].next_slave;
        slave_info->ports[i].delay_to_next_dc =
            data.ports[i].delay_to_next_dc;
    }
    slave_info->al_state = data.al_state;
    slave_info->error_flag = data.error_flag;
    slave_info->sync_count = data.sync_count;
    slave_info->sdo_count = data.sdo_count;
    strncpy(slave_info->name, data.name, EC_MAX_STRING_LENGTH);
    return 0;
}

/****************************************************************************/

int ecrt_master_get_sync_manager(ec_master_t *master, uint16_t slave_position,
        uint8_t sync_index, ec_sync_info_t *sync)
{
    ec_ioctl_slave_sync_t data;
    int ret;

    if (sync_index >= EC_MAX_SYNC_MANAGERS) {
        return -ENOENT;
    }

    memset(&data, 0x00, sizeof(ec_ioctl_slave_sync_t));
    data.slave_position = slave_position;
    data.sync_index = sync_index;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE_SYNC, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get sync manager information: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
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

/****************************************************************************/

int ecrt_master_get_pdo(ec_master_t *master, uint16_t slave_position,
        uint8_t sync_index, uint16_t pos, ec_pdo_info_t *pdo)
{
    ec_ioctl_slave_sync_pdo_t data;
    int ret;

    if (sync_index >= EC_MAX_SYNC_MANAGERS)
        return -ENOENT;

    memset(&data, 0x00, sizeof(ec_ioctl_slave_sync_pdo_t));
    data.slave_position = slave_position;
    data.sync_index = sync_index;
    data.pdo_pos = pos;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE_SYNC_PDO, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get pdo information: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    pdo->index = data.index;
    pdo->n_entries = data.entry_count;
    pdo->entries = NULL;

    return 0;
}

/****************************************************************************/

int ecrt_master_get_pdo_entry(ec_master_t *master, uint16_t slave_position,
        uint8_t sync_index, uint16_t pdo_pos, uint16_t entry_pos,
        ec_pdo_entry_info_t *entry)
{
    ec_ioctl_slave_sync_pdo_entry_t data;
    int ret;

    if (sync_index >= EC_MAX_SYNC_MANAGERS)
        return -ENOENT;

    memset(&data, 0x00, sizeof(ec_ioctl_slave_sync_pdo_entry_t));
    data.slave_position = slave_position;
    data.sync_index = sync_index;
    data.pdo_pos = pdo_pos;
    data.entry_pos = entry_pos;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE_SYNC_PDO_ENTRY, &data);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get pdo entry information: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    entry->index = data.index;
    entry->subindex = data.subindex;
    entry->bit_length = data.bit_length;

    return 0;
}

/****************************************************************************/

int ecrt_master_sdo_download(ec_master_t *master, uint16_t slave_position,
        uint16_t index, uint8_t subindex, const uint8_t *data,
        size_t data_size, uint32_t *abort_code)
{
    ec_ioctl_slave_sdo_download_t download;
    int ret;

    download.slave_position = slave_position;
    download.sdo_index = index;
    download.sdo_entry_subindex = subindex;
    download.complete_access = 0;
    download.data_size = data_size;
    download.data = data;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE_SDO_DOWNLOAD, &download);
    if (EC_IOCTL_IS_ERROR(ret)) {
        if (EC_IOCTL_ERRNO(ret) == EIO && abort_code) {
            *abort_code = download.abort_code;
        }
        fprintf(stderr, "Failed to execute SDO download: %s\n",
            strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    return 0;
}

/****************************************************************************/

int ecrt_master_sdo_download_complete(ec_master_t *master,
        uint16_t slave_position, uint16_t index, const uint8_t *data,
        size_t data_size, uint32_t *abort_code)
{
    ec_ioctl_slave_sdo_download_t download;
    int ret;

    download.slave_position = slave_position;
    download.sdo_index = index;
    download.sdo_entry_subindex = 0;
    download.complete_access = 1;
    download.data_size = data_size;
    download.data = data;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE_SDO_DOWNLOAD, &download);
    if (EC_IOCTL_IS_ERROR(ret)) {
        if (EC_IOCTL_ERRNO(ret) == EIO && abort_code) {
            *abort_code = download.abort_code;
        }
        fprintf(stderr, "Failed to execute SDO download: %s\n",
            strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    return 0;
}

/****************************************************************************/

int ecrt_master_sdo_upload(ec_master_t *master, uint16_t slave_position,
        uint16_t index, uint8_t subindex, uint8_t *target,
        size_t target_size, size_t *result_size, uint32_t *abort_code)
{
    ec_ioctl_slave_sdo_upload_t upload;
    int ret;

    upload.slave_position = slave_position;
    upload.sdo_index = index;
    upload.sdo_entry_subindex = subindex;
    upload.target_size = target_size;
    upload.target = target;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE_SDO_UPLOAD, &upload);
    if (EC_IOCTL_IS_ERROR(ret)) {
        if (EC_IOCTL_ERRNO(ret) == EIO && abort_code) {
            *abort_code = upload.abort_code;
        }
        fprintf(stderr, "Failed to execute SDO upload: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    *result_size = upload.data_size;
    return 0;
}

/****************************************************************************/

int ecrt_master_write_idn(ec_master_t *master, uint16_t slave_position,
        uint8_t drive_no, uint16_t idn, uint8_t *data, size_t data_size,
        uint16_t *error_code)
{
    ec_ioctl_slave_soe_write_t io;
    int ret;

    io.slave_position = slave_position;
    io.drive_no = drive_no;
    io.idn = idn;
    io.data_size = data_size;
    io.data = data;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE_SOE_WRITE, &io);
    if (EC_IOCTL_IS_ERROR(ret)) {
        if (EC_IOCTL_ERRNO(ret) == EIO && error_code) {
            *error_code = io.error_code;
        }
        fprintf(stderr, "Failed to write IDN: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    return 0;
}

/****************************************************************************/

int ecrt_master_read_idn(ec_master_t *master, uint16_t slave_position,
        uint8_t drive_no, uint16_t idn, uint8_t *target, size_t target_size,
        size_t *result_size, uint16_t *error_code)
{
    ec_ioctl_slave_soe_read_t io;
    int ret;

    io.slave_position = slave_position;
    io.drive_no = drive_no;
    io.idn = idn;
    io.mem_size = target_size;
    io.data = target;

    ret = ioctl(master->fd, EC_IOCTL_SLAVE_SOE_READ, &io);
    if (EC_IOCTL_IS_ERROR(ret)) {
        if (EC_IOCTL_ERRNO(ret) == EIO && error_code) {
            *error_code = io.error_code;
        }
        fprintf(stderr, "Failed to read IDN: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    *result_size = io.data_size;
    return 0;
}

/****************************************************************************/

int ecrt_master_activate(ec_master_t *master)
{
    ec_ioctl_master_activate_t io;
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_ACTIVATE, &io);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to activate master: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    master->process_data_size = io.process_data_size;

    if (master->process_data_size) {
#ifdef USE_RTDM
        /* memory-mapping was already done in kernel. The user-space addess is
         * provided in the ioctl data.
         */
        master->process_data = io.process_data;
#else
        master->process_data = mmap(0, master->process_data_size,
                PROT_READ | PROT_WRITE, MAP_SHARED, master->fd, 0);
        if (master->process_data == MAP_FAILED) {
            fprintf(stderr, "Failed to map process data: %s\n",
                    strerror(errno));
            master->process_data = NULL;
            master->process_data_size = 0;
            return -errno;
        }
#endif

        // Access the mapped region to cause the initial page fault
        master->process_data[0] = 0x00;
    }

    return 0;
}

/****************************************************************************/

void ecrt_master_deactivate(ec_master_t *master)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_DEACTIVATE, NULL);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to deactivate master: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return;
    }

    ec_master_clear_config(master);
}

/****************************************************************************/

int ecrt_master_set_send_interval(ec_master_t *master,
        size_t send_interval_us)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_SET_SEND_INTERVAL, &send_interval_us);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to set send interval: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    return 0;
}

/****************************************************************************/

size_t ecrt_master_send(ec_master_t *master)
{
    int ret;
    size_t sent_bytes = 0;

    ret = ioctl(master->fd, EC_IOCTL_SEND, &sent_bytes);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to send: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }

    return sent_bytes;
}

/****************************************************************************/

void ecrt_master_receive(ec_master_t *master)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_RECEIVE, NULL);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to receive: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/****************************************************************************/

void ecrt_master_state(const ec_master_t *master, ec_master_state_t *state)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_MASTER_STATE, state);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get master state: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/****************************************************************************/

int ecrt_master_link_state(const ec_master_t *master, unsigned int dev_idx,
        ec_master_link_state_t *state)
{
    ec_ioctl_link_state_t io;
    int ret;

    io.dev_idx = dev_idx;
    io.state = state;

    ret = ioctl(master->fd, EC_IOCTL_MASTER_LINK_STATE, &io);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to get link state: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
        return -EC_IOCTL_ERRNO(ret);
    }

    return 0;
}

/****************************************************************************/

void ecrt_master_application_time(ec_master_t *master, uint64_t app_time)
{
    uint64_t time;
    int ret;

    time = app_time;

    ret = ioctl(master->fd, EC_IOCTL_APP_TIME, &time);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to set application time: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/****************************************************************************/

void ecrt_master_sync_reference_clock(ec_master_t *master)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_SYNC_REF, NULL);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to sync reference clock: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/****************************************************************************/

void ecrt_master_sync_reference_clock_to(ec_master_t *master,
        uint64_t sync_time)
{
    uint64_t time;
    int ret;

    time = sync_time;

    ret = ioctl(master->fd, EC_IOCTL_SYNC_REF_TO, &time);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to sync reference clock: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/****************************************************************************/

void ecrt_master_sync_slave_clocks(ec_master_t *master)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_SYNC_SLAVES, NULL);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to sync slave clocks: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/*****************************************************************************/

int ecrt_master_reference_clock_time(ec_master_t *master, uint32_t *time)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_REF_CLOCK_TIME, time);
    if (EC_IOCTL_IS_ERROR(ret)) {
        ret = EC_IOCTL_ERRNO(ret);
        if (ret != EIO && ret != ENXIO) {
            // do not log if no refclk or no refclk time yet
            fprintf(stderr, "Failed to get reference clock time: %s\n",
                    strerror(ret));
        }
        return -ret;
    }

    return 0;
}

/****************************************************************************/

void ecrt_master_sync_monitor_queue(ec_master_t *master)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_SYNC_MON_QUEUE, NULL);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to queue sync monitor datagram: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/****************************************************************************/

uint32_t ecrt_master_sync_monitor_process(ec_master_t *master)
{
    uint32_t time_diff;
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_SYNC_MON_PROCESS, &time_diff);
    if (EC_IOCTL_IS_ERROR(ret)) {
        time_diff = 0xffffffff;
        fprintf(stderr, "Failed to process sync monitor datagram: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }

    return time_diff;
}

/****************************************************************************/

void ecrt_master_reset(ec_master_t *master)
{
    int ret;

    ret = ioctl(master->fd, EC_IOCTL_RESET, NULL);
    if (EC_IOCTL_IS_ERROR(ret)) {
        fprintf(stderr, "Failed to reset master: %s\n",
                strerror(EC_IOCTL_ERRNO(ret)));
    }
}

/****************************************************************************/
