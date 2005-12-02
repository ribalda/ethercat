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
  unsigned int rx_data_length; /**< Länge der Daten im Empfangsspeicher */

  unsigned char *process_data; /**< Zeiger auf Speicher mit Prozessdaten */
  unsigned int process_data_length; /**< Länge der Prozessdaten */

  int debug_level; /**< Debug-Level im Master-Code */
}
EtherCAT_master_t;

/***************************************************************/

// Master creation and deletion
void EtherCAT_master_init(EtherCAT_master_t *);
void EtherCAT_master_clear(EtherCAT_master_t *);

// Registration of devices
int EtherCAT_register_device(EtherCAT_master_t *, EtherCAT_device_t *);
void EtherCAT_unregister_device(EtherCAT_master_t *, EtherCAT_device_t *);

// Sending and receiving
int EtherCAT_simple_send_receive(EtherCAT_master_t *, EtherCAT_command_t *);
int EtherCAT_simple_send(EtherCAT_master_t *, EtherCAT_command_t *);
int EtherCAT_simple_receive(EtherCAT_master_t *, EtherCAT_command_t *);

// Slave management
int EtherCAT_check_slaves(EtherCAT_master_t *, EtherCAT_slave_t *, unsigned int);
void EtherCAT_clear_slaves(EtherCAT_master_t *);
int EtherCAT_read_slave_information(EtherCAT_master_t *,
                                    unsigned short int,
                                    unsigned short int,
                                    unsigned int *);
int EtherCAT_activate_slave(EtherCAT_master_t *, EtherCAT_slave_t *);
int EtherCAT_deactivate_slave(EtherCAT_master_t *, EtherCAT_slave_t *);
int EtherCAT_activate_all_slaves(EtherCAT_master_t *);
int EtherCAT_deactivate_all_slaves(EtherCAT_master_t *);
int EtherCAT_state_change(EtherCAT_master_t *, EtherCAT_slave_t *, unsigned char);

// Process data
int EtherCAT_write_process_data(EtherCAT_master_t *);
int EtherCAT_read_process_data(EtherCAT_master_t *);
void EtherCAT_clear_process_data(EtherCAT_master_t *);

// Private functions
void output_debug_data(const EtherCAT_master_t *);

/***************************************************************/

#endif
