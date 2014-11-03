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
#include <iomanip>
#include <string.h>
using namespace std;

#include "CommandCStruct.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandCStruct::CommandCStruct():
    Command("cstruct", "Generate slave PDO information in C language.")
{
}

/*****************************************************************************/

string CommandCStruct::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "The output C code can be used directly with the" << endl
        << "ecrt_slave_config_pdos() function of the application" << endl
        << "interface." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandCStruct::execute(const StringVector &args)
{
    MasterIndexList masterIndices;
    SlaveList slaves;
    SlaveList::const_iterator si;

    if (args.size()) {
        stringstream err;
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

    masterIndices = getMasterIndices();
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        m.open(MasterDevice::Read);
        slaves = selectedSlaves(m);

        for (si = slaves.begin(); si != slaves.end(); si++) {
            generateSlaveCStruct(m, *si);
        }
    }
}

/****************************************************************************/

void CommandCStruct::generateSlaveCStruct(
        MasterDevice &m,
        const ec_ioctl_slave_t &slave
        )
{
    ec_ioctl_slave_sync_t sync;
    ec_ioctl_slave_sync_pdo_t pdo;
    ec_ioctl_slave_sync_pdo_entry_t entry;
    unsigned int i, j, k, pdo_pos = 0, entry_pos = 0;
    stringstream id, syncs, pdos, entries;

    if (!slave.sync_count)
        return;

    id << "slave_" << dec << slave.position << "_";

    for (i = 0; i < slave.sync_count; i++) {
        m.getSync(&sync, slave.position, i);

        syncs << "    {" << dec << sync.sync_index
            << ", " << (EC_READ_BIT(&sync.control_register, 2) ?
                    "EC_DIR_OUTPUT" : "EC_DIR_INPUT")
            << ", " << dec << (unsigned int) sync.pdo_count
            << ", ";
        if (sync.pdo_count) {
            syncs << id.str() << "pdos + " << dec << pdo_pos;
        } else {
            syncs << "NULL";
        }
        syncs << ", " << (EC_READ_BIT(&sync.control_register, 6) ?
                "EC_WD_ENABLE" : "EC_WD_DISABLE")
            << "},";
        syncs << endl;
        pdo_pos += sync.pdo_count;

        for (j = 0; j < sync.pdo_count; j++) {
            m.getPdo(&pdo, slave.position, i, j);

            pdos << "    {0x" << hex << setfill('0')
                << setw(4) << pdo.index
                << ", " << dec << (unsigned int) pdo.entry_count
                << ", ";
                if (pdo.entry_count) {
                    pdos << id.str() << "pdo_entries + " << dec << entry_pos;
                } else {
                    pdos << "NULL";
                }
            pdos << "},";
            if (strlen((const char *) pdo.name)) {
                pdos << " /* " << pdo.name << " */";
            }
            pdos << endl;
            entry_pos += pdo.entry_count;

            for (k = 0; k < pdo.entry_count; k++) {
                m.getPdoEntry(&entry, slave.position, i, j, k);

                entries << "    {0x" << hex << setfill('0')
                    << setw(4) << entry.index
                    << ", 0x" << setw(2) << (unsigned int) entry.subindex
                    << ", " << dec << (unsigned int) entry.bit_length
                    << "},";
                if (strlen((const char *) entry.name)) {
                    entries << " /* " << entry.name << " */";
                }
                entries << endl;
            }
        }
    }

    cout
        << "/* Master " << m.getIndex() << ", Slave " << slave.position;
    if (strlen(slave.order)) {
        cout << ", \"" << slave.order << "\"";
    }

    cout << endl
        << " * Vendor ID:       0x" << hex << setfill('0')
        << setw(8) << slave.vendor_id << endl
        << " * Product code:    0x" << hex << setfill('0')
        << setw(8) << slave.product_code << endl
        << " * Revision number: 0x" << hex << setfill('0')
        << setw(8) << slave.revision_number << endl
        << " */" << endl
        << endl;

    if (entry_pos) {
        cout << "ec_pdo_entry_info_t " << id.str()
            << "pdo_entries[] = {" << endl
            << entries.str()
            << "};" << endl
            << endl;
    }

    if (pdo_pos) {
        cout << "ec_pdo_info_t " << id.str() << "pdos[] = {" << endl
            << pdos.str()
            << "};" << endl
            << endl;
    }

    cout << "ec_sync_info_t " << id.str() << "syncs[] = {" << endl
        << syncs.str()
        << "    {0xff}" << endl
        << "};" << endl
        << endl;
}

/*****************************************************************************/
