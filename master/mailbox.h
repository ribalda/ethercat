/******************************************************************************
 *
 *  m a i l b o x . h
 *
 *  Mailbox functionality.
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _EC_MAILBOX_H_
#define _EC_MAILBOX_H_

#include "slave.h"

/*****************************************************************************/

uint8_t *ec_slave_mbox_prepare_send(ec_slave_t *, uint8_t, size_t);
int      ec_slave_mbox_prepare_check(ec_slave_t *);
int      ec_slave_mbox_check(const ec_slave_t *);
int      ec_slave_mbox_prepare_fetch(ec_slave_t *);
uint8_t *ec_slave_mbox_fetch(ec_slave_t *, uint8_t, size_t *);

uint8_t *ec_slave_mbox_simple_io(ec_slave_t *, size_t *);
uint8_t *ec_slave_mbox_simple_receive(ec_slave_t *, uint8_t, size_t *);

/*****************************************************************************/

#endif
