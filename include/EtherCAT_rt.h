/******************************************************************************
 *
 * Oeffentliche EtherCAT-Schnittstellen fuer Echtzeitprozesse.
 *
 * $Id$
 *
 *****************************************************************************/

#ifndef _ETHERCAT_RT_H_
#define _ETHERCAT_RT_H_

/*****************************************************************************/

struct ec_master;
typedef struct ec_master ec_master_t;

struct ec_slave_type;
typedef struct ec_slave_type ec_slave_type_t;

struct ec_slave;
typedef struct ec_slave ec_slave_t;

/*****************************************************************************/

ec_master_t *EtherCAT_rt_request_master(unsigned int master_index);

void EtherCAT_rt_release_master(ec_master_t *master);

ec_slave_t *EtherCAT_rt_register_slave(ec_master_t *master,
                                       unsigned int slave_index,
                                       const char *vendor_name,
                                       const char *product_name,
                                       unsigned int domain);

int EtherCAT_rt_activate_slaves(ec_master_t *master);

int EtherCAT_rt_deactivate_slaves(ec_master_t *master);

int EtherCAT_rt_exchange_io(ec_master_t *master, unsigned int domain,
                            unsigned int timeout_us);

/*****************************************************************************/

/**
   EtherCAT-Slave
*/

struct ec_slave
{
    // Base data
    unsigned char base_type; /**< Slave-Typ */
    unsigned char base_revision; /**< Revision */
    unsigned short base_build; /**< Build-Nummer */

    // Addresses
    short ring_position; /**< (Negative) Position des Slaves im Bus */
    unsigned short station_address; /**< Konfigurierte Slave-Adresse */

    // Slave information interface
    unsigned int sii_vendor_id; /**< Identifikationsnummer des Herstellers */
    unsigned int sii_product_code; /**< Herstellerspezifischer Produktcode */
    unsigned int sii_revision_number; /**< Revisionsnummer */
    unsigned int sii_serial_number; /**< Seriennummer der Klemme */

    const ec_slave_type_t *type; /**< Zeiger auf die Beschreibung
                                    des Slave-Typs */

    unsigned int logical_address; /**< Konfigurierte, logische Adresse */

    void *process_data; /**< Zeiger auf den Speicherbereich
                           innerhalb eines Prozessdatenobjekts */
    void *private_data; /**< Zeiger auf privaten Datenbereich */
    int (*configure)(ec_slave_t *); /**< Zeiger auf die Funktion zur
                                     Konfiguration */

    unsigned char registered; /**< Der Slave wurde registriert */

    unsigned int domain; /**< Prozessdatendomäne */

    int error_reported; /**< Ein Zugriffsfehler wurde bereits gemeldet */
};

/*****************************************************************************/

#endif
