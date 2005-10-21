//---------------------------------------------------------------
//
//  e c _ m a s t e r . h
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

#include <pcap.h>
#include <libnet.h>
#include <pthread.h>

#include "ec_slave.h"
#include "ec_command.h"

//---------------------------------------------------------------

typedef struct
{
  EtherCAT_slave_t *slaves; // Slaves array
  unsigned int slave_count; // Number of slaves

  EtherCAT_command_t *first_command; // List of commands

  pcap_t *pcap_handle;  // Handle for libpcap
  libnet_t *net_handle; // Handle for libnet

  unsigned char command_index; // Current command index

  unsigned char *process_data;
  unsigned int process_data_length;
  void (*pre_cb)(unsigned char *);
  void (*post_cb)(unsigned char *);
  pthread_t thread;
  int thread_continue;
  unsigned int cycle_time;

  double bus_time;

  double last_jitter;
  double last_cycle_time;
  double last_cycle_work_time;
  double last_cycle_busy_rate;
}
EtherCAT_master_t;

//---------------------------------------------------------------

// Master creation and deletion
int EtherCAT_master_init(EtherCAT_master_t *, char *);
void EtherCAT_master_clear(EtherCAT_master_t *);

// Checking for slaves
int EtherCAT_check_slaves(EtherCAT_master_t *, EtherCAT_slave_t *, unsigned int);
void EtherCAT_clear_slaves(EtherCAT_master_t *);

// Slave information interface
int EtherCAT_read_slave_information(EtherCAT_master_t *,
                                    unsigned short int,
                                    unsigned short int,
                                    unsigned int *);

// EtherCAT commands
EtherCAT_command_t *EtherCAT_read(EtherCAT_master_t *,
                                  unsigned short,
                                  unsigned short,
                                  unsigned int);
EtherCAT_command_t *EtherCAT_write(EtherCAT_master_t *,
                                   unsigned short,
                                   unsigned short,
                                   unsigned int,
                                   const unsigned char *);
EtherCAT_command_t *EtherCAT_position_read(EtherCAT_master_t *,
                                           short,
                                           unsigned short,
                                           unsigned int);
EtherCAT_command_t *EtherCAT_position_write(EtherCAT_master_t *,
                                            short,
                                            unsigned short,
                                            unsigned int,
                                            const unsigned char *);
EtherCAT_command_t *EtherCAT_broadcast_read(EtherCAT_master_t *,
                                            unsigned short,
                                            unsigned int);
EtherCAT_command_t *EtherCAT_broadcast_write(EtherCAT_master_t *,
                                             unsigned short,
                                             unsigned int,
                                             const unsigned char *);
EtherCAT_command_t *EtherCAT_logical_read_write(EtherCAT_master_t *,
                                                unsigned int,
                                                unsigned int,
                                                unsigned char *);

void EtherCAT_remove_command(EtherCAT_master_t *, EtherCAT_command_t *);

// Slave states
int EtherCAT_state_change(EtherCAT_master_t *, EtherCAT_slave_t *, unsigned char);
int EtherCAT_broadcast_state_change(EtherCAT_master_t *, unsigned char);

int EtherCAT_activate_slave(EtherCAT_master_t *, EtherCAT_slave_t *);
int EtherCAT_deactivate_slave(EtherCAT_master_t *, EtherCAT_slave_t *);

// Sending and receiving
int EtherCAT_send_receive(EtherCAT_master_t *);

int EtherCAT_start(EtherCAT_master_t *,
                   unsigned int,
                   void (*)(unsigned char *),
                   void (*)(unsigned char *),
                   unsigned int);
int EtherCAT_stop(EtherCAT_master_t *);

// Private functions
void add_command(EtherCAT_master_t *, EtherCAT_command_t *);
void set_byte(unsigned char *, unsigned int, unsigned char);
void set_word(unsigned char *, unsigned int, unsigned int);
void *thread_function(void *);

//---------------------------------------------------------------
