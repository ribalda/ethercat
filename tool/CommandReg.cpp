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

#include "CommandReg.h"

/*****************************************************************************/

CommandReg::CommandReg(const string &name, const string &briefDesc):
    Command(name, briefDesc)
{
}

/****************************************************************************/

const CommandReg::DataType *CommandReg::findDataType(
        const string &str
        )
{
    const DataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (str == d->name)
            return d;

    return NULL;
}

/****************************************************************************/

const CommandReg::DataType CommandReg::dataTypes[] = {
    {"int8",         1},
    {"int16",        2},
    {"int32",        4},
    {"int64",        8},
    {"uint8",        1},
    {"uint16",       2},
    {"uint32",       4},
    {"uint64",       8},
    {"string",       0},
    {"raw",          0},
    {}
};

/*****************************************************************************/
