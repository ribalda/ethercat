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
#include <algorithm>
using namespace std;

#include "CommandStates.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandStates::CommandStates():
    Command("states", "Request application-layer states.")
{
}

/*****************************************************************************/

string CommandStates::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS] <STATE>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Arguments:" << endl
        << "  STATE can be 'INIT', 'PREOP', 'BOOT', 'SAFEOP', or 'OP'." << endl
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

void CommandStates::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    SlaveList slaves;
    SlaveList::const_iterator si;
    stringstream err;
    string stateStr;
    uint8_t state = 0x00;

    if (args.size() != 1) {
        err << "'" << getName() << "' takes exactly one argument!";
        throwInvalidUsageException(err);
    }

    stateStr = args[0];
    transform(stateStr.begin(), stateStr.end(),
            stateStr.begin(), (int (*) (int)) std::toupper);

    if (stateStr == "INIT") {
        state = 0x01;
    } else if (stateStr == "PREOP") {
        state = 0x02;
    } else if (stateStr == "BOOT") {
        state = 0x03;
    } else if (stateStr == "SAFEOP") {
        state = 0x04;
    } else if (stateStr == "OP") {
        state = 0x08;
    } else {
        err << "Invalid state '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

	masterIndices = getMasterIndices();
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        m.open(MasterDevice::ReadWrite);
        slaves = selectedSlaves(m);

        for (si = slaves.begin(); si != slaves.end(); si++) {
            m.requestState(si->position, state);
        }
    }
}

/*****************************************************************************/
