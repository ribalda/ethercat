/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2 of the License.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

/**
   \file
   EtherCAT realtime interface.
*/

/**
   \defgroup RealtimeInterface EtherCAT realtime interface
   EtherCAT interface for realtime modules.
   This interface is designed for realtime modules that want to use EtherCAT.
   There are functions to request a master, to map process data, to communicate
   with slaves via CoE and to configure and activate the bus.
*/

/*****************************************************************************/

#ifndef __ECRT_H__
#define __ECRT_H__

#include <asm/byteorder.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/*****************************************************************************/

struct ec_master;
typedef struct ec_master ec_master_t;

struct ec_domain;
typedef struct ec_domain ec_domain_t;

struct ec_slave;
typedef struct ec_slave ec_slave_t;

/**
   Initialization type for field registrations.
   This type is used as a parameter for the ec_domain_register_field_list()
   function.
*/

typedef struct
{
    void **data_ptr; /**< address of the process data pointer */
    const char *slave_address; /**< slave address string (see
                                  ecrt_master_get_slave()) */
    const char *vendor_name; /**< vendor name */
    const char *product_name; /**< product name */
    const char *field_name; /**< data field name */
    unsigned int field_index; /**< index in data fields with same name */
    unsigned int field_count; /**< number of data fields with same name */
}
ec_field_init_t;

/******************************************************************************
 *  Master request functions
 *****************************************************************************/

ec_master_t *ecrt_request_master(unsigned int master_index);
void ecrt_release_master(ec_master_t *master);

/******************************************************************************
 *  Master methods
 *****************************************************************************/

void ecrt_master_callbacks(ec_master_t *master, int (*request_cb)(void *),
                           void (*release_cb)(void *), void *cb_data);

ec_domain_t *ecrt_master_create_domain(ec_master_t *master);

int ecrt_master_activate(ec_master_t *master);
void ecrt_master_deactivate(ec_master_t *master);

int ecrt_master_fetch_sdo_lists(ec_master_t *master);

void ecrt_master_sync_io(ec_master_t *master);
void ecrt_master_async_send(ec_master_t *master);
void ecrt_master_async_receive(ec_master_t *master);
void ecrt_master_prepare_async_io(ec_master_t *master);

void ecrt_master_run(ec_master_t *master);

void ecrt_master_debug(ec_master_t *master, int level);
void ecrt_master_print(const ec_master_t *master, unsigned int verbosity);

ec_slave_t *ecrt_master_get_slave(const ec_master_t *, const char *);

/******************************************************************************
 *  Domain Methods
 *****************************************************************************/

ec_slave_t *ecrt_domain_register_field(ec_domain_t *domain,
                                       const char *address,
                                       const char *vendor_name,
                                       const char *product_name,
                                       void **data_ptr, const char *field_name,
                                       unsigned int field_index,
                                       unsigned int field_count);
int ecrt_domain_register_field_list(ec_domain_t *domain,
                                    const ec_field_init_t *fields);

void ecrt_domain_queue(ec_domain_t *domain);
void ecrt_domain_process(ec_domain_t *domain);

int ecrt_domain_state(ec_domain_t *domain);

/******************************************************************************
 *  Slave Methods
 *****************************************************************************/

int ecrt_slave_sdo_read_exp8(ec_slave_t *slave, uint16_t sdo_index,
                              uint8_t sdo_subindex, uint8_t *value);
int ecrt_slave_sdo_read_exp16(ec_slave_t *slave, uint16_t sdo_index,
                              uint8_t sdo_subindex, uint16_t *value);
int ecrt_slave_sdo_read_exp32(ec_slave_t *slave, uint16_t sdo_index,
                              uint8_t sdo_subindex, uint32_t *value);
int ecrt_slave_sdo_write_exp8(ec_slave_t *slave, uint16_t sdo_index,
                              uint8_t sdo_subindex, uint8_t value);
int ecrt_slave_sdo_write_exp16(ec_slave_t *slave, uint16_t sdo_index,
                               uint8_t sdo_subindex, uint16_t value);
int ecrt_slave_sdo_write_exp32(ec_slave_t *slave, uint16_t sdo_index,
                               uint8_t sdo_subindex, uint32_t value);
int ecrt_slave_sdo_read(ec_slave_t *slave, uint16_t sdo_index,
                        uint8_t sdo_subindex, uint8_t *data, size_t *size);

int ecrt_slave_write_alias(ec_slave_t *slave, uint16_t alias);

/******************************************************************************
 *  Bitwise read/write macros
 *****************************************************************************/

/**
   Read a certain bit of an EtherCAT data byte.
   \param DATA EtherCAT data pointer
   \param POS bit position
*/

#define EC_READ_BIT(DATA, POS) ((*((uint8_t *) (DATA)) >> (POS)) & 0x01)

/**
   Write a certain bit of an EtherCAT data byte.
   \param DATA EtherCAT data pointer
   \param POS bit position
   \param VAL new bit value
*/

#define EC_WRITE_BIT(DATA, POS, VAL) \
    do { \
        if (VAL) *((uint8_t *) (DATA)) |=  (1 << (POS)); \
        else     *((uint8_t *) (DATA)) &= ~(1 << (POS)); \
    } while (0)

/******************************************************************************
 *  Read macros
 *****************************************************************************/

/**
   Read an 8-bit unsigned value from EtherCAT data.
   \return EtherCAT data value
*/

#define EC_READ_U8(DATA) \
    ((uint8_t) *((uint8_t *) (DATA)))

/**
   Read an 8-bit signed value from EtherCAT data.
   \param DATA EtherCAT data pointer
   \return EtherCAT data value
*/

#define EC_READ_S8(DATA) \
     ((int8_t)  *((uint8_t *) (DATA)))

/**
   Read a 16-bit unsigned value from EtherCAT data.
   \param DATA EtherCAT data pointer
   \return EtherCAT data value
*/

#define EC_READ_U16(DATA) \
     ((uint16_t) le16_to_cpup((void *) (DATA)))

/**
   Read a 16-bit signed value from EtherCAT data.
   \param DATA EtherCAT data pointer
   \return EtherCAT data value
*/

#define EC_READ_S16(DATA) \
     ((int16_t)  le16_to_cpup((void *) (DATA)))

/**
   Read a 32-bit unsigned value from EtherCAT data.
   \param DATA EtherCAT data pointer
   \return EtherCAT data value
*/

#define EC_READ_U32(DATA) \
     ((uint32_t) le32_to_cpup((void *) (DATA)))

/**
   Read a 32-bit signed value from EtherCAT data.
   \param DATA EtherCAT data pointer
   \return EtherCAT data value
*/

#define EC_READ_S32(DATA) \
     ((int32_t)  le32_to_cpup((void *) (DATA)))


/******************************************************************************
 *  Write macros
 *****************************************************************************/

/**
   Write an 8-bit unsigned value to EtherCAT data.
   \param DATA EtherCAT data pointer
   \param VAL new value
*/

#define EC_WRITE_U8(DATA, VAL) \
    do { \
        *((uint8_t *)(DATA)) = ((uint8_t) (VAL)); \
    } while (0)

/**
   Write an 8-bit signed value to EtherCAT data.
   \param DATA EtherCAT data pointer
   \param VAL new value
*/

#define EC_WRITE_S8(DATA, VAL) EC_WRITE_U8(DATA, VAL)

/**
   Write a 16-bit unsigned value to EtherCAT data.
   \param DATA EtherCAT data pointer
   \param VAL new value
*/

#define EC_WRITE_U16(DATA, VAL) \
    do { \
        *((uint16_t *) (DATA)) = (uint16_t) (VAL); \
        cpu_to_le16s(DATA); \
    } while (0)

/**
   Write a 16-bit signed value to EtherCAT data.
   \param DATA EtherCAT data pointer
   \param VAL new value
*/

#define EC_WRITE_S16(DATA, VAL) EC_WRITE_U16(DATA, VAL)

/**
   Write a 32-bit unsigned value to EtherCAT data.
   \param DATA EtherCAT data pointer
   \param VAL new value
*/

#define EC_WRITE_U32(DATA, VAL) \
    do { \
        *((uint32_t *) (DATA)) = (uint32_t) (VAL); \
        cpu_to_le16s(DATA); \
    } while (0)

/**
   Write a 32-bit signed value to EtherCAT data.
   \param DATA EtherCAT data pointer
   \param VAL new value
*/

#define EC_WRITE_S32(DATA, VAL) EC_WRITE_U32(DATA, VAL)

/*****************************************************************************/

#endif
