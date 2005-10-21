//---------------------------------------------------------------
//
//  e c _ s l a v e . h
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

#define SIMPLE 0
#define MAILBOX 1

//---------------------------------------------------------------

typedef struct slave_desc EtherCAT_slave_desc_t;

typedef struct
{
  // Base data
  unsigned char type;
  unsigned char revision;
  unsigned short build;

  // Addresses
  short ring_position;
  unsigned short station_address;

  // Slave information interface
  unsigned int vendor_id;
  unsigned int product_code;
  unsigned int revision_number;

  const EtherCAT_slave_desc_t *desc;

  unsigned int logical_address0;

  unsigned int current_state;
  unsigned int requested_state;

  unsigned char *process_data;
}
EtherCAT_slave_t;

#define ECAT_INIT_SLAVE(TYPE) {0, 0, 0, 0, 0, 0, 0, 0, TYPE, 0, 0, 0, NULL}

//---------------------------------------------------------------

// Slave construction and deletion
void EtherCAT_slave_init(EtherCAT_slave_t *);
void EtherCAT_slave_clear(EtherCAT_slave_t *);

// Debug
void EtherCAT_slave_print(EtherCAT_slave_t *);

//---------------------------------------------------------------

typedef struct slave_desc
{
  const char *vendor_name;
  const char *product_name;
  const char *product_desc;

  const int type;

  const unsigned char *sm0;
  const unsigned char *sm1;
  const unsigned char *sm2;
  const unsigned char *sm3;

  const unsigned char *fmmu0;

  const unsigned int data_length;
}
EtherCAT_slave_desc_t;

extern EtherCAT_slave_desc_t Beckhoff_EK1100[];
extern EtherCAT_slave_desc_t Beckhoff_EL1014[];
extern EtherCAT_slave_desc_t Beckhoff_EL2004[];
extern EtherCAT_slave_desc_t Beckhoff_EL3102[];
extern EtherCAT_slave_desc_t Beckhoff_EL3162[];
extern EtherCAT_slave_desc_t Beckhoff_EL4102[];
extern EtherCAT_slave_desc_t Beckhoff_EL5001[];

//---------------------------------------------------------------

struct slave_ident
{
  const unsigned int vendor_id;
  const unsigned int product_code;
  const EtherCAT_slave_desc_t *desc;
};

extern struct slave_ident slave_idents[];
extern unsigned int slave_idents_count;

//---------------------------------------------------------------
