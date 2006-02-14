/******************************************************************************
 *
 *  m a s t e r . c
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

#include "../include/EtherCAT_rt.h"
#include "globals.h"
#include "master.h"
#include "slave.h"
#include "types.h"
#include "device.h"
#include "command.h"

/*****************************************************************************/

// Prototypen

int ec_simple_send(ec_master_t *, ec_command_t *);
int ec_simple_receive(ec_master_t *, ec_command_t *);
void ec_output_debug_data(const ec_master_t *);
int ec_sii_read(ec_master_t *, unsigned short, unsigned short, unsigned int *);
void ec_output_lost_frames(ec_master_t *);

/*****************************************************************************/

/**
   Konstruktor des EtherCAT-Masters.
*/

void ec_master_init(ec_master_t *master
                    /**< Zeiger auf den zu initialisierenden EtherCAT-Master */
                    )
{
  master->bus_slaves = NULL;
  master->bus_slaves_count = 0;
  master->device_registered = 0;
  master->command_index = 0x00;
  master->tx_data_length = 0;
  master->rx_data_length = 0;
  master->domain_count = 0;
  master->debug_level = 0;
  master->bus_time = 0;
  master->frames_lost = 0;
  master->t_lost_output = 0;
}

/*****************************************************************************/

/**
   Destruktor eines EtherCAT-Masters.

   Entfernt alle Kommandos aus der Liste, löscht den Zeiger
   auf das Slave-Array und gibt die Prozessdaten frei.
*/

void ec_master_clear(ec_master_t *master
                     /**< Zeiger auf den zu löschenden Master */
                     )
{
  if (master->bus_slaves) {
    kfree(master->bus_slaves);
    master->bus_slaves = NULL;
  }

  ec_device_clear(&master->device);

  master->domain_count = 0;
}

/*****************************************************************************/

/**
   Setzt den Master in den Ausgangszustand.

   Bei einem "release" sollte immer diese Funktion aufgerufen werden,
   da sonst Slave-Liste, Domains, etc. weiter existieren.
*/

void ec_master_reset(ec_master_t *master
                     /**< Zeiger auf den zurückzusetzenden Master */
                     )
{
  if (master->bus_slaves) {
    kfree(master->bus_slaves);
    master->bus_slaves = NULL;
  }

  master->bus_slaves_count = 0;
  master->command_index = 0;
  master->tx_data_length = 0;
  master->rx_data_length = 0;
  master->domain_count = 0;
  master->debug_level = 0;
  master->bus_time = 0;
  master->frames_lost = 0;
  master->t_lost_output = 0;
}

/*****************************************************************************/

/**
   Öffnet das EtherCAT-Geraet des Masters.

   \return 0, wenn alles o.k., < 0, wenn kein Gerät registriert wurde oder
   es nicht geoeffnet werden konnte.
*/

int ec_master_open(ec_master_t *master /**< Der EtherCAT-Master */)
{
  if (!master->device_registered) {
    printk(KERN_ERR "EtherCAT: No device registered!\n");
    return -1;
  }

  if (ec_device_open(&master->device) < 0) {
    printk(KERN_ERR "EtherCAT: Could not open device!\n");
    return -1;
  }

  return 0;
}

/*****************************************************************************/

/**
   Schliesst das EtherCAT-Geraet, auf dem der Master arbeitet.
*/

void ec_master_close(ec_master_t *master /**< EtherCAT-Master */)
{
  if (!master->device_registered) {
    printk(KERN_WARNING "EtherCAT: Warning -"
           " Trying to close an unregistered device!\n");
    return;
  }

  if (ec_device_close(&master->device) < 0) {
    printk(KERN_WARNING "EtherCAT: Warning - Could not close device!\n");
  }
}

/*****************************************************************************/

/**
   Sendet ein einzelnes Kommando in einem Frame und
   wartet auf dessen Empfang.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_simple_send_receive(ec_master_t *master,
                           /**< EtherCAT-Master */
                           ec_command_t *cmd
                           /**< Kommando zum Senden/Empfangen */
                           )
{
  unsigned int tries_left;

  if (unlikely(ec_simple_send(master, cmd) < 0))
    return -1;

  tries_left = 20;

  do
  {
    udelay(1);
    ec_device_call_isr(&master->device);
    tries_left--;
  }
  while (unlikely(master->device.state == EC_DEVICE_STATE_SENT && tries_left));

  if (unlikely(ec_simple_receive(master, cmd) < 0))
    return -1;

  return 0;
}

/*****************************************************************************/

/**
   Sendet ein einzelnes Kommando in einem Frame.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_simple_send(ec_master_t *master, /**< EtherCAT-Master */
                   ec_command_t *cmd /**< Kommando zum Senden */
                   )
{
  unsigned int length, framelength, i;

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "EtherCAT: ec_simple_send\n");
  }

  if (unlikely(cmd->state != EC_COMMAND_STATE_READY)) {
    printk(KERN_WARNING "EtherCAT: cmd not in ready state!\n");
  }

  length = cmd->data_length + 12;
  framelength = length + 2;

  if (unlikely(framelength > EC_FRAME_SIZE)) {
    printk(KERN_ERR "EtherCAT: Frame too long (%i)!\n", framelength);
    return -1;
  }

  if (framelength < 46) framelength = 46;

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "EtherCAT: Frame length: %i\n", framelength);
  }

  master->tx_data[0] = length & 0xFF;
  master->tx_data[1] = ((length & 0x700) >> 8) | 0x10;

  cmd->index = master->command_index;
  master->command_index = (master->command_index + 1) % 0x0100;

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "EtherCAT: Sending command index 0x%X\n", cmd->index);
  }

  cmd->state = EC_COMMAND_STATE_SENT;

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

  if (likely(cmd->type == EC_COMMAND_APWR
             || cmd->type == EC_COMMAND_NPWR
             || cmd->type == EC_COMMAND_BWR
             || cmd->type == EC_COMMAND_LRW)) // Write commands
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
    printk(KERN_DEBUG "EtherCAT: Device send...\n");
  }

  // Send frame
  if (unlikely(ec_device_send(&master->device, master->tx_data,
                              framelength) != 0)) {
    printk(KERN_ERR "EtherCAT: Could not send!\n");
    return -1;
  }

  if (unlikely(master->debug_level > 0)) {
    printk(KERN_DEBUG "EtherCAT: ec_simple_send done.\n");
  }

  return 0;
}

/*****************************************************************************/

/**
   Wartet auf den Empfang eines einzeln gesendeten
   Kommandos.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_simple_receive(ec_master_t *master, /**< EtherCAT-Master */
                      ec_command_t *cmd /**< Gesendetes Kommando */
                      )
{
  unsigned int length;
  int ret;
  unsigned char command_type, command_index;

  if (unlikely((ret = ec_device_receive(&master->device,
                                        master->rx_data)) < 0))
    return -1;

  master->rx_data_length = (unsigned int) ret;

  if (unlikely(master->rx_data_length < 2)) {
    printk(KERN_ERR "EtherCAT: Received frame with incomplete EtherCAT"
           " header!\n");
    ec_output_debug_data(master);
    return -1;
  }

  // Länge des gesamten Frames prüfen
  length = ((master->rx_data[1] & 0x07) << 8)
    | (master->rx_data[0] & 0xFF);

  if (unlikely(length > master->rx_data_length)) {
    printk(KERN_ERR "EtherCAT: Received corrupted frame (length does"
           " not match)!\n");
    ec_output_debug_data(master);
    return -1;
  }

  command_type = master->rx_data[2];
  command_index = master->rx_data[2 + 1];
  length = (master->rx_data[2 + 6] & 0xFF)
    | ((master->rx_data[2 + 7] & 0x07) << 8);

  if (unlikely(master->rx_data_length - 2 < length + 12)) {
    printk(KERN_ERR "EtherCAT: Received frame with"
           " incomplete command data!\n");
    ec_output_debug_data(master);
    return -1;
  }

  if (likely(cmd->state == EC_COMMAND_STATE_SENT
             && cmd->type == command_type
             && cmd->index == command_index
             && cmd->data_length == length))
  {
    cmd->state = EC_COMMAND_STATE_RECEIVED;

    // Empfangene Daten in Kommandodatenspeicher kopieren
    memcpy(cmd->data, master->rx_data + 2 + 10, length);

    // Working-Counter setzen
    cmd->working_counter
      = ((master->rx_data[length + 2 + 10] & 0xFF)
         | ((master->rx_data[length + 2 + 11] & 0xFF) << 8));

    if (unlikely(master->debug_level > 1)) {
      ec_output_debug_data(master);
    }
  }
  else
  {
    printk(KERN_WARNING "EtherCAT: WARNING - Send/Receive anomaly!\n");
    ec_output_debug_data(master);
  }

  master->device.state = EC_DEVICE_STATE_READY;

  return 0;
}

/*****************************************************************************/

/**
   Durchsucht den Bus nach Slaves.

   @return 0 bei Erfolg, sonst < 0
*/

int ec_scan_for_slaves(ec_master_t *master /**< EtherCAT-Master */)
{
  ec_command_t cmd;
  ec_slave_t *slave;
  unsigned int i, j;
  unsigned char data[2];

  // Determine number of slaves on bus

  ec_command_broadcast_read(&cmd, 0x0000, 4);

  if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
    return -1;

  master->bus_slaves_count = cmd.working_counter;
  printk("EtherCAT: Found %i slaves on bus.\n", master->bus_slaves_count);

  if (!master->bus_slaves_count) return 0;

  if (!(master->bus_slaves = (ec_slave_t *) kmalloc(master->bus_slaves_count
                                                    * sizeof(ec_slave_t),
                                                    GFP_KERNEL))) {
    printk(KERN_ERR "EtherCAT: Could not allocate memory for bus slaves!\n");
    return -1;
  }

  // For every slave in the list
  for (i = 0; i < master->bus_slaves_count; i++)
  {
    slave = master->bus_slaves + i;

    ec_slave_init(slave);

    // Set ring position

    slave->ring_position = -i;
    slave->station_address = i + 1;

    // Write station address

    data[0] = slave->station_address & 0x00FF;
    data[1] = (slave->station_address & 0xFF00) >> 8;

    ec_command_position_write(&cmd, slave->ring_position, 0x0010, 2, data);

    if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Slave %i did not repond while writing"
             " station address!\n", i);
      return -1;
    }

    // Read base data

    ec_command_read(&cmd, slave->station_address, 0x0000, 4);

    if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Slave %i did not respond while reading base"
             " data!\n", i);
      return -1;
    }

    // Get base data

    slave->base_type = cmd.data[0];
    slave->base_revision = cmd.data[1];
    slave->base_build = cmd.data[2] | (cmd.data[3] << 8);

    // Read identification from "Slave Information Interface" (SII)

    if (unlikely(ec_sii_read(master, slave->station_address, 0x0008,
                             &slave->sii_vendor_id) != 0)) {
      printk(KERN_ERR "EtherCAT: Could not read SII vendor id!\n");
      return -1;
    }

    if (unlikely(ec_sii_read(master, slave->station_address, 0x000A,
                             &slave->sii_product_code) != 0)) {
      printk(KERN_ERR "EtherCAT: Could not read SII product code!\n");
      return -1;
    }

    if (unlikely(ec_sii_read(master, slave->station_address, 0x000C,
                             &slave->sii_revision_number) != 0)) {
      printk(KERN_ERR "EtherCAT: Could not read SII revision number!\n");
      return -1;
    }

    if (unlikely(ec_sii_read(master, slave->station_address, 0x000E,
                             &slave->sii_serial_number) != 0)) {
      printk(KERN_ERR "EtherCAT: Could not read SII serial number!\n");
      return -1;
    }

    // Search for identification in "database"

    for (j = 0; j < slave_ident_count; j++)
    {
      if (unlikely(slave_idents[j].vendor_id == slave->sii_vendor_id
                   && slave_idents[j].product_code == slave->sii_product_code))
      {
        slave->type = slave_idents[j].type;
        break;
      }
    }

    if (unlikely(!slave->type)) {
      printk(KERN_WARNING "EtherCAT: Unknown slave device (vendor 0x%08X, code"
             " 0x%08X) at position %i.\n", slave->sii_vendor_id,
             slave->sii_product_code, i);
      return 0;
    }
  }

  return 0;
}

/*****************************************************************************/

/**
   Liest Daten aus dem Slave-Information-Interface
   eines EtherCAT-Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_sii_read(ec_master_t *master,
                /**< EtherCAT-Master */
                unsigned short int node_address,
                /**< Knotenadresse des Slaves */
                unsigned short int offset,
                /**< Adresse des zu lesenden SII-Registers */
                unsigned int *target
                /**< Zeiger auf einen 4 Byte großen Speicher zum Ablegen der
                   Daten */
                )
{
  ec_command_t cmd;
  unsigned char data[10];
  unsigned int tries_left;

  // Initiate read operation

  data[0] = 0x00;
  data[1] = 0x01;
  data[2] = offset & 0xFF;
  data[3] = (offset & 0xFF00) >> 8;
  data[4] = 0x00;
  data[5] = 0x00;

  ec_command_write(&cmd, node_address, 0x502, 6, data);

  if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
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

    ec_command_read(&cmd, node_address, 0x502, 10);

    if (unlikely(ec_simple_send_receive(master, &cmd) != 0))
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
   Ändert den Zustand eines Slaves.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_state_change(ec_master_t *master,
                    /**<EtherCAT-Master */
                    ec_slave_t *slave,
                    /**< Slave, dessen Zustand geändert werden soll */
                    unsigned char state_and_ack
                    /**< Neuer Zustand, evtl. mit gesetztem Acknowledge-Flag */
                    )
{
  ec_command_t cmd;
  unsigned char data[2];
  unsigned int tries_left;

  data[0] = state_and_ack;
  data[1] = 0x00;

  ec_command_write(&cmd, slave->station_address, 0x0120, 2, data);

  if (unlikely(ec_simple_send_receive(master, &cmd) != 0)) {
    printk(KERN_ERR "EtherCAT: Could not set state %02X - Unable to send!\n",
           state_and_ack);
    return -1;
  }

  if (unlikely(cmd.working_counter != 1)) {
    printk(KERN_ERR "EtherCAT: Could not set state %02X - Slave %i did not"
           " respond!\n", state_and_ack, slave->ring_position * (-1));
    return -1;
  }

  tries_left = 100;
  while (likely(tries_left))
  {
    udelay(10);

    ec_command_read(&cmd, slave->station_address, 0x0130, 2);

    if (unlikely(ec_simple_send_receive(master, &cmd) != 0)) {
      printk(KERN_ERR "EtherCAT: Could not check state %02X - Unable to"
             " send!\n", state_and_ack);
      return -1;
    }

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Could not check state %02X - Device %i did"
             " not respond!\n", state_and_ack, slave->ring_position * (-1));
      return -1;
    }

    if (unlikely(cmd.data[0] & 0x10)) { // State change error
      printk(KERN_ERR "EtherCAT: Could not set state %02X - Device %i refused"
             " state change (code %02X)!\n", state_and_ack,
             slave->ring_position * (-1), cmd.data[0]);
      return -1;
    }

    if (likely(cmd.data[0] == (state_and_ack & 0x0F))) {
      // State change successful
      break;
    }

    tries_left--;
  }

  if (unlikely(!tries_left)) {
    printk(KERN_ERR "EtherCAT: Could not check state %02X of slave %i -"
           " Timeout while checking!\n", state_and_ack,
           slave->ring_position * (-1));
    return -1;
  }

  return 0;
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus.
*/

void ec_output_debug_data(const ec_master_t *master /**< EtherCAT-Master */)
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

/**
   Gibt von Zeit zu Zeit die Anzahl verlorener Frames aus.
*/

void ec_output_lost_frames(ec_master_t *master /**< EtherCAT-Master */)
{
  unsigned long int t;

  if (master->frames_lost) {
    rdtscl(t);
    if ((t - master->t_lost_output) / cpu_khz > 1000) {
      printk(KERN_ERR "EtherCAT: %u frame(s) LOST!\n", master->frames_lost);
      master->frames_lost = 0;
      master->t_lost_output = t;
    }
  }
}

/*****************************************************************************/

/**
   Wandelt eine ASCII-kodierte Bus-Adresse in einen Slave-Zeiger.

   Gültige Adress-Strings sind Folgende:

   - \a "X" = der X. Slave im Bus,
   - \a "X:Y" = der Y. Slave hinter dem X. Buskoppler,
   - \a "#X" = der Slave mit der SSID X,
   - \a "#X:Y" = der Y. Slave hinter dem Buskoppler mit der SSID X.

   \return Zeiger auf Slave bei Erfolg, sonst NULL
*/

ec_slave_t *ec_address(const ec_master_t *master,
                       /**< EtherCAT-Master */
                       const char *address
                       /**< Address-String */
                       )
{
  unsigned long first, second;
  char *remainder, *remainder2;
  unsigned int i;
  int coupler_idx, slave_idx;
  ec_slave_t *slave;

  if (!address || address[0] == 0) return NULL;

  if (address[0] == '#') {
    printk(KERN_ERR "EtherCAT: Bus ID - #<SSID> not implemented yet!\n");
    return NULL;
  }

  first = simple_strtoul(address, &remainder, 0);
  if (remainder == address) {
    printk(KERN_ERR "EtherCAT: Bus ID - First number empty!\n");
    return NULL;
  }

  if (!remainder[0]) { // absolute position
    if (first < master->bus_slaves_count) {
      return master->bus_slaves + first;
    }

    printk(KERN_ERR "EtherCAT: Bus ID - Absolute position illegal!\n");
  }

  else if (remainder[0] == ':') { // field position

    remainder++;
    second = simple_strtoul(remainder, &remainder2, 0);

    if (remainder2 == remainder) {
      printk(KERN_ERR "EtherCAT: Bus ID - Sencond number empty!\n");
      return NULL;
    }

    if (remainder2[0]) {
      printk(KERN_ERR "EtherCAT: Bus ID - Illegal trailer (2)!\n");
      return NULL;
    }

    coupler_idx = -1;
    slave_idx = 0;
    for (i = 0; i < master->bus_slaves_count; i++, slave_idx++) {
      slave = master->bus_slaves + i;
      if (!slave->type) continue;

      if (strcmp(slave->type->vendor_name, "Beckhoff") == 0 &&
          strcmp(slave->type->product_name, "EK1100") == 0) {
        coupler_idx++;
        slave_idx = 0;
      }

      if (coupler_idx == first && slave_idx == second) return slave;
    }
  }

  else {
    printk(KERN_ERR "EtherCAT: Bus ID - Illegal trailer!\n");
  }

  return NULL;
}

/******************************************************************************
 *
 * Echtzeitschnittstelle
 *
 *****************************************************************************/

/**
   Registriert einen Slave beim Master.

   \return Zeiger auf den Slave bei Erfolg, sonst NULL
*/

ec_slave_t *EtherCAT_rt_register_slave(ec_master_t *master,
                                       /**< EtherCAT-Master */
                                       const char *address,
                                       /**< ASCII-Addresse des Slaves, siehe
                                          auch ec_address() */
                                       const char *vendor_name,
                                       /**< Herstellername */
                                       const char *product_name,
                                       /**< Produktname */
                                       int domain
                                       /**< Domäne */
                                       )
{
  ec_slave_t *slave;
  const ec_slave_type_t *type;
  ec_domain_t *dom;
  unsigned int j;

  if (domain < 0) {
    printk(KERN_ERR "EtherCAT: Invalid domain: %i\n", domain);
    return NULL;
  }

  if ((slave = ec_address(master, address)) == NULL) {
    printk(KERN_ERR "EtherCAT: Illegal address: \"%s\"\n", address);
    return NULL;
  }

  if (slave->registered) {
    printk(KERN_ERR "EtherCAT: Slave \"%s\" (position %i) has already been"
           " registered!\n", address, slave->ring_position * (-1));
    return NULL;
  }

  if (!slave->type) {
    printk(KERN_ERR "EtherCAT: Slave \"%s\" (position %i) has unknown type!\n",
           address, slave->ring_position * (-1));
    return NULL;
  }

  type = slave->type;

  if (strcmp(vendor_name, type->vendor_name) ||
      strcmp(product_name, type->product_name)) {
    printk(KERN_ERR "Invalid Slave Type! Requested: \"%s %s\", found: \"%s"
           " %s\".\n", vendor_name, product_name, type->vendor_name,
           type->product_name);
    return NULL;
  }

  // Check, if process data domain already exists...
  dom = NULL;
  for (j = 0; j < master->domain_count; j++) {
    if (domain == master->domains[j].number) {
      dom = master->domains + j;
      break;
    }
  }

  // Create process data domain
  if (!dom) {
    if (master->domain_count > EC_MAX_DOMAINS - 1) {
      printk(KERN_ERR "EtherCAT: Too many domains!\n");
      return NULL;
    }

    dom = master->domains + master->domain_count;
    ec_domain_init(dom);
    dom->number = domain;
    dom->logical_offset = master->domain_count * EC_FRAME_SIZE;
    master->domain_count++;
  }

  if (dom->data_size + type->process_data_size > EC_FRAME_SIZE - 14) {
    printk(KERN_ERR "EtherCAT: Oversized domain %i: %i / %i Bytes!\n",
           dom->number, dom->data_size + type->process_data_size,
           EC_FRAME_SIZE - 14);
    return NULL;
  }

  slave->process_data = dom->data + dom->data_size;
  slave->logical_address = dom->data_size;
  slave->registered = 1;

  dom->data_size += type->process_data_size;

  return slave;
}

/*****************************************************************************/

/**
   Registriert eine ganze Liste von Slaves beim Master.

   \return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_rt_register_slave_list(ec_master_t *master,
                                    /**< EtherCAT-Master */
                                    const ec_slave_init_t *slaves,
                                    /**< Array von Slave-Initialisierungs-
                                       strukturen */
                                    unsigned int count
                                    /**< Anzahl der Strukturen in \a slaves */
                                    )
{
  unsigned int i;

  for (i = 0; i < count; i++)
  {
    if ((*(slaves[i].slave_ptr) =
         EtherCAT_rt_register_slave(master, slaves[i].address,
                                    slaves[i].vendor_name,
                                    slaves[i].product_name,
                                    slaves[i].domain)) == NULL)
      return -1;
  }

  return 0;
}

/*****************************************************************************/

/**
   Konfiguriert alle Slaves und setzt den Operational-Zustand.

   Führt die komplette Konfiguration und Aktivierunge aller registrierten
   Slaves durch. Setzt Sync-Manager und FMMU's, führt die entsprechenden
   Zustandsübergänge durch, bis der Slave betriebsbereit ist.

   \return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_rt_activate_slaves(ec_master_t *master /**< EtherCAT-Master */)
{
  unsigned int i;
  ec_slave_t *slave;
  ec_command_t cmd;
  const ec_slave_type_t *type;
  unsigned char fmmu[16];
  unsigned char data[256];

  for (i = 0; i < master->bus_slaves_count; i++)
  {
    slave = master->bus_slaves + i;

    if (unlikely(ec_state_change(master, slave, EC_SLAVE_STATE_INIT) != 0))
      return -1;

    // Check if slave was registered...
    if (!slave->registered) {
      printk(KERN_INFO "EtherCAT: Slave %i was not registered.\n", i);
      continue;
    }

    type = slave->type;

    // Resetting FMMU's

    memset(data, 0x00, 256);

    ec_command_write(&cmd, slave->station_address, 0x0600, 256, data);

    if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
      return -1;

    if (unlikely(cmd.working_counter != 1)) {
      printk(KERN_ERR "EtherCAT: Resetting FMMUs - Slave %04X did not"
             " respond!\n", slave->station_address);
      return -1;
    }

    // Resetting Sync Manager channels

    if (type->features != EC_NOSYNC_SLAVE)
    {
      memset(data, 0x00, 256);

      ec_command_write(&cmd, slave->station_address, 0x0800, 256, data);

      if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
        return -1;

      if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Resetting SMs - Slave %04X did not"
               " respond!\n", slave->station_address);
        return -1;
      }
    }

    // Init Mailbox communication

    if (type->features == EC_MAILBOX_SLAVE)
    {
      if (type->sm0)
      {
        ec_command_write(&cmd, slave->station_address, 0x0800, 8, type->sm0);

        if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
          return -1;

        if (unlikely(cmd.working_counter != 1)) {
          printk(KERN_ERR "EtherCAT: Setting SM0 - Slave %04X did not"
                 " respond!\n", slave->station_address);
          return -1;
        }
      }

      if (type->sm1)
      {
        ec_command_write(&cmd, slave->station_address, 0x0808, 8, type->sm1);

        if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
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

    if (unlikely(ec_state_change(master, slave, EC_SLAVE_STATE_PREOP) != 0))
      return -1;

    // Set FMMU's

    if (type->fmmu0)
    {
      if (unlikely(!slave->process_data)) {
        printk(KERN_ERR "EtherCAT: Warning - Slave %04X is not assigned to any"
               " process data object!\n", slave->station_address);
        return -1;
      }

      memcpy(fmmu, type->fmmu0, 16);

      fmmu[0] = slave->logical_address & 0x000000FF;
      fmmu[1] = (slave->logical_address & 0x0000FF00) >> 8;
      fmmu[2] = (slave->logical_address & 0x00FF0000) >> 16;
      fmmu[3] = (slave->logical_address & 0xFF000000) >> 24;

      ec_command_write(&cmd, slave->station_address, 0x0600, 16, fmmu);

      if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
        return -1;

      if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Setting FMMU0 - Slave %04X did not"
               " respond!\n", slave->station_address);
        return -1;
      }
    }

    // Set Sync Managers

    if (type->features != EC_MAILBOX_SLAVE)
    {
      if (type->sm0)
      {
        ec_command_write(&cmd, slave->station_address, 0x0800, 8, type->sm0);

        if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
          return -1;

        if (unlikely(cmd.working_counter != 1)) {
          printk(KERN_ERR "EtherCAT: Setting SM0 - Slave %04X did not"
                 " respond!\n", slave->station_address);
          return -1;
        }
      }

      if (type->sm1)
      {
        ec_command_write(&cmd, slave->station_address, 0x0808, 8, type->sm1);

        if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
          return -1;

        if (unlikely(cmd.working_counter != 1)) {
          printk(KERN_ERR "EtherCAT: Setting SM1 - Slave %04X did not"
                 " respond!\n", slave->station_address);
          return -1;
        }
      }
    }

    if (type->sm2)
    {
      ec_command_write(&cmd, slave->station_address, 0x0810, 8, type->sm2);

      if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
        return -1;

      if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Setting SM2 - Slave %04X did not"
               " respond!\n", slave->station_address);
        return -1;
      }
    }

    if (type->sm3)
    {
      ec_command_write(&cmd, slave->station_address, 0x0818, 8, type->sm3);

      if (unlikely(ec_simple_send_receive(master, &cmd) < 0))
        return -1;

      if (unlikely(cmd.working_counter != 1)) {
        printk(KERN_ERR "EtherCAT: Setting SM3 - Slave %04X did not"
               " respond!\n", slave->station_address);
        return -1;
      }
    }

    // Change state to SAVEOP
    if (unlikely(ec_state_change(master, slave, EC_SLAVE_STATE_SAVEOP) != 0))
      return -1;

    // Change state to OP
    if (unlikely(ec_state_change(master, slave, EC_SLAVE_STATE_OP) != 0))
      return -1;
  }

  return 0;
}

/*****************************************************************************/

/**
   Setzt alle Slaves zurück in den Init-Zustand.

   \return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_rt_deactivate_slaves(ec_master_t *master /**< EtherCAT-Master */)
{
  ec_slave_t *slave;
  unsigned int i;

  for (i = 0; i < master->bus_slaves_count; i++)
  {
    slave = master->bus_slaves + i;

    if (unlikely(ec_state_change(master, slave, EC_SLAVE_STATE_INIT) != 0))
      return -1;
  }

  return 0;
}

/*****************************************************************************/

/**
   Sendet und empfängt Prozessdaten der angegebenen Domäne

   \return 0 bei Erfolg, sonst < 0
*/

int EtherCAT_rt_domain_xio(ec_master_t *master,
                           /**< EtherCAT-Master */
                           unsigned int domain,
                           /**< Domäne */
                           unsigned int timeout_us
                           /**< Timeout in Mikrosekunden */
                           )
{
  unsigned int i;
  ec_domain_t *dom;
  unsigned long start_ticks, end_ticks, timeout_ticks;

  ec_output_lost_frames(master); // Evtl. verlorene Frames ausgeben

  // Domäne bestimmen
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

  ec_command_logical_read_write(&dom->command, dom->logical_offset,
                                dom->data_size, dom->data);

  rdtscl(start_ticks); // Sendezeit nehmen

  if (unlikely(ec_simple_send(master, &dom->command) < 0)) {
    printk(KERN_ERR "EtherCAT: Could not send process data command!\n");
    return -1;
  }

  timeout_ticks = timeout_us * cpu_khz / 1000;

  // Warten
  do {
    ec_device_call_isr(&master->device);
    rdtscl(end_ticks); // Empfangszeit nehmen
  }
  while (unlikely(master->device.state == EC_DEVICE_STATE_SENT
                  && end_ticks - start_ticks < timeout_ticks));

  master->bus_time = (end_ticks - start_ticks) * 1000 / cpu_khz;

  if (unlikely(end_ticks - start_ticks >= timeout_ticks)) {
    master->device.state = EC_DEVICE_STATE_READY;
    master->frames_lost++;
    ec_output_lost_frames(master);
    return -1;
  }

  if (unlikely(ec_simple_receive(master, &dom->command) < 0)) {
    printk(KERN_ERR "EtherCAT: Could not receive cyclic command!\n");
    return -1;
  }

  if (unlikely(dom->command.state != EC_COMMAND_STATE_RECEIVED)) {
    printk(KERN_WARNING "EtherCAT: Process data command not received!\n");
    return -1;
  }

  if (dom->command.working_counter != dom->response_count) {
    dom->response_count = dom->command.working_counter;
    printk(KERN_INFO "EtherCAT: Domain %i State change - %i slaves"
           " responding.\n", dom->number, dom->response_count);
  }

  // Daten vom Kommando in den Prozessdatenspeicher kopieren
  memcpy(dom->data, dom->command.data, dom->data_size);

  return 0;
}

/*****************************************************************************/

/**
   Setzt die Debug-Ebene des Masters.

   Folgende Debug-level sind definiert:

   - 1: Nur Positionsmarken in bestimmten Funktionen
   - 2: Komplette Frame-Inhalte
*/

void EtherCAT_rt_debug_level(ec_master_t *master,
                             /**< EtherCAT-Master */
                             int level
                             /**< Debug-Level */
                             )
{
  master->debug_level = level;
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_rt_register_slave);
EXPORT_SYMBOL(EtherCAT_rt_register_slave_list);
EXPORT_SYMBOL(EtherCAT_rt_activate_slaves);
EXPORT_SYMBOL(EtherCAT_rt_deactivate_slaves);
EXPORT_SYMBOL(EtherCAT_rt_domain_xio);
EXPORT_SYMBOL(EtherCAT_rt_debug_level);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:2 ***
;;; End: ***
*/
