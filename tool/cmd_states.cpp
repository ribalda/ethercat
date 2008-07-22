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
    "[OPTIONS]\n"
    "\n"
    "\n"
    "Command-specific options:\n";

/****************************************************************************/

void command_states(void)
{
    string stateStr;
    uint8_t state;
    
    if (commandArgs.size() != 1) {
        stringstream err;
        err << "'state' takes exactly one argument!";
        throw MasterDeviceException(err.str());
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
        stringstream err;
        err << "Invalid state '" << commandArgs[0] << "'!";
        throw MasterDeviceException(err.str());
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
