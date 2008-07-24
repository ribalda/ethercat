/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <sstream>
#include <iomanip>
using namespace std;

#include "CommandDebug.h"

/*****************************************************************************/

CommandDebug::CommandDebug():
    Command("debug", "Set the master's debug level.")
{
}

/*****************************************************************************/

string CommandDebug::helpString() const
{
    stringstream str;

	str << getName() << " <LEVEL>" << endl
		<< endl
    	<< getBriefDescription() << endl
    	<< endl
    	<< "Debug messages are printed to syslog." << endl
    	<< endl
    	<< "Arguments:" << endl
    	<< "  LEVEL can have one of the following values:" << endl
    	<< "        0 for no debugging output," << endl
    	<< "        1 for some debug messages, or" << endl
    	<< "        2 for printing all frame contents (use with caution!)."
		<< endl << endl
    	<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandDebug::execute(MasterDevice &m, const StringVector &args)
{
    stringstream str;
    int debugLevel;
    
    if (args.size() != 1) {
        stringstream err;
        err << "'" << getName() << "' takes exactly one argument!";
        throwInvalidUsageException(err);
    }

    str << args[0];
    str >> resetiosflags(ios::basefield) // guess base from prefix
        >> debugLevel;

    if (str.fail()) {
        stringstream err;
        err << "Invalid debug level '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

    m.open(MasterDevice::ReadWrite);
    m.setDebug(debugLevel);
}

/*****************************************************************************/
