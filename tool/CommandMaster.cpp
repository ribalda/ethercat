/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandMaster.h"

/*****************************************************************************/

CommandMaster::CommandMaster():
    Command("master", "Show master and Ethernet device information.")
{
}

/****************************************************************************/

string CommandMaster::helpString() const
{
    stringstream str;

	str << getName() << " [OPTIONS]" << endl
    	<< endl
    	<< getBriefDescription() << endl
    	<< endl
    	<< "Command-specific options:" << endl
    	<< "  --master -m <index>  Index of the master to use. Default: 0."
		<< endl << endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandMaster::execute(MasterDevice &m, const StringVector &args)
{
    ec_ioctl_master_t data;
    stringstream err;
    unsigned int i;
    
    m.open(MasterDevice::Read);
    m.getMaster(&data);

    cout
        << "Master" << m.getIndex() << endl
        << "  Phase: ";

    switch (data.phase) {
        case 0:  cout << "Waiting for device..."; break;
        case 1:  cout << "Idle"; break;
        case 2:  cout << "Operation"; break;
        default: cout << "???";
    }

    cout << endl
        << "  Slaves: " << data.slave_count << endl;

    for (i = 0; i < 2; i++) {
        cout << "  Device" << i << ": ";
        if (data.devices[i].address[0] == 0x00
                && data.devices[i].address[1] == 0x00
                && data.devices[i].address[2] == 0x00
                && data.devices[i].address[3] == 0x00
                && data.devices[i].address[4] == 0x00
                && data.devices[i].address[5] == 0x00) {
            cout << "None.";
        } else {
            cout << hex << setfill('0')
                << setw(2) << (unsigned int) data.devices[i].address[0] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[1] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[2] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[3] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[4] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[5] << " ("
                << (data.devices[i].attached ? "attached" : "waiting...")
                << ")" << endl << dec
                << "    Tx count: " << data.devices[i].tx_count << endl
                << "    Rx count: " << data.devices[i].rx_count;
        }
        cout << endl;
    }
}

/*****************************************************************************/
