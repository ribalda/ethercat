/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2014  Florian Pose, Ingenieurgemeinschaft IgH
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
#include <string.h>
using namespace std;

#include "CommandXml.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandXml::CommandXml():
    Command("xml", "Generate slave information XML.")
{
}

/*****************************************************************************/

string CommandXml::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Note that the PDO information can either originate" << endl
        << "from the SII or from the CoE communication area. For" << endl
        << "slaves, that support configuring PDO assignment and" << endl
        << "mapping, the output depends on the last configuration." << endl
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

void CommandXml::execute(const StringVector &args)
{
    SlaveList slaves;
    SlaveList::const_iterator si;

    if (args.size()) {
        stringstream err;
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);

    cout << "<?xml version=\"1.0\" ?>" << endl;
    if (slaves.size() > 1) {
        cout << "<EtherCATInfoList>" << endl;
    }

    for (si = slaves.begin(); si != slaves.end(); si++) {
        generateSlaveXml(m, *si, slaves.size() > 1 ? 1 : 0);
    }

    if (slaves.size() > 1) {
        cout << "</EtherCATInfoList>" << endl;
    }
}

/****************************************************************************/

void CommandXml::generateSlaveXml(
        MasterDevice &m,
        const ec_ioctl_slave_t &slave,
        unsigned int indent
        )
{
    ec_ioctl_slave_sync_t sync;
    ec_ioctl_slave_sync_pdo_t pdo;
    string pdoType, in;
    ec_ioctl_slave_sync_pdo_entry_t entry;
    unsigned int i, j, k;

    for (i = 0; i < indent; i++) {
        in += "  ";
    }

    cout
        << in << "<EtherCATInfo>" << endl
        << in << "  <!-- Slave " << dec << slave.position << " -->" << endl
        << in << "  <Vendor>" << endl
        << in << "    <Id>" << slave.vendor_id << "</Id>" << endl
        << in << "  </Vendor>" << endl
        << in << "  <Descriptions>" << endl
        << in << "    <Devices>" << endl
        << in << "      <Device>" << endl
        << in << "        <Type ProductCode=\"#x"
        << hex << setfill('0') << setw(8) << slave.product_code
        << "\" RevisionNo=\"#x"
        << hex << setfill('0') << setw(8) << slave.revision_number
        << "\">" << slave.order << "</Type>" << endl;

    if (strlen(slave.name)) {
        cout
            << in << "        <Name><![CDATA["
            << slave.name
            << "]]></Name>" << endl;
    }

    for (i = 0; i < slave.sync_count; i++) {
        m.getSync(&sync, slave.position, i);

        cout
            << in << "        <Sm Enable=\""
            << dec << (unsigned int) sync.enable
            << "\" StartAddress=\"#x" << hex << sync.physical_start_address
            << "\" ControlByte=\"#x"
            << hex << (unsigned int) sync.control_register
            << "\" DefaultSize=\"" << dec << sync.default_size
            << "\" />" << endl;
    }

    for (i = 0; i < slave.sync_count; i++) {
        m.getSync(&sync, slave.position, i);

        for (j = 0; j < sync.pdo_count; j++) {
            m.getPdo(&pdo, slave.position, i, j);
            pdoType = (sync.control_register & 0x04 ? "R" : "T");
            pdoType += "xPdo"; // last 2 letters lowercase in XML!

            cout
                << in << "        <" << pdoType
                << " Sm=\"" << i << "\" Fixed=\"1\" Mandatory=\"1\">" << endl
                << in << "          <Index>#x"
                << hex << setfill('0') << setw(4) << pdo.index
                << "</Index>" << endl
                << in << "          <Name>" << pdo.name << "</Name>" << endl;

            for (k = 0; k < pdo.entry_count; k++) {
                m.getPdoEntry(&entry, slave.position, i, j, k);

                cout
                    << in << "          <Entry>" << endl
                    << in << "            <Index>#x"
                    << hex << setfill('0') << setw(4) << entry.index
                    << "</Index>" << endl;
                if (entry.index)
                    cout
                        << in << "            <SubIndex>"
                        << dec << (unsigned int) entry.subindex
                        << "</SubIndex>" << endl;

                cout
                    << in << "            <BitLen>"
                    << dec << (unsigned int) entry.bit_length
                    << "</BitLen>" << endl;

                if (entry.index) {
                    cout
                        << in << "            <Name>" << entry.name
                        << "</Name>" << endl
                        << in << "            <DataType>";

                    if (entry.bit_length == 1) {
                        cout << "BOOL";
                    } else if (!(entry.bit_length % 8)) {
                        if (entry.bit_length <= 64) {
                            cout << "UINT" << (unsigned int) entry.bit_length;
                        } else {
                            cout << "STRING("
                                << (unsigned int) (entry.bit_length / 8)
                                << ")";
                        }
                    } else {
                        cout << "BIT" << (unsigned int) entry.bit_length;
                    }

                    cout << "</DataType>" << endl;
                }

                cout << in << "          </Entry>" << endl;
            }

            cout
                << in << "        </" << pdoType << ">" << endl;
        }
    }

    cout
        << in << "      </Device>" << endl
        << in << "    </Devices>" << endl
        << in << "  </Descriptions>" << endl
        << in << "</EtherCATInfo>" << endl;
}

/*****************************************************************************/
