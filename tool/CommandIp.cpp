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
 *  vim: expandtab
 *
 ****************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <iostream>
#include <algorithm>
using namespace std;

#include "CommandIp.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandIp::CommandIp():
    Command("ip", "Set EoE IP parameters.")
{
}

/*****************************************************************************/

string CommandIp::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS] <ARGS>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
        << endl
        << "IP parameters can be appended as argument pairs:" << endl
        << endl
        << "  addr <IPv4>[/prefix]  IP address (optionally with" << endl
        << "                        decimal subnet prefix)" << endl
        << "  link <MAC>            Link-layer address (may contain" << endl
        << "                        colons or hyphens)" << endl
        << "  default <IPv4>        Default gateway" << endl
        << "  dns <IPv4>            DNS server" << endl
        << "  name <hostname>       Host name (max. 32 byte)" << endl
        << endl
        << "IPv4 adresses can be given either in dot notation or as" << endl
        << "hostnames, which will be automatically resolved." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandIp::execute(const StringVector &args)
{
    if (args.size() <= 0) {
        return;
    }

    if (args.size() % 2) {
        stringstream err;
        err << "'" << getName() << "' needs an even number of arguments!";
        throwInvalidUsageException(err);
    }

    ec_ioctl_slave_eoe_ip_t io = {};

    for (unsigned int argIdx = 0; argIdx < args.size(); argIdx += 2) {
        string arg = args[argIdx];
        string val = args[argIdx + 1];
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

        if (arg == "link") {
            parseMac(io.mac_address, val);
            io.mac_address_included = 1;
        }
        else if (arg == "addr") {
            parseIpv4Prefix(&io, val);
            io.ip_address_included = 1;
        }
        else if (arg == "default") {
            resolveIpv4(&io.gateway, val);
            io.gateway_included = 1;
        }
        else if (arg == "dns") {
            resolveIpv4(&io.dns, val);
            io.dns_included = 1;
        }
        else if (arg == "name") {
            if (val.size() > EC_MAX_HOSTNAME_SIZE - 1) {
                stringstream err;
                err << "Name too long!";
                throwInvalidUsageException(err);
            }
            unsigned int i;
            for (i = 0; i < val.size(); i++) {
                io.name[i] = val[i];
            }
            io.name[i] = 0;
            io.name_included = 1;
        }
        else {
            stringstream err;
            err << "Unknown argument '" << args[argIdx] << "'!";
            throwInvalidUsageException(err);
        }
    }

    MasterDevice m(getSingleMasterIndex());
    m.open(MasterDevice::ReadWrite);
    SlaveList slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    io.slave_position = slaves.front().position;

    // execute actual request
    try {
        m.setIpParam(&io);
    } catch (MasterDeviceException &e) {
        throw e;
    }
}

/*****************************************************************************/

void CommandIp::parseMac(unsigned char mac[EC_ETH_ALEN], const string &str)
{
    unsigned int pos = 0;

    for (unsigned int i = 0; i < EC_ETH_ALEN; i++) {
        if (pos + 2 > str.size()) {
            stringstream err;
            err << "Incomplete MAC address!";
            throwInvalidUsageException(err);
        }

        string byteStr = str.substr(pos, 2);
        pos += 2;

        stringstream s;
        s << byteStr;
        unsigned int byteValue;
        s >> hex >> byteValue;
        if (s.fail() || !s.eof() || byteValue > 0xff) {
            stringstream err;
            err << "Invalid MAC address!";
            throwInvalidUsageException(err);
        }
        mac[i] = byteValue;

        while (pos < str.size() && (str[pos] == ':' || str[pos] == '-')) {
            pos++;
        }
    }
}

/*****************************************************************************/

void CommandIp::parseIpv4Prefix(ec_ioctl_slave_eoe_ip_t *io,
        const string &str)
{
    size_t pos = str.find('/');
    string host;

    io->subnet_mask_included = pos != string::npos;

    if (pos == string::npos) { // no prefix found
        host = str;
    }
    else {
        host = str.substr(0, pos);
        string prefixStr = str.substr(pos + 1, string::npos);
        stringstream s;
        s << prefixStr;
        unsigned int prefix;
        s >> prefix;
        if (s.fail() || !s.eof() || prefix > 32) {
            stringstream err;
            err << "Invalid prefix '" << prefixStr << "'!";
            throwInvalidUsageException(err);
        }
        uint32_t mask = 0;
        for (unsigned int bit = 0; bit < prefix; bit++) {
            mask |= (1 << (31 - bit));
        }
        io->subnet_mask = htonl(mask);
    }

    resolveIpv4(&io->ip_address, host);
}

/*****************************************************************************/

void CommandIp::resolveIpv4(uint32_t *addr, const string &str)
{
    struct addrinfo hints = {};
    struct addrinfo *res;

    hints.ai_family = AF_INET; // only IPv4

    int ret = getaddrinfo(str.c_str(), NULL, &hints, &res);
    if (ret) {
        stringstream err;
        err << "Lookup of '" << str << "' failed: "
            << gai_strerror(ret) << endl;
        throwCommandException(err.str());
    }

    if (!res) { // returned list is empty
        stringstream err;
        err << "Lookup of '" << str << "' failed." << endl;
        throwCommandException(err.str());
    }

    sockaddr_in *sin = (sockaddr_in *) res->ai_addr;
    for (unsigned int i = 0; i < 4; i++) {
        ((unsigned char *) addr)[i] =
            ((unsigned char *) &sin->sin_addr.s_addr)[i];
    }

    freeaddrinfo(res);
}

/****************************************************************************/
