/******************************************************************************
 *
 *  e c _ m a s t e r . c
 *
 *  Methoden für einen EtherCAT-Master.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "ec_globals.h"
#include "ec_master.h"

/*****************************************************************************/

/**
   Konstruktor des EtherCAT-Masters.

   @param master Zeiger auf den zu initialisierenden EtherCAT-Master
*/

void EtherCAT_master_init(EtherCAT_master_t *master)
{
  master->dev = NULL;
  master->command_index = 0x00;
  master->tx_data_length = 0;
  master->rx_data_length = 0;
  master->domain_count = 0;
  master->debug_level = 0;
  master->bus_time = 0;
}

/*****************************************************************************/

/**
   Destruktor eines EtherCAT-Masters.

   Entfernt alle Kommandos aus der Liste, löscht den Zeiger
   auf das Slave-Array und gibt die Prozessdaten frei.

   @param master Zeiger auf den zu löschenden Master
*/

void EtherCAT_master_clear(EtherCAT_master_t *master)
{
  unsigned int i;

  // Remove domains
  for (i = 0; i < master->domain_count; i++) {
    EtherCAT_domain_clear(master->domains + i);
  }

  master->domain_count = 0;
}

/*****************************************************************************/

/**
   Öffnet ein EtherCAT-Geraet für den Master.

   Registriert das Geraet beim Master, der es daraufhin oeffnet.

   @param master Der EtherCAT-Master
   @param device Das EtherCAT-Geraet
   @return 0, wenn alles o.k.,
   < 0, wenn bereits ein Geraet registriert
   oder das Geraet nicht geoeffnet werden konnte.
*/

int EtherCAT_master_open(EtherCAT_master_t *master,
                         EtherCAT_device_t *device)
{
  if (!master || !device) {
    printk(KERN_ERR "EtherCAT: Illegal parameters for master_open()!\n");
    return -1;
  }

  if (master->dev) {
    printk(KERN_ERR "EtherCAT: Master already has a device.\n");
    return -1;
  }

  if (EtherCAT_device_open(device) < 0) {
    printk(KERN_ERR "EtherCAT: Could not open device %X!\n",
           (unsigned int) master->dev);
    return -1;
  }

  master->dev = device;

  return 0;
}

/*****************************************************************************/

/**
   Schliesst das EtherCAT-Geraet, auf dem der Master arbeitet.

   @param master Der EtherCAT-Master
   @param device Das EtherCAT-Geraet
*/

void EtherCAT_master_close(EtherCAT_master_t *master,
                           EtherCAT_device_t *device)
{
  if (master->dev != device) {
    printk(KERN_WARNING "EtherCAT: Warning -"
           " Trying to close an unknown device!\n");
    return;
  }

  if (EtherCAT_device_close(master->dev) < 0) {
    printk(KERN_WARNING "EtherCAT: Warning -"
           " Could not close device!\n");
  }

  master->dev = NULL;
}

/*****************************************************************************/

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

  if (unlikely(EtherCAT_simple_send(master, cmd) < 0))
    return -1;

  udelay(3);

  EtherCAT_device_call_isr(master->dev);

  tries_left = 20;
  while (unlikely(master->dev->state == ECAT_DS_SENT
                  && tries_left)) {
    udelay(1);
    EtherCAT_device_call_isr(master->dev);
    tries_left--;
  }

  if (unlikely(EtherCAT_simple_receive(master, cmd) < 0))
    return -1;

  return 0;
}

/*****************************************************************************/

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

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "EtherCAT_send_receive_command\n");
  }

  if (unlikely(cmd->state != ECAT_CS_READY)) {
    printk(KERN_WARNING "EtherCAT_send_receive_command:"
           "Command not in ready state!\n");
  }

  length = cmd->data_length + 12;
  framelength = length + 2;

  if (unlikely(framelength > ECAT_FRAME_BUFFER_SIZE)) {
    printk(KERN_ERR "EtherCAT: Frame too long (%i)!\n", framelength);
    return -1;
  }

  if (framelength < 46) framelength = 46;

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "Frame length: %i\n", framelength);
  }

  master->tx_data[0] = length & 0xFF;
  master->tx_data[1] = ((length & 0x700) >> 8) | 0x10;

  cmd->index = master->command_index;
  master->command_index = (master->command_index + 1) % 0x0100;

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "Sending command index %i\n", cmd->index);
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

  if (likely(cmd->type == ECAT_CMD_APWR
             || cmd->type == ECAT_CMD_NPWR
             || cmd->type == ECAT_CMD_BWR
             || cmd->type == ECAT_CMD_LRW)) // Write commands
  {
    for (i = 0; i < cmd->data_length; i++)
      master->tx_data[2 + 10 + i] = cmd->data[i];
  }
  else // Read commands
  {
    for (i = 0; i < cmd->data_length; i++) master->tx_data[2 + 10 + i] = 0x00;
  }

  master->tx_data[2 + 10 + cmd->data_length] = 0x00;
  master->tx_data[2 + 11 + cmd->data_length] = 0x00;

  // Pad with zeros
  for (i = cmd->data_length + 12 + 2; i < 46; i++) master->tx_data[i] = 0x00;

  master->tx_data_length = framelength;

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "device send...\n");
  }

  // Send frame
  if (unlikely(EtherCAT_device_send(master->dev,
                                    master->tx_data,
                                    framelength) != 0)) {
    printk(KERN_ERR "EtherCAT: Could not send!\n");
    return -1;
  }

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "EtherCAT_send done.\n");
  }

  return 0;
}

/*****************************************************************************/

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
  int ret;
  unsigned char command_type, command_index;

  if (unlikely((ret = EtherCAT_device_receive(master->dev,
                                              master->rx_data)) < 0))
    return -1;

  master->rx_data_length = (unsigned int) ret;

  if (unlikely(master->rx_data_length < 2)) {
    printk(KERN_ERR "EtherCAT: Received frame with incomplete"
           " EtherCAT header!\n");
    output_debug_data(master);
    return -1;
  }

  // Länge des gesamten Frames prüfen
  length = ((master->rx_data[1] & 0x07) << 8)
    | (master->rx_data[0] & 0xFF);

  if (unlikely(length > master->rx_data_length)) {
    printk(KERN_ERR "EtherCAT: Received corrupted frame (length does"
           " not match)!\n");
    output_debug_data(master);
    return -1;
  }

  command_type = master->rx_data[2];
  command_index = master->rx_data[2 + 1];
  length = (master->rx_data[2 + 6] & 0xFF)
    | ((master->rx_data[2 + 7] & 0x07) << 8);

  if (unlikely(master->rx_data_length - 2 < length + 12)) {
    printk(KERN_ERR "EtherCAT: Received frame with"
           " incomplete command data!\n");
    output_debug_data(master);
    return -1;
  }

  if (likely(cmd->state == ECAT_CS_SENT
             && cmd->type == command_type
             && cmd->index == command_index
             && cmd->data_length == length))
  {
    cmd->state = ECAT_CS_RECEIVED;

    // Empfangene Daten in Kommandodatenspeicher kopieren
    memcpy(cmd->data, master->rx_data + 2 + 10, length);

    // Working-Counter setzen
    cmd->working_counter
      = ((master->rx_data[length + 2 + 10] & 0xFF)
         | ((master->rx_data[length + 2 + 11] & 0xFF) << 8));
  }
  else
  {
    printk(KERN_WARNING "EtherCAT: WARNING - Send/Receive anomaly!\n");
    output_debug_data(master);
  }

  master->dev->state = ECAT_DS_READY;

  return 0;
}

/*****************************************************************************/

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
  unsigned int i, j, found, size, offset;
  unsigned char data[2];
  EtherCAT_domain_t *dom;

  // Clear domains
  for (i = 0; i < master->domain_count; i++) {
    printk(KERN_DEBUG "EtherCAT: Clearing domain %i!\n",
           master->domains[i].number);
    EtherCAT_domain_clear(master->domains + i);
  }
  master->domain_count = 0;

  if (unlikely(!slave_count)) {
    printk(KERN_ERR "EtherCAT: No slaves in list!\n");
    return -1;
  }

  // Determine number of slaves on bus

  EtherCAT_command_broadcast_read(&cmd, 0x0000, 4);

  if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
    return -1;

  if (unlikely(cmd.working_counter != slave_count)) {
    printk(KERN_ERR "EtherCAT: Wrong number of slaves on bus: %i / %i\n",
           cmd.working_counter, slave_count);
    return -1;
  }

  printk("EtherCAT: Found all %i slaves.\n", slave_count);

  // For every slave in the list
  for (i = 0; i < slave_count; i++)
  {
    cur = &slaves[i];

    if (unlikely(!cur->desc)) {
      printk(KERN_ERR "EtherCAT: Slave %i has no description.\n", i);
      return -1;
    }

    // Set ring position
    cur->ring_position = -i;
    cur->station_address = i + 1;

    // Write station address

    data[0] = cur->station_address & 0x00FF;
    data[1] = (cur->station_address & 0xFF00) >> 8;

    EtherCAT_command_position_write(&cmd, cur->ring_position,
                                    0x0010, 2, data);

    if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Slave %i did not repond"
             " while writing station address!\n", i);
      return -1;
    }

    // Read base data

    EtherCAT_command_read(&cmd, cur->station_address, 0x0000, 4);

    if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Slave %i did not respond"
             " while reading base data!\n", i);
      return -1;
    }

    // Get base data

    cur->type = cmd.data[0];
    cur->revision = cmd.data[1];
    cur->build = cmd.data[2] | (cmd.data[3] << 8);

    // Read identification from "Slave Information Interface" (SII)

    if (unlikely(EtherCAT_read_slave_information(master,
                                                 cur->station_address,
                                                 0x0008,
                                                 &cur->vendor_id) != 0))
    {
      printk(KERN_ERR "EtherCAT: Could not read SII vendor id!\n");
      return -1;
    }

    if (unlikely(EtherCAT_read_slave_information(master,
                                                 cur->station_address,
                                                 0x000A,
                                                 &cur->product_code) != 0))
    {
      printk(KERN_ERR "EtherCAT: Could not read SII product code!\n");
      return -1;
    }

    if (unlikely(EtherCAT_read_slave_information(master,
                                                 cur->station_address,
                                                 0x000C,
                                                 &cur->revision_number) != 0))
    {
      printk(KERN_ERR "EtherCAT: Could not read SII revision number!\n");
      return -1;
    }

    if (unlikely(EtherCAT_read_slave_information(master,
                                                 cur->station_address,
                                                 0x000E,
                                                 &cur->serial_number) != 0))
    {
      printk(KERN_ERR "EtherCAT: Could not read SII serial number!\n");
      return -1;
    }

    // Search for identification in "database"

    found = 0;

    for (j = 0; j < slave_ident_count; j++)
    {
      if (unlikely(slave_idents[j].vendor_id == cur->vendor_id
                   && slave_idents[j].product_code == cur->product_code))
      {
        found = 1;

        if (unlikely(cur->desc != slave_idents[j].desc)) {
          printk(KERN_ERR "EtherCAT: Unexpected slave device"
                 " \"%s %s\" at position %i. Expected: \"%s %s\"\n",
                 slave_idents[j].desc->vendor_name,
                 slave_idents[j].desc->product_name, i,
                 cur->desc->vendor_name, cur->desc->product_name);
          return -1;
        }

        break;
      }
    }

    if (unlikely(!found)) {
      printk(KERN_ERR "EtherCAT: Unknown slave device"
             " (vendor %X, code %X) at position %i.\n",
             cur->vendor_id, cur->product_code, i);
      return -1;
    }

    // Check, if process data domain already exists...
    found = 0;
    for (j = 0; j < master->domain_count; j++) {
      if (cur->domain == master->domains[j].number) {
        found = 1;
      }
    }

    // Create process data domain
    if (!found) {
      if (master->domain_count + 1 >= ECAT_MAX_DOMAINS) {
        printk(KERN_ERR "EtherCAT: Too many domains!\n");
        return -1;
      }

      EtherCAT_domain_init(&master->domains[master->domain_count]);
      master->domains[master->domain_count].number = cur->domain;
      master->domain_count++;
    }
  }

  // Calculate domain sizes
  offset = 0;
  for (i = 0; i < master->domain_count; i++)
  {
    dom = master->domains + i;

    dom->logical_offset = offset;

    // For every slave in the list
    size = 0;
    for (j = 0; j < slave_count; j++) {
      if (slaves[j].domain == dom->number) {
        size += slaves[j].desc->process_data_size;
      }
    }

    if (size > ECAT_FRAME_BUFFER_SIZE - 14) {
      printk(KERN_ERR "EtherCAT: Oversized domain %i: %i / %i Bytes!\n",
             dom->number, size, ECAT_FRAME_BUFFER_SIZE - 14);
      return -1;
    }

    if (!(dom->data = (unsigned char *) kmalloc(sizeof(unsigned char)
                                               * size, GFP_KERNEL))) {
      printk(KERN_ERR "EtherCAT: Could not allocate"
             " %i bytes of domain data.\n", size);
      return -1;
    }

    dom->data_size = size;
    memset(dom->data, 0x00, size);

    printk(KERN_INFO "EtherCAT: Domain %i: %i Bytes of process data.\n",
           dom->number, size);

    // Set logical addresses and data pointers of domain slaves
    size = 0;
    for (j = 0; j < slave_count; j++) {
      if (slaves[j].domain == dom->number) {
        slaves[j].process_data = dom->data + size;
        slaves[j].logical_address = dom->logical_offset + size;
        size += slaves[j].desc->process_data_size;
      }
    }

    offset += size;
  }

  return 0;
}

/*****************************************************************************/

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

  if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
    return -1;

  if (unlikely(cmd.working_counter != 1)) {
    printk(KERN_ERR "EtherCAT: SII-read - Slave %04X did not respond!\n",
           node_address);
    return -1;
  }

  // Der Slave legt die Informationen des Slave-Information-Interface
  // in das Datenregister und löscht daraufhin ein Busy-Bit. Solange
  // den Status auslesen, bis das Bit weg ist.

  tries_left = 100;
  while (likely(tries_left))
  {
    udelay(10);

    EtherCAT_command_read(&cmd, node_address, 0x502, 10);

    if (unlikely(EtherCAT_simple_send_receive(master, &cmd) != 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: SII-read status -"
             " Slave %04X did not respond!\n", node_address);
      return -1;
    }

    if (likely((cmd.data[1] & 0x81) == 0)) {
      memcpy(target, cmd.data + 6, 4);
      break;
    }

    tries_left--;
  }

  if (unlikely(!tries_left)) {
    printk(KERN_WARNING "EtherCAT: SSI-read. Slave %04X timed out!\n",
           node_address);
    return -1;
  }

  return 0;
}

/*****************************************************************************/

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

  EtherCAT_command_write(&cmd, slave->station_address,
                         0x0120, 2, data);

  if (unlikely(EtherCAT_simple_send_receive(master, &cmd) != 0)) {
    printk(KERN_ERR "EtherCAT: Could not set state %02X - Unable to send!\n",
           state_and_ack);
    return -1;
  }

  if (unlikely(cmd.working_counter != 1)) {
    printk(KERN_ERR "EtherCAT: Could not set state %02X - Device \"%s %s\""
           " (%d) did not respond!\n", state_and_ack, slave->desc->vendor_name,
           slave->desc->product_name, slave->ring_position * (-1));
    return -1;
  }

  slave->requested_state = state_and_ack & 0x0F;

  tries_left = 100;
  while (likely(tries_left))
  {
    udelay(10);

    EtherCAT_command_read(&cmd, slave->station_address, 0x0130, 2);

    if (unlikely(EtherCAT_simple_send_receive(master, &cmd) != 0)) {
      printk(KERN_ERR "EtherCAT: Could not check state %02X - Unable to"
             " send!\n", state_and_ack);
      return -1;
    }

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Could not check state %02X - Device did not"
             " respond!\n", state_and_ack);
      return -1;
    }

    if (unlikely(cmd.data[0] & 0x10)) { // State change error
      printk(KERN_ERR "EtherCAT: Could not set state %02X - Device refused"
             " state change (code %02X)!\n", state_and_ack, cmd.data[0]);
      return -1;
    }

    if (likely(cmd.data[0] == (state_and_ack & 0x0F))) {
      // State change successful
      break;
    }

    tries_left--;
  }

  if (unlikely(!tries_left)) {
    printk(KERN_ERR "EtherCAT: Could not check state %02X - Timeout while"
           " checking!\n", state_and_ack);
    return -1;
  }

  slave->current_state = state_and_ack & 0x0F;

  return 0;
}

/*****************************************************************************/

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

  if (unlikely(EtherCAT_state_change(master, slave, ECAT_STATE_INIT) != 0))
    return -1;

  // Resetting FMMU's

  memset(data, 0x00, 256);

  EtherCAT_command_write(&cmd, slave->station_address, 0x0600, 256, data);

  if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
    return -1;

  if (unlikely(cmd.working_counter != 1)) {
    printk(KERN_ERR "EtherCAT: Resetting FMMUs - Slave %04X did not"
           " respond!\n", slave->station_address);
    return -1;
  }

  // Resetting Sync Manager channels

  if (desc->type != ECAT_ST_SIMPLE_NOSYNC)
  {
    memset(data, 0x00, 256);

    EtherCAT_command_write(&cmd, slave->station_address, 0x0800, 256, data);

    if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Resetting SMs - Slave %04X did not"
             " respond!\n", slave->station_address);
      return -1;
    }
  }

  // Init Mailbox communication

  if (desc->type == ECAT_ST_MAILBOX)
  {
    if (desc->sm0)
    {
      EtherCAT_command_write(&cmd, slave->station_address, 0x0800, 8,
                             desc->sm0);

      if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
        return -1;

      if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Setting SM0 - Slave %04X did not"
               " respond!\n", slave->station_address);
        return -1;
      }
    }

    if (desc->sm1)
    {
      EtherCAT_command_write(&cmd, slave->station_address, 0x0808, 8,
                             desc->sm1);

      if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
        return -1;

      if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Setting SM1 -"
               " Slave %04X did not respond!\n",
               slave->station_address);
        return -1;
      }
    }
  }

  // Change state to PREOP

  if (unlikely(EtherCAT_state_change(master, slave, ECAT_STATE_PREOP) != 0))
    return -1;

  // Set FMMU's

  if (desc->fmmu0)
  {
    if (unlikely(!slave->process_data)) {
      printk(KERN_ERR "EtherCAT: Warning - Slave %04X is not assigned to any"
             " process data object!\n", slave->station_address);
      return -1;
    }

    memcpy(fmmu, desc->fmmu0, 16);

    fmmu[0] = slave->logical_address & 0x000000FF;
    fmmu[1] = (slave->logical_address & 0x0000FF00) >> 8;
    fmmu[2] = (slave->logical_address & 0x00FF0000) >> 16;
    fmmu[3] = (slave->logical_address & 0xFF000000) >> 24;

    EtherCAT_command_write(&cmd, slave->station_address, 0x0600, 16, fmmu);

    if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Setting FMMU0 - Slave %04X did not"
             " respond!\n", slave->station_address);
      return -1;
    }
  }

  // Set Sync Managers

  if (desc->type != ECAT_ST_MAILBOX)
  {
    if (desc->sm0)
    {
      EtherCAT_command_write(&cmd, slave->station_address, 0x0800, 8,
                             desc->sm0);

      if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
        return -1;

      if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Setting SM0 - Slave %04X did not"
               " respond!\n", slave->station_address);
        return -1;
      }
    }

    if (desc->sm1)
    {
      EtherCAT_command_write(&cmd, slave->station_address, 0x0808, 8,
                             desc->sm1);

      if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
        return -1;

      if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Setting SM1 - Slave %04X did not"
               " respond!\n", slave->station_address);
        return -1;
      }
    }
  }

  if (desc->sm2)
  {
    EtherCAT_command_write(&cmd, slave->station_address, 0x0810, 8, desc->sm2);

    if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Setting SM2 - Slave %04X did not respond!\n",
             slave->station_address);
      return -1;
    }
  }

  if (desc->sm3)
  {
    EtherCAT_command_write(&cmd, slave->station_address, 0x0818, 8, desc->sm3);

    if (unlikely(EtherCAT_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Setting SM3 - Slave %04X did not respond!\n",
             slave->station_address);
      return -1;
    }
  }

  // Change state to SAVEOP
  if (unlikely(EtherCAT_state_change(master, slave, ECAT_STATE_SAVEOP) != 0))
    return -1;

  // Change state to OP
  if (unlikely(EtherCAT_state_change(master, slave, ECAT_STATE_OP) != 0))
    return -1;

  return 0;
}

/*****************************************************************************/

/**
   Setzt einen Slave zurück in den Init-Zustand.

   @param master EtherCAT-Master
   @param slave Zu deaktivierender Slave

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_deactivate_slave(EtherCAT_master_t *master,
                              EtherCAT_slave_t *slave)
{
  if (unlikely(EtherCAT_state_change(master, slave,
                                     ECAT_STATE_INIT) != 0))
    return -1;

  return 0;
}

/*****************************************************************************/

/**
   Sendet und empfängt Prozessdaten der angegebenen Domäne

   @param master     EtherCAT-Master
          domain     Domäne
          timeout_us Timeout in Mikrosekunden

   @return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_process_data_cycle(EtherCAT_master_t *master, unsigned int domain,
                                unsigned int timeout_us)
{
  unsigned int i;
  EtherCAT_domain_t *dom;
  unsigned long start_ticks, end_ticks, timeout_ticks;

  dom = NULL;
  for (i = 0; i < master->domain_count; i++) {
    if (master->domains[i].number == domain) {
      dom = master->domains + i;
      break;
    }
  }

  if (unlikely(!dom)) {
    printk(KERN_ERR "EtherCAT: No such domain: %i!\n", domain);
    return -1;
  }

  EtherCAT_command_logical_read_write(&dom->command,
                                      dom->logical_offset, dom->data_size,
                                      dom->data);

  rdtscl(start_ticks); // Sendezeit nehmen

  if (unlikely(EtherCAT_simple_send(master, &dom->command) < 0)) {
    printk(KERN_ERR "EtherCAT: Could not send process data command!\n");
    return -1;
  }

  timeout_ticks = timeout_us * cpu_khz / 1000;

  // Warten
  do {
    EtherCAT_device_call_isr(master->dev);
    rdtscl(end_ticks); // Empfangszeit nehmen
  }
  while (unlikely(master->dev->state == ECAT_DS_SENT
                  && end_ticks - start_ticks < timeout_ticks));

  master->bus_time = (end_ticks - start_ticks) * 1000 / cpu_khz;

  if (unlikely(end_ticks - start_ticks >= timeout_ticks)) {
    printk(KERN_ERR "EtherCAT: Timeout while receiving process data!\n");
    return -1;
  }

  if (unlikely(EtherCAT_simple_receive(master, &dom->command) < 0)) {
    printk(KERN_ERR "EtherCAT: Could not receive cyclic command!\n");
    return -1;
  }

  if (unlikely(dom->command.state != ECAT_CS_RECEIVED)) {
    printk(KERN_WARNING "EtherCAT: Process data command not received!\n");
    return -1;
  }

  // Daten vom Kommando in den Prozessdatenspeicher kopieren
  memcpy(dom->data, dom->command.data, dom->data_size);

  return 0;
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus.

   @param master EtherCAT-Master
*/

void output_debug_data(const EtherCAT_master_t *master)
{
  unsigned int i;

  printk(KERN_DEBUG "EtherCAT: tx_data content (%i Bytes):\n",
         master->tx_data_length);

  printk(KERN_DEBUG);
  for (i = 0; i < master->tx_data_length; i++)
  {
    printk("%02X ", master->tx_data[i]);
    if ((i + 1) % 16 == 0) printk("\n" KERN_DEBUG);
  }
  printk("\n");

  printk(KERN_DEBUG "EtherCAT: rx_data content (%i Bytes):\n",
         master->rx_data_length);

  printk(KERN_DEBUG);
  for (i = 0; i < master->rx_data_length; i++)
  {
    printk("%02X ", master->rx_data[i]);
    if ((i + 1) % 16 == 0) printk("\n" KERN_DEBUG);
  }
  printk("\n");
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_master_init);
EXPORT_SYMBOL(EtherCAT_master_clear);
EXPORT_SYMBOL(EtherCAT_master_open);
EXPORT_SYMBOL(EtherCAT_master_close);
EXPORT_SYMBOL(EtherCAT_check_slaves);
EXPORT_SYMBOL(EtherCAT_activate_slave);
EXPORT_SYMBOL(EtherCAT_deactivate_slave);
EXPORT_SYMBOL(EtherCAT_process_data_cycle);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
