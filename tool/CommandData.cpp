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
    	<< "  --domain -d <index>  Positive numerical domain index, or" << endl
    	<< "                       'all' for all domains (default)." << endl
    	<< endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandData::execute(MasterDevice &m, const StringVector &args)
{
    m.open(MasterDevice::Read);

    if (domainIndex == -1) {
        unsigned int i;
        ec_ioctl_master_t master;

        m.getMaster(&master);

        for (i = 0; i < master.domain_count; i++) {
            outputDomainData(m, i);
        }
    } else {
        outputDomainData(m, domainIndex);
    }
}

/****************************************************************************/

void CommandData::outputDomainData(MasterDevice &m, unsigned int domainIndex)
{
    ec_ioctl_domain_t domain;
    ec_ioctl_domain_data_t data;
    unsigned char *processData;
    unsigned int i;
    
    m.getDomain(&domain, domainIndex);

    if (!domain.data_size)
        return;

    processData = new unsigned char[domain.data_size];

    try {
        m.getData(&data, domainIndex, domain.data_size, processData);
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
