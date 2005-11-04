/****************************************************************
 *
 *  e c _ m a s t e r . c
 *
 *  Methoden für einen EtherCAT-Master.
 *
 *  $Date$
 *  $Author$
 *
 ***************************************************************/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "ec_globals.h"
#include "ec_master.h"
#include "ec_dbg.h"

/***************************************************************/

/**
   Konstruktor des EtherCAT-Masters.

   @param master Zeiger auf den zu initialisierenden
   EtherCAT-Master
   @param dev Zeiger auf das EtherCAT-gerät, mit dem der
   Master arbeiten soll

   @return 0 bei Erfolg, sonst < 0 (= dev ist NULL)
*/

int EtherCAT_master_init(EtherCAT_master_t *master,
                         EtherCAT_device_t *dev)
{
  unsigned int i;

  if (!dev)
  {
    EC_DBG(KERN_ERR "EtherCAT: Master init without device!\n");
    return -1;
  }

  master->slaves = NULL;
  master->slave_count = 0;
  master->first_command = NULL;
  master->process_data_command = NULL;
  master->dev = dev;
  master->command_index = 0x00;
  master->tx_data_length = 0;
  master->process_data = NULL;
  master->process_data_length = 0;
  master->cmd_ring_index = 0;
  master->debug_level = 0;

  for (i = 0; i < ECAT_COMMAND_RING_SIZE; i++)
  {
    EtherCAT_command_init(&master->cmd_ring[i]);
    master->cmd_reserved[i] = 0;
  }

  return 0;
}

/***************************************************************/

/**
   Destruktor eines EtherCAT-Masters.

   Entfernt alle Kommandos aus der Liste, löscht den Zeiger
   auf das Slave-Array und gibt die Prozessdaten frei.

   @param master Zeiger auf den zu löschenden Master
*/

void EtherCAT_master_clear(EtherCAT_master_t *master)
{
  // Remove all pending commands
  while (master->first_command)
  {
    EtherCAT_remove_command(master, master->first_command);
  }

  // Remove all slaves
  EtherCAT_clear_slaves(master);

  if (master->process_data)
  {
    kfree(master->process_data);
    master->process_data = NULL;
  }

  master->process_data_length = 0;
}

/***************************************************************/

/**
   Überprüft die angeschlossenen Slaves.

   Vergleicht die an den Bus angeschlossenen Slaves mit
   den im statischen-Slave-Array vorgegebenen Konfigurationen.
   Stimmen Anzahl oder Typen nicht überein, gibt diese
   Methode einen Fehler aus.

   @param master Der EtherCAT-Master
   @param slaves Zeiger auf ein statisches Slave-Array
   @param slave_count Anzahl der Slaves im Array

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_check_slaves(EtherCAT_master_t *master,
                          EtherCAT_slave_t *slaves,
                          unsigned int slave_count)
{
  EtherCAT_command_t *cmd;
  EtherCAT_slave_t *cur;
  unsigned int i, j, found, length, pos;
  unsigned char data[2];

  // EtherCAT_clear_slaves() must be called before!
  if (master->slaves || master->slave_count)
  {
    EC_DBG(KERN_ERR "EtherCAT duplicate slave check!");
    return -1;
  }

  // No slaves.
  if (slave_count == 0)
  {
    EC_DBG(KERN_ERR "EtherCAT: No slaves in list!");
    return -1;
  }

  // Determine number of slaves on bus

  if ((cmd = EtherCAT_broadcast_read(master, 0x0000, 4)) == NULL)
  {
    return -1;
  }

  if (EtherCAT_async_send_receive(master) != 0)
  {
    return -1;
  }

  if (cmd->working_counter != slave_count)
  {
    EC_DBG(KERN_ERR "EtherCAT: Wrong number of slaves on bus: %i / %i\n",
           cmd->working_counter, slave_count);
    EtherCAT_remove_command(master, cmd);

    return -1;
  }
  else
  {
    EC_DBG("EtherCAT: Found all %i slaves.\n", slave_count);
  }

  EtherCAT_remove_command(master, cmd);

  // For every slave in the list
  for (i = 0; i < slave_count; i++)
  {
    cur = &slaves[i];

    if (!cur->desc)
    {
      EC_DBG(KERN_ERR "EtherCAT: Slave %i has no description.\n", i);
      return -1;
    }

    // Set ring position
    cur->ring_position = -i;
    cur->station_address = i + 1;

    // Write station address

    data[0] = cur->station_address & 0x00FF;
    data[1] = (cur->station_address & 0xFF00) >> 8;

    if ((cmd = EtherCAT_position_write(master, cur->ring_position, 0x0010, 2, data)) == NULL)
    {
      return -1;
    }

    if (EtherCAT_async_send_receive(master) != 0)
    {
      return -1;
    }

    if (cmd->working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: Slave %i did not repond while writing station address!\n", i);
      return -1;
    }

    EtherCAT_remove_command(master, cmd);

    // Read base data

    if ((cmd = EtherCAT_read(master, cur->station_address, 0x0000, 4)) == NULL)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not create command!\n");
      return -1;
    }

    if (EtherCAT_async_send_receive(master) != 0)
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Could not send command!\n");
      return -1;
    }

    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Slave %i did not respond while reading base data!\n", i);
      return -1;
    }

    // Get base data
    cur->type = cmd->data[0];
    cur->revision = cmd->data[1];
    cur->build = cmd->data[2] | (cmd->data[3] << 8);

    EtherCAT_remove_command(master, cmd);

    // Read identification from "Slave Information Interface" (SII)

    if (EtherCAT_read_slave_information(master, cur->station_address,
                                        0x0008, &cur->vendor_id) != 0)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not read SII vendor id!\n");
      return -1;
    }

    if (EtherCAT_read_slave_information(master, cur->station_address,
                                        0x000A, &cur->product_code) != 0)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not read SII product code!\n");
      return -1;
    }

    if (EtherCAT_read_slave_information(master, cur->station_address,
                                        0x000E, &cur->revision_number) != 0)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not read SII revision number!\n");
      return -1;
    }

    if (EtherCAT_read_slave_information(master, cur->station_address,
                                        0x0012, &cur->serial_number) != 0)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not read SII serial number!\n");
      return -1;
    }

    // Search for identification in "database"

    found = 0;

    for (j = 0; j < slave_idents_count; j++)
    {
      if (slave_idents[j].vendor_id == cur->vendor_id
          && slave_idents[j].product_code == cur->product_code)
      {
        found = 1;

        if (cur->desc != slave_idents[j].desc)
        {
          EC_DBG(KERN_ERR "EtherCAT: Unexpected slave device \"%s %s\""
                 " at position %i. Expected: \"%s %s\"\n",
                 slave_idents[j].desc->vendor_name,
                 slave_idents[j].desc->product_name, i,
                 cur->desc->vendor_name, cur->desc->product_name);
          return -1;
        }

        break;
      }
    }

    if (!found)
    {
      EC_DBG(KERN_ERR "EtherCAT: Unknown slave device"
             " (vendor %X, code %X) at position %i.\n",
             i, cur->vendor_id, cur->product_code);
      return -1;
    }
  }

  length = 0;
  for (i = 0; i < slave_count; i++)
  {
    length += slaves[i].desc->data_length;
  }

  if ((master->process_data = (unsigned char *)
       kmalloc(sizeof(unsigned char) * length, GFP_KERNEL)) == NULL)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not allocate %i bytes for process data.\n", length);
    return -1;
  }

  master->process_data_length = length;
  memset(master->process_data, 0x00, length);

  pos = 0;
  for (i = 0; i < slave_count; i++)
  {
    slaves[i].process_data = master->process_data + pos;
    slaves[i].logical_address0 = pos;

    EC_DBG(KERN_DEBUG "EtherCAT: Slave %i - Address 0x%X, \"%s %s\", s/n %u\n",
           i, pos, slaves[i].desc->vendor_name, slaves[i].desc->product_name,
           slaves[i].serial_number);

    pos += slaves[i].desc->data_length;
  }

  master->slaves = slaves;
  master->slave_count = slave_count;

  return 0;
}

/***************************************************************/

/**
   Entfernt den Zeiger auf das Slave-Array.

   @param master EtherCAT-Master
*/

void EtherCAT_clear_slaves(EtherCAT_master_t *master)
{
  master->slaves = NULL;
  master->slave_count = 0;
}

/***************************************************************/

/**
   Liest Daten aus dem Slave-Information-Interface
   eines EtherCAT-Slaves.

   @param master EtherCAT-Master
   @param node_address Knotenadresse des Slaves
   @param offset Adresse des zu lesenden SII-Registers
   @param target Zeiger auf einen 4 Byte großen Speicher
   zum Ablegen der Daten

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_read_slave_information(EtherCAT_master_t *master,
                                    unsigned short int node_address,
                                    unsigned short int offset,
                                    unsigned int *target)
{
  EtherCAT_command_t *cmd;
  unsigned char data[10];
  unsigned int tries_left;

  // Initiate read operation

  data[0] = 0x00;
  data[1] = 0x01;
  data[2] = offset & 0xFF;
  data[3] = (offset & 0xFF00) >> 8;
  data[4] = 0x00;
  data[5] = 0x00;

  if ((cmd = EtherCAT_write(master, node_address, 0x502, 6, data)) == NULL)
    return -2;

  if (EtherCAT_async_send_receive(master) != 0)
  {
    EtherCAT_remove_command(master, cmd);
    return -3;
  }

  if (cmd->working_counter != 1)
  {
    EtherCAT_remove_command(master, cmd);
    EC_DBG(KERN_ERR "EtherCAT: SII-read - Slave %04X did not respond!\n",
           node_address);
    return -4;
  }

  EtherCAT_remove_command(master, cmd);

  // Get status of read operation

  // ?? FIXME warum hier tries ?? Hm

  // Der Slave legt die Informationen des Slave-Information-Interface
  // in das Datenregister und löscht daraufhin ein Busy-Bit. Solange
  // den Status auslesen, bis das Bit weg ist. fp

  tries_left = 1000;
  while (tries_left)
  {
    udelay(10);

    if ((cmd = EtherCAT_read(master, node_address, 0x502, 10)) == NULL)
      return -2;

    if (EtherCAT_async_send_receive(master) != 0)
    {
      EtherCAT_remove_command(master, cmd);
      return -3;
    }

    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);
      EC_DBG(KERN_ERR "EtherCAT: SII-read status - Slave %04X did not respond!\n",
             node_address);
      return -4;
    }

    if ((cmd->data[1] & 0x81) == 0)
    {
      memcpy(target, cmd->data + 6, 4);
      EtherCAT_remove_command(master, cmd);
      break;
    }

    EtherCAT_remove_command(master, cmd);

    tries_left--;
  }

  if (!tries_left)
  {
    EC_DBG(KERN_WARNING "EtherCAT: SSI-read. Slave %04X timed out!\n",
           node_address);
    return -1;
  }

  return 0;
}

/***************************************************************/

/**
   Führt ein asynchrones Senden und Empfangen aus.

   Sendet alle wartenden Kommandos und wartet auf die
   entsprechenden Antworten.

   @param master EtherCAT-Master

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_async_send_receive(EtherCAT_master_t *master)
{
  unsigned int wait_cycles;
  int i;

  // Send all commands

  for (i = 0; i < ECAT_NUM_RETRIES; i++)
  {
    if (EtherCAT_send(master) < 0)
    {
      return -1;
    }

    // Wait until something is received or an error has occurred

    wait_cycles = 10;
    EtherCAT_device_call_isr(master->dev);

    while (master->dev->state == ECAT_DS_SENT && wait_cycles)
    {
      udelay(1000);
      wait_cycles--;
      EtherCAT_device_call_isr(master->dev);
    }

    //EC_DBG("Master async send: tries %d",tries_left);

    if (!wait_cycles)
    {
      EC_DBG(KERN_ERR "EtherCAT: Asynchronous receive timeout.\n");
      continue;
    }

    if (master->dev->state != ECAT_DS_RECEIVED)
    {
      EC_DBG(KERN_ERR "EtherCAT: Asyncronous send error. State %i\n", master->dev->state);
      continue;
    }

    // Receive all commands
    if (EtherCAT_receive(master) < 0)
    {
      // Noch mal versuchen
      master->dev->state = ECAT_DS_READY;
      EC_DBG("Retry Asynchronous send/recieve: %d", i);
      continue;
    }

    return 0; // Erfolgreich
  }

  return -1;
}

/***************************************************************/

/**
   Sendet alle wartenden Kommandos.

   Errechnet erst die benötigte Gesamt-Rahmenlänge, und erstellt
   dann alle Kommando-Bytefolgen im statischen Sendespeicher.
   Danach wird die Sendefunktion des EtherCAT-Gerätes aufgerufen.

   @param master EtherCAT-Master

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_send(EtherCAT_master_t *master)
{
  unsigned int i, length, framelength, pos;
  EtherCAT_command_t *cmd;
  int cmdcnt = 0;

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "EtherCAT_send, first_command = %X\n", (int) master->first_command);
  }

  length = 0;
  for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
  {
    if (cmd->state != ECAT_CS_READY) continue;
    length += cmd->data_length + 12;
    cmdcnt++;
  }

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "%i commands to send.\n", cmdcnt);
  }

  if (length == 0)
  {
    EC_DBG(KERN_WARNING "EtherCAT: Nothing to send...\n");
    return 0;
  }

  framelength = length + 2;
  if (framelength < 46) framelength = 46;

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "framelength: %i\n", framelength);
  }

  master->tx_data[0] = length & 0xFF;
  master->tx_data[1] = ((length & 0x700) >> 8) | 0x10;
  pos = 2;

  for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
  {
    if (cmd->state != ECAT_CS_READY) continue;

    cmd->index = master->command_index;
    master->command_index = (master->command_index + 1) % 0x0100;

    if (master->debug_level > 0)
    {
      EC_DBG(KERN_DEBUG "Sending command index %i\n", cmd->index);
    }

    cmd->state = ECAT_CS_SENT;

    master->tx_data[pos + 0] = cmd->type;
    master->tx_data[pos + 1] = cmd->index;

    master->tx_data[pos + 2] = cmd->address.raw[0];
    master->tx_data[pos + 3] = cmd->address.raw[1];
    master->tx_data[pos + 4] = cmd->address.raw[2];
    master->tx_data[pos + 5] = cmd->address.raw[3];

    master->tx_data[pos + 6] = cmd->data_length & 0xFF;
    master->tx_data[pos + 7] = (cmd->data_length & 0x700) >> 8;

    if (cmd->next) master->tx_data[pos + 7] |= 0x80;

    master->tx_data[pos + 8] = 0x00;
    master->tx_data[pos + 9] = 0x00;

    if (cmd->type == ECAT_CMD_APWR
        || cmd->type == ECAT_CMD_NPWR
        || cmd->type == ECAT_CMD_BWR
        || cmd->type == ECAT_CMD_LRW) // Write
    {
      for (i = 0; i < cmd->data_length; i++) master->tx_data[pos + 10 + i] = cmd->data[i];
    }
    else // Read
    {
      for (i = 0; i < cmd->data_length; i++) master->tx_data[pos + 10 + i] = 0x00;
    }

    master->tx_data[pos + 10 + cmd->data_length] = 0x00;
    master->tx_data[pos + 11 + cmd->data_length] = 0x00;

    pos += 12 + cmd->data_length;
  }

  // Pad with zeros
  while (pos < 46) master->tx_data[pos++] = 0x00;

  master->tx_data_length = framelength;

#ifdef DEBUG_SEND_RECEIVE
  EC_DBG(KERN_DEBUG "\n");
  EC_DBG(KERN_DEBUG ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
  EC_DBG(KERN_DEBUG);
  for (i = 0; i < framelength; i++)
  {
    EC_DBG("%02X ", master->tx_data[i]);

    if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
  }
  EC_DBG("\n");
  EC_DBG(KERN_DEBUG "-----------------------------------------------\n");
#endif

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "device send...\n");
  }

  // Send frame
  if (EtherCAT_device_send(master->dev, master->tx_data, framelength) != 0)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not send!\n");
    EC_DBG(KERN_DEBUG "EtherCAT: tx_data content:\n");
    EC_DBG(KERN_DEBUG);
    for (i = 0; i < framelength; i++)
    {
      EC_DBG("%02X ", master->tx_data[i]);
      if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
    }
    EC_DBG("\n");
    return -1;
  }

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "EtherCAT_send done.\n");
  }

  return 0;
}

/***************************************************************/

/**
   Holt alle empfangenen Kommandos vom EtherCAT-Gerät.

   Kopiert einen empfangenen Rahmen vom EtherCAT-Gerät
   in den Statischen Empfangsspeicher und ordnet dann
   allen gesendeten Kommandos ihre Antworten zu.

   @param master EtherCAT-Master

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_receive(EtherCAT_master_t *master)
{
  EtherCAT_command_t *cmd;
  unsigned int length, pos, found, rx_data_length;
  unsigned int command_follows, command_type, command_index;
  unsigned int i;

  // Copy received data into master buffer (with locking)
  rx_data_length = EtherCAT_device_receive(master->dev, master->rx_data,
                                           ECAT_FRAME_BUFFER_SIZE);

#ifdef DEBUG_SEND_RECEIVE
  for (i = 0; i < rx_data_length; i++)
  {
    if (master->rx_data[i] == master->tx_data[i]) EC_DBG("   ");
    else EC_DBG("%02X ", master->rx_data[i]);
    if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
  }
  EC_DBG("\n");
  EC_DBG(KERN_DEBUG "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
  EC_DBG(KERN_DEBUG "\n");
#endif

  if (rx_data_length < 2)
  {
    EC_DBG(KERN_ERR "EtherCAT: Received corrupted frame (illegal length)!\n");
    EC_DBG(KERN_DEBUG "EtherCAT: tx_data content:\n");
    EC_DBG(KERN_DEBUG);
    for (i = 0; i < master->tx_data_length; i++)
    {
      EC_DBG("%02X ", master->tx_data[i]);
      if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
    }
    EC_DBG("\n");
    EC_DBG(KERN_DEBUG "EtherCAT: rx_data content:\n");
    EC_DBG(KERN_DEBUG);
    for (i = 0; i < rx_data_length; i++)
    {
      EC_DBG("%02X ", master->rx_data[i]);
      if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
    }
    EC_DBG("\n");
    return -1;
  }

  // Länge des gesamten Frames prüfen
  length = ((master->rx_data[1] & 0x07) << 8) | (master->rx_data[0] & 0xFF);

  if (length > rx_data_length)
  {
    EC_DBG(KERN_ERR "EtherCAT: Received corrupted frame (length does not match)!\n");
    EC_DBG(KERN_DEBUG "EtherCAT: tx_data content:\n");
    EC_DBG(KERN_DEBUG);
    for (i = 0; i < master->tx_data_length; i++)
    {
      EC_DBG("%02X ", master->tx_data[i]);
      if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
    }
    EC_DBG("\n");
    EC_DBG(KERN_DEBUG "EtherCAT: rx_data content:\n");
    EC_DBG(KERN_DEBUG);
    for (i = 0; i < rx_data_length; i++)
    {
      EC_DBG("%02X ", master->rx_data[i]);
      if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
    }
    EC_DBG("\n");
    return -1;
  }

  pos = 2; // LibPCAP: 16
  command_follows = 1;
  while (command_follows)
  {
    if (pos + 10 > rx_data_length)
    {
      EC_DBG(KERN_ERR "EtherCAT: Received frame with incomplete command header!\n");
      EC_DBG(KERN_DEBUG "EtherCAT: tx_data content:\n");
      EC_DBG(KERN_DEBUG);
      for (i = 0; i < master->tx_data_length; i++)
      {
        EC_DBG("%02X ", master->tx_data[i]);
        if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
      }
      EC_DBG("\n");
      EC_DBG(KERN_DEBUG "EtherCAT: rx_data content:\n");
      EC_DBG(KERN_DEBUG);
      for (i = 0; i < rx_data_length; i++)
      {
        EC_DBG("%02X ", master->rx_data[i]);
        if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
      }
      EC_DBG("\n");
      return -1;
    }

    command_type = master->rx_data[pos];
    command_index = master->rx_data[pos + 1];
    length = (master->rx_data[pos + 6] & 0xFF)
      | ((master->rx_data[pos + 7] & 0x07) << 8);
    command_follows = master->rx_data[pos + 7] & 0x80;

    if (pos + length + 12 > rx_data_length)
    {
      EC_DBG(KERN_ERR "EtherCAT: Received frame with incomplete command data!\n");
      EC_DBG(KERN_DEBUG "EtherCAT: tx_data content:\n");
      EC_DBG(KERN_DEBUG);
      for (i = 0; i < master->tx_data_length; i++)
      {
        EC_DBG("%02X ", master->tx_data[i]);
        if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
      }
      EC_DBG("\n");
      EC_DBG(KERN_DEBUG "EtherCAT: rx_data content:\n");
      EC_DBG(KERN_DEBUG);
      for (i = 0; i < rx_data_length; i++)
      {
        EC_DBG("%02X ", master->rx_data[i]);
        if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
      }
      EC_DBG("\n");
      return -1;
    }

    found = 0;

    for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
    {
      if (cmd->state == ECAT_CS_SENT
          && cmd->type == command_type
          && cmd->index == command_index
          && cmd->data_length == length)
      {
        found = 1;
        cmd->state = ECAT_CS_RECEIVED;

        // Empfangene Daten in Kommandodatenspeicher kopieren
        memcpy(cmd->data, master->rx_data + pos + 10, length);

        // Working-Counter setzen
        cmd->working_counter = (master->rx_data[pos + length + 10] & 0xFF)
          | ((master->rx_data[pos + length + 11] & 0xFF) << 8);
      }
    }

    if (!found)
    {
      EC_DBG(KERN_WARNING "EtherCAT: WARNING - Command not assigned!\n");
    }

    pos += length + 12;
  }

  for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
  {
    if (cmd->state == ECAT_CS_SENT)
    {
      EC_DBG(KERN_WARNING "EtherCAT: WARNING - One command received no response!\n");
    }
  }

  master->dev->state = ECAT_DS_READY;

  return 0;
}

/***************************************************************/

#define ECAT_FUNC_HEADER \
  EtherCAT_command_t *cmd; \
  if ((cmd = alloc_cmd(master)) == NULL) \
  { \
    EC_DBG(KERN_ERR "EtherCAT: Out of memory while creating command!\n"); \
    return NULL; \
  } \
  EtherCAT_command_init(cmd)

#define ECAT_FUNC_WRITE_FOOTER \
  cmd->data_length = length; \
  memcpy(cmd->data, data, length); \
  if (add_command(master, cmd) < 0) return NULL; \
  return cmd

#define ECAT_FUNC_READ_FOOTER \
  cmd->data_length = length; \
  if (add_command(master, cmd) < 0) return NULL; \
  return cmd

/***************************************************************/

/**
   Erstellt ein EtherCAT-NPRD-Kommando.

   Alloziert ein "node-adressed physical read"-Kommando
   und fügt es in die Liste des Masters ein.

   @param master EtherCAT-Master
   @param node_address Adresse des Knotens (Slaves)
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten

   @return Adresse des Kommandos bei Erfolg, sonst NULL
*/

EtherCAT_command_t *EtherCAT_read(EtherCAT_master_t *master,
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
   Erstellt ein EtherCAT-NPWR-Kommando.

   Alloziert ein "node-adressed physical write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param master EtherCAT-Master
   @param node_address Adresse des Knotens (Slaves)
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu schreibenden Daten
   @param data Zeiger auf Speicher mit zu schreibenden Daten

   @return Adresse des Kommandos bei Erfolg, sonst NULL
*/

EtherCAT_command_t *EtherCAT_write(EtherCAT_master_t *master,
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
   Erstellt ein EtherCAT-APRD-Kommando.

   Alloziert ein "autoincerement physical read"-Kommando
   und fügt es in die Liste des Masters ein.

   @param master EtherCAT-Master
   @param ring_position (Negative) Position des Slaves im Bus
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten

   @return Adresse des Kommandos bei Erfolg, sonst NULL
*/

EtherCAT_command_t *EtherCAT_position_read(EtherCAT_master_t *master,
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
   Erstellt ein EtherCAT-APWR-Kommando.

   Alloziert ein "autoincrement physical write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param master EtherCAT-Master
   @param ring_position (Negative) Position des Slaves im Bus
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu schreibenden Daten
   @param data Zeiger auf Speicher mit zu schreibenden Daten

   @return Adresse des Kommandos bei Erfolg, sonst NULL
*/

EtherCAT_command_t *EtherCAT_position_write(EtherCAT_master_t *master,
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
   Erstellt ein EtherCAT-BRD-Kommando.

   Alloziert ein "broadcast read"-Kommando
   und fügt es in die Liste des Masters ein.

   @param master EtherCAT-Master
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu lesenden Daten

   @return Adresse des Kommandos bei Erfolg, sonst NULL
*/

EtherCAT_command_t *EtherCAT_broadcast_read(EtherCAT_master_t *master,
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
   Erstellt ein EtherCAT-BWR-Kommando.

   Alloziert ein "broadcast write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param master EtherCAT-Master
   @param offset Physikalische Speicheradresse im Slave
   @param length Länge der zu schreibenden Daten
   @param data Zeiger auf Speicher mit zu schreibenden Daten

   @return Adresse des Kommandos bei Erfolg, sonst NULL
*/

EtherCAT_command_t *EtherCAT_broadcast_write(EtherCAT_master_t *master,
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
   Erstellt ein EtherCAT-LRW-Kommando.

   Alloziert ein "logical read write"-Kommando
   und fügt es in die Liste des Masters ein.

   @param master EtherCAT-Master
   @param offset Logische Speicheradresse
   @param length Länge der zu lesenden/schreibenden Daten
   @param data Zeiger auf Speicher mit zu lesenden/schreibenden Daten

   @return Adresse des Kommandos bei Erfolg, sonst NULL
*/

EtherCAT_command_t *EtherCAT_logical_read_write(EtherCAT_master_t *master,
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

/**
   Alloziert ein neues Kommando aus dem Kommandoring.

   Durchsucht den Kommandoring nach einem freien Kommando,
   reserviert es und gibt dessen Adresse zurück.

   @param master EtherCAT-Master

   @return Adresse des Kommandos bei Erfolg, sonst NULL
*/

EtherCAT_command_t *alloc_cmd(EtherCAT_master_t *master)
{
  int j;

  for (j = 0; j < ECAT_COMMAND_RING_SIZE; j++) // Einmal rum suchen
  {
    // Solange suchen, bis freies Kommando gefunden
    if (master->cmd_reserved[master->cmd_ring_index] == 0)
    {
      master->cmd_reserved[master->cmd_ring_index] = 1; // Belegen

      if (master->debug_level)
      {
        EC_DBG(KERN_DEBUG "Allocating command %i (addr %X).\n",
               master->cmd_ring_index,
               (int) &master->cmd_ring[master->cmd_ring_index]);
      }

      return &master->cmd_ring[master->cmd_ring_index];
    }

    if (master->debug_level)
    {
      EC_DBG(KERN_DEBUG "Command %i (addr %X) is reserved...\n",
             master->cmd_ring_index,
             (int) &master->cmd_ring[master->cmd_ring_index]);
    }

    master->cmd_ring_index++;
    master->cmd_ring_index %= ECAT_COMMAND_RING_SIZE;
  }

  EC_DBG(KERN_WARNING "EtherCAT: Command ring full!\n");

  return NULL; // Nix gefunden
}

/***************************************************************/

/**
   Fügt ein Kommando in die Liste des Masters ein.

   @param master EtherCAT-Master
   @param cmd Zeiger auf das einzufügende Kommando
*/

int add_command(EtherCAT_master_t *master,
                EtherCAT_command_t *new_cmd)
{
  EtherCAT_command_t *cmd, **last_cmd;

  for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
  {
    if (cmd == new_cmd)
    {
      EC_DBG(KERN_WARNING "EtherCAT: Trying to add a command"
             " that is already in the list!\n");
      return -1;
    }
  }

  // Find the address of the last pointer in the list
  last_cmd = &(master->first_command);
  while (*last_cmd) last_cmd = &(*last_cmd)->next;

  // Let this pointer point to the new command
  *last_cmd = new_cmd;

  return 0;
}

/***************************************************************/

/**
   Entfernt ein Kommando aus der Liste und gibt es frei.

   Prüft erst, ob das Kommando in der Liste des Masters
   ist. Wenn ja, wird es entfernt und die Reservierung wird
   aufgehoben.

   @param master EtherCAT-Master
   @param rem_cmd Zeiger auf das zu entfernende Kommando

   @return 0 bei Erfolg, sonst < 0
*/

void EtherCAT_remove_command(EtherCAT_master_t *master,
                             EtherCAT_command_t *rem_cmd)
{
  EtherCAT_command_t **last_cmd;
  int i;

  last_cmd = &master->first_command;
  while (*last_cmd)
  {
    if (*last_cmd == rem_cmd)
    {
      *last_cmd = rem_cmd->next;
      EtherCAT_command_clear(rem_cmd);

      // Reservierung des Kommandos aufheben
      for (i = 0; i < ECAT_COMMAND_RING_SIZE; i++)
      {
        if (&master->cmd_ring[i] == rem_cmd)
        {
          master->cmd_reserved[i] = 0;
          return;
        }
      }

      EC_DBG(KERN_WARNING "EtherCAT: Could not remove command reservation!\n");
      return;
    }

    last_cmd = &(*last_cmd)->next;
  }

  EC_DBG(KERN_WARNING "EtherCAT: Trying to remove non-existent command!\n");
}

/***************************************************************/

/**
   Ändert den Zustand eines Slaves (asynchron).

   Führt eine (asynchrone) Zustandsänderung bei einem Slave durch.

   @param master EtherCAT-Master
   @param slave Slave, dessen Zustand geändert werden soll
   @param state_and_ack Neuer Zustand, evtl. mit gesetztem
   Acknowledge-Flag

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_state_change(EtherCAT_master_t *master,
                          EtherCAT_slave_t *slave,
                          unsigned char state_and_ack)
{
  EtherCAT_command_t *cmd;
  unsigned char data[2];
  unsigned int tries_left;

  data[0] = state_and_ack;
  data[1] = 0x00;

  if ((cmd = EtherCAT_write(master, slave->station_address,
                            0x0120, 2, data)) == NULL)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not set state %02X - Out of memory!\n", state_and_ack);
    return -1;
  }

  if (EtherCAT_async_send_receive(master) != 0)
  {
    EtherCAT_remove_command(master, cmd);

    EC_DBG(KERN_ERR "EtherCAT: Could not set state %02X - Unable to send!\n", state_and_ack);
    return -2;
  }

  if (cmd->working_counter != 1)
  {
    EtherCAT_remove_command(master, cmd);

    EC_DBG(KERN_ERR "EtherCAT: Could not set state %02X - Device did not respond!\n", state_and_ack);
    return -3;
  }

  EtherCAT_remove_command(master, cmd);

  slave->requested_state = state_and_ack & 0x0F;

  tries_left = 1000;

  while (tries_left)
  {
    udelay(10);

    if ((cmd = EtherCAT_read(master, slave->station_address,
                             0x0130, 2)) == NULL)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not check state %02X - Out of memory!\n", state_and_ack);
      return -1;
    }

    if (EtherCAT_async_send_receive(master) != 0)
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Could not check state %02X - Unable to send!\n", state_and_ack);
      return -2;
    }

    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Could not check state %02X - Device did not respond!\n", state_and_ack);
      return -3;
    }

    if (cmd->data[0] & 0x10) // State change error
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Could not set state %02X - Device refused state change (code %02X)!\n", state_and_ack, cmd->data[0]);
      return -4;
    }

    if (cmd->data[0] == (state_and_ack & 0x0F)) // State change successful
    {
      EtherCAT_remove_command(master, cmd);

      break;
    }

    EtherCAT_remove_command(master, cmd);

    tries_left--;
  }

  if (!tries_left)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not check state %02X - Timeout while checking!\n", state_and_ack);
    return -5;
  }

  slave->current_state = state_and_ack & 0x0F;

  return 0;
}

/***************************************************************/

/**
   Konfiguriert einen Slave und setzt den Operational-Zustand.

   Führt eine komplette Konfiguration eines Slaves durch,
   setzt Sync-Manager und FMMU's, führt die entsprechenden
   Zustandsübergänge durch, bis der Slave betriebsbereit ist.

   @param master EtherCAT-Master
   @param slave Zu aktivierender Slave

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_activate_slave(EtherCAT_master_t *master,
                            EtherCAT_slave_t *slave)
{
  EtherCAT_command_t *cmd;
  const EtherCAT_slave_desc_t *desc;
  unsigned char fmmu[16];
  unsigned char data[256];

  desc = slave->desc;

  if (EtherCAT_state_change(master, slave, ECAT_STATE_INIT) != 0)
  {
    return -1;
  }

  // Resetting FMMU's

  memset(data, 0x00, 256);

  if ((cmd = EtherCAT_write(master, slave->station_address, 0x0600, 256, data)) == NULL)
    return -1;

  if (EtherCAT_async_send_receive(master) < 0)
  {
    EtherCAT_remove_command(master, cmd);

    return -1;
  }

  if (cmd->working_counter != 1)
  {
    EtherCAT_remove_command(master, cmd);

    EC_DBG(KERN_ERR "EtherCAT: Resetting FMMUs - Slave %04X did not respond!\n",
           slave->station_address);
    return -2;
  }

  EtherCAT_remove_command(master, cmd);

  // Resetting Sync Manager channels

  if (desc->type != ECAT_ST_SIMPLE_NOSYNC)
  {
    memset(data, 0x00, 256);

    if ((cmd = EtherCAT_write(master, slave->station_address, 0x0800, 256, data)) == NULL)
      return -1;

    if (EtherCAT_async_send_receive(master) < 0)
    {
      EtherCAT_remove_command(master, cmd);

      return -1;
    }

    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Resetting SMs - Slave %04X did not respond!\n",
             slave->station_address);
      return -2;
    }

    EtherCAT_remove_command(master, cmd);
  }

  // Init Mailbox communication

  if (desc->type == ECAT_ST_MAILBOX)
  {
    if (desc->sm0)
    {
      if ((cmd = EtherCAT_write(master, slave->station_address, 0x0800, 8, desc->sm0)) == NULL)
        return -1;

      if (EtherCAT_async_send_receive(master) < 0)
      {
        EtherCAT_remove_command(master, cmd);

        return -1;
      }

      if (cmd->working_counter != 1)
      {
        EtherCAT_remove_command(master, cmd);

        EC_DBG(KERN_ERR "EtherCAT: Setting SM0 - Slave %04X did not respond!\n",
               slave->station_address);
        return -3;
      }

      EtherCAT_remove_command(master, cmd);
    }

    if (desc->sm1)
    {
      if ((cmd = EtherCAT_write(master, slave->station_address, 0x0808, 8, desc->sm1)) == NULL)
        return -1;

      if (EtherCAT_async_send_receive(master) < 0)
      {
        EtherCAT_remove_command(master, cmd);

        return -1;
      }

      if (cmd->working_counter != 1)
      {
        EtherCAT_remove_command(master, cmd);

        EC_DBG(KERN_ERR "EtherCAT: Setting SM1 - Slave %04X did not respond!\n",
               slave->station_address);
        return -2;
      }

      EtherCAT_remove_command(master, cmd);
    }
  }

  // Change state to PREOP

  if (EtherCAT_state_change(master, slave, ECAT_STATE_PREOP) != 0)
  {
    return -5;
  }

  // Set FMMU's

  if (desc->fmmu0)
  {
    memcpy(fmmu, desc->fmmu0, 16);

    fmmu[0] = slave->logical_address0 & 0x000000FF;
    fmmu[1] = (slave->logical_address0 & 0x0000FF00) >> 8;
    fmmu[2] = (slave->logical_address0 & 0x00FF0000) >> 16;
    fmmu[3] = (slave->logical_address0 & 0xFF000000) >> 24;

    if ((cmd = EtherCAT_write(master, slave->station_address, 0x0600, 16, fmmu)) == NULL)
      return -1;

    if (EtherCAT_async_send_receive(master) < 0)
    {
      EtherCAT_remove_command(master, cmd);

      return -1;
    }

    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Setting FMMU0 - Slave %04X did not respond!\n",
             slave->station_address);
      return -2;
    }

    EtherCAT_remove_command(master, cmd);
  }

  // Set Sync Managers

  if (desc->type != ECAT_ST_MAILBOX)
  {
    if (desc->sm0)
    {
      if ((cmd = EtherCAT_write(master, slave->station_address, 0x0800, 8, desc->sm0)) == NULL)
        return -1;

      if (EtherCAT_async_send_receive(master) < 0)
      {
        EtherCAT_remove_command(master, cmd);

        return -1;
      }

      if (cmd->working_counter != 1)
      {
        EtherCAT_remove_command(master, cmd);

        EC_DBG(KERN_ERR "EtherCAT: Setting SM0 - Slave %04X did not respond!\n",
               slave->station_address);
        return -3;
      }

      EtherCAT_remove_command(master, cmd);
    }

    if (desc->sm1)
    {
      if ((cmd = EtherCAT_write(master, slave->station_address, 0x0808, 8, desc->sm1)) == NULL)
        return -1;

      if (EtherCAT_async_send_receive(master) < 0)
      {
        EtherCAT_remove_command(master, cmd);

        return -1;
      }

      if (cmd->working_counter != 1)
      {
        EtherCAT_remove_command(master, cmd);

        EC_DBG(KERN_ERR "EtherCAT: Setting SM1 - Slave %04X did not respond!\n",
               slave->station_address);
        return -3;
      }

      EtherCAT_remove_command(master, cmd);
    }
  }

  if (desc->sm2)
  {
    if ((cmd = EtherCAT_write(master, slave->station_address, 0x0810, 8, desc->sm2)) == NULL)
      return -1;

    if (EtherCAT_async_send_receive(master) < 0)
    {
      EtherCAT_remove_command(master, cmd);

      return -1;
    }

    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Setting SM2 - Slave %04X did not respond!\n",
             slave->station_address);
      return -3;
    }

    EtherCAT_remove_command(master, cmd);
  }

  if (desc->sm3)
  {
    if ((cmd = EtherCAT_write(master, slave->station_address, 0x0818, 8, desc->sm3)) == NULL)
      return -1;

    if (EtherCAT_async_send_receive(master) < 0)
    {
      EtherCAT_remove_command(master, cmd);

      return -1;
    }

    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);

      EC_DBG(KERN_ERR "EtherCAT: Setting SM3 - Slave %04X did not respond!\n",
             slave->station_address);
      return -3;
    }

    EtherCAT_remove_command(master, cmd);
  }

  // Change state to SAVEOP
  if (EtherCAT_state_change(master, slave, ECAT_STATE_SAVEOP) != 0)
  {
    return -12;
  }

  // Change state to OP
  if (EtherCAT_state_change(master, slave, ECAT_STATE_OP) != 0)
  {
    return -13;
  }

  return 0;
}

/***************************************************************/

/**
   Setzt einen Slave zurück in den Init-Zustand.

   @param master EtherCAT-Master
   @param slave Zu deaktivierender Slave

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_deactivate_slave(EtherCAT_master_t *master,
                            EtherCAT_slave_t *slave)
{
  if (EtherCAT_state_change(master, slave, ECAT_STATE_INIT) != 0)
  {
    return -1;
  }

  return 0;
}

/***************************************************************/

/**
   Aktiviert alle Slaves.

   @see EtherCAT_activate_slave

   @param master EtherCAT-Master

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_activate_all_slaves(EtherCAT_master_t *master)
{
  unsigned int i;

  for (i = 0; i < master->slave_count; i++)
  {
    if (EtherCAT_activate_slave(master, &master->slaves[i]) < 0)
    {
      return -1;
    }
  }

  return 0;
}

/***************************************************************/

/**
   Deaktiviert alle Slaves.

   @see EtherCAT_deactivate_slave

   @param master EtherCAT-Master

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_deactivate_all_slaves(EtherCAT_master_t *master)
{
  unsigned int i;
  int ret = 0;

  for (i = 0; i < master->slave_count; i++)
  {
    if (EtherCAT_deactivate_slave(master, &master->slaves[i]) < 0)
    {
      ret = -1;
    }
  }

  return ret;
}

/***************************************************************/

/**
   Sendet alle Prozessdaten an die Slaves.

   Erstellt ein "logical read write"-Kommando mit den
   Prozessdaten des Masters und sendet es an den Bus.

   @param master EtherCAT-Master

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_write_process_data(EtherCAT_master_t *master)
{
  if (master->process_data_command)
  {
    EtherCAT_remove_command(master, master->process_data_command);
    EtherCAT_command_clear(master->process_data_command);
    master->process_data_command = NULL;
  }

  if ((master->process_data_command
       = EtherCAT_logical_read_write(master,
                                     0, master->process_data_length,
                                     master->process_data)) == NULL)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not allocate process data command!\n");
    return -1;
  }

  if (EtherCAT_send(master) < 0)
  {
    EtherCAT_remove_command(master, master->process_data_command);
    EtherCAT_command_clear(master->process_data_command);
    master->process_data_command = NULL;
    EC_DBG(KERN_ERR "EtherCAT: Could not send process data command!\n");
    return -1;
  }

  return 0;
}

/***************************************************************/

/**
   Empfängt alle Prozessdaten von den Slaves.

   Empfängt ein zuvor gesendetes "logical read write"-Kommando
   und kopiert die empfangenen daten in den Prozessdatenspeicher
   des Masters.

   @param master EtherCAT-Master

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_read_process_data(EtherCAT_master_t *master)
{
  int ret = -1;

  if (!master->process_data_command)
  {
    EC_DBG(KERN_WARNING "EtherCAT: No process data command available!\n");
    return -1;
  }

  EtherCAT_device_call_isr(master->dev);

  if (EtherCAT_receive(master) < 0)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not receive cyclic command!\n");
  }
  else if (master->process_data_command->state != ECAT_CS_RECEIVED)
  {
    EC_DBG(KERN_WARNING "EtherCAT: Process data command not received!\n");
  }
  else
  {
    // Daten von Kommando in Prozessdaten des Master kopieren
    memcpy(master->process_data, master->process_data_command->data, master->process_data_length);
    ret = 0;
  }

  EtherCAT_remove_command(master, master->process_data_command);
  EtherCAT_command_clear(master->process_data_command);
  master->process_data_command = NULL;

  return ret;
}

/***************************************************************/

/**
   Verwirft ein zuvor gesendetes Prozessdatenkommando.

   Diese Funktion sollte nach Ende des zyklischen Betriebes
   aufgerufen werden, um das noch wartende Prozessdaten-Kommando
   zu entfernen.

   @param master EtherCAT-Master
*/

void EtherCAT_clear_process_data(EtherCAT_master_t *master)
{
  if (!master->process_data_command) return;

  EtherCAT_remove_command(master, master->process_data_command);
  EtherCAT_command_clear(master->process_data_command);
  master->process_data_command = NULL;
}

/***************************************************************/

/**
   Setzt einen Byte-Wert im Speicher.

   @param data Startadresse
   @param offset Byte-Offset
   @param value Wert
*/

void set_byte(unsigned char *data,
              unsigned int offset,
              unsigned char value)
{
  data[offset] = value;
}

/***************************************************************/

/**
   Setzt einen Word-Wert im Speicher.

   @param data Startadresse
   @param offset Byte-Offset
   @param value Wert
*/

void set_word(unsigned char *data,
              unsigned int offset,
              unsigned int value)
{
  data[offset] = value & 0xFF;
  data[offset + 1] = (value & 0xFF00) >> 8;
}

/***************************************************************/
