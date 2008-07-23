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
    "<LEVEL>\n"
    "\n"
    "Set the master debug level.\n"
    "\n"
    "Debug messages are printed to syslog.\n"
    "\n"
    "Arguments:\n"
    "  LEVEL can have one of the following values:\n"
    "        0 for no debugging output,\n"
    "        1 for some debug messages, or\n"
    "        2 for printing all frame contents (use with caution!).\n"
    "\n"
    "Numerical values can be specified either with decimal (no prefix),\n"
    "octal (prefix '0') or hexadecimal (prefix '0x') base.\n";

/****************************************************************************/

void command_debug(void)
{
    stringstream str;
    int debugLevel;
    
    if (commandArgs.size() != 1) {
        stringstream err;
        err << "'" << commandName << "' takes exactly one argument!";
        throw InvalidUsageException(err);
    }

    str << commandArgs[0];
    str >> resetiosflags(ios::basefield) // guess base from prefix
        >> debugLevel;

    if (str.fail()) {
        stringstream err;
        err << "Invalid debug level '" << commandArgs[0] << "'!";
        throw InvalidUsageException(err);
    }

    masterDev.open(MasterDevice::ReadWrite);
    masterDev.setDebug(debugLevel);
}

/*****************************************************************************/
