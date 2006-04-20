/******************************************************************************
 *
 *  m a s t e r . h
 *
 *  EtherCAT master structure.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_MASTER_H_
#define _EC_MASTER_H_

#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/timer.h>

#include "device.h"
#include "domain.h"

/*****************************************************************************/

/**
   EtherCAT master mode.
*/

typedef enum
{
    EC_MASTER_MODE_IDLE,
    EC_MASTER_MODE_FREERUN,
    EC_MASTER_MODE_RUNNING
}
ec_master_mode_t;

/*****************************************************************************/

/**
   Cyclic EtherCAT statistics.
*/

typedef struct
{
    unsigned int timeouts; /**< command timeouts */
    unsigned int delayed; /**< delayed commands */
    unsigned int corrupted; /**< corrupted frames */
    unsigned int unmatched; /**< unmatched commands */
    unsigned int eoe_errors; /**< Ethernet-over-EtherCAT errors */
    cycles_t t_last; /**< time of last output */
}
ec_stats_t;

/*****************************************************************************/

/**
   EtherCAT-Master.
   Manages slaves, domains and IO.
*/

struct ec_master
{
    struct list_head list; /**< list item */
    struct kobject kobj; /**< kobject */
    unsigned int index; /**< master index */
    struct list_head slaves; /**< list of slaves on the bus */
    unsigned int slave_count; /**< number of slaves on the bus */
    ec_device_t *device; /**< EtherCAT device */
    struct list_head command_queue; /**< command queue */
    uint8_t command_index; /**< current command index */
    struct list_head domains; /**< list of domains */
    ec_command_t simple_command; /**< command structure for initialization */
    ec_command_t watch_command; /**< command for watching the slaves */
    unsigned int slaves_responding; /**< number of responding slaves */
    ec_slave_state_t slave_states; /**< states of the responding slaves */
    int debug_level; /**< master debug level */
    ec_stats_t stats; /**< cyclic statistics */
    unsigned int timeout; /**< timeout in synchronous IO */
    struct list_head eoe_slaves; /**< Ethernet-over-EtherCAT slaves */
    unsigned int reserved; /**< true, if the master is reserved for RT */
    struct timer_list freerun_timer; /**< timer object for free run mode */
    ec_master_mode_t mode; /**< master mode */
};

/*****************************************************************************/

// master creation and deletion
int ec_master_init(ec_master_t *, unsigned int);
void ec_master_clear(struct kobject *);
void ec_master_reset(ec_master_t *);

// free run
void ec_master_freerun_start(ec_master_t *);
void ec_master_freerun_stop(ec_master_t *);

// IO
void ec_master_receive(ec_master_t *, const uint8_t *, size_t);
void ec_master_queue_command(ec_master_t *, ec_command_t *);
int ec_master_simple_io(ec_master_t *, ec_command_t *);

// slave management
int ec_master_bus_scan(ec_master_t *);

// misc.
void ec_master_debug(const ec_master_t *);
void ec_master_output_stats(ec_master_t *);
void ec_master_run_eoe(ec_master_t *);

/*****************************************************************************/

#endif
