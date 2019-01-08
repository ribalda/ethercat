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
#include <iomanip>
using namespace std;

#include "CommandEoeDelIf.h"
#include "CommandSlaves.h"
#include "MasterDevice.h"
#include "NumberListParser.h"

/*****************************************************************************/

class NumberParser:
    public NumberListParser
{
    public:
        NumberParser() {};

    protected:
        int getMax() {
            return 0;
        };
};

/*****************************************************************************/

CommandEoeDelIf::CommandEoeDelIf():
    Command("eoe_delif", "Remove an EOE interface from a master.")
{
}

/****************************************************************************/

string CommandEoeDelIf::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Delete an EOE network interface for the given" << endl
        << "slave alias / position." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --master   -m <indices>  Master index" << endl
        << "  --alias    -a <alias>    Slave alias" << endl
        << "  --position -p <pos>      Slave position" << endl
        << endl << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandEoeDelIf::execute(const StringVector &args)
{
    MasterIndexList masterIndices;
    SlaveList slaves;
    ec_ioctl_master_t master;
    uint16_t alias = 0;
    uint16_t posn = 0;
    stringstream err;

    if (args.size()) {
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }
    
    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::ReadWrite);
    slaves = selectedSlaves(m);
    
    m.getMaster(&master);

    // get alias
    NumberParser ap;
    NumberListParser::List aliasList = ap.parse(aliases.c_str());
    
    // get position
    NumberParser pp;
    NumberListParser::List posList = pp.parse(positions.c_str());

    if ( (aliases != "-") && (aliasList.size() == 1) && 
         (positions == "-") ) {
        alias = aliasList.front();
        posn  = 0;
    } else if ( (aliases == "-") && 
                (positions != "-") && (posList.size() == 1) ) {
        alias = 0;
        posn  = posList.front();
    } else {
        stringstream err;
        err << getName() << " requires a single alias or position!";
        throwInvalidUsageException(err);
    }    
    
    m.delEoeIf(alias, posn);
}

/*****************************************************************************/
