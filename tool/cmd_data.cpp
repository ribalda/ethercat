/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
using namespace std;

#include "globals.h"

/*****************************************************************************/

const char *help_data =
    "[OPTIONS]\n"
    "\n"
    "Output binary domain data.\n"
    "\n"
    "Command-specific options:\n"
    "  --domain -d <index> Positive numerical domain index. If this option\n"
    "                      is not specified, data of all domains are\n"
    "                      output.\n";

/****************************************************************************/

void outputDomainData(unsigned int);

/****************************************************************************/

void command_data()
{
    masterDev.open(MasterDevice::Read);

    if (domainIndex == -1) {
        unsigned int i;
        ec_ioctl_master_t master;

        masterDev.getMaster(&master);

        for (i = 0; i < master.domain_count; i++) {
            outputDomainData(i);
        }
    } else {
        outputDomainData(domainIndex);
    }
}

/****************************************************************************/

void outputDomainData(unsigned int domainIndex)
{
    ec_ioctl_domain_t domain;
    ec_ioctl_domain_data_t data;
    unsigned char *processData;
    unsigned int i;
    
    masterDev.getDomain(&domain, domainIndex);

    if (!domain.data_size)
        return;

    processData = new unsigned char[domain.data_size];

    try {
        masterDev.getData(&data, domainIndex, domain.data_size, processData);
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
