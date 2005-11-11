/****************************************************************
 *
 *  e c _ m a s t e r . h
 *
 *  Struktur für einen EtherCAT-Master.
 *
 *  $Date$
 *  $Author$
 *
 ***************************************************************/

#ifndef _EC_MASTER_H_
#define _EC_MASTER_H_

#include "ec_device.h"
#include "ec_slave.h"
#include "ec_command.h"

/***************************************************************/

/**
   EtherCAT-Master

   Verwaltet die EtherCAT-Slaves und kommuniziert mit
   dem zugewiesenen EtherCAT-Gerät.
*/

typedef struct
{
  EtherCAT_slave_t *slaves; /**< Zeiger auf statischen Speicher
                               mit Slave-Informationen */
  unsigned int slave_count; /**< Anzahl der Slaves in slaves */

  EtherCAT_command_t *first_command; /**< Zeiger auf das erste
                                        Kommando in der Liste */
  EtherCAT_command_t process_data_command; /**< Kommando zum Senden und
                                              Empfangen der Prozessdaten */

  EtherCAT_device_t *dev; /**< Zeiger auf das zugewiesene EtherCAT-Gerät */

  unsigned char command_index; /**< Aktueller Kommando-Index */

  unsigned char tx_data[ECAT_FRAME_BUFFER_SIZE]; /**< Statischer Speicher
                                                    für zu sendende Daten */
  unsigned int tx_data_length; /**< Länge der Daten im Sendespeicher */
  unsigned char rx_data[ECAT_FRAME_BUFFER_SIZE]; /**< Statische Speicher für
                                                    eine Kopie des Rx-Buffers
                                                    im EtherCAT-Gerät */

  unsigned char *process_data; /**< Zeiger auf Speicher mit Prozessdaten */
  unsigned int process_data_length; /**< Länge der Prozessdaten */

  EtherCAT_command_t cmd_ring[ECAT_COMMAND_RING_SIZE]; /**< Statischer Kommandoring */
  int cmd_reserved[ECAT_COMMAND_RING_SIZE]; /**< Reservierungsflags für die Kommandos */
  unsigned int cmd_ring_index; /**< Index des nächsten Kommandos im Ring */

  int debug_level; /**< Debug-Level im Master-Code */
}
EtherCAT_master_t;

/***************************************************************/

// Master creation and deletion
int EtherCAT_master_init(EtherCAT_master_t *, EtherCAT_device_t *);
void EtherCAT_master_clear(EtherCAT_master_t *);

// Slave management
int EtherCAT_check_slaves(EtherCAT_master_t *, EtherCAT_slave_t *, unsigned int);
void EtherCAT_clear_slaves(EtherCAT_master_t *);
int EtherCAT_activate_slave(EtherCAT_master_t *, EtherCAT_slave_t *);
int EtherCAT_deactivate_slave(EtherCAT_master_t *, EtherCAT_slave_t *);
int EtherCAT_activate_all_slaves(EtherCAT_master_t *);
int EtherCAT_deactivate_all_slaves(EtherCAT_master_t *);

// Sending and receiving
#if 0
int EtherCAT_async_send_receive(EtherCAT_master_t *);
int EtherCAT_send(EtherCAT_master_t *);
int EtherCAT_receive(EtherCAT_master_t *);
#endif
int EtherCAT_simple_send_receive(EtherCAT_master_t *, EtherCAT_command_t *);
int EtherCAT_simple_send(EtherCAT_master_t *, EtherCAT_command_t *);
int EtherCAT_simple_receive(EtherCAT_master_t *, EtherCAT_command_t *);

int EtherCAT_write_process_data(EtherCAT_master_t *);
int EtherCAT_read_process_data(EtherCAT_master_t *);
void EtherCAT_clear_process_data(EtherCAT_master_t *);

/***************************************************************/

// Slave information interface
int EtherCAT_read_slave_information(EtherCAT_master_t *,
                                    unsigned short int,
                                    unsigned short int,
                                    unsigned int *);

// EtherCAT commands
#if 0
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
#endif

// Slave states
int EtherCAT_state_change(EtherCAT_master_t *, EtherCAT_slave_t *, unsigned char);

/***************************************************************/

// Private functions
#if 0
EtherCAT_command_t *alloc_cmd(EtherCAT_master_t *);
int add_command(EtherCAT_master_t *, EtherCAT_command_t *);
#endif
void set_byte(unsigned char *, unsigned int, unsigned char);
void set_word(unsigned char *, unsigned int, unsigned int);
void output_debug_data(unsigned char *, unsigned int);

/***************************************************************/

#endif
