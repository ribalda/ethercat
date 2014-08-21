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
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandSdos.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandSdos::CommandSdos():
    SdoCommand("sdos", "List SDO dictionaries.")
{
}

/*****************************************************************************/

string CommandSdos::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "SDO dictionary information is displayed in two layers," << endl
        << "which are indented accordingly:" << endl
        << endl
        << "1) SDOs - Hexadecimal SDO index and the name. Example:" << endl
        << endl
        << "   SDO 0x1018, \"Identity object\"" << endl
        << endl
        << "2) SDO entries - SDO index and SDO entry subindex (both" << endl
        << "   hexadecimal) followed by the access rights (see" << endl
        << "   below), the data type, the length in bit, and the" << endl
        << "   description. Example:" << endl
        << endl
        << "   0x1018:01, rwrwrw, uint32, 32 bit, \"Vendor id\"" << endl
        << endl
        << "The access rights are specified for the AL states PREOP," << endl
        << "SAFEOP and OP. An 'r' means, that the entry is readable" << endl
        << "in the corresponding state, an 'w' means writable," << endl
        << "respectively. If a right is not granted, a dash '-' is" << endl
        << "shown." << endl
        << endl
        << "If the --quiet option is given, only the SDOs are output."
        << endl << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --quiet    -q          Only output SDOs (without the" << endl
        << "                         SDO entries)." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandSdos::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    SlaveList slaves;
    SlaveList::const_iterator si;
    bool showHeader, multiMaster;

    ec_ioctl_slave_dict_upload_t data;

    if (args.size()) {
        stringstream err;
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

	masterIndices = getMasterIndices();
    multiMaster = masterIndices.size() > 1;
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        m.open(MasterDevice::Read);
        slaves = selectedSlaves(m);
        for (si = slaves.begin(); si != slaves.end(); si++) {
            if (si->coe_details.enable_sdo_info) {
                data.slave_position = si->position;
                try {
                    m.dictUpload(&data);
                } catch (MasterDeviceException &e) {
                    throw e;
                }
            }
        }
        m.close();
        m.open(MasterDevice::Read);
        slaves = selectedSlaves(m);
        showHeader = multiMaster || slaves.size() > 1;

        for (si = slaves.begin(); si != slaves.end(); si++) {
            listSlaveSdos(m, *si, showHeader);
        }
    }
}

/****************************************************************************/

void CommandSdos::listSlaveSdos(
        MasterDevice &m,
        const ec_ioctl_slave_t &slave,
        bool showHeader
        )
{
    ec_ioctl_slave_sdo_t sdo;
    ec_ioctl_slave_sdo_entry_t entry;
    unsigned int i, j;
    const DataType *d;

    if (showHeader && slave.sdo_count)
        cout << "=== Master " << m.getIndex()
            << ", Slave " << slave.position << " ===" << endl;

    for (i = 0; i < slave.sdo_count; i++) {
        m.getSdo(&sdo, slave.position, i);

        cout << "SDO 0x"
            << hex << setfill('0')
            << setw(4) << sdo.sdo_index
            << ", \"" << sdo.name << "\"" << endl;

        if (getVerbosity() == Quiet)
            continue;

        for (j = 0; j <= sdo.max_subindex; j++) {
            try {
                m.getSdoEntry(&entry, slave.position, -i, j);
            }
            catch (MasterDeviceException &e) {
                continue;
            }

            cout << "  0x" << hex << setfill('0')
                << setw(4) << sdo.sdo_index << ":"
                << setw(2) << (unsigned int) entry.sdo_entry_subindex
                << ", "
                << (entry.read_access[EC_SDO_ENTRY_ACCESS_PREOP] ? "r" : "-")
                << (entry.write_access[EC_SDO_ENTRY_ACCESS_PREOP] ? "w" : "-")
                << (entry.read_access[EC_SDO_ENTRY_ACCESS_SAFEOP] ? "r" : "-")
                << (entry.write_access[EC_SDO_ENTRY_ACCESS_SAFEOP] ? "w" : "-")
                << (entry.read_access[EC_SDO_ENTRY_ACCESS_OP] ? "r" : "-")
                << (entry.write_access[EC_SDO_ENTRY_ACCESS_OP] ? "w" : "-")
                << ", ";

            if ((d = findDataType(entry.data_type))) {
                cout << d->name;
            } else {
                cout << "type " << setw(4) << entry.data_type;
            }

            cout << ", " << dec << entry.bit_length << " bit, \""
                << entry.description << "\"" << endl;
        }
    }
}

/*****************************************************************************/
