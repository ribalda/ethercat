/******************************************************************************
 *
 *  l i b e c . h
 *
 *  EtherCAT-Library fuer Echtzeitmodule
 *
 *  $Id$
 *
 *****************************************************************************/

#ifndef _LIBEC_H_
#define _LIBEC_H_

#ifndef _ETHERCAT_RT_H_
#include "../include/EtherCAT_rt.h"
#endif

/*****************************************************************************/

int LEC_read_EL10XX(ec_slave_t *slave, unsigned int channel);
int LEC_read_EL31XX(ec_slave_t *slave, unsigned int channel);

void LEC_write_EL20XX(ec_slave_t *slave, unsigned int channel, int value);
void LEC_write_EL41XX(ec_slave_t *slave, unsigned int channel, int value);

/*****************************************************************************/

#endif

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
