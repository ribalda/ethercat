/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandDomains.h"

/*****************************************************************************/

CommandDomains::CommandDomains():
    Command("domains", "Show configured domains.")
{
}

/*****************************************************************************/

string CommandDomains::helpString() const
{
    stringstream str;

	str << getName() << " [OPTIONS]" << endl
    	<< endl
    	<< getBriefDescription() << endl
        << endl
    	<< "Without the --verbose option, the domains are displayed" << endl
        << "one-per-line. Example:" << endl
    	<< endl
    	<< "Domain0: LogBaseAddr 0x00000000, Size   6, WorkingCounter 0/1"
		<< endl << endl
    	<< "The domain's base address for the logical datagram" << endl
    	<< "(LRD/LWR/LRW) is displayed followed by the domain's" << endl
    	<< "process data size in byte. The last values are the current" << endl
    	<< "datagram working counter sum and the expected working" << endl
    	<< "counter sum. If the values are equal, all Pdos were exchanged."
		<< endl << endl
    	<< "If the --verbose option is given, the participating slave" << endl
    	<< "configurations/FMMUs and the current process data are" << endl
    	<< "additionally displayed:" << endl
    	<< endl
    	<< "Domain1: LogBaseAddr 0x00000006, Size   6, WorkingCounter 0/1"
		<< endl
    	<< "  SlaveConfig 1001:0, SM3 ( Input), LogAddr 0x00000006, Size 6"
		<< endl
    	<< "    0x00 0x00 0x00 0x00 0x00 0x00" << endl
    	<< endl
    	<< "The process data are displayed as hexadecimal bytes." << endl
    	<< endl
    	<< "Command-specific options:" << endl
    	<< "  --domain  -d <index>  Positive numerical domain index," << endl
    	<< "                        or 'all' for all domains (default)."
		<< endl
    	<< "  --verbose -v          Show FMMUs and process data" << endl
		<< "                        in addition." << endl
    	<< endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandDomains::execute(MasterDevice &m, const StringVector &args)
{
	DomainList domains;
	DomainList::const_iterator di;
	
    m.open(MasterDevice::Read);
	domains = selectedDomains(m);

	for (di = domains.begin(); di != domains.end(); di++) {
		showDomain(m, *di);
	}
}

/****************************************************************************/

void CommandDomains::showDomain(
		MasterDevice &m,
		const ec_ioctl_domain_t &domain
		)
{
    unsigned char *processData;
    ec_ioctl_domain_data_t data;
    unsigned int i, j;
    ec_ioctl_domain_fmmu_t fmmu;
    unsigned int dataOffset;
    
	cout << "Domain" << dec << domain.index << ":"
		<< " LogBaseAddr 0x"
		<< hex << setfill('0')
        << setw(8) << domain.logical_base_address
		<< ", Size " << dec << setfill(' ')
        << setw(3) << domain.data_size
		<< ", WorkingCounter "
		<< domain.working_counter << "/"
        << domain.expected_working_counter << endl;

    if (!domain.data_size || getVerbosity() != Verbose)
        return;

    processData = new unsigned char[domain.data_size];

    try {
        m.getData(&data, domain.index, domain.data_size, processData);
    } catch (MasterDeviceException &e) {
        delete [] processData;
        throw e;
    }

    for (i = 0; i < domain.fmmu_count; i++) {
        m.getFmmu(&fmmu, domain.index, i);

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
            throwCommandException(err);
        }

        cout << "    " << hex << setfill('0');
        for (j = 0; j < fmmu.data_size; j++) {
            if (j && !(j % BreakAfterBytes))
                cout << endl << "    ";
            cout << "0x" << setw(2)
                << (unsigned int) *(processData + dataOffset + j) << " ";
        }
        cout << endl;
    }

    delete [] processData;
}

/*****************************************************************************/
