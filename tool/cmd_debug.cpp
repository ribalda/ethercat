/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <sstream>
#include <iomanip>
using namespace std;

#include "globals.h"

/*****************************************************************************/

const char *help_debug =
    "[OPTIONS]\n"
    "\n"
    "\n"
    "Command-specific options:\n";

/****************************************************************************/

void command_debug(void)
{
    stringstream str;
    int debugLevel;
    
    if (commandArgs.size() != 1) {
        stringstream err;
        err << "'debug' takes exactly one argument!";
        throw MasterDeviceException(err.str());
    }

    str << commandArgs[0];
    str >> resetiosflags(ios::basefield) // guess base from prefix
        >> debugLevel;

    if (str.fail()) {
        stringstream err;
        err << "Invalid debug level '" << commandArgs[0] << "'!";
        throw MasterDeviceException(err.str());
    }

    masterDev.open(MasterDevice::ReadWrite);
    masterDev.setDebug(debugLevel);
}

/*****************************************************************************/
