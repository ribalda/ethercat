/******************************************************************************
 *
 *  d o m a i n . h
 *
 *  EtherCAT domain structure.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_DOMAIN_H_
#define _EC_DOMAIN_H_

#include <linux/list.h>
#include <linux/kobject.h>

#include "globals.h"
#include "slave.h"
#include "command.h"

/*****************************************************************************/

/**
   Data field registration type.
*/

typedef struct
{
    struct list_head list; /**< list item */
    ec_slave_t *slave; /**< slave */
    const ec_sync_t *sync; /**< sync manager */
    uint32_t field_offset; /**< data field offset */
    void **data_ptr; /**< pointer to process data pointer(s) */
}
ec_field_reg_t;

/*****************************************************************************/

/**
   EtherCAT domain.
   Handles the process data and the therefore needed commands of a certain
   group of slaves.
*/

struct ec_domain
{
    struct kobject kobj; /**< kobject */
    struct list_head list; /**< list item */
    unsigned int index; /**< domain index (just a number) */
    ec_master_t *master; /**< EtherCAT master owning the domain */
    size_t data_size; /**< size of the process data */
    struct list_head commands; /**< process data commands */
    uint32_t base_address; /**< logical offset address of the process data */
    unsigned int response_count; /**< number of responding slaves */
    struct list_head field_regs; /**< data field registrations */
};

/*****************************************************************************/

int ec_domain_init(ec_domain_t *, ec_master_t *, unsigned int);
void ec_domain_clear(struct kobject *);
int ec_domain_alloc(ec_domain_t *, uint32_t);

/*****************************************************************************/

#endif
