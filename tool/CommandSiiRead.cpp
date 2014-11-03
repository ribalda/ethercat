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

#include "CommandSiiRead.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandSiiRead::CommandSiiRead():
    Command("sii_read", "Output a slave's SII contents.")
{
}

/*****************************************************************************/

string CommandSiiRead::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "Without the --verbose option, binary SII contents are" << endl
        << "output." << endl
        << endl
        << "With the --verbose option given, a textual representation" << endl
        << "of the data is output, that is separated by SII category" << endl
        << "names." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --verbose  -v          Output textual data with" << endl
        << "                         category names." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandSiiRead::execute(const StringVector &args)
{
    SlaveList slaves;
    ec_ioctl_slave_t *slave;
    ec_ioctl_slave_sii_t data;
    unsigned int i;
    const uint16_t *categoryHeader;
    uint16_t categoryType, categorySize;
    stringstream err;

    if (args.size()) {
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);

    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    slave = &slaves.front();
    data.slave_position = slave->position;

    if (!slave->sii_nwords)
        return;

    data.offset = 0;
    data.nwords = slave->sii_nwords;
    data.words = new uint16_t[data.nwords];

    try {
        m.readSii(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.words;
        throw e;
    }

    if (getVerbosity() == Verbose) {
        cout << "SII Area:" << hex << setfill('0');
        for (i = 0; i < min(data.nwords, 0x0040U) * 2; i++) {
            if (i % BreakAfterBytes) {
                cout << " ";
            } else {
                cout << endl << "  ";
            }
            cout << setw(2) << (unsigned int) *((uint8_t *) data.words + i);
        }
        cout << endl;

        if (data.nwords > 0x0040U) {
            // cycle through categories
            categoryHeader = data.words + 0x0040U;
            categoryType = le16_to_cpup(categoryHeader);
            while (categoryType != 0xffff) {
                cout << "SII Category 0x" << hex
                    << setw(4) << categoryType
                    << " (" << getCategoryName(categoryType) << ")" << flush;

                if (categoryHeader + 1 > data.words + data.nwords) {
                    err << "SII data seem to be corrupted!";
                    throwCommandException(err);
                }
                categorySize = le16_to_cpup(categoryHeader + 1);
                cout << ", " << dec << categorySize << " words" << flush;

                if (categoryHeader + 2 + categorySize
                        > data.words + data.nwords) {
                    err << "SII data seem to be corrupted!";
                    throwCommandException(err);
                }

                cout << hex;
                for (i = 0; i < categorySize * 2U; i++) {
                    if (i % BreakAfterBytes) {
                        cout << " ";
                    } else {
                        cout << endl << "  ";
                    }
                    cout << setw(2) << (unsigned int)
                        *((uint8_t *) (categoryHeader + 2) + i);
                }
                cout << endl;

                if (categoryHeader + 2 + categorySize + 1
                        > data.words + data.nwords) {
                    err << "SII data seem to be corrupted!";
                    throwCommandException(err);
                }
                categoryHeader += 2 + categorySize;
                categoryType = le16_to_cpup(categoryHeader);
            }
        }
    } else {
        for (i = 0; i < data.nwords; i++) {
            uint16_t *w = data.words + i;
            cout << *(uint8_t *) w << *((uint8_t *) w + 1);
        }
    }

    delete [] data.words;
}

/****************************************************************************/

const CommandSiiRead::CategoryName CommandSiiRead::categoryNames[] = {
    {0x000a, "STRINGS"},
    {0x0014, "DataTypes"},
    {0x001e, "General"},
    {0x0028, "FMMU"},
    {0x0029, "SyncM"},
    {0x0032, "TXPDO"},
    {0x0033, "RXPDO"},
    {0x003c, "DC"},
    {}
};

/****************************************************************************/

const char *CommandSiiRead::getCategoryName(uint16_t type)
{
    const CategoryName *cn = categoryNames;

    while (cn->type) {
        if (cn->type == type) {
            return cn->name;
        }
        cn++;
    }

    return "unknown";
}

/*****************************************************************************/
