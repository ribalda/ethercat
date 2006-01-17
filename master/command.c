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

#include "command.h"

/*****************************************************************************/

/**
   Kommando-Konstruktor.

   Initialisiert alle Variablen innerhalb des Kommandos auf die
   Default-Werte.

   @param cmd Zeiger auf das zu initialisierende Kommando.
*/

void ec_command_init(ec_command_t *cmd)
{
  cmd->type = EC_COMMAND_NONE;
  cmd->address.logical = 0x00000000;
  cmd->data_length = 0;
  cmd->state = EC_COMMAND_STATE_READY;
  cmd->index = 0;
  cmd->working_counter = 0;
}

/*****************************************************************************/

/**
   Kommando-Destruktor.

   Setzt alle Attribute auf den Anfangswert zurueck.

   @param cmd Zeiger auf das zu initialisierende Kommando.
*/

void ec_command_clear(ec_command_t *cmd)
{
  ec_command_init(cmd);
}

/*****************************************************************************/

#define EC_FUNC_HEADER \
  ec_command_init(cmd)

#define EC_FUNC_WRITE_FOOTER \
  cmd->data_length = length; \
  memcpy(cmd->data, data, length);

#define EC_FUNC_READ_FOOTER \
  cmd->data_length = length;

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-NPRD-Kommando.

   @param cmd Zeiger auf das Kommando
   @param node_address Adresse des Knotens (Slaves)
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten
*/

void ec_command_read(ec_command_t *cmd, unsigned short node_address,
                     unsigned short offset, unsigned int length)
{
  if (unlikely(node_address == 0x0000))
    printk(KERN_WARNING "EtherCAT: Warning - Using node address 0x0000!\n");

  EC_FUNC_HEADER;

  cmd->type = EC_COMMAND_NPRD;
  cmd->address.phy.dev.node = node_address;
  cmd->address.phy.mem = offset;

  EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-NPWR-Kommando.

   Alloziert ein "node-adressed physical write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param node_address Adresse des Knotens (Slaves)
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu schreibenden Daten
   @param data Zeiger auf Speicher mit zu schreibenden Daten
*/

void ec_command_write(ec_command_t *cmd, unsigned short node_address,
                      unsigned short offset, unsigned int length,
                      const unsigned char *data)
{
  if (unlikely(node_address == 0x0000))
    printk(KERN_WARNING "EtherCAT: Warning - Using node address 0x0000!\n");

  EC_FUNC_HEADER;

  cmd->type = EC_COMMAND_NPWR;
  cmd->address.phy.dev.node = node_address;
  cmd->address.phy.mem = offset;

  EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-APRD-Kommando.

   Alloziert ein "autoincerement physical read"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param ring_position (Negative) Position des Slaves im Bus
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten
*/

void ec_command_position_read(ec_command_t *cmd, short ring_position,
                              unsigned short offset, unsigned int length)
{
  EC_FUNC_HEADER;

  cmd->type = EC_COMMAND_APRD;
  cmd->address.phy.dev.pos = ring_position;
  cmd->address.phy.mem = offset;

  EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-APWR-Kommando.

   Alloziert ein "autoincrement physical write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param ring_position (Negative) Position des Slaves im Bus
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu schreibenden Daten
   @param data Zeiger auf Speicher mit zu schreibenden Daten
*/

void ec_command_position_write(ec_command_t *cmd, short ring_position,
                               unsigned short offset, unsigned int length,
                               const unsigned char *data)
{
  EC_FUNC_HEADER;

  cmd->type = EC_COMMAND_APWR;
  cmd->address.phy.dev.pos = ring_position;
  cmd->address.phy.mem = offset;

  EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-BRD-Kommando.

   Alloziert ein "broadcast read"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten
*/

void ec_command_broadcast_read(ec_command_t *cmd, unsigned short offset,
                               unsigned int length)
{
  EC_FUNC_HEADER;

  cmd->type = EC_COMMAND_BRD;
  cmd->address.phy.dev.node = 0x0000;
  cmd->address.phy.mem = offset;

  EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-BWR-Kommando.

   Alloziert ein "broadcast write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu schreibenden Daten
   @param data Zeiger auf Speicher mit zu schreibenden Daten
*/

void ec_command_broadcast_write(ec_command_t *cmd, unsigned short offset,
                                unsigned int length, const unsigned char *data)
{
  EC_FUNC_HEADER;

  cmd->type = EC_COMMAND_BWR;
  cmd->address.phy.dev.node = 0x0000;
  cmd->address.phy.mem = offset;

  EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-LRW-Kommando.

   Alloziert ein "logical read write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param offset Logische Speicheradresse
   @param length Länge der zu lesenden/schreibenden Daten
   @param data Zeiger auf Speicher mit zu lesenden/schreibenden Daten
*/

void ec_command_logical_read_write(ec_command_t *cmd, unsigned int offset,
                                   unsigned int length, unsigned char *data)
{
  EC_FUNC_HEADER;

  cmd->type = EC_COMMAND_LRW;
  cmd->address.logical = offset;

  EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/
