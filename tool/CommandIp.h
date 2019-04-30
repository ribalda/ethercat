/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2014  Florian Pose, Ingenieurgemeinschaft IgH
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


#ifndef __COMMANDIP_H__
#define __COMMANDIP_H__

#include "Command.h"

/****************************************************************************/

class CommandIp:
    public Command
{
    public:
        CommandIp();

        string helpString(const string &) const;
        void execute(const StringVector &);

    protected:
        static void parseMac(unsigned char [6], const string &);
        static void parseIpv4Prefix(ec_ioctl_slave_eoe_ip_t *,
                const string &);
        static void resolveIpv4(uint32_t *, const string &);
};

/****************************************************************************/

#endif
