/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandSdos.h"

/*****************************************************************************/

CommandSdos::CommandSdos():
    SdoCommand("sdos", "List SDO dictionaries.")
{
}

/*****************************************************************************/

string CommandSdos::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS]" << endl
    	<< endl
    	<< getBriefDescription() << endl
    	<< endl
    	<< "SDO dictionary information is displayed in two layers," << endl
    	<< "which are indented accordingly:" << endl
    	<< endl
    	<< "1) SDOs - Hexadecimal SDO index and the name. Example:" << endl
    	<< endl
    	<< "   SDO 0x1018, \"Identity object\"" << endl
    	<< endl
    	<< "2) SDO entries - SDO index and SDO entry subindex (both" << endl
		<< "   hexadecimal) followed by the data type, the length in" << endl
		<< "   bit, and the description. Example:" << endl
    	<< endl
    	<< "   0x1018:01, uint32, 32 bit, \"Vendor id\"" << endl
    	<< endl
    	<< "If the --quiet option is given, only the SDOs are output."
		<< endl << endl
    	<< "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
    	<< "  --quiet    -q          Only output SDOs (without the" << endl
		<< "                         SDO entries)." << endl
    	<< endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandSdos::execute(MasterDevice &m, const StringVector &args)
{
    SlaveList slaves;
    SlaveList::const_iterator si;
    bool showHeader;

    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);
    showHeader = slaves.size() > 1;

    for (si = slaves.begin(); si != slaves.end(); si++) {
        listSlaveSdos(m, *si, showHeader);
    }
}

/****************************************************************************/

void CommandSdos::listSlaveSdos(
		MasterDevice &m,
        const ec_ioctl_slave_t &slave,
		bool showHeader
		)
{
    ec_ioctl_slave_sdo_t sdo;
    ec_ioctl_slave_sdo_entry_t entry;
    unsigned int i, j;
    const DataType *d;
    
    if (showHeader)
        cout << "=== Slave " << slave.position << " ===" << endl;

    for (i = 0; i < slave.sdo_count; i++) {
        m.getSdo(&sdo, slave.position, i);

        cout << "SDO 0x"
            << hex << setfill('0')
            << setw(4) << sdo.sdo_index
            << ", \"" << sdo.name << "\"" << endl;

        if (getVerbosity() == Quiet)
            continue;

        for (j = 0; j <= sdo.max_subindex; j++) {
            m.getSdoEntry(&entry, slave.position, -i, j);

            cout << "  0x" << hex << setfill('0')
                << setw(4) << sdo.sdo_index << ":"
                << setw(2) << (unsigned int) entry.sdo_entry_subindex
                << ", ";

            if ((d = findDataType(entry.data_type))) {
                cout << d->name;
            } else {
                cout << "type " << setw(4) << entry.data_type;
            }

            cout << ", " << dec << entry.bit_length << " bit, \""
                << entry.description << "\"" << endl;
        }
    }
}

/*****************************************************************************/
