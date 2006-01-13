/******************************************************************************
 *
 *  e c _ d o m a i n . c
 *
 *  Methoden für Gruppen von EtherCAT-Slaves.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>

#include "ec_globals.h"
#include "ec_domain.h"

/*****************************************************************************/

/**
   Konstruktor einer EtherCAT-Domäne.

   @param pd Zeiger auf die zu initialisierende Domäne
*/

void EtherCAT_domain_init(EtherCAT_domain_t *dom)
{
  dom->number = 0;
  dom->data_size = 0;
  dom->logical_offset = 0;
  dom->response_count = 0;

  memset(dom->data, 0x00, ECAT_FRAME_BUFFER_SIZE);
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
