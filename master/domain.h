/******************************************************************************
 *
 *  d o m a i n . h
 *
 *  Struktur für eine Gruppe von EtherCAT-Slaves.
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
   Datenfeld-Konfiguration.
*/

typedef struct
{
    struct list_head list;
    ec_slave_t *slave;
    const ec_sync_t *sync;
    uint32_t field_offset;
    void **data_ptr;
}
ec_field_reg_t;

/*****************************************************************************/

/**
   EtherCAT-Domäne

   Verwaltet die Prozessdaten und das hierfür nötige Kommando einer bestimmten
   Menge von Slaves.
*/

struct ec_domain
{
    struct kobject kobj; /**< Kobject der Domäne */
    struct list_head list; /**< Listenkopf */
    unsigned int index; /**< Domänen-Index */
    ec_master_t *master; /**< EtherCAT-Master, zu der die Domäne gehört. */
    size_t data_size; /**< Größe der Prozessdaten */
    struct list_head commands; /**< EtherCAT-Kommandos für die Prozessdaten */
    uint32_t base_address; /**< Logische Basisaddresse der Domain */
    unsigned int response_count; /**< Anzahl antwortender Slaves */
    struct list_head field_regs; /**< Liste der Datenfeldregistrierungen */
};

/*****************************************************************************/

int ec_domain_init(ec_domain_t *, ec_master_t *, unsigned int);
void ec_domain_clear(struct kobject *);
int ec_domain_alloc(ec_domain_t *, uint32_t);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
