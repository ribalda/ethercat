/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  The right to use EtherCAT Technology is granted and comes free of
 *  charge under condition of compatibility of product made by
 *  Licensee. People intending to distribute/sell products based on the
 *  code, have to sign an agreement to guarantee that products using
 *  software based on IgH EtherCAT master stay compatible with the actual
 *  EtherCAT specification (which are released themselves as an open
 *  standard) as the (only) precondition to have the right to use EtherCAT
 *  Technology, IP and trade marks.
 *
 *****************************************************************************/

/** \file
 *
 * EtherCAT Realtime Interface.
 *
 * \defgroup RealtimeInterface EtherCAT Realtime Interface
 *
 * EtherCAT interface for realtime modules. This interface is designed for
 * realtime modules that want to use EtherCAT. There are functions to request
 * a master, to map process data, to communicate with slaves via CoE and to
 * configure and activate the bus.
 *
 * Changes in Version 1.4:
 *
 * - Replaced ec_slave_t with ec_slave_config_t, separating the slave objects
 *   from the requested bus configuration. Therefore, renamed
 *   ecrt_master_get_slave() to ecrt_master_slave_config().
 * - Replaced slave address string with alias and position values. See
 *   ecrt_master_slave_config().
 * - Removed ecrt_master_get_slave_by_pos(), because it is no longer
 *   necessary due to alias/position addressing.
 * - Added ec_slave_config_state_t for the new method
 *   ecrt_slave_config_state().
 * - Process data memory for a domain can now be allocated externally. This
 *   offers the possibility to use a shared-memory region. Therefore,
 *   added the domain methods ecrt_domain_size() and
 *   ecrt_domain_external_memory().
 * - Replaced the process data pointers in the Pdo entry registration
 *   functions with a process data offset, that is now returned by
 *   ecrt_slave_config_reg_pdo_entry(). This was necessary for the external
 *   domain memory. An additional advantage is, that the returned offset value
 *   is directly usable. If the domain's process data is allocated internally,
 *   the start address can be retrieved with ecrt_domain_data().
 * - Replaced ecrt_slave_pdo_mapping/add/clear() with
 *   ecrt_slave_config_pdo() to add a Pdo to the mapping and
 *   ecrt_slave_config_pdo_entry() to add a Pdo entry to a Pdo configuration.
 *   ecrt_slave_config_mapping() is a convenience function for
 *   both, that uses the new data types ec_pdo_info_t and ec_pdo_entry_info_t.
 *   Mapped Pdo entries can now immediately be registered.
 * - Renamed ec_bus_status_t, ec_master_status_t to ec_bus_state_t and
 *   ec_master_state_t, respectively. Renamed ecrt_master_get_status() to
 *   ecrt_master_state(), for consistency reasons.
 * - Added ec_domain_state_t and ec_wc_state_t for a new output parameter
 *   of ecrt_domain_state(). The domain state object does now contain
 *   information, if the process data was exchanged completely.
 * - Former "Pdo registration" meant Pdo entry registration in fact, therefore
 *   renamed ec_pdo_reg_t to ec_pdo_entry_reg_t and ecrt_domain_register_pdo()
 *   to ecrt_slave_config_reg_pdo_entry().
 * - Removed ecrt_domain_register_pdo_range(), because it's functionality can
 *   be reached by specifying an explicit Pdo mapping and registering those
 *   Pdo entries.
 * - Added an Sdo access interface, working with Sdo requests. These can be
 *   scheduled for reading and writing during realtime operation.
 *
 * @{
 */

/*****************************************************************************/

#ifndef __ECRT_H__
#define __ECRT_H__

#include <asm/byteorder.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/******************************************************************************
 * Global definitions
 *****************************************************************************/

/** EtherCAT realtime interface major version number.
 */
#define ECRT_VER_MAJOR 1

/** EtherCAT realtime interface minor version number.
 */
#define ECRT_VER_MINOR 4

/** EtherCAT realtime interface version word generator.
 */
#define ECRT_VERSION(a, b) (((a) << 8) + (b))

/** EtherCAT realtime interface version word.
 */
#define ECRT_VERSION_MAGIC ECRT_VERSION(ECRT_VER_MAJOR, ECRT_VER_MINOR)

/******************************************************************************
 * Data types 
 *****************************************************************************/

struct ec_master;
typedef struct ec_master ec_master_t; /**< \see ec_master */

struct ec_slave_config;
typedef struct ec_slave_config ec_slave_config_t; /**< \see ec_slave_config */

struct ec_domain;
typedef struct ec_domain ec_domain_t; /**< \see ec_domain */

struct ec_sdo_request;
typedef struct ec_sdo_request ec_sdo_request_t; /**< \see ec_sdo_request. */

/*****************************************************************************/

/** Bus state.
 *
 * This is used in ec_master_state_t.
 */
typedef enum {
    EC_BUS_FAILURE = -1, /**< At least one configured slave is offline. */
    EC_BUS_OK            /**< All configured slaves are online. */
} ec_bus_state_t;

/*****************************************************************************/

/** Master state.
 *
 * This is used for the output parameter of ecrt_master_state().
 */
typedef struct {
    ec_bus_state_t bus_state; /**< \see ec_bus_state_t */
    unsigned int bus_tainted; /**< Non-zero, if the bus topology differs from
                                the requested configuration. */
    unsigned int slaves_responding; /**< Number of slaves in the bus. */
} ec_master_state_t;

/*****************************************************************************/

/** Slave configuration state.
 *
 * \see ecrt_slave_config_state().
 */
typedef struct  {
    unsigned int online : 1; /**< The slave is online. */
    unsigned int configured : 1; /**< The slave was configured according to
                                   the specified configuration. */
} ec_slave_config_state_t;

/*****************************************************************************/

/** Domain working counter interpretation.
 *
 * This is used in ec_domain_state_t.
 */
typedef enum {
    EC_WC_ZERO = 0,   /**< No Pdos were exchanged. */
    EC_WC_INCOMPLETE, /**< Some of the registered Pdos were exchanged. */
    EC_WC_COMPLETE    /**< All registered Pdos were exchanged. */
} ec_wc_state_t;

/*****************************************************************************/

/** Domain state.
 *
 * This is used for the output parameter of ecrt_domain_state().
 */
typedef struct {
    unsigned int working_counter; /**< Value of the last working counter. */
    ec_wc_state_t wc_state; /**< Working counter interpretation. */
} ec_domain_state_t;

/*****************************************************************************/

/** Direction type for Pdo mapping functions.
 */
typedef enum {
    EC_DIR_OUTPUT, /**< Values written by the master. */
    EC_DIR_INPUT   /**< Values read by the master. */
} ec_direction_t;

/*****************************************************************************/

/** Pdo entry mapping.
 *
 * \see ecrt_slave_config_mapping().
 */
typedef struct {
    uint16_t index; /**< Index of the Pdo entry to add to the Pdo
                            configuration. */
    uint8_t subindex; /**< Subindex of the Pdo entry to add to the
                              Pdo configuration. */
    uint8_t bit_length; /**< Size of the Pdo entry in bit. */
} ec_pdo_entry_info_t;

/*****************************************************************************/

/** Pdo information.
 *
 * \see ecrt_slave_config_mapping().
 */
typedef struct {
    ec_direction_t dir; /**< Pdo direction (input/output). */
    uint16_t index; /**< Index of the Pdo to map. */
    unsigned int n_entries; /**< Number of Pdo entries for the Pdo
                              configuration. Zero means, that the default Pdo
                              configuration shall be used. */
    ec_pdo_entry_info_t *entries; /**< Pdo configuration array. This
                                    array must contain at least \a
                                    n_entries values. */
} ec_pdo_info_t;

/*****************************************************************************/

/** List record type for Pdo entry mass-registration.
 *
 * This type is used for the array parameter of the
 * ecrt_domain_reg_pdo_entry_list() convenience function.
 */
typedef struct {
    uint16_t alias; /**< Slave alias address. */
    uint16_t position; /**< Slave position. */
    uint32_t vendor_id; /**< Slave vendor ID. */
    uint32_t product_code; /**< Slave product code. */
    uint16_t index; /**< Pdo entry index. */
    uint8_t subindex; /**< Pdo entry subindex. */
    unsigned int *offset; /**< Pointer to a variable to store the Pdo's
                       offset in the process data. */
} ec_pdo_entry_reg_t;

/*****************************************************************************/

/** Sdo request state.
 *
 * This is used as return type of ecrt_sdo_request_state().
 */
typedef enum {
    EC_SDO_REQUEST_UNUSED, /**< Not requested. */
    EC_SDO_REQUEST_BUSY, /**< Request is being processed. */
    EC_SDO_REQUEST_SUCCESS, /**< Request was processed successfully. */
    EC_SDO_REQUEST_ERROR, /**< Request processing failed. */
} ec_sdo_request_state_t;

/******************************************************************************
 * Global functions
 *****************************************************************************/

/** Returns the version magic of the realtime interface.
 *
 * \return Value of ECRT_VERSION_MAGIC() at EtherCAT master compile time.
 */
unsigned int ecrt_version_magic(void);

/** Requests an EtherCAT master for realtime operation.
 * 
 * \return pointer to reserved master, or NULL on error
 */
ec_master_t *ecrt_request_master(
        unsigned int master_index /**< Index of the master to request. */
        );

/** Releases a requested EtherCAT master.
 */
void ecrt_release_master(
        ec_master_t *master /**< EtherCAT master */
        );

/******************************************************************************
 * Master methods
 *****************************************************************************/

/** Sets the locking callbacks.
 *
 * The request_cb function must return zero, to allow another instance
 * (the EoE process for example) to access the master. Non-zero means,
 * that access is forbidden at this time.
 */
void ecrt_master_callbacks(
        ec_master_t *master, /**< EtherCAT master */
        int (*request_cb)(void *), /**< Lock request function. */
        void (*release_cb)(void *), /**< Lock release function. */
        void *cb_data /**< Arbitrary user data. */
        );

/** Creates a new domain.
 *
 * \return Pointer to the new domain on success, else NULL.
 */
ec_domain_t *ecrt_master_create_domain(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Obtains a slave configuration.
 *
 * Creates a slave configuration object for the given \a alias and \a position
 * tuple and returns it. If a configuration with the same \a alias and \a
 * position already exists, it will be re-used. In the latter case, the given
 * vendor ID and product code are compared to the stored ones. On mismatch, an
 * error message is raised and the function returns \a NULL.
 *
 * Slaves are addressed with the \a alias and \a position parameters.
 * - If \a alias is zero, \a position is interpreted as the desired slave's
 *   ring position.
 * - If \a alias is non-zero, it matches a slave with the given alias. In this
 *   case, \a position is interpreted as ring offset, starting from the
 *   aliased slave, so a position of zero means the aliased slave itself and a
 *   positive value matches the n-th slave behind the aliased one.
 *
 * If the slave with the given address is found during the bus configuration,
 * its vendor ID and product code are matched against the given value. On
 * mismatch, the slave is not configured and an error message is raised.
 *
 * If different slave configurations are pointing to the same slave during bus
 * configuration, a warning is raised and only the first configuration is
 * applied.
 *
 * \retval >0 Pointer to the slave configuration structure.
 * \retval NULL in the error case.
 */
ec_slave_config_t *ecrt_master_slave_config(
        ec_master_t *master, /**< EtherCAT master */
        uint16_t alias, /**< Slave alias. */
        uint16_t position, /**< Slave position. */
        uint32_t vendor_id, /**< Expected vendor ID. */
        uint32_t product_code /**< Expected product code. */
        );

/** Applies the bus configuration and switches to realtime mode.
 *
 * Does the complete configuration and activation for all slaves. Sets sync
 * managers and FMMUs, and does the appropriate transitions, until the slave
 * is operational.
 *
 * \return 0 in case of success, else < 0
 */
int ecrt_master_activate(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Sends all datagrams in the queue.
 *
 * \todo doc
 */
void ecrt_master_send(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Fetches received frames from the hardware and processes the datagrams.
 */
void ecrt_master_receive(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Reads the current master state.
 *
 * Stores the master state information in the given \a state structure.
 */
void ecrt_master_state(
        const ec_master_t *master, /**< EtherCAT master. */
        ec_master_state_t *state /**< Structure to store the information. */
        );

/******************************************************************************
 * Slave configuration methods
 *****************************************************************************/

/** Add a Pdo to the slave's Pdo mapping for the given direction.
 *
 * The first call of this function for a given \a dir will clear the default
 * mapping.
 *
 * \see ecrt_slave_config_mapping()
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_pdo(
        ec_slave_config_t *sc, /**< Slave configuration. */
        ec_direction_t dir, /**< Pdo direction (input/output). */
        uint16_t index /**< Index of the Pdo to map. */
        );

/** Add a Pdo entry to the given Pdo's configuration.
 *
 * The first call of this function for a given \a pdo_index will clear the
 * default Pdo configuration.
 *
 * \see ecrt_slave_config_mapping()
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_pdo_entry(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t pdo_index, /**< Index of the Pdo to configure. */
        uint16_t entry_index, /**< Index of the Pdo entry to add to the Pdo's
                                configuration. */
        uint8_t entry_subindex, /**< Subindex of the Pdo entry to add to the
                                  Pdo's configuration. */
        uint8_t entry_bit_length /**< Size of the Pdo entry in bit. */
        );

/** Specify the Pdo mapping and (optionally) the Pdo configuration.
 *
 * This function is a convenience function for the ecrt_slave_config_pdo()
 * and ecrt_slave_config_pdo_entry() functions, that are better suitable
 * for automatic code generation.
 *
 * The following example shows, how to specify a complete Pdo mapping
 * including the Pdo configuration. With this information, the master is able
 * to reserve the complete process data, even if the slave is not present
 * at configuration time:
 *
 * \code
 * const ec_pdo_entry_info_t el3162_channel1[] = {
 *     {0x3101, 1,  8}, // status
 *     {0x3101, 2, 16}  // value
 * };
 * 
 * const ec_pdo_entry_info_t el3162_channel2[] = {
 *     {0x3102, 1,  8}, // status
 *     {0x3102, 2, 16}  // value
 * };
 * 
 * const ec_pdo_info_t el3162_mapping[] = {
 *     {EC_DIR_INPUT, 0x1A00, 2, el3162_channel1},
 *     {EC_DIR_INPUT, 0x1A01, 2, el3162_channel2},
 * };
 * 
 * if (ecrt_slave_config_mapping(sc, 2, el3162_mapping))
 *     return -1; // error
 * \endcode
 *
 * The next example shows, how to configure only the Pdo mapping. The entries
 * for each mapped Pdo are taken from the default Pdo configuration. Please
 * note, that Pdo entry registration will fail, if the Pdo configuration is
 * left empty and the slave is offline.
 *
 * \code
 * const ec_pdo_info_t pdo_mapping[] = {
 *     {EC_DIR_INPUT, 0x1600}, // Channel 1
 *     {EC_DIR_INPUT, 0x1601}  // Channel 2
 * };
 * 
 * if (ecrt_slave_config_mapping(slave_config_ana_in, 2, pdo_mapping))
 *     return -1; // error
 * \endcode
 *
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_mapping(
        ec_slave_config_t *sc, /**< Slave configuration. */
        unsigned int n_infos, /**< Number of Pdo infos in \a pdo_infos. */
        const ec_pdo_info_t pdo_infos[] /**< List with Pdo mapping. */
        );

/** Registers a Pdo entry for process data exchange in a domain.
 *
 * Searches the current mapping and Pdo configurations for the given Pdo
 * entry. An error is raised, if the given entry is not mapped. Otherwise, the
 * corresponding sync manager and FMMU configurations are provided for slave
 * configuration and the respective sync manager's Pdos are appended to the
 * given domain, if not already done. The offset of the requested Pdo entry's
 * data inside the domain's process data is returned.
 *
 * \retval >=0 Success: Offset of the Pdo entry's process data.
 * \retval -1  Error: Pdo entry not found.
 * \retval -2  Error: Failed to register Pdo entry.
 */
int ecrt_slave_config_reg_pdo_entry(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t entry_index, /**< Index of the Pdo entry to register. */
        uint8_t entry_subindex, /**< Subindex of the Pdo entry to register. */
        ec_domain_t *domain /**< Domain. */
        );

/** Add a configuration value for an 8-bit SDO.
 *
 * \todo doc
 * \return 0 in case of success, else < 0
 */
int ecrt_slave_config_sdo8(
        ec_slave_config_t *sc, /**< Slave configuration */
        uint16_t sdo_index, /**< Index of the SDO to configure. */
        uint8_t sdo_subindex, /**< Subindex of the SDO to configure. */
        uint8_t value /**< Value to set. */
        );

/** Add a configuration value for a 16-bit SDO.
 *
 * \todo doc
 * \return 0 in case of success, else < 0
 */
int ecrt_slave_config_sdo16(
        ec_slave_config_t *sc, /**< Slave configuration */
        uint16_t sdo_index, /**< Index of the SDO to configure. */
        uint8_t sdo_subindex, /**< Subindex of the SDO to configure. */
        uint16_t value /**< Value to set. */
        );

/** Add a configuration value for a 32-bit SDO.
 *
 * \todo doc
 * \return 0 in case of success, else < 0
 */
int ecrt_slave_config_sdo32(
        ec_slave_config_t *sc, /**< Slave configuration */
        uint16_t sdo_index, /**< Index of the SDO to configure. */
        uint8_t sdo_subindex, /**< Subindex of the SDO to configure. */
        uint32_t value /**< Value to set. */
        );

/** Create an Sdo request to exchange Sdos during realtime operation.
 *
 * The created Sdo request object is freed automatically when the master is
 * released.
 */
ec_sdo_request_t *ecrt_slave_config_create_sdo_request(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t index, /**< Sdo index. */
        uint8_t subindex, /**< Sdo subindex. */
        size_t size /**< Data size to reserve. */
        );

/** Outputs the state of the slave configuration.
 *
 * Stores the state information in the given \a state structure.
 */
void ecrt_slave_config_state(
        const ec_slave_config_t *sc, /**< Slave configuration */
        ec_slave_config_state_t *state /**< State object to write to. */
        );

/******************************************************************************
 * Domain methods
 *****************************************************************************/

/** Registers a bunch of Pdo entries for a domain.
 *
 * \todo doc
 * \attention The registration array has to be terminated with an empty
 *            structure, or one with the \a index field set to zero!
 * \return 0 on success, else non-zero.
 */
int ecrt_domain_reg_pdo_entry_list(
        ec_domain_t *domain, /**< Domain. */
        const ec_pdo_entry_reg_t *pdo_entry_regs /**< Array of Pdo
                                                   registrations. */
        );

/** Returns the current size of the domain's process data.
 *
 * \return Size of the process data image.
 */
size_t ecrt_domain_size(
        ec_domain_t *domain /**< Domain. */
        );

/** Provide external memory to store the domain's process data.
 *
 * Call this after all Pdo entries have been registered and before activating
 * the master.
 *
 * The size of the allocated memory must be at least ecrt_domain_size(), after
 * all Pdo entries have been registered.
 */
void ecrt_domain_external_memory(
        ec_domain_t *domain, /**< Domain. */
        uint8_t *memory /**< Address of the memory to store the process
                          data in. */
        );

/** Returns the domain's process data.
 *
 * If external memory was provided with ecrt_domain_external_memory(), the
 * returned pointer will contain the address of that memory. Otherwise it will
 * point to the internally allocated memory.
 *
 * \return Pointer to the process data memory.
 */
uint8_t *ecrt_domain_data(
        ec_domain_t *domain /**< Domain. */
        );

/** Processes received datagrams.
 *
 * \todo doc
 */
void ecrt_domain_process(
        ec_domain_t *domain /**< Domain. */
        );

/** (Re-)queues all domain datagrams in the master's datagram queue.
 *
 * \todo doc
 */
void ecrt_domain_queue(
        ec_domain_t *domain /**< Domain. */
        );

/** Reads the state of a domain.
 *
 * Stores the domain state in the giveb \a state structure.
 */
void ecrt_domain_state(
        const ec_domain_t *domain, /**< Domain. */
        ec_domain_state_t *state /**< Pointer to a state object to store the
                                   information. */
        );

/*****************************************************************************
 * Sdo request methods.
 ****************************************************************************/

/** Set the timeout for an Sdo request.
 *
 * If the request cannot be processed in the specified time, if will be marked
 * as failed.
 *
 * \todo The timeout functionality is not yet implemented.
 */
void ecrt_sdo_request_timeout(
        ec_sdo_request_t *req, /**< Sdo request. */
        uint32_t timeout /**< Timeout in milliseconds. */
        );

/** Access to the Sdo request's data.
 *
 * \attention The return value can be invalid during a read operation, because
 * the internal Sdo data memory could be re-allocated if the read Sdo data do
 * not fit inside.
 *
 * \return Pointer to the internal Sdo data memory.
 */
uint8_t *ecrt_sdo_request_data(
        ec_sdo_request_t *req /**< Sdo request. */
        );

/** Get the current state of the Sdo request.
 *
 * \return Request state.
 */
ec_sdo_request_state_t ecrt_sdo_request_state(
    const ec_sdo_request_t *req /**< Sdo request. */
    );

/** Schedule an Sdo write operation.
 *
 * \attention This method may not be called while ecrt_sdo_request_state()
 * returns EC_SDO_REQUEST_BUSY.
 */
void ecrt_sdo_request_write(
        ec_sdo_request_t *req /**< Sdo request. */
        );

/** Schedule an Sdo read operation.
 *
 * \attention This method may not be called while ecrt_sdo_request_state()
 * returns EC_SDO_REQUEST_BUSY.
 *
 * \attention After calling this function, the return value of
 * ecrt_sdo_request_data() must be considered as invalid while
 * ecrt_sdo_request_state() returns EC_SDO_REQUEST_BUSY.
 */
void ecrt_sdo_request_read(
        ec_sdo_request_t *req /**< Sdo request. */
        );

/******************************************************************************
 * Bitwise read/write macros
 *****************************************************************************/

/** Read a certain bit of an EtherCAT data byte.
 * \param DATA EtherCAT data pointer
 * \param POS bit position
 */
#define EC_READ_BIT(DATA, POS) ((*((uint8_t *) (DATA)) >> (POS)) & 0x01)

/** Write a certain bit of an EtherCAT data byte.
 * \param DATA EtherCAT data pointer
 * \param POS bit position
 * \param VAL new bit value
 */
#define EC_WRITE_BIT(DATA, POS, VAL) \
    do { \
        if (VAL) *((uint8_t *) (DATA)) |=  (1 << (POS)); \
        else     *((uint8_t *) (DATA)) &= ~(1 << (POS)); \
    } while (0)

/******************************************************************************
 * Read macros
 *****************************************************************************/

/** Read an 8-bit unsigned value from EtherCAT data.
 * \return EtherCAT data value
 */
#define EC_READ_U8(DATA) \
    ((uint8_t) *((uint8_t *) (DATA)))

/** Read an 8-bit signed value from EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S8(DATA) \
     ((int8_t) *((uint8_t *) (DATA)))

/** Read a 16-bit unsigned value from EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_U16(DATA) \
     ((uint16_t) le16_to_cpup((void *) (DATA)))

/** Read a 16-bit signed value from EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S16(DATA) \
     ((int16_t) le16_to_cpup((void *) (DATA)))

/** Read a 32-bit unsigned value from EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_U32(DATA) \
     ((uint32_t) le32_to_cpup((void *) (DATA)))

/** Read a 32-bit signed value from EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S32(DATA) \
     ((int32_t) le32_to_cpup((void *) (DATA)))

/******************************************************************************
 * Write macros
 *****************************************************************************/

/** Write an 8-bit unsigned value to EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U8(DATA, VAL) \
    do { \
        *((uint8_t *)(DATA)) = ((uint8_t) (VAL)); \
    } while (0)

/** Write an 8-bit signed value to EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S8(DATA, VAL) EC_WRITE_U8(DATA, VAL)

/** Write a 16-bit unsigned value to EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U16(DATA, VAL) \
    do { \
        *((uint16_t *) (DATA)) = (uint16_t) (VAL); \
        cpu_to_le16s((uint16_t *) (DATA)); \
    } while (0)

/** Write a 16-bit signed value to EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S16(DATA, VAL) EC_WRITE_U16(DATA, VAL)

/** Write a 32-bit unsigned value to EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U32(DATA, VAL) \
    do { \
        *((uint32_t *) (DATA)) = (uint32_t) (VAL); \
        cpu_to_le32s((uint32_t *) (DATA)); \
    } while (0)

/** Write a 32-bit signed value to EtherCAT data.
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S32(DATA, VAL) EC_WRITE_U32(DATA, VAL)

/*****************************************************************************/

/** @} */

#endif
