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
    "\n"
    "Command-specific options:\n";

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
            err << "Fmmu information corrupted!";
            delete [] processData;
            throw MasterDeviceException(err.str());
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
