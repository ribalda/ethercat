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
   @param dev Zeiger auf das EtherCAT-Gerät, mit dem der
   Master arbeiten soll

   @return 0 bei Erfolg, sonst < 0 (= dev ist NULL)
*/

int EtherCAT_master_init(EtherCAT_master_t *master,
                         EtherCAT_device_t *dev)
{
  if (!dev)
  {
    EC_DBG(KERN_ERR "EtherCAT: Master init without device!\n");
    return -1;
  }

  master->slaves = NULL;
  master->slave_count = 0;
  master->dev = dev;
  master->command_index = 0x00;
  master->tx_data_length = 0;
  master->process_data = NULL;
  master->process_data_length = 0;
  master->debug_level = 0;

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
   Sendet ein einzelnes Kommando in einem Frame und
   wartet auf dessen Empfang.

   @param master EtherCAT-Master
   @param cmd    Kommando zum Senden/Empfangen

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_simple_send_receive(EtherCAT_master_t *master,
                                 EtherCAT_command_t *cmd)
{
  unsigned int tries_left;

  if (EtherCAT_simple_send(master, cmd) < 0) return -1;

  EtherCAT_device_call_isr(master->dev);

  tries_left = 1000;
  while (master->dev->state == ECAT_DS_SENT && tries_left)
  {
    udelay(1);
    EtherCAT_device_call_isr(master->dev);
    tries_left--;
  }

  if (EtherCAT_simple_receive(master, cmd) < 0) return -1;

  return 0;
}

/***************************************************************/

/**
   Sendet ein einzelnes Kommando in einem Frame.

   @param master EtherCAT-Master
   @param cmd    Kommando zum Senden

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_simple_send(EtherCAT_master_t *master,
                         EtherCAT_command_t *cmd)
{
  unsigned int length, framelength, i;

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "EtherCAT_send_receive_command\n");
  }

  if (cmd->state != ECAT_CS_READY)
  {
    EC_DBG(KERN_WARNING "EtherCAT_send_receive_command: Command not in ready state!\n");
  }

  length = cmd->data_length + 12;
  framelength = length + 2;

  if (framelength > ECAT_FRAME_BUFFER_SIZE)
  {
    EC_DBG(KERN_ERR "EtherCAT: Frame too long (%i)!\n", framelength);
    return -1;
  }

  if (framelength < 46) framelength = 46;

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "Frame length: %i\n", framelength);
  }

  master->tx_data[0] = length & 0xFF;
  master->tx_data[1] = ((length & 0x700) >> 8) | 0x10;

  cmd->index = master->command_index;
  master->command_index = (master->command_index + 1) % 0x0100;

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "Sending command index %i\n", cmd->index);
  }

  cmd->state = ECAT_CS_SENT;

  master->tx_data[2 + 0] = cmd->type;
  master->tx_data[2 + 1] = cmd->index;
  master->tx_data[2 + 2] = cmd->address.raw[0];
  master->tx_data[2 + 3] = cmd->address.raw[1];
  master->tx_data[2 + 4] = cmd->address.raw[2];
  master->tx_data[2 + 5] = cmd->address.raw[3];
  master->tx_data[2 + 6] = cmd->data_length & 0xFF;
  master->tx_data[2 + 7] = (cmd->data_length & 0x700) >> 8;
  master->tx_data[2 + 8] = 0x00;
  master->tx_data[2 + 9] = 0x00;

  if (cmd->type == ECAT_CMD_APWR
      || cmd->type == ECAT_CMD_NPWR
      || cmd->type == ECAT_CMD_BWR
      || cmd->type == ECAT_CMD_LRW) // Write
  {
    for (i = 0; i < cmd->data_length; i++) master->tx_data[2 + 10 + i] = cmd->data[i];
  }
  else // Read
  {
    for (i = 0; i < cmd->data_length; i++) master->tx_data[2 + 10 + i] = 0x00;
  }

  master->tx_data[2 + 10 + cmd->data_length] = 0x00;
  master->tx_data[2 + 11 + cmd->data_length] = 0x00;

  // Pad with zeros
  for (i = cmd->data_length + 12 + 2; i < 46; i++) master->tx_data[i] = 0x00;

  master->tx_data_length = framelength;

  if (master->debug_level > 0)
  {
    EC_DBG(KERN_DEBUG "device send...\n");
  }

  // Send frame
  if (EtherCAT_device_send(master->dev, master->tx_data, framelength) != 0)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not send!\n");
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
   Wartet auf den Empfang eines einzeln gesendeten
   Kommandos.

   @param master EtherCAT-Master
   @param cmd    Gesendetes Kommando

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_simple_receive(EtherCAT_master_t *master,
                            EtherCAT_command_t *cmd)
{
  unsigned int length;
  int receive_ret;
  unsigned char command_type, command_index;

  if ((receive_ret = EtherCAT_device_receive(master->dev,
                                             master->rx_data)) < 0)
  {
    return -1;
  }

  master->rx_data_length = (unsigned int) receive_ret;

  if (master->rx_data_length < 2)
  {
    EC_DBG(KERN_ERR "EtherCAT: Received frame with incomplete EtherCAT header!\n");
    output_debug_data(master);
    return -1;
  }

  // Länge des gesamten Frames prüfen
  length = ((master->rx_data[1] & 0x07) << 8) | (master->rx_data[0] & 0xFF);

  if (length > master->rx_data_length)
  {
    EC_DBG(KERN_ERR "EtherCAT: Received corrupted frame (length does not match)!\n");
    output_debug_data(master);
    return -1;
  }

  command_type = master->rx_data[2];
  command_index = master->rx_data[2 + 1];
  length = (master->rx_data[2 + 6] & 0xFF) | ((master->rx_data[2 + 7] & 0x07) << 8);

  if (master->rx_data_length - 2 < length + 12)
  {
    EC_DBG(KERN_ERR "EtherCAT: Received frame with incomplete command data!\n");
    output_debug_data(master);
    return -1;
  }

  if (cmd->state == ECAT_CS_SENT
      && cmd->type == command_type
      && cmd->index == command_index
      && cmd->data_length == length)
  {
    cmd->state = ECAT_CS_RECEIVED;

    // Empfangene Daten in Kommandodatenspeicher kopieren
    memcpy(cmd->data, master->rx_data + 2 + 10, length);

    // Working-Counter setzen
    cmd->working_counter = ((master->rx_data[length + 2 + 10] & 0xFF)
                            | ((master->rx_data[length + 2 + 11] & 0xFF) << 8));
  }
  else
  {
    EC_DBG(KERN_WARNING "EtherCAT: WARNING - Send/Receive anomaly!\n");
    output_debug_data(master);
  }

  master->dev->state = ECAT_DS_READY;

  return 0;
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
  EtherCAT_command_t cmd;
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

  EtherCAT_command_broadcast_read(&cmd, 0x0000, 4);

  if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

  if (cmd.working_counter != slave_count)
  {
    EC_DBG(KERN_ERR "EtherCAT: Wrong number of slaves on bus: %i / %i\n",
           cmd.working_counter, slave_count);
    return -1;
  }
  else
  {
    EC_DBG("EtherCAT: Found all %i slaves.\n", slave_count);
  }

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

    EtherCAT_command_position_write(&cmd, cur->ring_position, 0x0010, 2, data);
    if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

    if (cmd.working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: Slave %i did not repond while writing station address!\n", i);
      return -1;
    }

    // Read base data

    EtherCAT_command_read(&cmd, cur->station_address, 0x0000, 4);
    if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

    if (cmd.working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: Slave %i did not respond while reading base data!\n", i);
      return -1;
    }

    // Get base data
    cur->type = cmd.data[0];
    cur->revision = cmd.data[1];
    cur->build = cmd.data[2] | (cmd.data[3] << 8);

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
                                        0x000C, &cur->revision_number) != 0)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not read SII revision number!\n");
      return -1;
    }

    if (EtherCAT_read_slave_information(master, cur->station_address,
                                        0x000E, &cur->serial_number) != 0)
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
  EtherCAT_command_t cmd;
  unsigned char data[10];
  unsigned int tries_left;

  // Initiate read operation

  data[0] = 0x00;
  data[1] = 0x01;
  data[2] = offset & 0xFF;
  data[3] = (offset & 0xFF00) >> 8;
  data[4] = 0x00;
  data[5] = 0x00;

  EtherCAT_command_write(&cmd, node_address, 0x502, 6, data);
  if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -3;

  if (cmd.working_counter != 1)
  {
    EC_DBG(KERN_ERR "EtherCAT: SII-read - Slave %04X did not respond!\n",
           node_address);
    return -4;
  }

  // Der Slave legt die Informationen des Slave-Information-Interface
  // in das Datenregister und löscht daraufhin ein Busy-Bit. Solange
  // den Status auslesen, bis das Bit weg ist.

  tries_left = 100;
  while (tries_left)
  {
    udelay(10);

    EtherCAT_command_read(&cmd, node_address, 0x502, 10);
    if (EtherCAT_simple_send_receive(master, &cmd) != 0) return -3;

    if (cmd.working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: SII-read status - Slave %04X did not respond!\n",
             node_address);
      return -4;
    }

    if ((cmd.data[1] & 0x81) == 0)
    {
      memcpy(target, cmd.data + 6, 4);
      break;
    }

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
  EtherCAT_command_t cmd;
  unsigned char data[2];
  unsigned int tries_left;

  data[0] = state_and_ack;
  data[1] = 0x00;

  EtherCAT_command_write(&cmd, slave->station_address, 0x0120, 2, data);

  if (EtherCAT_simple_send_receive(master, &cmd) != 0)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not set state %02X - Unable to send!\n", state_and_ack);
    return -2;
  }

  if (cmd.working_counter != 1)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not set state %02X - Device did not respond!\n", state_and_ack);
    return -3;
  }

  slave->requested_state = state_and_ack & 0x0F;

  tries_left = 100;
  while (tries_left)
  {
    udelay(10);

    EtherCAT_command_read(&cmd, slave->station_address, 0x0130, 2);

    if (EtherCAT_simple_send_receive(master, &cmd) != 0)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not check state %02X - Unable to send!\n", state_and_ack);
      return -2;
    }

    if (cmd.working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not check state %02X - Device did not respond!\n", state_and_ack);
      return -3;
    }

    if (cmd.data[0] & 0x10) // State change error
    {
      EC_DBG(KERN_ERR "EtherCAT: Could not set state %02X - Device refused state change (code %02X)!\n", state_and_ack, cmd.data[0]);
      return -4;
    }

    if (cmd.data[0] == (state_and_ack & 0x0F)) // State change successful
    {
      break;
    }

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
  EtherCAT_command_t cmd;
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

  EtherCAT_command_write(&cmd, slave->station_address, 0x0600, 256, data);

  if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

  if (cmd.working_counter != 1)
  {
    EC_DBG(KERN_ERR "EtherCAT: Resetting FMMUs - Slave %04X did not respond!\n",
           slave->station_address);
    return -2;
  }

  // Resetting Sync Manager channels

  if (desc->type != ECAT_ST_SIMPLE_NOSYNC)
  {
    memset(data, 0x00, 256);

    EtherCAT_command_write(&cmd, slave->station_address, 0x0800, 256, data);
    if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

    if (cmd.working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: Resetting SMs - Slave %04X did not respond!\n",
             slave->station_address);
      return -2;
    }
  }

  // Init Mailbox communication

  if (desc->type == ECAT_ST_MAILBOX)
  {
    if (desc->sm0)
    {
      EtherCAT_command_write(&cmd, slave->station_address, 0x0800, 8, desc->sm0);
      if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

      if (cmd.working_counter != 1)
      {
        EC_DBG(KERN_ERR "EtherCAT: Setting SM0 - Slave %04X did not respond!\n",
               slave->station_address);
        return -3;
      }
    }

    if (desc->sm1)
    {
      EtherCAT_command_write(&cmd, slave->station_address, 0x0808, 8, desc->sm1);
      if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

      if (cmd.working_counter != 1)
      {
        EC_DBG(KERN_ERR "EtherCAT: Setting SM1 - Slave %04X did not respond!\n",
               slave->station_address);
        return -2;
      }
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

    EtherCAT_command_write(&cmd, slave->station_address, 0x0600, 16, fmmu);
    if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

    if (cmd.working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: Setting FMMU0 - Slave %04X did not respond!\n",
             slave->station_address);
      return -2;
    }
  }

  // Set Sync Managers

  if (desc->type != ECAT_ST_MAILBOX)
  {
    if (desc->sm0)
    {
      EtherCAT_command_write(&cmd, slave->station_address, 0x0800, 8, desc->sm0);
      if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

      if (cmd.working_counter != 1)
      {
        EC_DBG(KERN_ERR "EtherCAT: Setting SM0 - Slave %04X did not respond!\n",
               slave->station_address);
        return -3;
      }
    }

    if (desc->sm1)
    {
      EtherCAT_command_write(&cmd, slave->station_address, 0x0808, 8, desc->sm1);
      if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

      if (cmd.working_counter != 1)
      {
        EC_DBG(KERN_ERR "EtherCAT: Setting SM1 - Slave %04X did not respond!\n",
               slave->station_address);
        return -3;
      }
    }
  }

  if (desc->sm2)
  {
    EtherCAT_command_write(&cmd, slave->station_address, 0x0810, 8, desc->sm2);
    if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

    if (cmd.working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: Setting SM2 - Slave %04X did not respond!\n",
             slave->station_address);
      return -3;
    }
  }

  if (desc->sm3)
  {
    EtherCAT_command_write(&cmd, slave->station_address, 0x0818, 8, desc->sm3);
    if (EtherCAT_simple_send_receive(master, &cmd) < 0) return -1;

    if (cmd.working_counter != 1)
    {
      EC_DBG(KERN_ERR "EtherCAT: Setting SM3 - Slave %04X did not respond!\n",
             slave->station_address);
      return -3;
    }
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
  EtherCAT_command_logical_read_write(&master->process_data_command,
                                      0, master->process_data_length,
                                      master->process_data);

  if (EtherCAT_simple_send(master, &master->process_data_command) < 0)
  {
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
  unsigned int tries_left;

  EtherCAT_device_call_isr(master->dev);

  tries_left = 1000;
  while (master->dev->state == ECAT_DS_SENT && tries_left)
  {
    udelay(1);
    EtherCAT_device_call_isr(master->dev);
    tries_left--;
  }

  if (!tries_left)
  {
    EC_DBG(KERN_ERR "EtherCAT: Timeout while receiving process data!\n");
    return -1;
  }

  if (EtherCAT_simple_receive(master, &master->process_data_command) < 0)
  {
    EC_DBG(KERN_ERR "EtherCAT: Could not receive cyclic command!\n");
    return -1;
  }

  if (master->process_data_command.state != ECAT_CS_RECEIVED)
  {
    EC_DBG(KERN_WARNING "EtherCAT: Process data command not received!\n");
    return -1;
  }

  // Daten von Kommando in Prozessdaten des Master kopieren
  memcpy(master->process_data, master->process_data_command.data,
         master->process_data_length);

  return 0;
}

/***************************************************************/

/**
   Verwirft das zuletzt gesendete Prozessdatenkommando.

   @param master EtherCAT-Master
*/

void EtherCAT_clear_process_data(EtherCAT_master_t *master)
{
  EtherCAT_device_call_isr(master->dev);
  master->dev->state = ECAT_DS_READY;
}

/***************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus.

   @param data Startadresse
   @param length Länge der Daten
*/

void output_debug_data(const EtherCAT_master_t *master)
{
  unsigned int i;

  EC_DBG(KERN_DEBUG "EtherCAT: tx_data content (%i Bytes):\n",
         master->tx_data_length);

  EC_DBG(KERN_DEBUG);
  for (i = 0; i < master->tx_data_length; i++)
  {
    EC_DBG("%02X ", master->tx_data[i]);
    if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
  }
  EC_DBG("\n");

  EC_DBG(KERN_DEBUG "EtherCAT: rx_data content (%i Bytes):\n",
         master->rx_data_length);

  EC_DBG(KERN_DEBUG);
  for (i = 0; i < master->rx_data_length; i++)
  {
    EC_DBG("%02X ", master->rx_data[i]);
    if ((i + 1) % 16 == 0) EC_DBG("\n" KERN_DEBUG);
  }
  EC_DBG("\n");
}

/***************************************************************/
