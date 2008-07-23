/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "globals.h"
#include "coe_datatypes.h"

/****************************************************************************/

const char *help_sdos =
    "[OPTIONS]\n"
    "\n"
    "Displays the Sdo dictionary with Sdos and Sdo entries.\n"
    "\n"
    "Sdo dictionary information is displayed in two layers, with are\n"
    "indented accordingly:\n"
    "\n"
    "1) Sdos - Hexadecimal Sdo index and the name. Example:\n"
    "\n"
    "   Sdo 0x1018, \"Identity object\"\n"
    "\n"
    "2) Sdo entries - Sdo index and Sdo entry subindex (both hexadecimal)\n"
    "   followed by the data type, the length in bit, and the description.\n"
    "   Example:\n"
    "\n"
    "   0x1018:01, uint32, 32 bit, \"Vendor id\"\n"
    "\n"
    "If the --quiet option is given, only the Sdos are printed.\n"
    "\n"
    "Command-specific options:\n"
    "  --slave -s <index>  Positive numerical ring position, or 'all' for\n"
    "                      all slaves (default).\n"
    "  --quiet -q          Print only Sdos (without Sdo entries).\n"
    "\n"
    "Numerical values can be specified either with decimal (no prefix),\n"
    "octal (prefix '0') or hexadecimal (prefix '0x') base.\n";

/****************************************************************************/

void listSlaveSdos(uint16_t, bool);

/****************************************************************************/

void command_sdos(void)
{
    masterDev.open(MasterDevice::Read);

    if (slavePosition == -1) {
        unsigned int numSlaves = masterDev.slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            listSlaveSdos(i, true);
        }
    } else {
        listSlaveSdos(slavePosition, false);
    }
}

/****************************************************************************/

void listSlaveSdos(
		uint16_t slavePosition,
		bool withHeader
		)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_slave_sdo_t sdo;
    ec_ioctl_slave_sdo_entry_t entry;
    unsigned int i, j;
    const CoEDataType *d;
    
    masterDev.getSlave(&slave, slavePosition);

    if (withHeader)
        cout << "=== Slave " << slavePosition << " ===" << endl;

    for (i = 0; i < slave.sdo_count; i++) {
        masterDev.getSdo(&sdo, slavePosition, i);

        cout << "Sdo 0x"
            << hex << setfill('0')
            << setw(4) << sdo.sdo_index
            << ", \"" << sdo.name << "\"" << endl;

        if (verbosity == Quiet)
            continue;

        for (j = 0; j <= sdo.max_subindex; j++) {
            masterDev.getSdoEntry(&entry, slavePosition, -i, j);

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
