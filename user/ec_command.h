//---------------------------------------------------------------
//
//  e c _ c o m m a n d . h
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

typedef enum {Waiting, Sent, Received} EtherCAT_cmd_status_t;

//---------------------------------------------------------------

typedef struct EtherCAT_command
{
  unsigned char command_type;
  short ring_position;
  unsigned short node_address;
  unsigned short mem_address;
  unsigned int logical_address;
  unsigned int data_length;

  struct EtherCAT_command *next;

  EtherCAT_cmd_status_t status;
  unsigned char command_index;
  unsigned int working_counter;

  unsigned char *data;
  
}
EtherCAT_command_t;

//---------------------------------------------------------------

void EtherCAT_command_init(EtherCAT_command_t *);
void EtherCAT_command_clear(EtherCAT_command_t *);

// Debug
void EtherCAT_command_print_data(EtherCAT_command_t *);

//---------------------------------------------------------------
