/******************************************************************************
 *
 *  c o m m a n d . c
 *
 *  Methoden für ein EtherCAT-Kommando.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/slab.h>
#include <linux/delay.h>

#include "../include/EtherCAT_si.h"
#include "command.h"
#include "master.h"

/*****************************************************************************/

#define EC_FUNC_HEADER \
    command->index = 0; \
    command->working_counter = 0; \
    command->state = EC_CMD_INIT;

#define EC_FUNC_WRITE_FOOTER \
    command->data_size = data_size; \
    memcpy(command->data, data, data_size);

#define EC_FUNC_READ_FOOTER \
    command->data_size = data_size; \
    memset(command->data, 0x00, data_size);

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-NPRD-Kommando.

   Node-adressed physical read.
*/

void ec_command_init_nprd(ec_command_t *command,
                          /**< EtherCAT-Rahmen */
                          uint16_t node_address,
                          /**< Adresse des Knotens (Slaves) */
                          uint16_t offset,
                          /**< Physikalische Speicheradresse im Slave */
                          size_t data_size
                          /**< Länge der zu lesenden Daten */
                          )
{
    if (unlikely(node_address == 0x0000))
        EC_WARN("Using node address 0x0000!\n");

    EC_FUNC_HEADER;

    command->type = EC_CMD_NPRD;
    command->address.physical.slave = node_address;
    command->address.physical.mem = offset;

    EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-NPWR-Kommando.

   Node-adressed physical write.
*/

void ec_command_init_npwr(ec_command_t *command,
                          /**< EtherCAT-Rahmen */
                          uint16_t node_address,
                          /**< Adresse des Knotens (Slaves) */
                          uint16_t offset,
                          /**< Physikalische Speicheradresse im Slave */
                          size_t data_size,
                          /**< Länge der zu schreibenden Daten */
                          const uint8_t *data
                          /**< Zeiger auf Speicher mit zu schreibenden Daten */
                          )
{
    if (unlikely(node_address == 0x0000))
        EC_WARN("Using node address 0x0000!\n");

    EC_FUNC_HEADER;

    command->type = EC_CMD_NPWR;
    command->address.physical.slave = node_address;
    command->address.physical.mem = offset;

    EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-APRD-Kommando.

   Autoincrement physical read.
*/

void ec_command_init_aprd(ec_command_t *command,
                          /**< EtherCAT-Rahmen */
                          uint16_t ring_position,
                          /**< Position des Slaves im Bus */
                          uint16_t offset,
                          /**< Physikalische Speicheradresse im Slave */
                          size_t data_size
                          /**< Länge der zu lesenden Daten */
                          )
{
    EC_FUNC_HEADER;

    command->type = EC_CMD_APRD;
    command->address.physical.slave = (int16_t) ring_position * (-1);
    command->address.physical.mem = offset;

    EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-APWR-Kommando.

   Autoincrement physical write.
*/

void ec_command_init_apwr(ec_command_t *command,
                          /**< EtherCAT-Rahmen */
                          uint16_t ring_position,
                          /**< Position des Slaves im Bus */
                          uint16_t offset,
                          /**< Physikalische Speicheradresse im Slave */
                          size_t data_size,
                          /**< Länge der zu schreibenden Daten */
                          const uint8_t *data
                          /**< Zeiger auf Speicher mit zu schreibenden Daten */
                          )
{
    EC_FUNC_HEADER;

    command->type = EC_CMD_APWR;
    command->address.physical.slave = (int16_t) ring_position * (-1);
    command->address.physical.mem = offset;

    EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-BRD-Kommando.

   Broadcast read.
*/

void ec_command_init_brd(ec_command_t *command,
                         /**< EtherCAT-Rahmen */
                         uint16_t offset,
                         /**< Physikalische Speicheradresse im Slave */
                         size_t data_size
                         /**< Länge der zu lesenden Daten */
                         )
{
    EC_FUNC_HEADER;

    command->type = EC_CMD_BRD;
    command->address.physical.slave = 0x0000;
    command->address.physical.mem = offset;

    EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-BWR-Kommando.

   Broadcast write.
*/

void ec_command_init_bwr(ec_command_t *command,
                         /**< EtherCAT-Rahmen */
                         uint16_t offset,
                         /**< Physikalische Speicheradresse im Slave */
                         size_t data_size,
                         /**< Länge der zu schreibenden Daten */
                         const uint8_t *data
                         /**< Zeiger auf Speicher mit zu schreibenden Daten */
                         )
{
    EC_FUNC_HEADER;

    command->type = EC_CMD_BWR;
    command->address.physical.slave = 0x0000;
    command->address.physical.mem = offset;

    EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-LRW-Kommando.

   Logical read write.
*/

void ec_command_init_lrw(ec_command_t *command,
                         /**< EtherCAT-Rahmen */
                         uint32_t offset,
                         /**< Logische Startadresse */
                         size_t data_size,
                         /**< Länge der zu lesenden/schreibenden Daten */
                         uint8_t *data
                         /**< Zeiger auf die Daten */
                         )
{
    EC_FUNC_HEADER;

    command->type = EC_CMD_LRW;
    command->address.logical = offset;

    EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
