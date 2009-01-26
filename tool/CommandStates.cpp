/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <algorithm>
using namespace std;

#include "CommandStates.h"

/*****************************************************************************/

CommandStates::CommandStates():
    Command("states", "Request application-layer states.")
{
}

/*****************************************************************************/

string CommandStates::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <STATE>" << endl
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

void CommandStates::execute(MasterDevice &m, const StringVector &args)
{
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

    m.open(MasterDevice::ReadWrite);
    slaves = selectedSlaves(m);

    if (!slaves.size() && getVerbosity() != Quiet) {
        cerr << "Warning: Selection matches no slaves!" << endl;
    }

    for (si = slaves.begin(); si != slaves.end(); si++) {
        m.requestState(si->position, state);
    }
}

/*****************************************************************************/
