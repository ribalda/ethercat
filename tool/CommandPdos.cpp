/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandPdos.h"

/*****************************************************************************/

CommandPdos::CommandPdos():
    Command("pdos", "List Sync managers, Pdo assignment and mapping.")
{
}

/*****************************************************************************/

string CommandPdos::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS]" << endl
    	<< endl
    	<< getBriefDescription() << endl
        << endl
    	<< "The information is displayed in three layers, which are" << endl
    	<< "indented accordingly:" << endl
    	<< endl
    	<< "1) Sync managers - Contains the sync manager information" << endl
    	<< "   from the SII: Index, physical start address, default" << endl
    	<< "   size (value from the SII), control register and enable" << endl
    	<< "   word. Example:" << endl
		<< endl
    	<< "   SM3: PhysAddr 0x1100, DefaultSize 0, ControlRegister 0x20, "
		<< "Enable 1" << endl
    	<< endl
    	<< "2) Assigned Pdos - Pdo direction, hexadecimal index and" << endl
		<< "   the Pdo name, if avaliable. Note that a 'Tx' and 'Rx'" << endl
        << "   are seen from the slave's point of view. Example:" << endl
    	<< endl
    	<< "   TxPdo 0x1a00 \"Channel1\"" << endl
    	<< endl
    	<< "3) Mapped Pdo entries - Pdo entry index and subindex (both" << endl
    	<< "   hexadecimal), the length in bit and the description, if" << endl
    	<< "   available. Example:" << endl
    	<< endl
    	<< "   Pdo entry 0x3101:01, 8 bit, \"Status\"" << endl
    	<< endl
    	<< "Note, that the displayed Pdo assignment and Pdo mapping" << endl
    	<< "information can either originate from the SII or from the" << endl
		<< "CoE communication area." << endl
    	<< endl
    	<< "Command-specific options:" << endl
    	<< "  --slave -s <index>  Positive numerical ring position," << endl
    	<< "                      or 'all' for all slaves (default)." << endl
    	<< endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandPdos::execute(MasterDevice &m, const StringVector &args)
{
    m.open(MasterDevice::Read);

    if (slavePosition == -1) {
        unsigned int numSlaves = m.slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            listSlavePdos(m, i, true);
        }
    } else {
        listSlavePdos(m, slavePosition, false);
    }
}

/****************************************************************************/

void CommandPdos::listSlavePdos(
		MasterDevice &m,
		uint16_t slavePosition,
		bool withHeader
		)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_slave_sync_t sync;
    ec_ioctl_slave_sync_pdo_t pdo;
    ec_ioctl_slave_sync_pdo_entry_t entry;
    unsigned int i, j, k;
    
    m.getSlave(&slave, slavePosition);

    if (withHeader)
        cout << "=== Slave " << slavePosition << " ===" << endl;

    for (i = 0; i < slave.sync_count; i++) {
        m.getSync(&sync, slavePosition, i);

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
            m.getPdo(&pdo, slavePosition, i, j);

            cout << "  " << (sync.control_register & 0x04 ? "R" : "T")
                << "xPdo 0x"
                << hex << setfill('0')
                << setw(4) << pdo.index
                << " \"" << pdo.name << "\"" << endl;

            if (getVerbosity() == Quiet)
                continue;

            for (k = 0; k < pdo.entry_count; k++) {
                m.getPdoEntry(&entry, slavePosition, i, j, k);

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
