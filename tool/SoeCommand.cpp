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

#include <iomanip>
using namespace std;

#include "SoeCommand.h"

/*****************************************************************************/

SoeCommand::SoeCommand(const string &name, const string &briefDesc):
    Command(name, briefDesc)
{
}

/*****************************************************************************/

uint16_t SoeCommand::parseIdn(const string &str)
{
    uint16_t idn = 0x0000;
    stringstream s, err;

    if (!str.length()) {
        err << "Zero-size string not allowed!";
        throw runtime_error(err.str());
    }

    if (str[0] == 'S' || str[0] == 'P') {
        unsigned int num;
        unsigned char c;

        s << str;

        s >> c;
        if (c == 'P') {
            idn |= 0x8000;
        }

        s >> c;
        if (s.fail() || c != '-') {
             err << "'-' expected!";
             throw runtime_error(err.str());
        }

        s >> num;
        if (s.fail() || num > 7) {
            err << "Invalid parameter set number!";
            throw runtime_error(err.str());
        }
        idn |= num << 12;

        s >> c;
        if (s.fail() || c != '-') {
             err << "'-' expected!";
             throw runtime_error(err.str());
        }

        s >> num;
        if (s.fail() || num > 4095) {
            err << "Invalid data block number!";
            throw runtime_error(err.str());
        }
        idn |= num;

        s.peek();
        if (!s.eof()) {
            err << "Additional input!";
            throw runtime_error(err.str());
        }
    } else {
        s << str;
        s >> resetiosflags(ios::basefield) >> idn;
        if (s.fail()) {
            err << "Invalid number!";
            throw runtime_error(err.str());
        }
    }

    return idn;
}

/****************************************************************************/
