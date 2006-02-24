/******************************************************************************
 *
 * Oeffentliche EtherCAT-Schnittstellen fuer Echtzeitprozesse.
 *
 * $Id$
 *
 *****************************************************************************/

#ifndef _ETHERCAT_RT_H_
#define _ETHERCAT_RT_H_

/*****************************************************************************/

struct ec_master;
typedef struct ec_master ec_master_t;

struct ec_domain;
typedef struct ec_domain ec_domain_t;

struct ec_slave;
typedef struct ec_slave ec_slave_t;

typedef enum
{
    ec_sync,
    ec_async
}
ec_domain_mode_t;

typedef enum
{
    ec_status,
    ec_control,
    ec_ipvalue,
    ec_opvalue
}
ec_field_type_t;

typedef struct
{
    void **data;
    const char *address;
    const char *vendor;
    const char *product;
    ec_field_type_t field_type;
    unsigned int field_index;
    unsigned int field_count;
}
ec_field_init_t;

/*****************************************************************************/
// Master request functions

ec_master_t *EtherCAT_rt_request_master(unsigned int master_index);

void EtherCAT_rt_release_master(ec_master_t *master);

/*****************************************************************************/
// Master methods

ec_domain_t *EtherCAT_rt_master_register_domain(ec_master_t *master,
                                                ec_domain_mode_t mode,
                                                unsigned int timeout_us);

int EtherCAT_rt_master_activate(ec_master_t *master);

int EtherCAT_rt_master_deactivate(ec_master_t *master);

void EtherCAT_rt_master_debug(ec_master_t *master, int level);
void EtherCAT_rt_master_print(const ec_master_t *master);

/*****************************************************************************/
// Domain Methods

ec_slave_t *EtherCAT_rt_register_slave_field(ec_domain_t *domain,
                                             const char *address,
                                             const char *vendor_name,
                                             const char *product_name,
                                             void **data_ptr,
                                             ec_field_type_t field_type,
                                             unsigned int field_index,
                                             unsigned int field_count);

int EtherCAT_rt_domain_xio(ec_domain_t *domain);

/*****************************************************************************/
// Slave Methods

int EtherCAT_rt_canopen_sdo_write(ec_slave_t *slave,
                                  uint16_t sdo_index,
                                  uint8_t sdo_subindex,
                                  uint32_t value,
                                  size_t size);

/*****************************************************************************/

#endif
