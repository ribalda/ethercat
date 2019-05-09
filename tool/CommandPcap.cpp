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

#include "CommandPcap.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandPcap::CommandPcap():
    Command("pcap", "Output binary pcap capture data.")
{
}

/*****************************************************************************/

string CommandPcap::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Command-specific options:" << endl
        << "  --reset  -r   Flushes the retrieved pcap data and continues logging." << endl
        << endl;

    return str.str();
}

/****************************************************************************/

void CommandPcap::execute(const StringVector &args)
{
    MasterIndexList masterIndices;

    if (args.size() > 1) {
        stringstream err;
        err << "'" << getName() << "' takes max one argument!";
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

        outputPcapData(m, io.pcap_size);
    }
}

/****************************************************************************/

void CommandPcap::outputPcapData(
        MasterDevice &m,
        unsigned int pcap_size
        )
{
    ec_ioctl_pcap_data_t data;
    unsigned char pcap_reset = getReset();
    unsigned char *pcap_data;
    unsigned int i;

    if (!pcap_size)
        return;
      
    pcap_data = new unsigned char[pcap_size];

    try {
        m.getPcap(&data, pcap_reset, pcap_size, pcap_data);
    } catch (MasterDeviceException &e) {
        delete [] pcap_data;
        throw e;
    }

    for (i = 0; i < data.data_size; i++)
        cout << pcap_data[i];
    cout.flush();

    delete [] pcap_data;
}

/****************************************************************************/
