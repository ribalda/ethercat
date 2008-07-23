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
    "  LEVEL must be an unsigned integer, specified\n"
    "        either in decimal (no prefix), octal (prefix '0')\n"
    "        or hexadecimal (prefix '0x').\n"
    "        0 stands for no debugging output,\n"
    "        1 means some debug messages, and\n"
    "        2 outputs all frame data (use with caution!).\n";

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
