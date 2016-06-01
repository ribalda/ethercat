/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2014  Gavin Lambert
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
#include <algorithm>
using namespace std;

#include "CommandReboot.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandReboot::CommandReboot():
    Command("reboot", "Request device hardware reboot.")
{
}

/*****************************************************************************/

string CommandReboot::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS] [all]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Arguments:" << endl
        << "  If \"all\" is specified then a broadcast reboot request will be sent." << endl
        << "  Otherwise a single-slave request will be sent." << endl
        << "  Slaves will only be rebooted if their hardware supports this." << endl
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

void CommandReboot::execute(const StringVector &args)
{
    MasterIndexList masterIndices;
    SlaveList slaves;
    uint8_t broadcast = 0;
    string argStr;
    stringstream err;

    if (args.size() > 1) {
        err << "Too many arguments.";
        throwInvalidUsageException(err);
    } else if (args.size() == 1) {
        argStr = args[0];
        transform(argStr.begin(), argStr.end(),
                argStr.begin(), (int (*) (int)) std::toupper);
        if (argStr != "ALL") {
            err << "Unexpected argument '" << args[0] << "'!";
            throwInvalidUsageException(err);
        }
        broadcast = 1;
    }

    masterIndices = getMasterIndices();
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        m.open(MasterDevice::ReadWrite);

        if (broadcast) {
            m.requestRebootAll();
        } else {
            slaves = selectedSlaves(m);
            if (slaves.empty())
                continue;

            if (slaves.size() > 1) {
                // only one slave can be rebooted at a time because it
                // will trigger a network topology change if successful.
                cerr << "More than one slave selected; ignoring extras."
                     << endl;
            }
            m.requestReboot(slaves.front().position);
        }
    }
}

/*****************************************************************************/
