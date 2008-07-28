/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
using namespace std;

#include "CommandVersion.h"

/*****************************************************************************/

CommandVersion::CommandVersion():
    Command("version", "Show version information.")
{
}

/****************************************************************************/

string CommandVersion::helpString() const
{
    stringstream str;

	str << getName() << " [OPTIONS]" << endl
    	<< endl
    	<< getBriefDescription() << endl;

	return str.str();
}

/****************************************************************************/

void CommandVersion::execute(MasterDevice &m, const StringVector &args)
{
    cout << "IgH EtherCAT master " << EC_MASTER_VERSION << endl;
}

/*****************************************************************************/
