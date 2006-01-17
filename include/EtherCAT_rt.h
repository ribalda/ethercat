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

/*****************************************************************************/

ec_master_t *EtherCAT_rt_request_master(unsigned int master_index);

void EtherCAT_rt_release_master(ec_master_t *master);

void *EtherCAT_rt_register_slave(ec_master_t *master, unsigned int slave_index,
                                 const char *vendor_name,
                                 const char *product_name);

int EtherCAT_rt_activate_slaves(ec_master_t *master);

int EtherCAT_rt_deactivate_slaves(ec_master_t *master);

int EtherCAT_rt_domain_cycle(ec_master_t *master, unsigned int domain,
                             unsigned int timeout_us);

/*****************************************************************************/

#endif
