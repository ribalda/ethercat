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
 * EtherCAT master application interface.
 *
 * \defgroup ApplicationInterface EtherCAT Application Interface
 *
 * EtherCAT interface for realtime applications. This interface is designed
 * for realtime modules that want to use EtherCAT. There are functions to
 * request a master, to map process data, to communicate with slaves via CoE
 * and to configure and activate the bus.
 *
 * Changes in version 1.5:
 *
 * - Changed the return value of ecrt_request_master().
 *
 * Changes in Version 1.4:
 *
 * - Replaced ec_slave_t with ec_slave_config_t, separating the bus
 *   configuration from the actual slaves. Therefore, renamed
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
 * - Pdo entry registration functions do not return a process data pointer,
 *   but an offset in the domain's process data. In addition, an optional bit
 *   position can be requested. This was necessary for the external domain
 *   memory. An additional advantage is, that the returned offset is
 *   immediately valid. If the domain's process data is allocated internally,
 *   the start address can be retrieved with ecrt_domain_data().
 * - Replaced ecrt_slave_pdo_mapping/add/clear() with
 *   ecrt_slave_config_pdo_assign_add() to add a Pdo to a sync manager's Pdo
 *   assignment and ecrt_slave_config_pdo_mapping_add() to add a Pdo entry to a
 *   Pdo's mapping. ecrt_slave_config_pdos() is a convenience function
 *   for both, that uses the new data types ec_pdo_info_t and
 *   ec_pdo_entry_info_t. Pdo entries, that are mapped with these functions
 *   can now immediately be registered, even if the bus is offline.
 * - Renamed ec_bus_status_t, ec_master_status_t to ec_bus_state_t and
 *   ec_master_state_t, respectively. Renamed ecrt_master_get_status() to
 *   ecrt_master_state(), for consistency reasons.
 * - Added ec_domain_state_t and #ec_wc_state_t for a new output parameter
 *   of ecrt_domain_state(). The domain state object does now contain
 *   information, if the process data was exchanged completely.
 * - Former "Pdo registration" meant Pdo entry registration in fact, therefore
 *   renamed ec_pdo_reg_t to ec_pdo_entry_reg_t and ecrt_domain_register_pdo()
 *   to ecrt_slave_config_reg_pdo_entry().
 * - Removed ecrt_domain_register_pdo_range(), because it's functionality can
 *   be reached by specifying an explicit Pdo assignment/mapping and
 *   registering the mapped Pdo entries.
 * - Added an Sdo access interface, working with Sdo requests. These can be
 *   scheduled for reading and writing during realtime operation.
 * - Exported ecrt_slave_config_sdo(), the generic Sdo configuration function.
 * - Removed the bus_state and bus_tainted flags from ec_master_state_t.
 *
 * @{
 */

/*****************************************************************************/

#ifndef __ECRT_H__
#define __ECRT_H__

#ifdef __KERNEL__
#include <asm/byteorder.h>
#include <linux/types.h>
#else
#include <stdlib.h> // for size_t
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
#define ECRT_VER_MINOR 5

/** EtherCAT realtime interface version word generator.
 */
#define ECRT_VERSION(a, b) (((a) << 8) + (b))

/** EtherCAT realtime interface version word.
 */
#define ECRT_VERSION_MAGIC ECRT_VERSION(ECRT_VER_MAJOR, ECRT_VER_MINOR)

/*****************************************************************************/

/** End of list marker.
 *
 * This can be used with ecrt_slave_config_pdos().
 */
#define EC_END ~0U

/** Maximum number of sync managers per slave.
 */
#define EC_MAX_SYNC_MANAGERS 16

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

struct ec_voe_handler;
typedef struct ec_voe_handler ec_voe_handler_t; /**< \see ec_voe_handler. */

/*****************************************************************************/

/** Master state.
 *
 * This is used for the output parameter of ecrt_master_state().
 *
 * \see ecrt_master_state().
 */
typedef struct {
    unsigned int slaves_responding; /**< Number of slaves in the bus. */
    unsigned int al_states : 4; /**< Application-layer states of all slaves.
                                  The states are coded in the lower 4 bits.
                                  If a bit is set, it means that at least one
                                  slave in the bus is in the corresponding
                                  state:
                                  - Bit 0: \a INIT
                                  - Bit 1: \a PREOP
                                  - Bit 2: \a SAFEOP
                                  - Bit 3: \a OP */
    unsigned int link_up : 1; /**< \a true, if the network link is up. */
} ec_master_state_t;

/*****************************************************************************/

/** Slave configuration state.
 *
 * This is used as an output parameter of ecrt_slave_config_state().
 * 
 * \see ecrt_slave_config_state().
 */
typedef struct  {
    unsigned int online : 1; /**< The slave is online. */
    unsigned int operational : 1; /**< The slave was brought into \a OP state
                                    using the specified configuration. */
    unsigned int al_state : 4; /**< The application-layer state of the slave.
                                 - 1: \a INIT
                                 - 2: \a PREOP
                                 - 4: \a SAFEOP
                                 - 8: \a OP

                                 Note that each state is coded in a different
                                 bit! */
} ec_slave_config_state_t;

/*****************************************************************************/

/** Domain working counter interpretation.
 *
 * This is used in ec_domain_state_t.
 */
typedef enum {
    EC_WC_ZERO = 0,   /**< No registered process data were exchanged. */
    EC_WC_INCOMPLETE, /**< Some of the registered process data were
                        exchanged. */
    EC_WC_COMPLETE    /**< All registered process data were exchanged. */
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

/** Direction type for Pdo assignment functions.
 */
typedef enum {
    EC_DIR_INVALID, /**< Invalid direction. Do not use this value. */
    EC_DIR_OUTPUT, /**< Values written by the master. */
    EC_DIR_INPUT, /**< Values read by the master. */
    EC_DIR_COUNT /**< Number of directions. For internal use only. */
} ec_direction_t;

/*****************************************************************************/

/** Pdo entry configuration information.
 *
 * This is the data type of the \a entries field in ec_pdo_info_t.
 *
 * \see ecrt_slave_config_pdos().
 */
typedef struct {
    uint16_t index; /**< Pdo entry index. */
    uint8_t subindex; /**< Pdo entry subindex. */
    uint8_t bit_length; /**< Size of the Pdo entry in bit. */
} ec_pdo_entry_info_t;

/*****************************************************************************/

/** Pdo configuration information.
 * 
 * This is the data type of the \a pdos field in ec_sync_info_t.
 * 
 * \see ecrt_slave_config_pdos().
 */
typedef struct {
    uint16_t index; /**< Pdo index. */
    unsigned int n_entries; /**< Number of Pdo entries in \a entries to map.
                              Zero means, that the default mapping shall be
                              used (this can only be done if the slave is
                              present at bus configuration time). */
    ec_pdo_entry_info_t *entries; /**< Array of Pdo entries to map. Can either
                                    be \a NULL, or must contain at
                                    least \a n_entries values. */
} ec_pdo_info_t;

/*****************************************************************************/

/** Sync manager configuration information.
 *
 * This can be use to configure multiple sync managers including the Pdo
 * assignment and Pdo mapping. It is used as an input parameter type in
 * ecrt_slave_config_pdos().
 */
typedef struct {
    uint8_t index; /**< Sync manager index. Must be less
                     than #EC_MAX_SYNC_MANAGERS for a valid sync manager,
                     but can also be \a 0xff to mark the end of the list. */
    ec_direction_t dir; /**< Sync manager direction. */
    unsigned int n_pdos; /**< Number of Pdos in \a pdos. */
    ec_pdo_info_t *pdos; /**< Array with Pdos to assign. This must contain
                            at least \a n_pdos Pdos. */
} ec_sync_info_t;

/*****************************************************************************/

/** List record type for Pdo entry mass-registration.
 *
 * This type is used for the array parameter of the
 * ecrt_domain_reg_pdo_entry_list()
 */
typedef struct {
    uint16_t alias; /**< Slave alias address. */
    uint16_t position; /**< Slave position. */
    uint32_t vendor_id; /**< Slave vendor ID. */
    uint32_t product_code; /**< Slave product code. */
    uint16_t index; /**< Pdo entry index. */
    uint8_t subindex; /**< Pdo entry subindex. */
    unsigned int *offset; /**< Pointer to a variable to store the Pdo entry's
                       (byte-)offset in the process data. */
    unsigned int *bit_position; /**< Pointer to a variable to store a bit 
                                  position (0-7) within the \a offset. Can be
                                  NULL, in which case an error is raised if the
                                  Pdo entry does not byte-align. */
} ec_pdo_entry_reg_t;

/*****************************************************************************/

/** Request state.
 *
 * This is used as return type for ecrt_sdo_request_state() and
 * ecrt_voe_handler_state().
 */
typedef enum {
    EC_REQUEST_UNUSED, /**< Not requested. */
    EC_REQUEST_BUSY, /**< Request is being processed. */
    EC_REQUEST_SUCCESS, /**< Request was processed successfully. */
    EC_REQUEST_ERROR, /**< Request processing failed. */
} ec_request_state_t;

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
 * Before an application can access an EtherCAT master, it has to reserve one
 * for exclusive use.
 *
 * This function has to be the first function an application has to call to
 * use EtherCAT. The function takes the index of the master as its argument.
 * The first master has index 0, the n-th master has index n - 1. The number
 * of masters has to be specified when loading the master module.
 *
 * \attention In kernel context, the returned pointer has to be checked for
 * errors using the IS_ERR() macro.
 *
 * \return If \a IS_ERR() returns zero, the result is a pointer to the
 * reserved master, otherwise, the result is an error code.
 */
ec_master_t *ecrt_request_master(
        unsigned int master_index /**< Index of the master to request. */
        );

/** Releases a requested EtherCAT master.
 *
 * After use, a master it has to be released to make it available for other
 * applications.
 */
void ecrt_release_master(
        ec_master_t *master /**< EtherCAT master */
        );

/******************************************************************************
 * Master methods
 *****************************************************************************/

#ifdef __KERNEL__

/** Sets the locking callbacks.
 *
 * For concurrent master access, the application has to provide a locking
 * mechanism (see section FIXME in the docs). The method takes two function
 * pointers and a data value as its parameters. The arbitrary \a cb_data value
 * will be passed as argument on every callback. Asynchronous master access
 * (like EoE processing) is only possible if the callbacks have been set.
 *
 * The request_cb function must return zero, to allow another instance
 * (an EoE process for example) to access the master. Non-zero means,
 * that access is currently forbidden.
 */
void ecrt_master_callbacks(
        ec_master_t *master, /**< EtherCAT master */
        int (*request_cb)(void *), /**< Lock request function. */
        void (*release_cb)(void *), /**< Lock release function. */
        void *cb_data /**< Arbitrary user data. */
        );

#endif /* __KERNEL__ */

/** Creates a new process data domain.
 *
 * For process data exchange, at least one process data domain is needed.
 * This method creates a new process data domain and returns a pointer to the
 * new domain object. This object can be used for registering Pdos and
 * exchanging them in cyclic operation.
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

/** Finishes the configuration phase and prepares for cyclic operation.
 *
 * This function tells the master that the configuration phase is finished and
 * the realtime operation will begin. The function allocates internal memory
 * for the domains and calculates the logical FMMU addresses for domain
 * members. It tells the master state machine that the bus configuration is
 * now to be applied.
 *
 * \attention After this function has been called, the realtime application is
 * in charge of cyclically calling ecrt_master_send() and
 * ecrt_master_receive() to ensure bus communication. Before calling this
 * function, the master thread is responsible for that, so these functions may
 * not be called!
 *
 * \return 0 in case of success, else < 0
 */
int ecrt_master_activate(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Sends all datagrams in the queue.
 *
 * This method takes all datagrams, that have been queued for transmission,
 * puts them into frames, and passes them to the Ethernet device for sending.
 *
 * Has to be called cyclically by the application after ecrt_master_activate()
 * has returned.
 */
void ecrt_master_send(
        ec_master_t *master /**< EtherCAT master. */
        );

/** Fetches received frames from the hardware and processes the datagrams.
 *
 * Queries the network device for received frames by calling the interrupt
 * service routine. Extracts received datagrams and dispatches the results to
 * the datagram objects in the queue. Received datagrams, and the ones that
 * timed out, will be marked, and dequeued.
 *
 * Has to be called cyclically by the realtime application after
 * ecrt_master_activate() has returned.
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

/** Configure a sync manager.
 *
 * Sets the direction of a sync manager. This overrides the direction bits
 * from the default control register from SII.
 *
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_sync_manager(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t sync_index, /**< Sync manager index. Must be less
                              than #EC_MAX_SYNC_MANAGERS. */
        ec_direction_t dir /**< Input/Output. */
        );

/** Add a Pdo to a sync manager's Pdo assignment.
 *
 * \see ecrt_slave_config_pdos()
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_pdo_assign_add(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t sync_index, /**< Sync manager index. Must be less
                              than #EC_MAX_SYNC_MANAGERS. */
        uint16_t index /**< Index of the Pdo to assign. */
        );

/** Clear a sync manager's Pdo assignment.
 *
 * This can be called before assigning Pdos via
 * ecrt_slave_config_pdo_assign_add(), to clear the default assignment of a
 * sync manager.
 * 
 * \see ecrt_slave_config_pdos()
 */
void ecrt_slave_config_pdo_assign_clear(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint8_t sync_index /**< Sync manager index. Must be less
                              than #EC_MAX_SYNC_MANAGERS. */
        );

/** Add a Pdo entry to the given Pdo's mapping.
 *
 * \see ecrt_slave_config_pdos()
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_pdo_mapping_add(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t pdo_index, /**< Index of the Pdo. */
        uint16_t entry_index, /**< Index of the Pdo entry to add to the Pdo's
                                mapping. */
        uint8_t entry_subindex, /**< Subindex of the Pdo entry to add to the
                                  Pdo's mapping. */
        uint8_t entry_bit_length /**< Size of the Pdo entry in bit. */
        );

/** Clear the mapping of a given Pdo.
 *
 * This can be called before mapping Pdo entries via
 * ecrt_slave_config_pdo_mapping_add(), to clear the default mapping.
 *
 * \see ecrt_slave_config_pdos()
 */
void ecrt_slave_config_pdo_mapping_clear(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t pdo_index /**< Index of the Pdo. */
        );

/** Specify a complete Pdo configuration.
 *
 * This function is a convenience wrapper for the functions
 * ecrt_slave_config_sync_manager(), ecrt_slave_config_pdo_assign_clear(),
 * ecrt_slave_config_pdo_assign_add(), ecrt_slave_config_pdo_mapping_clear()
 * and ecrt_slave_config_pdo_mapping_add(), that are better suitable for
 * automatic code generation.
 *
 * The following example shows, how to specify a complete configuration,
 * including the Pdo mappings. With this information, the master is able to
 * reserve the complete process data, even if the slave is not present at
 * configuration time:
 *
 * \code
 * ec_pdo_entry_info_t el3162_channel1[] = {
 *     {0x3101, 1,  8}, // status
 *     {0x3101, 2, 16}  // value
 * };
 * 
 * ec_pdo_entry_info_t el3162_channel2[] = {
 *     {0x3102, 1,  8}, // status
 *     {0x3102, 2, 16}  // value
 * };
 * 
 * ec_pdo_info_t el3162_pdos[] = {
 *     {0x1A00, 2, el3162_channel1},
 *     {0x1A01, 2, el3162_channel2}
 * };
 * 
 * ec_sync_info_t el3162_syncs[] = {
 *     {2, EC_DIR_OUTPUT},
 *     {3, EC_DIR_INPUT, 2, el3162_pdos},
 *     {0xff}
 * };
 * 
 * if (ecrt_slave_config_pdos(sc_ana_in, EC_END, el3162_syncs)) {
 *     // handle error
 * }
 * \endcode
 * 
 * The next example shows, how to configure the Pdo assignment only. The
 * entries for each assigned Pdo are taken from the Pdo's default mapping.
 * Please note, that Pdo entry registration will fail, if the Pdo
 * configuration is left empty and the slave is offline.
 *
 * \code
 * ec_pdo_info_t pdos[] = {
 *     {0x1600}, // Channel 1
 *     {0x1601}  // Channel 2
 * };
 * 
 * ec_sync_info_t syncs[] = {
 *     {3, EC_DIR_INPUT, 2, pdos},
 * };
 * 
 * if (ecrt_slave_config_pdos(slave_config_ana_in, 1, syncs)) {
 *     // handle error
 * }
 * \endcode
 *
 * Processing of \a syncs will stop, if
 * - the number of processed items reaches \a n_syncs, or
 * - the \a index member of an ec_sync_info_t item is 0xff. In this case,
 *   \a n_syncs should set to a number greater than the number of list items;
 *   using EC_END is recommended.
 *
 * \return zero on success, else non-zero
 */
int ecrt_slave_config_pdos(
        ec_slave_config_t *sc, /**< Slave configuration. */
        unsigned int n_syncs, /**< Number of sync manager configurations in
                                \a syncs. */
        const ec_sync_info_t syncs[] /**< Array of sync manager
                                       configurations. */
        );

/** Registers a Pdo entry for process data exchange in a domain.
 *
 * Searches the assigned Pdos for the given Pdo entry. An error is raised, if
 * the given entry is not mapped. Otherwise, the corresponding sync manager
 * and FMMU configurations are provided for slave configuration and the
 * respective sync manager's assigned Pdos are appended to the given domain,
 * if not already done. The offset of the requested Pdo entry's data inside
 * the domain's process data is returned. Optionally, the Pdo entry bit
 * position (0-7) can be retrieved via the \a bit_position output parameter.
 * This pointer may be \a NULL, in this case an error is raised if the Pdo
 * entry does not byte-align.
 *
 * \retval >=0 Success: Offset of the Pdo entry's process data.
 * \retval -1  Error: Pdo entry not found.
 * \retval -2  Error: Failed to register Pdo entry.
 * \retval -3  Error: Pdo entry is not byte-aligned.
 */
int ecrt_slave_config_reg_pdo_entry(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t entry_index, /**< Index of the Pdo entry to register. */
        uint8_t entry_subindex, /**< Subindex of the Pdo entry to register. */
        ec_domain_t *domain, /**< Domain. */
        unsigned int *bit_position /**< Optional address if bit addressing 
                                 is desired */
        );

/** Add an Sdo configuration.
 *
 * An Sdo configuration is stored in the slave configuration object and is
 * downloaded to the slave whenever the slave is being configured by the
 * master. This usually happens once on master activation, but can be repeated
 * subsequently, for example after the slave's power supply failed.
 *
 * \attention The Sdos for Pdo assignment (\p 0x1C10 - \p 0x1C2F) and Pdo
 * mapping (\p 0x1600 - \p 0x17FF and \p 0x1A00 - \p 0x1BFF) should not be
 * configured with this function, because they are part of the slave
 * configuration done by the master. Please use ecrt_slave_config_pdos() and
 * friends instead.
 *
 * This is the generic function for adding an Sdo configuration. Please note
 * that the this function does not do any endianess correction. If
 * datatype-specific functions are needed (that automatically correct the
 * endianess), have a look at ecrt_slave_config_sdo8(),
 * ecrt_slave_config_sdo16() and ecrt_slave_config_sdo32().
 *
 * \return 0 in case of success, else < 0
 */
int ecrt_slave_config_sdo(
        ec_slave_config_t *sc, /**< Slave configuration. */
        uint16_t index, /**< Index of the Sdo to configure. */
        uint8_t subindex, /**< Subindex of the Sdo to configure. */
        const uint8_t *data, /**< Pointer to the data. */
        size_t size /**< Size of the \a data. */
        );

/** Add a configuration value for an 8-bit SDO.
 *
 * \see ecrt_slave_config_sdo().
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
 * \see ecrt_slave_config_sdo().
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
 * \see ecrt_slave_config_sdo().
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

/** Create an VoE handler to exchange vendor-specific data during realtime
 * operation.
 *
 * The created VoE handler object is freed automatically when the master is
 * released.
 */
ec_voe_handler_t *ecrt_slave_config_create_voe_handler(
        ec_slave_config_t *sc, /**< Slave configuration. */
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

#ifdef __KERNEL__

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

#endif /* __KERNEL__ */

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

/** Determines the states of the domain's datagrams.
 *
 * Evaluates the working counters of the received datagrams and outputs
 * statistics, if necessary. This must be called after ecrt_master_receive()
 * is expected to receive the domain datagrams in order to make
 * ecrt_domain_state() return the result of the last process data exchange.
 */
void ecrt_domain_process(
        ec_domain_t *domain /**< Domain. */
        );

/** (Re-)queues all domain datagrams in the master's datagram queue.
 *
 * Call this function to mark the domain's datagrams for exchanging at the
 * next call of ecrt_master_send().
 */
void ecrt_domain_queue(
        ec_domain_t *domain /**< Domain. */
        );

/** Reads the state of a domain.
 *
 * Stores the domain state in the given \a state structure.
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
 * The timeout is permanently stored in the request object and is valid until
 * the next call of this method.
 */
void ecrt_sdo_request_timeout(
        ec_sdo_request_t *req, /**< Sdo request. */
        uint32_t timeout /**< Timeout in milliseconds. Zero means no
                           timeout. */
        );

/** Access to the Sdo request's data.
 *
 * This function returns a pointer to the request's internal Sdo data memory.
 *
 * - After a read operation was successful, integer data can be evaluated using
 *   the EC_READ_*() macros as usual. Example:
 *   \code
 *   uint16_t value = EC_READ_U16(ecrt_sdo_request_data(sdo)));
 *   \endcode
 * - If a write operation shall be triggered, the data have to be written to
 *   the internal memory. Use the EC_WRITE_*() macros, if you are writing
 *   integer data. Be sure, that the data fit into the memory. The memory size
 *   is a parameter of ecrt_slave_config_create_sdo_request().
 *   \code
 *   EC_WRITE_U16(ecrt_sdo_request_data(sdo), 0xFFFF);
 *   \endcode
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

/** Returns the current Sdo data size.
 *
 * When the Sdo request is created, the data size is set to the size of the
 * reserved memory. After a read operation the size is set to the size of the
 * read data. The size is not modified in any other situation.
 *
 * \return Sdo data size in bytes.
 */
size_t ecrt_sdo_request_data_size(
        const ec_sdo_request_t *req /**< Sdo request. */
        );

/** Get the current state of the Sdo request.
 *
 * \return Request state.
 */
ec_request_state_t ecrt_sdo_request_state(
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

/*****************************************************************************
 * VoE handler methods.
 ****************************************************************************/

/** Sets the VoE header for future send operations.
 *
 * A VoE message shall contain a 4-byte vendor ID, followed by a 2-byte vendor
 * type at as header. These numbers can be set with this function.
 */
void ecrt_voe_handler_send_header(
        ec_voe_handler_t *voe, /**< VoE handler. */
        uint32_t vendor_id, /**< Vendor ID. */
        uint16_t vendor_type /**< Vendor-specific type. */
        );

/** Reads the header data of a received VoE message.
 *
 * This method can be used after a read operation has succeded, to get the
 * received header information.
 *
 * The header information is stored at the memory given by the pointer
 * parameters.
 */
void ecrt_voe_handler_received_header(
        const ec_voe_handler_t *voe, /**< VoE handler. */
        uint32_t *vendor_id, /**< Vendor ID. */
        uint16_t *vendor_type /**< Vendor-specific type. */
        );

/** Access to the VoE handler's data.
 *
 * This function returns a pointer to the VoE handler's internal memory, after
 * the VoE header (see ecrt_voe_handler_send_header()).
 *
 * - After a read operation was successful, the memory contains the received
 *   data. The size of the received data can be determined via
 *   ecrt_voe_handler_data_size().
 * - Before a write operation is triggered, the data have to be written to
 *   the internal memory. Be sure, that the data fit into the memory. The
 *   memory size is a parameter of ecrt_slave_config_create_voe_handler().
 *
 * \return Pointer to the internal memory.
 */
uint8_t *ecrt_voe_handler_data(
        ec_voe_handler_t *voe /**< VoE handler. */
        );

/** Returns the current data size.
 *
 * The data size is the size of the VoE data without the header (see
 * ecrt_voe_handler_send_header()).
 *
 * When the VoE handler is created, the data size is set to the size of the
 * reserved memory. At a write operation, the data size is set to the number
 * of bytes to write. After a read operation the size is set to the size of
 * the read data. The size is not modified in any other situation.
 *
 * \return Data size in bytes.
 */
size_t ecrt_voe_handler_data_size(
        const ec_voe_handler_t *voe /**< VoE handler. */
        );

/** Start a VoE write operation.
 *
 * After this function has been called, the ecrt_voe_handler_execute() method
 * must be called in every bus cycle as long as it returns EC_REQUEST_BUSY. No
 * other operation may be started while the handler is busy.
 */
void ecrt_voe_handler_write(
        ec_voe_handler_t *voe, /**< VoE handler. */
        size_t size /**< Number of bytes to write (without the VoE header). */
        );

/** Start a VoE read operation.
 *
 * After this function has been called, the ecrt_voe_handler_execute() method
 * must be called in every bus cycle as long as it returns EC_REQUEST_BUSY. No
 * other operation may be started while the handler is busy.
 *
 * On success, the size of the read data can be determined via
 * ecrt_voe_handler_data_size(), while the VoE header of the received data
 * can be retrieved with ecrt_voe_handler_received_header().
 */
void ecrt_voe_handler_read(
        ec_voe_handler_t *voe /**< VoE handler. */
        );

/** Execute the handler.
 *
 * This method executes the VoE handler. It has to be called in every bus cycle
 * as long as it returns EC_REQUEST_BUSY.
 *
 * \return Handler state.
 */
ec_request_state_t ecrt_voe_handler_execute(
    ec_voe_handler_t *voe /**< VoE handler. */
    );

/******************************************************************************
 * Bitwise read/write macros
 *****************************************************************************/

/** Read a certain bit of an EtherCAT data byte.
 *
 * \param DATA EtherCAT data pointer
 * \param POS bit position
 */
#define EC_READ_BIT(DATA, POS) ((*((uint8_t *) (DATA)) >> (POS)) & 0x01)

/** Write a certain bit of an EtherCAT data byte.
 *
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
 * Byte-swapping functions for user space
 *****************************************************************************/

#ifndef __KERNEL__

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define le16_to_cpu(x) x
#define le32_to_cpu(x) x

#define cpu_to_le16(x) x
#define cpu_to_le32(x) x

#elif __BYTE_ORDER == __BIG_ENDIAN

#define swap16(x) \
        ((uint16_t)( \
        (((uint16_t)(x) & 0x00ffU) << 8) | \
        (((uint16_t)(x) & 0xff00U) >> 8) ))
#define swap32(x) \
        ((uint32_t)( \
        (((uint32_t)(x) & 0x000000ffUL) << 24) | \
        (((uint32_t)(x) & 0x0000ff00UL) <<  8) | \
        (((uint32_t)(x) & 0x00ff0000UL) >>  8) | \
        (((uint32_t)(x) & 0xff000000UL) >> 24) ))

#define le16_to_cpu(x) swap16(x)
#define le32_to_cpu(x) swap32(x)

#define cpu_to_le16(x) swap16(x)
#define cpu_to_le32(x) swap32(x)

#endif

#define le16_to_cpup(x) le16_to_cpu(*((uint16_t *)(x)))
#define le32_to_cpup(x) le32_to_cpu(*((uint32_t *)(x)))

#endif /* ifndef __KERNEL__ */

/******************************************************************************
 * Read macros
 *****************************************************************************/

/** Read an 8-bit unsigned value from EtherCAT data.
 *
 * \return EtherCAT data value
 */
#define EC_READ_U8(DATA) \
    ((uint8_t) *((uint8_t *) (DATA)))

/** Read an 8-bit signed value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S8(DATA) \
     ((int8_t) *((uint8_t *) (DATA)))

/** Read a 16-bit unsigned value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_U16(DATA) \
     ((uint16_t) le16_to_cpup((void *) (DATA)))

/** Read a 16-bit signed value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S16(DATA) \
     ((int16_t) le16_to_cpup((void *) (DATA)))

/** Read a 32-bit unsigned value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_U32(DATA) \
     ((uint32_t) le32_to_cpup((void *) (DATA)))

/** Read a 32-bit signed value from EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \return EtherCAT data value
 */
#define EC_READ_S32(DATA) \
     ((int32_t) le32_to_cpup((void *) (DATA)))

/******************************************************************************
 * Write macros
 *****************************************************************************/

/** Write an 8-bit unsigned value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U8(DATA, VAL) \
    do { \
        *((uint8_t *)(DATA)) = ((uint8_t) (VAL)); \
    } while (0)

/** Write an 8-bit signed value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S8(DATA, VAL) EC_WRITE_U8(DATA, VAL)

/** Write a 16-bit unsigned value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U16(DATA, VAL) \
    do { \
        *((uint16_t *) (DATA)) = cpu_to_le16((uint16_t) (VAL)); \
    } while (0)

/** Write a 16-bit signed value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S16(DATA, VAL) EC_WRITE_U16(DATA, VAL)

/** Write a 32-bit unsigned value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_U32(DATA, VAL) \
    do { \
        *((uint32_t *) (DATA)) = cpu_to_le32((uint32_t) (VAL)); \
    } while (0)

/** Write a 32-bit signed value to EtherCAT data.
 *
 * \param DATA EtherCAT data pointer
 * \param VAL new value
 */
#define EC_WRITE_S32(DATA, VAL) EC_WRITE_U32(DATA, VAL)

/*****************************************************************************/

/** @} */

#endif
