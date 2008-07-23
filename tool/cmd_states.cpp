/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
using namespace std;

#include "globals.h"

/****************************************************************************/

const char *help_states =
    "[OPTIONS] <STATE>\n"
    "\n"
    "Request an application-layer state change for the specified slaves.\n"
    "\n"
    "Arguments:\n"
    "  STATE can be 'INIT', 'PREOP', 'SAFEOP', or 'OP'\n"
    "\n"
    "Command-specific options:\n"
    "  --slave -s <index>  Positive numerical ring position, or 'all' for\n"
    "                      all slaves (default).\n"
    "\n"
    "Numerical values can be specified either with decimal (no prefix),\n"
    "octal (prefix '0') or hexadecimal (prefix '0x') base.\n";

/****************************************************************************/

void command_states(void)
{
    stringstream err;
    string stateStr;
    uint8_t state;
    
    if (commandArgs.size() != 1) {
        err << "'" << commandName << "' takes exactly one argument!";
        throw InvalidUsageException(err);
    }

    stateStr = commandArgs[0];
    transform(stateStr.begin(), stateStr.end(),
            stateStr.begin(), (int (*) (int)) std::toupper);

    if (stateStr == "INIT") {
        state = 0x01;
    } else if (stateStr == "PREOP") {
        state = 0x02;
    } else if (stateStr == "SAFEOP") {
        state = 0x04;
    } else if (stateStr == "OP") {
        state = 0x08;
    } else {
        err << "Invalid state '" << commandArgs[0] << "'!";
        throw InvalidUsageException(err);
    }

    masterDev.open(MasterDevice::ReadWrite);

    if (slavePosition == -1) {
        unsigned int i, numSlaves = masterDev.slaveCount();
        for (i = 0; i < numSlaves; i++)
            masterDev.requestState(i, state);
    } else {
        masterDev.requestState(slavePosition, state);
    }
}

/*****************************************************************************/
