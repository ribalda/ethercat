/******************************************************************************
 *
 * Oeffentliche EtherCAT-Schnittstellen fuer Echtzeitprozesse.
 *
 * $Id$
 *
 *****************************************************************************/

#ifndef _ETHERCAT_RT_H_
#define _ETHERCAT_RT_H_

#include <asm/byteorder.h>

/*****************************************************************************/

struct ec_master;
typedef struct ec_master ec_master_t;

struct ec_domain;
typedef struct ec_domain ec_domain_t;

struct ec_slave;
typedef struct ec_slave ec_slave_t;

typedef struct
{
    void **data_ptr;
    const char *slave_address;
    const char *vendor_name;
    const char *product_name;
    const char *field_name;
    unsigned int field_index;
    unsigned int field_count;
}
ec_field_init_t;

/*****************************************************************************/
// Master request functions

ec_master_t *ecrt_request_master(unsigned int master_index);
void ecrt_release_master(ec_master_t *master);

/*****************************************************************************/
// Master methods

ec_domain_t *ecrt_master_create_domain(ec_master_t *master);
int ecrt_master_activate(ec_master_t *master);
void ecrt_master_deactivate(ec_master_t *master);
void ecrt_master_sync_io(ec_master_t *master);
void ecrt_master_async_send(ec_master_t *master);
void ecrt_master_async_receive(ec_master_t *master);
void ecrt_master_debug(ec_master_t *master, int level);
void ecrt_master_print(const ec_master_t *master);
int ecrt_master_sdo_write(ec_master_t *master,
                          const char *slave_addr,
                          uint16_t sdo_index,
                          uint8_t sdo_subindex,
                          uint32_t value,
                          size_t size);
int ecrt_master_sdo_read(ec_master_t *master,
                         const char *slave_addr,
                         uint16_t sdo_index,
                         uint8_t sdo_subindex,
                         uint32_t *value);

/*****************************************************************************/
// Domain Methods

ec_slave_t *ecrt_domain_register_field(ec_domain_t *domain,
                                       const char *address,
                                       const char *vendor_name,
                                       const char *product_name,
                                       void **data_ptr,
                                       const char *field_name,
                                       unsigned int field_index,
                                       unsigned int field_count);
int ecrt_domain_register_field_list(ec_domain_t *domain,
                                    ec_field_init_t *fields);
void ecrt_domain_queue(ec_domain_t *domain);
void ecrt_domain_process(ec_domain_t *domain);

/*****************************************************************************/
// Slave Methods

int ecrt_slave_sdo_write(ec_slave_t *slave,
                         uint16_t sdo_index,
                         uint8_t sdo_subindex,
                         uint32_t value,
                         size_t size);
int ecrt_slave_sdo_read(ec_slave_t *slave,
                        uint16_t sdo_index,
                        uint8_t sdo_subindex,
                        uint32_t *value);

/*****************************************************************************/
// Bitwise read/write macros

#define EC_READ_BIT(PD, CH) (*((uint8_t *) (PD)) >> (CH)) & 0x01)

#define EC_WRITE_BIT(PD, CH, VAL) \
    do { \
        if (VAL) *((uint8_t *) (PD)) |=  (1 << (CH)); \
        else     *((uint8_t *) (PD)) &= ~(1 << (CH)); \
    } while (0)

/*****************************************************************************/
// Read macros

#define EC_READ_U8(PD) ((uint8_t) *((uint8_t *) (PD)))
#define EC_READ_S8(PD) ((int8_t)  *((uint8_t *) (PD)))

#define EC_READ_U16(PD) ((uint16_t) le16_to_cpup((void *) (PD)))
#define EC_READ_S16(PD) ((int16_t)  le16_to_cpup((void *) (PD)))

#define EC_READ_U32(PD) ((uint32_t) le32_to_cpup((void *) (PD)))
#define EC_READ_S32(PD) ((int32_t)  le32_to_cpup((void *) (PD)))

/*****************************************************************************/
// Write macros

#define EC_WRITE_U8(PD, VAL) \
    do { \
        *((uint8_t *)(PD)) = ((uint8_t) (VAL)); \
    } while (0)

#define EC_WRITE_S8(PD, VAL) EC_WRITE_U8(PD, VAL)

#define EC_WRITE_U16(PD, VAL) \
    do { \
        *((uint16_t *) (PD)) = (uint16_t) (VAL); \
        cpu_to_le16s(PD); \
    } while (0)

#define EC_WRITE_S16(PD, VAL) EC_WRITE_U16(PD, VAL)

#define EC_WRITE_U32(PD, VAL) \
    do { \
        *((uint32_t *) (PD)) = (uint32_t) (VAL); \
        cpu_to_le16s(PD); \
    } while (0)

#define EC_WRITE_S32(PD, VAL) EC_WRITE_U32(PD, VAL)

/*****************************************************************************/

#endif
