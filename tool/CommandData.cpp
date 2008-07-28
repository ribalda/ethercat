/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
using namespace std;

#include "CommandData.h"

/*****************************************************************************/

CommandData::CommandData():
    Command("data", "Output binary domain process data.")
{
}

/*****************************************************************************/

string CommandData::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS]" << endl
    	<< endl
    	<< getBriefDescription() << endl
        << endl
        << "Data of multiple domains are concatenated." << endl
    	<< endl
    	<< "Command-specific options:" << endl
    	<< "  --domain -d <index>  Positive numerical domain index." << endl
    	<< "                       If omitted, data of all domains" << endl
    	<< "                       are output." << endl
    	<< endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandData::execute(MasterDevice &m, const StringVector &args)
{
	DomainList domains;
	DomainList::const_iterator di;
	
    m.open(MasterDevice::Read);
	domains = selectedDomains(m);

	for (di = domains.begin(); di != domains.end(); di++) {
		outputDomainData(m, *di);
	}
}

/****************************************************************************/

void CommandData::outputDomainData(
		MasterDevice &m,
		const ec_ioctl_domain_t &domain
		)
{
    ec_ioctl_domain_data_t data;
    unsigned char *processData;
    unsigned int i;
    
    if (!domain.data_size)
        return;

    processData = new unsigned char[domain.data_size];

    try {
        m.getData(&data, domain.index, domain.data_size, processData);
    } catch (MasterDeviceException &e) {
        delete [] processData;
        throw e;
    }

    for (i = 0; i < data.data_size; i++)
        cout << processData[i];
    cout.flush();

    delete [] processData;
}

/****************************************************************************/
