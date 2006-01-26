/******************************************************************************
 *
 *  d o m a i n . c
 *
 *  Methoden für Gruppen von EtherCAT-Slaves.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>

#include "globals.h"
#include "domain.h"

/*****************************************************************************/

/**
   Konstruktor einer EtherCAT-Domäne.

   @param dom Zeiger auf die zu initialisierende Domäne
*/

void ec_domain_init(ec_domain_t *dom)
{
  dom->number = -1;
  dom->data_size = 0;
  dom->logical_offset = 0;
  dom->response_count = 0xFFFFFFFF;

  memset(dom->data, 0x00, EC_FRAME_SIZE);
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
