/****************************************************************
 *
 *  e c _ c o m m a n d . c
 *
 *  Methoden für ein EtherCAT-Kommando.
 *
 *  $Date$
 *  $Author$
 *
 ***************************************************************/

#include <linux/slab.h>

#include "ec_command.h"
#include "ec_dbg.h"

/***************************************************************/

/**
   Kommando-Konstruktor.

   Initialisiert alle Variablen innerhalb des Kommandos auf die
   Default-Werte.

   @param cmd Zeiger auf das zu initialisierende Kommando.
*/

void EtherCAT_command_init(EtherCAT_command_t *cmd)
{
  cmd->type = ECAT_CMD_NONE;
  cmd->address.logical = 0x00000000;
  cmd->data_length = 0;
  //cmd->next = NULL;
  cmd->state = ECAT_CS_READY;
  cmd->index = 0;
  cmd->working_counter = 0;
}

/***************************************************************/

/**
   Kommando-Destruktor.

   Setzt alle Attribute auf den Anfangswert zurueck.

   @param cmd Zeiger auf das zu initialisierende Kommando.
*/

void EtherCAT_command_clear(EtherCAT_command_t *cmd)
{
  EtherCAT_command_init(cmd);
}

/***************************************************************/

#define ECAT_FUNC_HEADER \
  EtherCAT_command_init(cmd)

#define ECAT_FUNC_WRITE_FOOTER \
  cmd->data_length = length; \
  memcpy(cmd->data, data, length);

#define ECAT_FUNC_READ_FOOTER \
  cmd->data_length = length;

/***************************************************************/

/**
   Initialisiert ein EtherCAT-NPRD-Kommando.

   @param cmd Zeiger auf das Kommando
   @param node_address Adresse des Knotens (Slaves)
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten
*/

void EtherCAT_command_read(EtherCAT_command_t *cmd,
                           unsigned short node_address,
                           unsigned short offset,
                           unsigned int length)
{
  if (node_address == 0x0000)
    EC_DBG(KERN_WARNING "EtherCAT: Using node address 0x0000!\n");

  ECAT_FUNC_HEADER;

  cmd->type = ECAT_CMD_NPRD;
  cmd->address.phy.dev.node = node_address;
  cmd->address.phy.mem = offset;

  ECAT_FUNC_READ_FOOTER;
}

/***************************************************************/

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

void EtherCAT_command_write(EtherCAT_command_t *cmd,
                            unsigned short node_address,
                            unsigned short offset,
                            unsigned int length,
                            const unsigned char *data)
{
  if (node_address == 0x0000)
    EC_DBG(KERN_WARNING "WARNING: Using node address 0x0000!\n");

  ECAT_FUNC_HEADER;

  cmd->type = ECAT_CMD_NPWR;
  cmd->address.phy.dev.node = node_address;
  cmd->address.phy.mem = offset;

  ECAT_FUNC_WRITE_FOOTER;
}

/***************************************************************/

/**
   Initialisiert ein EtherCAT-APRD-Kommando.

   Alloziert ein "autoincerement physical read"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param ring_position (Negative) Position des Slaves im Bus
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten
*/

void EtherCAT_command_position_read(EtherCAT_command_t *cmd,
                                    short ring_position,
                                    unsigned short offset,
                                    unsigned int length)
{
  ECAT_FUNC_HEADER;

  cmd->type = ECAT_CMD_APRD;
  cmd->address.phy.dev.pos = ring_position;
  cmd->address.phy.mem = offset;

  ECAT_FUNC_READ_FOOTER;
}

/***************************************************************/

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

void EtherCAT_command_position_write(EtherCAT_command_t *cmd,
                                     short ring_position,
                                     unsigned short offset,
                                     unsigned int length,
                                     const unsigned char *data)
{
  ECAT_FUNC_HEADER;

  cmd->type = ECAT_CMD_APWR;
  cmd->address.phy.dev.pos = ring_position;
  cmd->address.phy.mem = offset;

  ECAT_FUNC_WRITE_FOOTER;
}

/***************************************************************/

/**
   Initialisiert ein EtherCAT-BRD-Kommando.

   Alloziert ein "broadcast read"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten
*/

void EtherCAT_command_broadcast_read(EtherCAT_command_t *cmd,
                                     unsigned short offset,
                                     unsigned int length)
{
  ECAT_FUNC_HEADER;

  cmd->type = ECAT_CMD_BRD;
  cmd->address.phy.dev.node = 0x0000;
  cmd->address.phy.mem = offset;

  ECAT_FUNC_READ_FOOTER;
}

/***************************************************************/

/**
   Initialisiert ein EtherCAT-BWR-Kommando.

   Alloziert ein "broadcast write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu schreibenden Daten
   @param data Zeiger auf Speicher mit zu schreibenden Daten
*/

void EtherCAT_command_broadcast_write(EtherCAT_command_t *cmd,
                                      unsigned short offset,
                                      unsigned int length,
                                      const unsigned char *data)
{
  ECAT_FUNC_HEADER;

  cmd->type = ECAT_CMD_BWR;
  cmd->address.phy.dev.node = 0x0000;
  cmd->address.phy.mem = offset;

  ECAT_FUNC_WRITE_FOOTER;
}

/***************************************************************/

/**
   Initialisiert ein EtherCAT-LRW-Kommando.

   Alloziert ein "logical read write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param cmd Zeiger auf das Kommando
   @param offset Logische Speicheradresse
   @param length Länge der zu lesenden/schreibenden Daten
   @param data Zeiger auf Speicher mit zu lesenden/schreibenden Daten
*/

void EtherCAT_command_logical_read_write(EtherCAT_command_t *cmd,
                                         unsigned int offset,
                                         unsigned int length,
                                         unsigned char *data)
{
  ECAT_FUNC_HEADER;

  cmd->type = ECAT_CMD_LRW;
  cmd->address.logical = offset;

  ECAT_FUNC_WRITE_FOOTER;
}

/***************************************************************/
