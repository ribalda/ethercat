/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "globals.h"

/****************************************************************************/

// FIXME
const char *help_pdos =
    "[OPTIONS]\n"
    "\n"
    "\n"
    "Command-specific options:\n";

/****************************************************************************/
	
void listSlavePdos(uint16_t, bool);

/****************************************************************************/

void command_pdos(void)
{
    masterDev.open(MasterDevice::Read);

    if (slavePosition == -1) {
        unsigned int numSlaves = masterDev.slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            listSlavePdos(i, true);
        }
    } else {
        listSlavePdos(slavePosition, false);
    }
}

/****************************************************************************/

void listSlavePdos(uint16_t slavePosition, bool withHeader)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_slave_sync_t sync;
    ec_ioctl_slave_sync_pdo_t pdo;
    ec_ioctl_slave_sync_pdo_entry_t entry;
    unsigned int i, j, k;
    
    masterDev.getSlave(&slave, slavePosition);

    if (withHeader)
        cout << "=== Slave " << slavePosition << " ===" << endl;

    for (i = 0; i < slave.sync_count; i++) {
        masterDev.getSync(&sync, slavePosition, i);

        cout << "SM" << i << ":"
            << " PhysAddr 0x"
            << hex << setfill('0')
            << setw(4) << sync.physical_start_address
            << ", DefaultSize "
            << dec << setfill(' ') << setw(4) << sync.default_size
            << ", ControlRegister 0x"
            << hex << setfill('0') << setw(2)
            << (unsigned int) sync.control_register
            << ", Enable " << dec << (unsigned int) sync.enable
            << endl;

        for (j = 0; j < sync.pdo_count; j++) {
            masterDev.getPdo(&pdo, slavePosition, i, j);

            cout << "  " << (sync.control_register & 0x04 ? "R" : "T")
                << "xPdo 0x"
                << hex << setfill('0')
                << setw(4) << pdo.index
                << " \"" << pdo.name << "\"" << endl;

            if (verbosity == Quiet)
                continue;

            for (k = 0; k < pdo.entry_count; k++) {
                masterDev.getPdoEntry(&entry, slavePosition, i, j, k);

                cout << "    Pdo entry 0x"
                    << hex << setfill('0')
                    << setw(4) << entry.index
                    << ":" << setw(2) << (unsigned int) entry.subindex
                    << ", " << dec << (unsigned int) entry.bit_length
                    << " bit, \"" << entry.name << "\"" << endl;
            }
        }
    }
}

/*****************************************************************************/
