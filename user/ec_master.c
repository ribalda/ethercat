//---------------------------------------------------------------
//
//  e c _ m a s t e r . c
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

#include "ec_globals.h"
#include "ec_master.h"

#define DEBUG_SEND_RECEIVE

//---------------------------------------------------------------

int EtherCAT_master_init(EtherCAT_master_t *master,
                         char *eth_dev)
{
  char errbuf_libpcap[PCAP_ERRBUF_SIZE];
  char errbuf_libnet[LIBNET_ERRBUF_SIZE];

  master->slaves = NULL;
  master->slave_count = 0;

  master->first_command = NULL;
  master->command_index = 0x00;

  master->process_data = NULL;
  master->pre_cb = NULL;
  master->post_cb = NULL;
  master->thread_continue = 0;
  master->cycle_time = 0;
  
  // Init Libpcap
  master->pcap_handle = pcap_open_live(eth_dev, BUFSIZ, 1, 0, errbuf_libpcap);

  if (master->pcap_handle == NULL)
  {
    fprintf(stderr, "Couldn't open device %s: %s\n", eth_dev, errbuf_libpcap);
    return 1;
  }

  // Init Libnet
  if (!(master->net_handle = libnet_init(LIBNET_LINK, eth_dev, errbuf_libnet)))
  {
    fprintf(stderr, "Could not init %s: %s!\n", eth_dev, errbuf_libnet);

    pcap_close(master->pcap_handle);

    return 1;
  }

  return 0;
}

//---------------------------------------------------------------

void EtherCAT_master_clear(EtherCAT_master_t *master)
{
  libnet_destroy(master->net_handle);
  pcap_close(master->pcap_handle);

  // Remove all pending commands
  while (master->first_command)
  {
    EtherCAT_remove_command(master, master->first_command);
  }

  // Remove all slaves
  EtherCAT_clear_slaves(master);
}

//---------------------------------------------------------------

int EtherCAT_check_slaves(EtherCAT_master_t *master,
                          EtherCAT_slave_t *slaves,
                          unsigned int slave_count)
{
  EtherCAT_command_t *cmd;
  EtherCAT_slave_t *cur;
  unsigned int i, j, found;
  unsigned char data[2];

  // EtherCAT_clear_slaves() must be called before!
  if (master->slave_count) return -1;

  // Determine number of slaves

  if ((cmd = EtherCAT_broadcast_read(master, 0x0000, 4)) == NULL)
  {
    return -1;
  }
  
  if (EtherCAT_send_receive(master) != 0)
  {
    return -1;
  }

  master->slave_count = cmd->working_counter;

  EtherCAT_remove_command(master, cmd);

  if (master->slave_count < slave_count)
  {
    fprintf(stderr, "ERROR: Too few slaves on EtherCAT bus!\n");
    return -1;
  }

  // No slaves. Stop further processing...
  if (master->slave_count == 0) return 0;

   // For every slave in the list
  for (i = 0; i < master->slave_count; i++)
  {
    cur = &slaves[i];

    if (!cur->desc)
    {
      fprintf(stderr, "ERROR: Slave has no description (list position %i)!\n", i);
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
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Could not create command!\n");
      return -1;
    }
    
    if (EtherCAT_send_receive(master) != 0)
    {
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Could not send command!\n");
      return -1;
    }
    
    if (cmd->working_counter != 1)
    {
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Slave %i did not repond while writing station address!\n", i);
      return -1;
    }
    
    EtherCAT_remove_command(master, cmd);

    // Read base data

    if ((cmd = EtherCAT_read(master, cur->station_address, 0x0000, 4)) == NULL)
    {
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Could not create command!\n");
      return -1;
    }

    if (EtherCAT_send_receive(master) != 0)
    {
      EtherCAT_remove_command(master, cmd);
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Could not send command!\n");
      return -4;
    }

    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Slave %i did not respond while reading base data!\n", i);
      return -5;
    }

    // Get base data
    cur->type = cmd->data[0];
    cur->revision = cmd->data[1];
    cur->build = cmd->data[2] | (cmd->data[3] << 8);

    EtherCAT_remove_command(master, cmd);

    // Read identification from "Slave Information Interface" (SII)

    if (EtherCAT_read_slave_information(master, cur->station_address, 0x0008, &cur->vendor_id) != 0)
    {
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Could not read SII!\n", i);
      return -1;
    }

    if (EtherCAT_read_slave_information(master, cur->station_address, 0x000A, &cur->product_code) != 0)
    {
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Could not read SII!\n", i);
      return -1;
    }

    if (EtherCAT_read_slave_information(master, cur->station_address, 0x000E, &cur->revision_number) != 0)
    {
      master->slave_count = 0;
      fprintf(stderr, "ERROR: Could not read SII!\n", i);
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
          fprintf(stderr, "ERROR: Unexpected slave device at position %i:"
                  "%s %s. Expected: %s %s\n",
                  i, slave_idents[j].desc->vendor_name, slave_idents[j].desc->product_name,
                  cur->desc->vendor_name, cur->desc->product_name);
          return -1;
        }

        break;
      }
    }

    if (!found)
    {
      fprintf(stderr, "ERROR: Unknown slave device at position %i: Vendor %X, Code %X",
              i, cur->vendor_id, cur->product_code);
      return -1;      
    }
  }

  return 0;
}

//---------------------------------------------------------------

void EtherCAT_clear_slaves(EtherCAT_master_t *master)
{
  unsigned int i;

  if (master->slave_count == 0) return;

  for (i = 0; i < master->slave_count; i++)
  {
    EtherCAT_slave_clear(&master->slaves[i]);
  }

  free(master->slaves);
  master->slaves = NULL;
}

//---------------------------------------------------------------

int EtherCAT_read_slave_information(EtherCAT_master_t *master,
                                    unsigned short int node_address,
                                    unsigned short int offset,
                                    unsigned int *target)
{
  EtherCAT_command_t *cmd;
  unsigned char data[10];
  unsigned int tries;

  // Initiate read operation

  data[0] = 0x00;
  data[1] = 0x01;
  data[2] = offset & 0xFF;
  data[3] = (offset & 0xFF00) >> 8;
  data[4] = 0x00;
  data[5] = 0x00;

  if ((cmd = EtherCAT_write(master, node_address, 0x502, 6, data)) == NULL)
  {
    fprintf(stderr, "ERROR: Could not allocate command!\n");
    return -2;
  }

  if (EtherCAT_send_receive(master) != 0)
  {
    EtherCAT_remove_command(master, cmd);
    fprintf(stderr, "ERROR: Could not write to slave!\n");
    return -3;
  }

  if (cmd->working_counter != 1)
  {
    EtherCAT_remove_command(master, cmd);
    fprintf(stderr, "ERROR: Command not processed by slave!\n");
    return -4;
  }

  EtherCAT_remove_command(master, cmd);

  // Get status of read operation

  tries = 0;
  while (tries < 10)
  {
    if ((cmd = EtherCAT_read(master, node_address, 0x502, 10)) == NULL)
    {
      fprintf(stderr, "ERROR: Could not allocate command!\n");
      return -2;
    }
    
    if (EtherCAT_send_receive(master) != 0)
    {
      EtherCAT_remove_command(master, cmd);

      fprintf(stderr, "ERROR: Could not read from slave!\n");
      return -3;
    }
    
    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);

      fprintf(stderr, "ERROR: Command not processed by slave!\n");
      return -4;
    }

    if ((cmd->data[1] & 0x81) == 0)
    {
#if 0
      printf("SLI status data: %02X %02X Address: %02X %02X\n", cmd->data[0], cmd->data[1], cmd->data[2], cmd->data[3]);
      printf("Data: %02X %02X %02X %02X\n", cmd->data[6], cmd->data[7], cmd->data[8], cmd->data[9]);
#endif

      memcpy(target, cmd->data + 6, 4);

      EtherCAT_remove_command(master, cmd);

      break;
    }
    
    EtherCAT_remove_command(master, cmd);

    tries++;
  }

  if (tries == 10) fprintf(stderr, "ERROR: Timeout while reading SII!\n");

  return 0;
}

//---------------------------------------------------------------

int EtherCAT_send_receive(EtherCAT_master_t *master)
{
  libnet_ptag_t ptag;
  struct pcap_pkthdr header;
  const unsigned char *packet;
  unsigned char dst[6], src[6];
  unsigned int i, length, framelength, pos, command_type, command_index;
  EtherCAT_command_t *cmd;
  unsigned char *data;
  int bytes, command_follows, found;

#ifdef DEBUG_SEND_RECEIVE
  found = 0;
#endif

  length = 0;
  for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
  {
    //if (cmd->status != Waiting) continue;
    length += cmd->data_length + 12;

#ifdef DEBUG_SEND_RECEIVE
    found++;
#endif
  }

#ifdef DEBUG_SEND_RECEIVE
  printf("Sending %i commands with length %i...\n", found, length);
#endif

  if (length == 0) return 0;

  framelength = length + 2;
  if (framelength < 46) framelength = 46;

  data = (unsigned char *) malloc(sizeof(unsigned char) * framelength);
  if (!data) return -1;

  data[0] = length & 0xFF;
  data[1] = ((length & 0x700) >> 8) | 0x10;
  pos = 2;

  for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
  {
    if (cmd->status != Waiting)
    {
      printf("Old command: %02X\n", cmd->command_type);
      continue;
    }

    cmd->command_index = master->command_index;
    master->command_index = (master->command_index + 1) % 0x0100;

    cmd->status = Sent;

    data[pos + 0] = cmd->command_type;
    data[pos + 1] = cmd->command_index;

    switch (cmd->command_type)
    {
      case EC_CMD_APRD:
      case EC_CMD_APWR:
        data[pos + 2] = cmd->ring_position & 0xFF;
        data[pos + 3] = (cmd->ring_position & 0xFF00) >> 8;
        data[pos + 4] = cmd->mem_address & 0xFF;
        data[pos + 5] = (cmd->mem_address & 0xFF00) >> 8;
        break;

      case EC_CMD_NPRD:
      case EC_CMD_NPWR:
        data[pos + 2] = cmd->node_address & 0xFF;
        data[pos + 3] = (cmd->node_address & 0xFF00) >> 8;
        data[pos + 4] = cmd->mem_address & 0xFF;
        data[pos + 5] = (cmd->mem_address & 0xFF00) >> 8;
        break;

      case EC_CMD_BRD:
      case EC_CMD_BWR:
        data[pos + 2] = 0x00;
        data[pos + 3] = 0x00;
        data[pos + 4] = cmd->mem_address & 0xFF;
        data[pos + 5] = (cmd->mem_address & 0xFF00) >> 8;
        break;

      case EC_CMD_LRW:
        data[pos + 2] = cmd->logical_address & 0x000000FF;
        data[pos + 3] = (cmd->logical_address & 0x0000FF00) >> 8;
        data[pos + 4] = (cmd->logical_address & 0x00FF0000) >> 16;
        data[pos + 5] = (cmd->logical_address & 0xFF000000) >> 24;
        break;

      default:
        data[pos + 2] = 0x00;
        data[pos + 3] = 0x00;
        data[pos + 4] = 0x00;
        data[pos + 5] = 0x00;
        fprintf(stderr, "WARNING: Default adress while frame construction...\n");
        break;
    }

    data[pos + 6] = cmd->data_length & 0xFF;
    data[pos + 7] = (cmd->data_length & 0x700) >> 8;

    if (cmd->next) data[pos + 7] |= 0x80;

    data[pos + 8] = 0x00;
    data[pos + 9] = 0x00;

    if (cmd->command_type == EC_CMD_APWR
        || cmd->command_type == EC_CMD_NPWR
        || cmd->command_type == EC_CMD_BWR
        || cmd->command_type == EC_CMD_LRW) // Write
    {
      for (i = 0; i < cmd->data_length; i++) data[pos + 10 + i] = cmd->data[i];
    }
    else // Read
    {
      for (i = 0; i < cmd->data_length; i++) data[pos + 10 + i] = 0x00;
    }

    data[pos + 10 + cmd->data_length] = 0x00;
    data[pos + 11 + cmd->data_length] = 0x00;

    pos += 12 + cmd->data_length;
  }

  // Pad with zeros
  while (pos < 46) data[pos++] = 0x00;

#ifdef DEBUG_SEND_RECEIVE
  printf("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
  for (i = 0; i < framelength; i++)
  {
    printf("%02X ", data[i]);

    if ((i + 1) % 16 == 0) printf("\n");
  }
  printf("\n-----------------------------------------------\n");
#endif
 
  dst[0] = 0xFF;
  dst[1] = 0xFF;
  dst[2] = 0xFF;
  dst[3] = 0xFF;
  dst[4] = 0xFF;
  dst[5] = 0xFF;

  src[0] = 0x00;
  src[1] = 0x00;
  src[2] = 0x00;
  src[3] = 0x00;
  src[4] = 0x00;
  src[5] = 0x00;

  // Send frame
  ptag = libnet_build_ethernet(dst, src, 0x88A4, data, framelength, master->net_handle, 0);
  bytes = libnet_write(master->net_handle);
  libnet_clear_packet(master->net_handle);

  if (bytes == -1)
  {
    free(data);
    fprintf(stderr, "Could not write!\n");
    return -1;
  }

  packet = pcap_next(master->pcap_handle, &header); // LibPCap receives sent frame first
  packet = pcap_next(master->pcap_handle, &header);

#ifdef DEBUG_SEND_RECEIVE
  for (i = 0; i < header.len - 14; i++)
  {
    if (packet[i + 14] == data[i]) printf("   ");
    else printf("%02X ", packet[i + 14]);

    if ((i + 1) % 16 == 0) printf("\n");
  }
  printf("\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n\n");
#endif

  // Free sent data
  free(data);

  pos = 16;
  command_follows = 1;
  while (command_follows)
  {
    command_type = packet[pos];
    command_index = packet[pos + 1];
    length = (packet[pos + 6] & 0xFF) | ((packet[pos + 7] & 0x07) << 8);
    command_follows = packet[pos + 7] & 0x80;

#if 0
    printf("Command %02X received!\n", command_index);
#endif

    found = 0;

    for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
    {
      if (cmd->status == Sent
          && cmd->command_type == command_type
          && cmd->command_index == command_index
          && cmd->data_length == length)
      {
        found = 1;
        cmd->status = Received;

        cmd->data = (unsigned char *) malloc(sizeof(unsigned char) * length);
        memcpy(cmd->data, packet + pos + 10, length);

        cmd->working_counter = (packet[pos + length + 10] & 0xFF)
          | ((packet[pos + length + 11] & 0xFF) << 8);
      }
    }

    if (!found)
    {
      fprintf(stderr, "WARNING: Command not assigned!\n");
    }

    pos += length + 12;
  }

  for (cmd = master->first_command; cmd != NULL; cmd = cmd->next)
  {
    if (cmd->status == Sent)
    {
      fprintf(stderr, "WARNING: Command not sent!\n");
    }
  }
      
  return 0;
}

//---------------------------------------------------------------

EtherCAT_command_t *EtherCAT_read(EtherCAT_master_t *master,
                                  unsigned short node_address,
                                  unsigned short offset,
                                  unsigned int length)
{
  EtherCAT_command_t *cmd;

  if (node_address == 0x0000) fprintf(stderr, "WARNING: Using node address 0x0000!\n");

  cmd = (EtherCAT_command_t *) malloc(sizeof(EtherCAT_command_t));

  if (cmd == NULL)
  {
    return NULL;
  }

  EtherCAT_command_init(cmd);

  cmd->command_type = EC_CMD_NPRD;
  cmd->node_address = node_address;
  cmd->mem_address = offset;
  cmd->data_length = length;

  // Add command to master's list
  add_command(master, cmd);

  return cmd;
}

//---------------------------------------------------------------

EtherCAT_command_t *EtherCAT_write(EtherCAT_master_t *master,
                                   unsigned short node_address,
                                   unsigned short offset,
                                   unsigned int length,
                                   const unsigned char *data)
{
  EtherCAT_command_t *cmd;

  if (node_address == 0x0000) fprintf(stderr, "WARNING: Using node address 0x0000!\n");

  cmd = (EtherCAT_command_t *) malloc(sizeof(EtherCAT_command_t));

  if (cmd == NULL)
  {
    return NULL;
  }

  EtherCAT_command_init(cmd);

  cmd->command_type = EC_CMD_NPWR;
  cmd->node_address = node_address;
  cmd->mem_address = offset;
  cmd->data_length = length;

  cmd->data = (unsigned char *) malloc(sizeof(unsigned char) * length);

  if (cmd->data == NULL)
  {
    free(cmd);
    return NULL;
  }

  memcpy(cmd->data, data, length);

  // Add command to master's list
  add_command(master, cmd);

  return cmd;
}

//---------------------------------------------------------------

EtherCAT_command_t *EtherCAT_position_read(EtherCAT_master_t *master,
                                           short ring_position,
                                           unsigned short offset,
                                           unsigned int length)
{
  EtherCAT_command_t *cmd;

  cmd = (EtherCAT_command_t *) malloc(sizeof(EtherCAT_command_t));

  if (cmd == NULL)
  {
    return NULL;
  }

  EtherCAT_command_init(cmd);

  cmd->command_type = EC_CMD_APRD;
  cmd->ring_position = ring_position;
  cmd->mem_address = offset;
  cmd->data_length = length;

  // Add command to master's list
  add_command(master, cmd);

  return cmd;
}

//---------------------------------------------------------------

EtherCAT_command_t *EtherCAT_position_write(EtherCAT_master_t *master,
                                            short ring_position,
                                            unsigned short offset,
                                            unsigned int length,
                                            const unsigned char *data)
{
  EtherCAT_command_t *cmd;

  cmd = (EtherCAT_command_t *) malloc(sizeof(EtherCAT_command_t));

  if (cmd == NULL)
  {
    return NULL;
  }

  EtherCAT_command_init(cmd);

  cmd->command_type = EC_CMD_APWR;
  cmd->ring_position = ring_position;
  cmd->mem_address = offset;
  cmd->data_length = length;

  cmd->data = (unsigned char *) malloc(sizeof(unsigned char) * length);

  if (cmd->data == NULL)
  {
    free(cmd);
    return NULL;
  }

  memcpy(cmd->data, data, length);

  // Add command to master's list
  add_command(master, cmd);

  return cmd;
}

//---------------------------------------------------------------

EtherCAT_command_t *EtherCAT_broadcast_read(EtherCAT_master_t *master,
                                            unsigned short offset,
                                            unsigned int length)
{
  EtherCAT_command_t *cmd;

  cmd = (EtherCAT_command_t *) malloc(sizeof(EtherCAT_command_t));

  if (cmd == NULL)
  {
    return NULL;
  }

  EtherCAT_command_init(cmd);

  cmd->command_type = EC_CMD_BRD;
  cmd->mem_address = offset;
  cmd->data_length = length;

  // Add command to master's list
  add_command(master, cmd);

  return cmd;
}

//---------------------------------------------------------------

EtherCAT_command_t *EtherCAT_broadcast_write(EtherCAT_master_t *master,
                                             unsigned short offset,
                                             unsigned int length,
                                             const unsigned char *data)
{
  EtherCAT_command_t *cmd;

  cmd = (EtherCAT_command_t *) malloc(sizeof(EtherCAT_command_t));

  if (cmd == NULL)
  {
    return NULL;
  }

  EtherCAT_command_init(cmd);

  cmd->command_type = EC_CMD_BWR;
  cmd->mem_address = offset;
  cmd->data_length = length;

  cmd->data = (unsigned char *) malloc(sizeof(unsigned char) * length);

  if (cmd->data == NULL)
  {
    free(cmd);
    return NULL;
  }

  memcpy(cmd->data, data, length);

  // Add command to master's list
  add_command(master, cmd);

  return cmd;
}

//---------------------------------------------------------------

EtherCAT_command_t *EtherCAT_logical_read_write(EtherCAT_master_t *master,
                                                unsigned int offset,
                                                unsigned int length,
                                                unsigned char *data)
{
  EtherCAT_command_t *cmd;

  cmd = (EtherCAT_command_t *) malloc(sizeof(EtherCAT_command_t));

  if (cmd == NULL)
  {
    return NULL;
  }

  EtherCAT_command_init(cmd);

  cmd->command_type = EC_CMD_LRW;
  cmd->mem_address = offset;
  cmd->data_length = length;

  cmd->data = (unsigned char *) malloc(sizeof(unsigned char) * length);

  if (cmd->data == NULL)
  {
    free(cmd);
    return NULL;
  }

  memcpy(cmd->data, data, length);

  // Add command to master's list
  add_command(master, cmd);

  return cmd;
}

//---------------------------------------------------------------

void add_command(EtherCAT_master_t *master, EtherCAT_command_t *cmd)
{
  EtherCAT_command_t **last_cmd;

  // Find last position in the list
  last_cmd = &(master->first_command);
  while (*last_cmd) last_cmd = &(*last_cmd)->next;

  *last_cmd = cmd;
}

//---------------------------------------------------------------

void EtherCAT_remove_command(EtherCAT_master_t *master,
                             EtherCAT_command_t *rem_cmd)
{
  EtherCAT_command_t **last_cmd;

  last_cmd = &(master->first_command);
  while (*last_cmd)
  {
    if (*last_cmd == rem_cmd)
    {
      *last_cmd = rem_cmd->next;
      EtherCAT_command_clear(rem_cmd);

      return;
    }

    last_cmd = &(*last_cmd)->next;
  }
}

//---------------------------------------------------------------

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
    return -1;
  }

  if (EtherCAT_send_receive(master) != 0)
  {
    EtherCAT_remove_command(master, cmd);
    return -2;
  }

  if (cmd->working_counter != 1)
  {
    EtherCAT_remove_command(master, cmd);
    return -3;
  }

  EtherCAT_remove_command(master, cmd);

  slave->requested_state = state_and_ack & 0x0F;

  tries_left = 10;

  while (tries_left)
  {
    if ((cmd = EtherCAT_read(master, slave->station_address,
                             0x0130, 2)) == NULL)
    {
      return -1;
    }
    
    if (EtherCAT_send_receive(master) != 0)
    {
      EtherCAT_remove_command(master, cmd);
      return -2;
    }
    
    if (cmd->working_counter != 1)
    {
      EtherCAT_remove_command(master, cmd);
      return -3;
    }

    if (cmd->data[0] & 0x10) // State change error
    {
      EtherCAT_remove_command(master, cmd);
      return -4;
    }

    if (cmd->data[0] == state_and_ack & 0x0F) // State change successful
    {
      EtherCAT_remove_command(master, cmd);
      break;
    }

    EtherCAT_remove_command(master, cmd);

    //printf("Trying again...\n");

    tries_left--;
  }

  if (!tries_left)
  {
    return -5;
  }

  slave->current_state = state_and_ack & 0x0F;

  return 0;
}

//---------------------------------------------------------------

int EtherCAT_broadcast_state_change(EtherCAT_master_t *master,
                                    unsigned char state)
{
  EtherCAT_command_t *cmd;
  unsigned char data[2];
  unsigned int tries_left;

  data[0] = state;
  data[1] = 0x00;

  if ((cmd = EtherCAT_broadcast_write(master, 0x0120, 2, data)) == NULL)
  {
    return -1;
  }

  if (EtherCAT_send_receive(master) != 0)
  {
    EtherCAT_remove_command(master, cmd);
    return -2;
  }

  if (cmd->working_counter != master->slave_count)
  {
    EtherCAT_remove_command(master, cmd);
    return -3;
  }

  EtherCAT_remove_command(master, cmd);

  tries_left = 10;

  while (tries_left)
  {
    if ((cmd = EtherCAT_broadcast_read(master, 0x0130, 2)) == NULL)
    {
      return -1;
    }
    
    if (EtherCAT_send_receive(master) != 0)
    {
      EtherCAT_remove_command(master, cmd);
      return -2;
    }
    
    if (cmd->working_counter != master->slave_count)
    {
      EtherCAT_remove_command(master, cmd);
      return -3;
    }

    if (cmd->data[0] & 0x10) // State change error
    {
      EtherCAT_remove_command(master, cmd);
      return -4;
    }

    if (cmd->data[0] == state) // State change successful
    {
      EtherCAT_remove_command(master, cmd);
      break;
    }

    EtherCAT_remove_command(master, cmd);

    //printf("Trying again...\n");

    tries_left--;
  }

  if (!tries_left)
  {
    return -5;
  }

  return 0;
}

//---------------------------------------------------------------

int EtherCAT_start(EtherCAT_master_t *master,
                   unsigned int length,
                   void (*pre_cb)(unsigned char *),
                   void (*post_cb)(unsigned char *),
                   unsigned int cycle_time)
{
  if (master->process_data)
  {
    fprintf(stderr, "ERROR: Process data already allocated!\n");
    return -1;
  }

  if ((master->process_data = (unsigned char *) malloc(length)) == NULL)
  {
    fprintf(stderr, "ERROR: Could not allocate process data block!\n");
    return -2;
  }

  memset(master->process_data, 0x00, length);

  master->process_data_length = length;
  master->pre_cb = pre_cb;
  master->post_cb = post_cb;
  master->cycle_time = cycle_time;

  master->thread_continue = 1;

  if (pthread_create(&master->thread, NULL, thread_function, (void *) master) != 0)
  {
    fprintf(stderr, "ERROR: Could not create thread!\n");
    return -3;
  }

  return 0;
}

//---------------------------------------------------------------

int EtherCAT_stop(EtherCAT_master_t *master)
{
  if (!master->thread_continue)
  {
    fprintf(stderr, "ERROR: Thread not running!\n");
    return -1;
  }

  master->thread_continue = 0;
  pthread_join(master->thread, NULL);

  if (master->process_data)
  {
    free(master->process_data);
    master->process_data = NULL;
  }

  master->pre_cb = NULL;
  master->post_cb = NULL;

  return 0;
}

//---------------------------------------------------------------

double current_timestamp()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + (double) tv.tv_usec / 1000000.0;
}

//---------------------------------------------------------------

void *thread_function(void *data)
{
  EtherCAT_master_t *master;
  EtherCAT_command_t *cmd;
  double bus_start_time, bus_end_time;
  double cycle_start_time, cycle_end_time, last_cycle_start_time;
  unsigned int wait_usecs;

  master = (EtherCAT_master_t *) data;

  last_cycle_start_time = 0.0;

  while (master->thread_continue)
  {
    cycle_start_time = current_timestamp();

    if (last_cycle_start_time != 0.0)
    {
      master->last_cycle_time = cycle_start_time - last_cycle_start_time;
      master->last_jitter = (master->last_cycle_time - (master->cycle_time / 1000000.0))
        / (master->cycle_time / 1000000.0) * 100.0;
    }

    last_cycle_start_time = cycle_start_time;

    if (master->pre_cb) master->pre_cb(master->process_data);

    cmd = EtherCAT_logical_read_write(master,
                                      0x00000000,
                                      master->process_data_length,
                                      master->process_data);

    bus_start_time = current_timestamp();

    EtherCAT_send_receive(master);

    bus_end_time = current_timestamp();
    master->bus_time = bus_end_time - bus_start_time;
    
#if 0
    printf("Working counter: %i\n", cmd->working_counter);
#endif
    
    memcpy(master->process_data, cmd->data, master->process_data_length);

    EtherCAT_remove_command(master, cmd);

    if (master->post_cb) master->post_cb(master->process_data);

    // Calculate working time

    cycle_end_time = current_timestamp();
    master->last_cycle_work_time = cycle_end_time - cycle_start_time;
    master->last_cycle_busy_rate = master->last_cycle_work_time
      / ((double) master->cycle_time / 1000000.0) * 100.0;
    wait_usecs = master->cycle_time - (unsigned int) (master->last_cycle_work_time * 1000000.0);

    //printf("USECS to wait: %i\n", wait_usecs);

    usleep(wait_usecs);

    //printf("waited: %lf\n", current_timestamp() - cycle_end_time);
  }

  return (void *) 0;
}

//---------------------------------------------------------------

int EtherCAT_activate_slave(EtherCAT_master_t *master,
                            EtherCAT_slave_t *slave)
{
  EtherCAT_command_t *cmd;
  const EtherCAT_slave_desc_t *desc;
  unsigned char fmmu[16];
  unsigned char data[256];

  if (EtherCAT_state_change(master, slave, EC_STATE_INIT) != 0)
  {
    return -1;
  }

  // Resetting FMMU's

  memset(data, 0x00, 256);
  cmd = EtherCAT_write(master, slave->station_address, 0x0600, 256, data);
  EtherCAT_send_receive(master);

  if (cmd->working_counter != 1)
  {
    fprintf(stderr, "ERROR: Slave did not respond!\n");
    return -2;
  }

  EtherCAT_remove_command(master, cmd);

  // Resetting Sync Manager channels

  memset(data, 0x00, 256);
  cmd = EtherCAT_write(master, slave->station_address, 0x0800, 256, data);
  EtherCAT_send_receive(master);

  if (cmd->working_counter != 1)
  {
    fprintf(stderr, "ERROR: Slave did not respond!\n");
    return -2;
  }

  EtherCAT_remove_command(master, cmd);

  desc = slave->desc;

  // Init Mailbox communication

  if (desc->type == MAILBOX)
  {
    if (desc->sm0)
    {
      cmd = EtherCAT_write(master, slave->station_address, 0x0800, 8, desc->sm0);
      
      EtherCAT_send_receive(master);
      
      if (cmd->working_counter != 1)
      {
        fprintf(stderr, "ERROR: Not all slaves responded!\n");
        return -3;
      }
      
      EtherCAT_remove_command(master, cmd);
    }

    if (desc->sm1)
    {
      cmd = EtherCAT_write(master, slave->station_address, 0x0808, 8, desc->sm1);
      
      EtherCAT_send_receive(master);
      
      if (cmd->working_counter != 1)
      {
        fprintf(stderr, "ERROR: Not all slaves responded!\n");
        return -4;
      }
      
      EtherCAT_remove_command(master, cmd);
    }
  }

  // Change state to PREOP

  if (EtherCAT_state_change(master, slave, EC_STATE_PREOP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
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

    cmd = EtherCAT_write(master, slave->station_address, 0x0600, 16, fmmu);
    EtherCAT_send_receive(master);

    if (cmd->working_counter != 1)
    {
      fprintf(stderr, "ERROR: Not all slaves responded (%i of 1)!\n",
              cmd->working_counter);
      return -6;
    }

    EtherCAT_remove_command(master, cmd);
  }

  // Set Sync Managers

  if (desc->type != MAILBOX)
  {
    if (desc->sm0)
    {
      cmd = EtherCAT_write(master, slave->station_address, 0x0800, 8, desc->sm0);
      
      EtherCAT_send_receive(master);
      
      if (cmd->working_counter != 1)
      {
        fprintf(stderr, "ERROR: Not all slaves responded!\n");
        return -8;
      }
      
      EtherCAT_remove_command(master, cmd);
    }

    if (desc->sm1)
    {
      cmd = EtherCAT_write(master, slave->station_address, 0x0808, 8, desc->sm1);
      
      EtherCAT_send_receive(master);
      
      if (cmd->working_counter != 1)
      {
        fprintf(stderr, "ERROR: Not all slaves responded!\n");
        return -9;
      }
      
      EtherCAT_remove_command(master, cmd);
    }
  }
  
  if (desc->sm2)
  {
    cmd = EtherCAT_write(master, slave->station_address, 0x0810, 8, desc->sm2);
      
    EtherCAT_send_receive(master);
      
    if (cmd->working_counter != 1)
    {
      fprintf(stderr, "ERROR: Not all slaves responded!\n");
      return -10;
    }
      
    EtherCAT_remove_command(master, cmd);
  }

  if (desc->sm3)
  {
    cmd = EtherCAT_write(master, slave->station_address, 0x0818, 8, desc->sm3);
      
    EtherCAT_send_receive(master);
      
    if (cmd->working_counter != 1)
    {
      fprintf(stderr, "ERROR: Not all slaves responded!\n");
      return -11;
    }
      
    EtherCAT_remove_command(master, cmd);
  }

  // Change state to SAVEOP

  if (EtherCAT_state_change(master, slave, EC_STATE_SAVEOP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -12;
  }

  // Change state to OP

  if (EtherCAT_state_change(master, slave, EC_STATE_OP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -13;
  }

  return 0;
}

//---------------------------------------------------------------

int EtherCAT_deactivate_slave(EtherCAT_master_t *master,
                            EtherCAT_slave_t *slave)
{
  if (EtherCAT_state_change(master, slave, EC_STATE_INIT) != 0)
  {
    return -1;
  }

  return 0;
}

//---------------------------------------------------------------

void set_byte(unsigned char *data,
              unsigned int offset,
              unsigned char value)
{
  data[offset] = value;
}

//---------------------------------------------------------------

void set_word(unsigned char *data,
              unsigned int offset,
              unsigned int value)
{
  data[offset] = value & 0xFF;
  data[offset + 1] = (value & 0xFF00) >> 8;
}

//---------------------------------------------------------------
