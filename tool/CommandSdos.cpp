/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandSdos.h"
#include "coe_datatypes.h"

/*****************************************************************************/

CommandSdos::CommandSdos():
    Command("sdos", "List Sdo dictionaries.")
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
    	<< "Sdo dictionary information is displayed in two layers," << endl
    	<< "which are indented accordingly:" << endl
    	<< endl
    	<< "1) Sdos - Hexadecimal Sdo index and the name. Example:" << endl
    	<< endl
    	<< "   Sdo 0x1018, \"Identity object\"" << endl
    	<< endl
    	<< "2) Sdo entries - Sdo index and Sdo entry subindex (both" << endl
		<< "   hexadecimal) followed by the data type, the length in" << endl
		<< "   bit, and the description. Example:" << endl
    	<< endl
    	<< "   0x1018:01, uint32, 32 bit, \"Vendor id\"" << endl
    	<< endl
    	<< "If the --quiet option is given, only the Sdos are printed."
		<< endl << endl
    	<< "Command-specific options:" << endl
    	<< "  --slave -s <index>  Positive numerical ring position," << endl
		<< "                      'all' for all slaves (default)." << endl
    	<< "  --quiet -q          Print only Sdos (without Sdo" << endl
		<< "                      entries)." << endl
    	<< endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandSdos::execute(MasterDevice &m, const StringVector &args)
{
    m.open(MasterDevice::Read);

    if (slavePosition == -1) {
        unsigned int numSlaves = m.slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            listSlaveSdos(m, i, true);
        }
    } else {
        listSlaveSdos(m, slavePosition, false);
    }
}

/****************************************************************************/

void CommandSdos::listSlaveSdos(
		MasterDevice &m,
		uint16_t slavePosition,
		bool withHeader
		)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_slave_sdo_t sdo;
    ec_ioctl_slave_sdo_entry_t entry;
    unsigned int i, j;
    const CoEDataType *d;
    
    m.getSlave(&slave, slavePosition);

    if (withHeader)
        cout << "=== Slave " << slavePosition << " ===" << endl;

    for (i = 0; i < slave.sdo_count; i++) {
        m.getSdo(&sdo, slavePosition, i);

        cout << "Sdo 0x"
            << hex << setfill('0')
            << setw(4) << sdo.sdo_index
            << ", \"" << sdo.name << "\"" << endl;

        if (getVerbosity() == Quiet)
            continue;

        for (j = 0; j <= sdo.max_subindex; j++) {
            m.getSdoEntry(&entry, slavePosition, -i, j);

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
