/******************************************************************************
 *
 *  t y p e s . h
 *
 *  EtherCAT slave types.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_TYPES_H_
#define _EC_TYPES_H_

#include <linux/types.h>

#include "../include/ecrt.h"

/*****************************************************************************/

#define EC_MAX_FIELDS 10
#define EC_MAX_SYNC   16

/*****************************************************************************/

/**
   Special slaves.
*/

typedef enum
{
    EC_TYPE_NORMAL, /**< no special slave */
    EC_TYPE_BUS_COUPLER, /**< slave is a bus coupler */
    EC_TYPE_EOE /**< slave is an EoE switch */
}
ec_special_type_t;

/*****************************************************************************/

/**
   Process data field.
*/

typedef struct
{
    const char *name; /**< field name */
    size_t size; /**< field size in bytes */
}
ec_field_t;

/*****************************************************************************/

/**
   Sync-Manager.
*/

typedef struct
{
    uint16_t physical_start_address; /**< physical start address */
    uint16_t size; /**< size in bytes */
    uint8_t control_byte; /**< control register value */
    const ec_field_t *fields[EC_MAX_FIELDS]; /**< field array */
}
ec_sync_t;

/*****************************************************************************/

/**
   Slave description type.
*/

typedef struct ec_slave_type
{
    const char *vendor_name; /**< vendor name*/
    const char *product_name; /**< product name */
    const char *description; /**< free description */
    ec_special_type_t special; /**< special slave type? */
    const ec_sync_t *sync_managers[EC_MAX_SYNC]; /**< sync managers */
}
ec_slave_type_t;

/*****************************************************************************/

/**
   Slave type identification.
*/

typedef struct
{
    uint32_t vendor_id; /**< vendor id */
    uint32_t product_code; /**< product code */
    const ec_slave_type_t *type; /**< associated slave description object */
}
ec_slave_ident_t;

extern ec_slave_ident_t slave_idents[]; /**< array with slave descriptions */

/*****************************************************************************/

#endif
