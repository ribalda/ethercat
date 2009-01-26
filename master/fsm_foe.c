/******************************************************************************
 *
 *  $Id:$
 *
 *  Copyright (C) 2008  Olav Zarges, imc Meﬂsysteme GmbH
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

/**
   \file
   EtherCAT FoE state machines.
*/

/*****************************************************************************/

#include "globals.h"
#include "master.h"
#include "mailbox.h"
#include "fsm_foe.h"
#include "foe.h"

/*****************************************************************************/

/** Maximum time in ms to wait for responses when reading out the dictionary.
 */
#define EC_FSM_FOE_TIMEOUT 3000

#define EC_MBOX_TYPE_FILEACCESS 0x04

/*****************************************************************************/

int		ec_foe_prepare_data_send( ec_fsm_foe_t * );
int		ec_foe_prepare_wrq_send( ec_fsm_foe_t * );
int		ec_foe_prepare_rrq_send( ec_fsm_foe_t * );
int		ec_foe_prepare_send_ack( ec_fsm_foe_t * );

void	ec_foe_set_tx_error( ec_fsm_foe_t *, uint32_t );
void	ec_foe_set_rx_error( ec_fsm_foe_t *, uint32_t );

void	ec_fsm_foe_write(ec_fsm_foe_t * );
void	ec_fsm_foe_read(ec_fsm_foe_t * );
void 	ec_fsm_foe_end( ec_fsm_foe_t * );
void 	ec_fsm_foe_error( ec_fsm_foe_t * );

void	ec_fsm_foe_state_wrq_sent( ec_fsm_foe_t * );
void	ec_fsm_foe_state_rrq_sent( ec_fsm_foe_t * );

void	ec_fsm_foe_state_ack_check( ec_fsm_foe_t * );
void	ec_fsm_foe_state_ack_read( ec_fsm_foe_t * );

void	ec_fsm_foe_state_data_sent( ec_fsm_foe_t * );

void	ec_fsm_foe_state_data_check( ec_fsm_foe_t * );
void	ec_fsm_foe_state_data_read ( ec_fsm_foe_t * );
void	ec_fsm_foe_state_sent_ack( ec_fsm_foe_t * );

void	ec_fsm_foe_write_start( ec_fsm_foe_t * );
void	ec_fsm_foe_read_start(ec_fsm_foe_t * );

/*****************************************************************************/

/**
   Constructor.
*/

void ec_fsm_foe_init(ec_fsm_foe_t *fsm, /**< finite state machine */
                     ec_datagram_t *datagram /**< datagram */
                     )
{
    fsm->state     = NULL;
    fsm->datagram  = datagram;
    fsm->rx_errors = 0;
    fsm->tx_errors = 0;
}

/*****************************************************************************/

/**
   Destructor.
*/

void ec_fsm_foe_clear(ec_fsm_foe_t *fsm /**< finite state machine */)
{
}

/*****************************************************************************/

/**
   Executes the current state of the state machine.
   \return false, if state machine has terminated
*/

int ec_fsm_foe_exec(ec_fsm_foe_t *fsm /**< finite state machine */)
{
    fsm->state(fsm);

    return fsm->state != ec_fsm_foe_end && fsm->state != ec_fsm_foe_error;
}

/*****************************************************************************/

/**
   Returns, if the state machine terminated with success.
   \return non-zero if successful.
*/

int ec_fsm_foe_success(ec_fsm_foe_t *fsm /**< Finite state machine */)
{
    return fsm->state == ec_fsm_foe_end;
}

/*****************************************************************************/

void ec_fsm_foe_transfer(
        ec_fsm_foe_t *fsm, /**< State machine. */
        ec_slave_t *slave, /**< EtherCAT slave. */
        ec_foe_request_t *request /**< Sdo request. */
        )
{
    fsm->slave = slave;
    fsm->request = request;
    if (request->dir == EC_DIR_OUTPUT) {
        fsm->state = ec_fsm_foe_write;
    }
    else {
        fsm->state = ec_fsm_foe_read;
    }
}

/*****************************************************************************/

/**
   State: ERROR.
*/

void ec_fsm_foe_error(ec_fsm_foe_t *fsm /**< finite state machine */)
{
#ifdef	myDEBUG
	printk("ec_fsm_foe_error()\n");
#endif
}

/*****************************************************************************/

/**
   State: END.
*/

void ec_fsm_foe_end(ec_fsm_foe_t *fsm /**< finite state machine */)
{
#ifdef	myDEBUG
    printk("ec_fsm_foe_end\n");
#endif
}

/*****************************************************************************/

#define	EC_MBOX_HEADER_SIZE		6
// uint16_t	Length
// uint16_t	Address
// uint8_t	reserved
// uint8_t	Type:4
// uint8_t	Counter:4

#define	EC_FOE_HEADER_SIZE		6
// uint8_t	OpMode
// uint8_t	reserved
// uint32_t	PacketNo, Password, ErrorCode

enum {
	EC_FOE_OPMODE_RRQ  = 1,
	EC_FOE_OPMODE_WRQ  = 2,
	EC_FOE_OPMODE_DATA = 3,
	EC_FOE_OPMODE_ACK  = 4,
	EC_FOE_OPMODE_ERR  = 5,
	EC_FOE_OPMODE_BUSY = 6
} ec_foe_opmode_t;

/*****************************************************************************/
/**
   Sends a file or the next fragment.
*/

int ec_foe_prepare_data_send( ec_fsm_foe_t *fsm ) {
    size_t       remaining_size, current_size;
    uint8_t*     data;

    remaining_size = fsm->tx_buffer_size - fsm->tx_buffer_offset;

    if (remaining_size < fsm->slave->sii.tx_mailbox_size - EC_MBOX_HEADER_SIZE - EC_FOE_HEADER_SIZE) {
        current_size = remaining_size;
        fsm->tx_last_packet = 1;
    }
    else {
        current_size = fsm->slave->sii.tx_mailbox_size - EC_MBOX_HEADER_SIZE - EC_FOE_HEADER_SIZE;
    }

    if (!(data = ec_slave_mbox_prepare_send(fsm->slave, fsm->datagram,
    		EC_MBOX_TYPE_FILEACCESS, current_size + EC_FOE_HEADER_SIZE)))
        return -1;

    EC_WRITE_U8 ( data, EC_FOE_OPMODE_DATA ); 		// OpMode = DataBlock req.
    EC_WRITE_U32( data + 2, fsm->tx_packet_no );	// PacketNo, Password

    memcpy(data + EC_FOE_HEADER_SIZE, fsm->tx_buffer + fsm->tx_buffer_offset, current_size);

    fsm->tx_current_size = current_size;

    return 0;
}

/*****************************************************************************/
/**
   Prepare a write request (WRQ) with filename
*/

int ec_foe_prepare_wrq_send( ec_fsm_foe_t *fsm ) {
    size_t current_size;
    uint8_t *data;

    fsm->tx_buffer_offset = 0;
    fsm->tx_current_size = 0;
    fsm->tx_packet_no = 0;
	fsm->tx_last_packet = 0;

    current_size = fsm->tx_filename_len;

    if (!(data = ec_slave_mbox_prepare_send(fsm->slave, fsm->datagram,
    		EC_MBOX_TYPE_FILEACCESS, current_size + EC_FOE_HEADER_SIZE)))
        return -1;

    EC_WRITE_U16( data, EC_FOE_OPMODE_WRQ); // fsm write request
    EC_WRITE_U32( data + 2, fsm->tx_packet_no );

    memcpy(data + EC_FOE_HEADER_SIZE, fsm->tx_filename, current_size);

    return 0;
}

/*****************************************************************************/

char		tx_buffer[0x1000];
void ec_fsm_foe_write(ec_fsm_foe_t *fsm /**< finite state machine */)
{
    fsm->tx_buffer = fsm->request->buffer;
    fsm->tx_buffer_size = fsm->request->data_size;
    fsm->tx_buffer_offset = 0;

	fsm->tx_filename = fsm->request->file_name;
	fsm->tx_filename_len = strlen(fsm->tx_filename);

	fsm->state = ec_fsm_foe_write_start;

#ifdef use_ext_buffer
	{
		int i;
		fsm->tx_data = tx_buffer;
		for (i=0 ; i<sizeof(tx_buffer) ; i++) {
			tx_buffer[i] = (uint8_t)(i);
		}
		fsm->tx_data_len = sizeof(tx_buffer);
	}
#endif
}

/*****************************************************************************/
/**
   Initializes the SII write state machine.
*/

void ec_fsm_foe_write_start(ec_fsm_foe_t *fsm /**< finite state machine */)
{
    ec_slave_t *slave = fsm->slave;

    fsm->tx_buffer_offset = 0;
    fsm->tx_current_size = 0;
    fsm->tx_packet_no = 0;
	fsm->tx_last_packet = 0;

#ifdef	myDEBUG
    printk("ec_fsm_foe_write_start()\n");
#endif

    if (!(slave->sii.mailbox_protocols & EC_MBOX_FOE)) {
        ec_foe_set_tx_error(fsm, FOE_MBOX_PROT_ERROR);
        EC_ERR("Slave %u does not support FoE!\n", slave->ring_position);
        return;
    }

    if (ec_foe_prepare_wrq_send(fsm)) {
        ec_foe_set_tx_error(fsm, FOE_PROT_ERROR);
        return;
    }

    fsm->state = ec_fsm_foe_state_wrq_sent;
}

/*****************************************************************************/

void ec_fsm_foe_state_ack_check( ec_fsm_foe_t *fsm ) {
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

#ifdef	myDEBUG
//	printk("ec_fsm_foe_ack_check()\n");
#endif
    if (datagram->state != EC_DATAGRAM_RECEIVED) {
    	ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_ERR("Failed to receive FoE mailbox check datagram for slave %u"
                " (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
    	// slave hat noch nichts in die Mailbox getan
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_ERR("Reception of FoE mailbox check datagram failed on slave %u: ",
               slave->ring_position);
        ec_datagram_print_wc_error(datagram);
	    return;
	}

    if (!ec_slave_mbox_check(datagram)) {
        unsigned long diff_ms =
            (datagram->jiffies_received - fsm->jiffies_start) * 1000 / HZ;
        if (diff_ms >= EC_FSM_FOE_TIMEOUT) {
        	ec_foe_set_tx_error(fsm, FOE_TIMEOUT_ERROR);
            EC_ERR("Timeout while waiting for ack response "
                    "on slave %u.\n", slave->ring_position);
            return;
        }
//        EC_ERR("WAIT!!!!!!!!!!!!!\n");
        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_foe_state_ack_read;
}

/*****************************************************************************/

void ec_fsm_foe_state_ack_read( ec_fsm_foe_t *fsm ) {

    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;
    uint8_t *data, mbox_prot;
    uint16_t opMode;
    size_t rec_size;

#ifdef	myDEBUG
	printk("ec_fsm_foe_ack_read()\n");
#endif
    if (datagram->state != EC_DATAGRAM_RECEIVED) {
    	ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_ERR("Failed to receive FoE ack response datagram for"
               " slave %u (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_ERR("Reception of FoE ack response failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

	if (!(data = ec_slave_mbox_fetch(fsm->slave, datagram, &mbox_prot, &rec_size))) {
    	ec_foe_set_tx_error(fsm, FOE_PROT_ERROR);
        return;
    }

    if (mbox_prot != EC_MBOX_TYPE_FILEACCESS) { // FoE
    	ec_foe_set_tx_error(fsm, FOE_MBOX_PROT_ERROR);
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        return;
    }

    opMode = EC_READ_U16(data);

    if ( opMode == EC_FOE_OPMODE_BUSY ) {
    	// slave ist noch nicht bereit
        if (ec_foe_prepare_data_send(fsm)) {
        	ec_foe_set_tx_error(fsm, FOE_PROT_ERROR);
            EC_ERR("Slave is busy.\n");
        	return;
        }
        fsm->state = ec_fsm_foe_state_data_sent;
        return;
    }

    if ( opMode == EC_FOE_OPMODE_ACK ) {
        fsm->tx_packet_no++;
        fsm->tx_buffer_offset += fsm->tx_current_size;

        if (fsm->tx_last_packet) {
        	fsm->state = ec_fsm_foe_end;
        	return;
        }

        if (ec_foe_prepare_data_send(fsm)) {
        	ec_foe_set_tx_error(fsm, FOE_PROT_ERROR);
	    	return;
	    }
        fsm->state = ec_fsm_foe_state_data_sent;
    	return;
    }
	ec_foe_set_tx_error(fsm, FOE_ACK_ERROR);
}

/*****************************************************************************/
/**
   State: WRQ SENT.
   Checks is the previous transmit datagram succeded and sends the next
   fragment, if necessary.
*/

void ec_fsm_foe_state_wrq_sent( ec_fsm_foe_t *fsm ) {
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

#ifdef	myDEBUG
	printk("ec_foe_state_sent_wrq()\n");
#endif
    if (datagram->state != EC_DATAGRAM_RECEIVED) {
    	ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_ERR("Failed to send FoE WRQ for slave %u"
                " (datagram state %u).\n",
                slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
    	// slave hat noch nichts in die Mailbox getan
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_ERR("Reception of FoE WRQ failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
    	return;
    }

    fsm->jiffies_start = datagram->jiffies_sent;

    ec_slave_mbox_prepare_check(fsm->slave, datagram); // can not fail.

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_foe_state_ack_check;
}

/*****************************************************************************/
/**
   State: WRQ SENT.
   Checks is the previous transmit datagram succeded and sends the next
   fragment, if necessary.
*/

void ec_fsm_foe_state_data_sent( ec_fsm_foe_t *fsm ) {
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

#ifdef	myDEBUG
	printk("ec_fsm_foe_state_data_sent()\n");
#endif
    if (fsm->datagram->state != EC_DATAGRAM_RECEIVED) {
    	ec_foe_set_tx_error(fsm, FOE_RECEIVE_ERROR);
        EC_ERR("Failed to receive FoE ack response datagram for"
               " slave %u (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (fsm->datagram->working_counter != 1) {
	    ec_foe_set_tx_error(fsm, FOE_WC_ERROR);
        EC_ERR("Reception of FoE data send failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    ec_slave_mbox_prepare_check(fsm->slave, fsm->datagram);
    fsm->jiffies_start = jiffies;
    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_foe_state_ack_check;
}

/*****************************************************************************/
/**
   Prepare a read request (RRQ) with filename
*/

int ec_foe_prepare_rrq_send( ec_fsm_foe_t *fsm ) {
    size_t current_size;
    uint8_t *data;

    current_size = fsm->rx_filename_len;

    if (!(data = ec_slave_mbox_prepare_send(fsm->slave, fsm->datagram,
    		EC_MBOX_TYPE_FILEACCESS, current_size + EC_FOE_HEADER_SIZE)))
        return -1;

    EC_WRITE_U16( data, EC_FOE_OPMODE_RRQ); // fsm read request
    EC_WRITE_U32( data + 2, 0 );

    memcpy(data + EC_FOE_HEADER_SIZE, fsm->rx_filename, current_size);

    return 0;
}


/*****************************************************************************/

int ec_foe_prepare_send_ack( ec_fsm_foe_t *foe ) {
    uint8_t *data;

    if (!(data = ec_slave_mbox_prepare_send(foe->slave, foe->datagram,
    		EC_MBOX_TYPE_FILEACCESS, EC_FOE_HEADER_SIZE)))
        return -1;

    EC_WRITE_U16( data, EC_FOE_OPMODE_ACK);
    EC_WRITE_U32( data + 2, foe->rx_expected_packet_no  );

    return 0;
}

/*****************************************************************************/
/**
   State: RRQ SENT.
   Checks is the previous transmit datagram succeded and sends the next
   fragment, if necessary.
*/

void ec_fsm_foe_state_rrq_sent( ec_fsm_foe_t *fsm ) {
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

#ifdef	myDEBUG
	printk("ec_foe_state_rrq_sent()\n");
#endif
    if (datagram->state != EC_DATAGRAM_RECEIVED) {
    	ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_ERR("Failed to send FoE RRQ for slave %u"
                " (datagram state %u).\n",
                slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
    	// slave hat noch nichts in die Mailbox getan
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_ERR("Reception of FoE RRQ failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
    	return;
    }

    fsm->jiffies_start = datagram->jiffies_sent;

    ec_slave_mbox_prepare_check(fsm->slave, datagram); // can not fail.

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_foe_state_data_check;
}

/*****************************************************************************/

#ifdef myDEBUG
char		rx_buffer[0x8000];
#endif

void ec_fsm_foe_read(ec_fsm_foe_t *fsm /**< finite state machine */)
{
	fsm->state = ec_fsm_foe_read_start;
	fsm->rx_filename = fsm->request->file_name;
	fsm->rx_filename_len = strlen(fsm->rx_filename);

	fsm->rx_buffer = fsm->request->buffer;
	fsm->rx_buffer_size = fsm->request->buffer_size;

#ifdef use_ext_buffer
	fsm->rx_buffer = rx_buffer;
	fsm->rx_buffer_size = sizeof(rx_buffer);
#endif
}

/*****************************************************************************/

void ec_fsm_foe_read_start(ec_fsm_foe_t *fsm /**< finite state machine */)
{
	size_t current_size;
    ec_slave_t *slave = fsm->slave;

    fsm->rx_buffer_offset = 0;
    fsm->rx_current_size = 0;
    fsm->rx_packet_no = 0;
    fsm->rx_expected_packet_no = 1;
    fsm->rx_last_packet = 0;

    current_size = fsm->rx_filename_len;

#ifdef	myDEBUG
	printk("ec_fsm_foe_read_start()\n");
#endif
    if (!(slave->sii.mailbox_protocols & EC_MBOX_FOE)) {
    	ec_foe_set_tx_error(fsm, FOE_MBOX_PROT_ERROR);
        EC_ERR("Slave %u does not support FoE!\n", slave->ring_position);
        return;
    }

    if (ec_foe_prepare_rrq_send(fsm)) {
        ec_foe_set_rx_error(fsm, FOE_PROT_ERROR);
        return;
    }

    fsm->state = ec_fsm_foe_state_rrq_sent;
}

/*****************************************************************************/

void ec_fsm_foe_state_data_check ( ec_fsm_foe_t *fsm ) {
    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

#ifdef	myDEBUG
	printk("ec_fsm_foe_state_data_check()\n");
#endif
	if (datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_ERR("Failed to send FoE DATA READ for slave %u"
                " (datagram state %u).\n",
                slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_ERR("Reception of FoE DATA READ on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

    if (!ec_slave_mbox_check(datagram)) {
        unsigned long diff_ms =
            (datagram->jiffies_received - fsm->jiffies_start) * 1000 / HZ;
        if (diff_ms >= EC_FSM_FOE_TIMEOUT) {
        	ec_foe_set_tx_error(fsm, FOE_TIMEOUT_ERROR);
            EC_ERR("Timeout while waiting for ack response "
                    "on slave %u.\n", slave->ring_position);
            return;
        }

        ec_slave_mbox_prepare_check(slave, datagram); // can not fail.
        fsm->retries = EC_FSM_RETRIES;
        return;
    }

    // Fetch response
    ec_slave_mbox_prepare_fetch(slave, datagram); // can not fail.

    fsm->retries = EC_FSM_RETRIES;
    fsm->state = ec_fsm_foe_state_data_read;

}

/*****************************************************************************/

void ec_fsm_foe_state_data_read ( ec_fsm_foe_t *fsm ) {
    size_t 	rec_size;
    uint8_t *data, opMode, packet_no, mbox_prot;

    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

#ifdef	myDEBUG
	printk("ec_fsm_foe_state_data_read()\n");
#endif
    if (datagram->state != EC_DATAGRAM_RECEIVED) {
    	ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_ERR("Failed to receive FoE DATA READ datagram for"
               " slave %u (datagram state %u).\n",
               slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_ERR("Reception of FoE DATA READ failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
        return;
    }

	if (!(data = ec_slave_mbox_fetch(slave, datagram, &mbox_prot, &rec_size))) {
    	ec_foe_set_rx_error(fsm, FOE_MBOX_FETCH_ERROR);
        return;
    }

	if (mbox_prot != EC_MBOX_TYPE_FILEACCESS) { // FoE
        EC_ERR("Received mailbox protocol 0x%02X as response.\n", mbox_prot);
        ec_foe_set_rx_error(fsm, FOE_PROT_ERROR);
        return;
    }

    opMode = EC_READ_U16(data);

    if (opMode == EC_FOE_OPMODE_BUSY) {
        if (ec_foe_prepare_send_ack(fsm)) {
            ec_foe_set_rx_error(fsm, FOE_PROT_ERROR);
        }
        return;
    }

    if (opMode != EC_FOE_OPMODE_DATA) {
        ec_foe_set_rx_error(fsm, FOE_OPMODE_ERROR);
        return;
    }

    packet_no = EC_READ_U16(data + 2);
    if (packet_no != fsm->rx_expected_packet_no) {
        ec_foe_set_rx_error(fsm, FOE_PACKETNO_ERROR);
        return;
    }

    rec_size -= EC_FOE_HEADER_SIZE;

    if ( fsm->rx_buffer_size >= fsm->rx_buffer_offset + rec_size ) {
        memcpy ( fsm->rx_buffer + fsm->rx_buffer_offset, data + EC_FOE_HEADER_SIZE, rec_size );
        fsm->rx_buffer_offset += rec_size;
    }

    fsm->rx_last_packet = (rec_size + EC_MBOX_HEADER_SIZE + EC_FOE_HEADER_SIZE != fsm->slave->sii.rx_mailbox_size);

    if (fsm->rx_last_packet ||
    	slave->sii.rx_mailbox_size - EC_MBOX_HEADER_SIZE - EC_FOE_HEADER_SIZE  + fsm->rx_buffer_offset <= fsm->rx_buffer_size) {
    	// either it was the last packet or a new packet will fit into the delivered buffer
#ifdef	myDEBUG
    	printk ("last_packet=true\n");
#endif
	    if (ec_foe_prepare_send_ack(fsm)) {
	    	ec_foe_set_rx_error(fsm, FOE_RX_DATA_ACK_ERROR);
	    	return;
	    }

	    fsm->state = ec_fsm_foe_state_sent_ack;
    }
    else {
    	// no more data fits into the deliverd buffer
    	// ... wait for new read request (an den Treiber)
    	printk ("ERROR: data doesn't fit in receive buffer\n");
    	printk ("       rx_buffer_size  = %d\n", fsm->rx_buffer_size);
    	printk ("       rx_buffer_offset= %d\n", fsm->rx_buffer_offset);
    	printk ("       rec_size        = %d\n", rec_size);
    	printk ("       rx_mailbox_size = %d\n", slave->sii.rx_mailbox_size);
    	printk ("       rx_last_packet  = %d\n", fsm->rx_last_packet);
//    	fsm->state = ec_fsm_state_wait_next_read;
    	fsm->request->abort_code = FOE_READY;
    }
}

/*****************************************************************************/

void ec_fsm_foe_state_sent_ack( ec_fsm_foe_t *fsm ) {

    ec_datagram_t *datagram = fsm->datagram;
    ec_slave_t *slave = fsm->slave;

#ifdef	myDEBUG
	printk("ec_foe_state_sent_ack()\n");
#endif
    if (datagram->state != EC_DATAGRAM_RECEIVED) {
        ec_foe_set_rx_error(fsm, FOE_RECEIVE_ERROR);
        EC_ERR("Failed to send FoE ACK for slave %u"
                " (datagram state %u).\n",
                slave->ring_position, datagram->state);
        return;
    }

    if (datagram->working_counter != 1) {
    	// slave hat noch nichts in die Mailbox getan
        ec_foe_set_rx_error(fsm, FOE_WC_ERROR);
        EC_ERR("Reception of FoE ACK failed on slave %u: ",
                slave->ring_position);
        ec_datagram_print_wc_error(datagram);
    	return;
    }

    fsm->jiffies_start = datagram->jiffies_sent;

    ec_slave_mbox_prepare_check(fsm->slave, datagram); // can not fail.

    if (fsm->rx_last_packet) {
    	fsm->rx_expected_packet_no = 0;
    	fsm->request->data_size = fsm->rx_buffer_offset;
    	fsm->state = ec_fsm_foe_end;
    }
    else {
    	fsm->rx_expected_packet_no++;
        fsm->retries = EC_FSM_RETRIES;
        fsm->state = ec_fsm_foe_state_data_check;
    }
}

/*****************************************************************************/

void ec_foe_set_tx_error( ec_fsm_foe_t *fsm, uint32_t errorcode ) {
	fsm->tx_errors++;
	fsm->request->abort_code = errorcode;
	fsm->state = ec_fsm_foe_error;
}

/*****************************************************************************/

void ec_foe_set_rx_error( ec_fsm_foe_t *fsm, uint32_t errorcode ) {
	fsm->rx_errors++;
	fsm->request->abort_code = errorcode;
	fsm->state = ec_fsm_foe_error;
}

/*****************************************************************************/
