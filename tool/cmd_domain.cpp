/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "globals.h"

/*****************************************************************************/

const char *help_domains =
    "[OPTIONS]\n"
    "\n"
    "Show information about the application's configured domains.\n"
    "\n"
    "Without the --verbose option, the domains are displayed one-per-line.\n"
    "Example:\n"
    "\n"
    "Domain0: LogBaseAddr 0x00000000, Size   6, WorkingCounter 0/1\n"
    "\n"
    "The domain's base address for the logical datagram (LRD/LWR/LRW)\n"
    "is displayed followed by the domain's process data size in byte.\n"
    "The last values are the current datagram working counter sum and\n"
    "the expected working counter sum. If the values are equal, all\n"
    "Pdos are exchanged.\n"
    "\n"
    "If the --verbose option is given, the participating slave\n"
    "configurations/FMMUs and the current process data are additionally\n"
    "displayed:\n"
    "\n"
    "Domain1: LogBaseAddr 0x00000006, Size   6, WorkingCounter 0/1\n"
    "  SlaveConfig 1001:0, SM3 ( Input), LogAddr 0x00000006, Size 6\n"
    "    00 00 00 00 00 00\n"
    "\n"
    "The process data are displayed as hexadecimal bytes.\n"
    "\n"
    "Command-specific options:\n"
    "  --domain   -d <index> Positive numerical domain index, or 'all'\n"
    "                        for all domains (default).\n"
    "  --verbose  -v         Show FMMUs and process data additionally.\n"
    "\n"
    "Numerical values can be specified either with decimal (no prefix),\n"
    "octal (prefix '0') or hexadecimal (prefix '0x') base.\n";

/****************************************************************************/

void showDomain(unsigned int);

/****************************************************************************/

void command_domains(void)
{
    masterDev.open(MasterDevice::Read);

    if (domainIndex == -1) {
        unsigned int i;
        ec_ioctl_master_t master;

        masterDev.getMaster(&master);

        for (i = 0; i < master.domain_count; i++) {
            showDomain(i);
        }
    } else {
        showDomain(domainIndex);
    }
}

/****************************************************************************/

void showDomain(unsigned int domainIndex)
{
    ec_ioctl_domain_t domain;
    unsigned char *processData;
    ec_ioctl_domain_data_t data;
    unsigned int i, j;
    ec_ioctl_domain_fmmu_t fmmu;
    unsigned int dataOffset;
    
    masterDev.getDomain(&domain, domainIndex);

	cout << "Domain" << dec << domainIndex << ":"
		<< " LogBaseAddr 0x"
		<< hex << setfill('0')
        << setw(8) << domain.logical_base_address
		<< ", Size " << dec << setfill(' ')
        << setw(3) << domain.data_size
		<< ", WorkingCounter "
		<< domain.working_counter << "/"
        << domain.expected_working_counter << endl;

    if (!domain.data_size || verbosity != Verbose)
        return;

    processData = new unsigned char[domain.data_size];

    try {
        masterDev.getData(&data, domainIndex, domain.data_size, processData);
    } catch (MasterDeviceException &e) {
        delete [] processData;
        throw e;
    }

    for (i = 0; i < domain.fmmu_count; i++) {
        masterDev.getFmmu(&fmmu, domainIndex, i);

        cout << "  SlaveConfig "
            << dec << fmmu.slave_config_alias
            << ":" << fmmu.slave_config_position
            << ", SM" << (unsigned int) fmmu.sync_index << " ("
            << setfill(' ') << setw(6)
            << (fmmu.dir == EC_DIR_INPUT ? "Input" : "Output")
            << "), LogAddr 0x"
            << hex << setfill('0')
            << setw(8) << fmmu.logical_address
            << ", Size " << dec << fmmu.data_size << endl;

        dataOffset = fmmu.logical_address - domain.logical_base_address;
        if (dataOffset + fmmu.data_size > domain.data_size) {
            stringstream err;
            delete [] processData;
            err << "Fmmu information corrupted!";
            throw CommandException(err);
        }

        cout << "    " << hex << setfill('0');
        for (j = 0; j < fmmu.data_size; j++) {
            if (j && !(j % BreakAfterBytes))
                cout << endl << "    ";
            cout << setw(2)
                << (unsigned int) *(processData + dataOffset + j) << " ";
        }
        cout << endl;
    }

    delete [] processData;
}

/*****************************************************************************/
