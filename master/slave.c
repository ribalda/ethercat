/******************************************************************************
 *
 *  s l a v e . c
 *
 *  Methoden für einen EtherCAT-Slave.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>

#include "globals.h"
#include "slave.h"

/*****************************************************************************/

/**
   EtherCAT-Slave-Konstruktor.

   Initialisiert einen EtherCAT-Slave.

   ACHTUNG! Dieser Konstruktor wird quasi nie aufgerufen. Bitte immer das
   Makro ECAT_INIT_SLAVE() in ec_slave.h anpassen!

   @param slave Zeiger auf den zu initialisierenden Slave
*/

void ec_slave_init(ec_slave_t *slave)
{
  slave->base_type = 0;
  slave->base_revision = 0;
  slave->base_build = 0;
  slave->ring_position = 0;
  slave->station_address = 0;
  slave->sii_vendor_id = 0;
  slave->sii_product_code = 0;
  slave->sii_revision_number = 0;
  slave->sii_serial_number = 0;
  slave->type = NULL;
  slave->logical_address = 0;
  slave->process_data = NULL;
  slave->private_data = NULL;
  slave->configure = NULL;
  slave->registered = 0;
  slave->domain = 0;
  slave->error_reported = 0;
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
