/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *  vim: expandtab
 *
 ****************************************************************************/

#include <iostream>
using namespace std;

#include "CommandData.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandData::CommandData():
    Command("data", "Output binary domain process data.")
{
}

/*****************************************************************************/

string CommandData::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
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

void CommandData::execute(const StringVector &args)
{
    MasterIndexList masterIndices;
    DomainList domains;
    DomainList::const_iterator di;

    if (args.size()) {
        stringstream err;
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

    masterIndices = getMasterIndices();
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        ec_ioctl_master_t io;
        MasterDevice m(*mi);
        m.open(MasterDevice::Read);
        m.getMaster(&io);

        domains = selectedDomains(m, io);

        for (di = domains.begin(); di != domains.end(); di++) {
            outputDomainData(m, *di);
        }
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
