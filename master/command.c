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

#include "command.h"
#include "master.h"

/*****************************************************************************/

#define EC_FUNC_HEADER \
    if (unlikely(ec_command_prealloc(command, data_size))) \
        return -1; \
    command->index = 0; \
    command->working_counter = 0; \
    command->state = EC_CMD_INIT;

#define EC_FUNC_FOOTER \
    command->data_size = data_size; \
    memset(command->data, 0x00, data_size); \
    return 0;

/*****************************************************************************/

/**
   EtherCAT-Kommando-Konstruktor.
*/

void ec_command_init(ec_command_t *command)
{
    command->type = EC_CMD_NONE;
    command->address.logical = 0x00000000;
    command->data = NULL;
    command->mem_size = 0;
    command->data_size = 0;
    command->index = 0x00;
    command->working_counter = 0x00;
    command->state = EC_CMD_INIT;
}

/*****************************************************************************/

/**
   EtherCAT-Kommando-Destruktor.
*/

void ec_command_clear(ec_command_t *command)
{
    if (command->data) kfree(command->data);
}

/*****************************************************************************/

/**
   Alloziert Speicher.
*/

int ec_command_prealloc(ec_command_t *command, size_t size)
{
    if (size <= command->mem_size) return 0;

    if (command->data) {
        kfree(command->data);
        command->data = NULL;
        command->mem_size = 0;
    }

    if (!(command->data = kmalloc(size, GFP_KERNEL))) {
        EC_ERR("Failed to allocate %i bytes of command memory!\n", size);
        return -1;
    }

    command->mem_size = size;
    return 0;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-NPRD-Kommando.

   Node-adressed physical read.
*/

int ec_command_nprd(ec_command_t *command,
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
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-NPWR-Kommando.

   Node-adressed physical write.
*/

int ec_command_npwr(ec_command_t *command,
                    /**< EtherCAT-Rahmen */
                    uint16_t node_address,
                    /**< Adresse des Knotens (Slaves) */
                    uint16_t offset,
                    /**< Physikalische Speicheradresse im Slave */
                    size_t data_size
                    /**< Länge der zu schreibenden Daten */
                    )
{
    if (unlikely(node_address == 0x0000))
        EC_WARN("Using node address 0x0000!\n");

    EC_FUNC_HEADER;
    command->type = EC_CMD_NPWR;
    command->address.physical.slave = node_address;
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-APRD-Kommando.

   Autoincrement physical read.
*/

int ec_command_aprd(ec_command_t *command,
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
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-APWR-Kommando.

   Autoincrement physical write.
*/

int ec_command_apwr(ec_command_t *command,
                    /**< EtherCAT-Rahmen */
                    uint16_t ring_position,
                    /**< Position des Slaves im Bus */
                    uint16_t offset,
                    /**< Physikalische Speicheradresse im Slave */
                    size_t data_size
                    /**< Länge der zu schreibenden Daten */
                    )
{
    EC_FUNC_HEADER;
    command->type = EC_CMD_APWR;
    command->address.physical.slave = (int16_t) ring_position * (-1);
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-BRD-Kommando.

   Broadcast read.
*/

int ec_command_brd(ec_command_t *command,
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
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-BWR-Kommando.

   Broadcast write.
*/

int ec_command_bwr(ec_command_t *command,
                   /**< EtherCAT-Rahmen */
                   uint16_t offset,
                   /**< Physikalische Speicheradresse im Slave */
                   size_t data_size
                   /**< Länge der zu schreibenden Daten */
                   )
{
    EC_FUNC_HEADER;
    command->type = EC_CMD_BWR;
    command->address.physical.slave = 0x0000;
    command->address.physical.mem = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-LRW-Kommando.

   Logical read write.
*/

int ec_command_lrw(ec_command_t *command,
                   /**< EtherCAT-Rahmen */
                   uint32_t offset,
                   /**< Logische Startadresse */
                   size_t data_size
                   /**< Länge der zu lesenden/schreibenden Daten */
                   )
{
    EC_FUNC_HEADER;
    command->type = EC_CMD_LRW;
    command->address.logical = offset;
    EC_FUNC_FOOTER;
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
