//---------------------------------------------------------------
//
//  m a i n . c
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

#include <stdio.h>
#include <string.h> // memset()
#include <unistd.h> // usleep()
#include <signal.h>

#include "ec_globals.h"
#include "ec_master.h"

//---------------------------------------------------------------

void signal_handler(int);
void write_data(unsigned char *);

int continue_running;
unsigned short int word;

//---------------------------------------------------------------

int main(int argc, char **argv)
{
  EtherCAT_master_t master;
  EtherCAT_command_t *cmd, *cmd2;
  unsigned char data[256];
  unsigned int i, number;
  struct sigaction sa;
  
  sa.sa_handler = signal_handler;
  sigaction(SIGINT, &sa, NULL);

  printf("CatEther-Testprogramm.\n");

  EtherCAT_master_init(&master, "eth1");

  if (EtherCAT_check_slaves(&master, NULL, 0) != 0)
  {
    fprintf(stderr, "ERROR while searching for slaves!\n");
    return -1;
  }

  if (master.slave_count == 0)
  {
    fprintf(stderr, "ERROR: No slaves found!\n");
    return -1;
  }

  for (i = 0; i < master.slave_count; i++)
  {
    printf("Slave found: Type %02X, Revision %02X, Build %04X\n",
           master.slaves[i].type, master.slaves[i].revision, master.slaves[i].build);
  }

  printf("Writing Station addresses.\n");

  for (i = 0; i < master.slave_count; i++)
  {
    data[0] = i & 0x00FF;
    data[1] = (i & 0xFF00) >> 8;

    cmd = EtherCAT_position_write(&master, 0 - i, 0x0010, 2, data);
    
    EtherCAT_send_receive(&master);
    
    if (cmd->working_counter != 1)
    {
      fprintf(stderr, "ERROR: Slave did'n repond!\n");
      return -1;
    }
    
    EtherCAT_remove_command(&master, cmd);
  }

  //----------

  for (i = 0; i < master.slave_count; i++)
  {
    printf("\nKlemme %i:\n", i);

    EtherCAT_read_slave_information(&master, i, 0x0008, &number);
    printf("Vendor ID: 0x%04X (%i)\n", number, number);

    EtherCAT_read_slave_information(&master, i, 0x000A, &number);
    printf("Product Code: 0x%04X (%i)\n", number, number);

    EtherCAT_read_slave_information(&master, i, 0x000E, &number);
    printf("Revision Number: 0x%04X (%i)\n", number, number);
  }

  //----------

  printf("\nResetting FMMU's.\n");

  memset(data, 0x00, 256);
  cmd = EtherCAT_broadcast_write(&master, 0x0600, 256, data);
  EtherCAT_send_receive(&master);

  if (cmd->working_counter != master.slave_count)
  {
    fprintf(stderr, "ERROR: Not all slaves responded (%i of %i)!\n",
            cmd->working_counter, master.slave_count);
    return -1;
  }

  EtherCAT_remove_command(&master, cmd);

  //----------

  printf("Resetting Sync Manager channels.\n");

  memset(data, 0x00, 256);
  cmd = EtherCAT_broadcast_write(&master, 0x0800, 256, data);
  EtherCAT_send_receive(&master);

  if (cmd->working_counter != master.slave_count)
  {
    fprintf(stderr, "ERROR: Not all slaves responded (%i of %i)!\n",
            cmd->working_counter, master.slave_count);
    return -1;
  }

  EtherCAT_remove_command(&master, cmd);

  //----------

  printf("Setting INIT state for devices.\n");

  if (EtherCAT_broadcast_state_change(&master, EC_STATE_INIT) != 0)
  {
    fprintf(stderr, "ERROR: Could not set INIT state!\n");
    return -1;
  }

  //----------

  printf("Setting PREOP state for bus coupler.\n");

  if (EtherCAT_state_change(&master, &master.slaves[0], EC_STATE_PREOP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

  printf("Setting Sync managers 0 and 1 of device 1.\n");

  data[0] = 0x00;
  data[1] = 0x18;
  data[2] = 0xF6;
  data[3] = 0x00;
  data[4] = 0x26;
  data[5] = 0x00;
  data[6] = 0x01;
  data[7] = 0x00;
  cmd = EtherCAT_write(&master, 0x0001, 0x0800, 8, data);

  data[0] = 0xF6;
  data[1] = 0x18;
  data[2] = 0xF6;
  data[3] = 0x00;
  data[4] = 0x22;
  data[5] = 0x00;
  data[6] = 0x01;
  data[7] = 0x00;
  cmd2 = EtherCAT_write(&master, 0x0001, 0x0808, 8, data);

  EtherCAT_send_receive(&master);

  if (cmd->working_counter != 1 || cmd2->working_counter != 1)
  {
    fprintf(stderr, "ERROR: Not all slaves responded!\n");

    return -1;
  }

  EtherCAT_remove_command(&master, cmd);
  EtherCAT_remove_command(&master, cmd2);


  //----------

  printf("Setting PREOP state for device 1.\n");

  if (EtherCAT_state_change(&master, &master.slaves[1], EC_STATE_PREOP))
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

  printf("Setting PREOP state for device 4.\n");

  if (EtherCAT_state_change(&master, &master.slaves[4], EC_STATE_PREOP))
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

#if 1
  printf("Setting FMMU 0 of device 1.\n");

  data[0] = 0x00; // Logical start address [4]
  data[1] = 0x00;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x04; // Length [2]
  data[5] = 0x00;
  data[6] = 0x00; // Start bit
  data[7] = 0x07; // End bit
  data[8] = 0x00; // Physical start address [2]
  data[9] = 0x10;
  data[10] = 0x00; // Physical start bit
  data[11] = 0x02; // Read/write enable
  data[12] = 0x01; // channel enable [2]
  data[13] = 0x00;
  data[14] = 0x00; // Reserved [2]
  data[15] = 0x00;
  cmd = EtherCAT_write(&master, 0x0001, 0x0600, 16, data);
  EtherCAT_send_receive(&master);

  if (cmd->working_counter != 1)
  {
    fprintf(stderr, "ERROR: Not all slaves responded (%i of 1)!\n",
            cmd->working_counter);
    return -1;
  }

  EtherCAT_remove_command(&master, cmd);
#endif

  //----------

#if 1
  printf("Setting FMMU 0 of device 4.\n");

  data[0] = 0x04; // Logical start address [4]
  data[1] = 0x00;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x01; // Length [2]
  data[5] = 0x00;
  data[6] = 0x00; // Start bit
  data[7] = 0x07; // End bit
  data[8] = 0x00; // Physical start address [2]
  data[9] = 0x0F;
  data[10] = 0x00; // Physical start bit
  data[11] = 0x02; // Read/write enable
  data[12] = 0x01; // channel enable [2]
  data[13] = 0x00;
  data[14] = 0x00; // Reserved [2]
  data[15] = 0x00;
  cmd = EtherCAT_write(&master, 0x0004, 0x0600, 16, data);
  EtherCAT_send_receive(&master);

  if (cmd->working_counter != 1)
  {
    fprintf(stderr, "ERROR: Not all slaves responded (%i of 1)!\n",
            cmd->working_counter);
    return -1;
  }

  EtherCAT_remove_command(&master, cmd);
#endif

  //----------

  printf("Setting Sync manager 2 of device 1.\n");

  data[0] = 0x00;
  data[1] = 0x10;
  data[2] = 0x04;
  data[3] = 0x00;
  data[4] = 0x24;
  data[5] = 0x00;
  data[6] = 0x01;
  data[7] = 0x00;
  cmd = EtherCAT_write(&master, 0x0001, 0x0810, 8, data);
  EtherCAT_send_receive(&master);

  if (cmd->working_counter != 1)
  {
    fprintf(stderr, "ERROR: Not all slaves responded (%i of 1)!\n", cmd->working_counter);
    return -1;
  }

  EtherCAT_remove_command(&master, cmd);

  //----------

  printf("Setting Sync manager 0 for device 4.\n");

  data[0] = 0x00;
  data[1] = 0x0F;
  data[2] = 0x01;
  data[3] = 0x00;
  data[4] = 0x46; // 46
  data[5] = 0x00;
  data[6] = 0x01;
  data[7] = 0x00;
  cmd = EtherCAT_write(&master, 0x0004, 0x0800, 8, data);

  EtherCAT_send_receive(&master);

  if (cmd->working_counter != 1)
  {
    fprintf(stderr, "ERROR: Not all slaves responded!\n");

    return -1;
  }

  EtherCAT_remove_command(&master, cmd);

  //----------

  printf("Setting SAVEOP state for bus coupler.\n");

  if (EtherCAT_state_change(&master, 0x0000, EC_STATE_SAVEOP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

  printf("Setting SAVEOP state for device 1.\n");

  if (EtherCAT_state_change(&master, &master.slaves[1], EC_STATE_SAVEOP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

  printf("Setting SAVEOP state for device 4.\n");

  if (EtherCAT_state_change(&master, &master.slaves[4], EC_STATE_SAVEOP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

  printf("Setting OP state for bus coupler.\n");

  if (EtherCAT_state_change(&master, &master.slaves[0], EC_STATE_OP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

  printf("Setting OP state for device 1.\n");

  if (EtherCAT_state_change(&master, &master.slaves[1], EC_STATE_OP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

  printf("Setting OP state for device 4.\n");

  if (EtherCAT_state_change(&master, &master.slaves[4], EC_STATE_OP) != 0)
  {
    fprintf(stderr, "ERROR: Could not set state!\n");
    return -1;
  }

  //----------

  word = 0;

  printf("Starting thread...\n");

  if (EtherCAT_start(&master, 5, write_data, NULL, 10000) != 0)
  {
    return -1;
  }

  continue_running = 1;

  while (continue_running)
  {
    usleep(200000);

    word += 1000;
    word = word & 0x7FFF;
  }

  //----------

  printf("Stopping master thread...\n");
  EtherCAT_stop(&master);

  EtherCAT_master_clear(&master);

  printf("Finished.\n");
  
  return 0;
}

//---------------------------------------------------------------

void write_data(unsigned char *data)
{
  data[0] = word & 0xFF;
  data[1] = (word & 0xFF00) >> 8;
  data[2] = word & 0xFF;
  data[3] = (word & 0xFF00) >> 8;

  data[4] = 0x01;
}

//---------------------------------------------------------------

void signal_handler(int signum)
{
  if (signum == SIGINT || signum == SIGTERM)
  {
    continue_running = 0;
  }
}

//---------------------------------------------------------------
