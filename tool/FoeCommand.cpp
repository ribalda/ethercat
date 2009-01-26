/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include "FoeCommand.h"
#include "foe.h"

/*****************************************************************************/

FoeCommand::FoeCommand(const string &name, const string &briefDesc):
    Command(name, briefDesc)
{
}

/****************************************************************************/

std::string FoeCommand::errorString(int abort_code)
{
	switch (abort_code) {
		case FOE_BUSY:
			return "FOE_BUSY";
		case FOE_READY:
			return "FOE_READY";
		case FOE_IDLE:
			return "FOE_IDLE";
		case FOE_WC_ERROR:
			return "FOE_WC_ERROR";
		case FOE_RECEIVE_ERROR:
			return "FOE_RECEIVE_ERROR";
		case FOE_PROT_ERROR:
			return "FOE_PROT_ERROR";
		case FOE_NODATA_ERROR:
			return "FOE_NODATA_ERROR";
		case FOE_PACKETNO_ERROR:
			return "FOE_PACKETNO_ERROR";
		case FOE_OPMODE_ERROR:
			return "FOE_OPMODE_ERROR";
		case FOE_TIMEOUT_ERROR:
			return "FOE_TIMEOUT_ERROR";
		case FOE_SEND_RX_DATA_ERROR:
			return "FOE_SEND_RX_DATA_ERROR";
		case FOE_RX_DATA_ACK_ERROR:
			return "FOE_RX_DATA_ACK_ERROR";
		case FOE_ACK_ERROR:
			return "FOE_ACK_ERROR";
		case FOE_MBOX_FETCH_ERROR:
			return "FOE_MBOX_FETCH_ERROR";
		case FOE_READ_NODATA_ERROR:
			return "FOE_READ_NODATA_ERROR";
		case FOE_MBOX_PROT_ERROR:
			return "FOE_MBOX_PROT_ERROR";
		default:
			return "???";
	}
}

/****************************************************************************/
