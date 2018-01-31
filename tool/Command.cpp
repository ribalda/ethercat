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
 *  vim: expandtab
 *
 ****************************************************************************/

#include <map>
#include <iostream>
using namespace std;

#include "Command.h"
#include "MasterDevice.h"
#include "NumberListParser.h"

/*****************************************************************************/

typedef map<uint16_t, ec_ioctl_config_t> AliasMap;
typedef map<uint16_t, AliasMap> ConfigMap;

/*****************************************************************************/

class MasterIndexParser:
    public NumberListParser
{
    protected:
        int getMax() {
            MasterDevice dev;
            dev.setIndex(0U);
            dev.open(MasterDevice::Read);
            return (int) dev.getMasterCount() - 1;
        };
};

/*****************************************************************************/

class SlaveAliasParser:
    public NumberListParser
{
    public:
        SlaveAliasParser(ec_ioctl_master_t &master, MasterDevice &dev):
            master(master), dev(dev) {}

    protected:
        int getMax() {
            unsigned int i;

            uint16_t maxAlias = 0;
            for (i = 0; i < master.slave_count; i++) {
                ec_ioctl_slave_t slave;
                dev.getSlave(&slave, i);
                if (slave.alias > maxAlias) {
                    maxAlias = slave.alias;
                }
            }
            return maxAlias ? maxAlias : -1;
        };

    private:
        ec_ioctl_master_t &master;
        MasterDevice &dev;
};

/*****************************************************************************/

class ConfigAliasParser:
    public NumberListParser
{
    public:
        ConfigAliasParser(unsigned int maxAlias):
            maxAlias(maxAlias) {}

    protected:
        int getMax() { return maxAlias; };

    private:
        unsigned int maxAlias;
};

/*****************************************************************************/

class PositionParser:
    public NumberListParser
{
    public:
        PositionParser(unsigned int count):
            count(count) {}

    protected:
        int getMax() {
            return count - 1;
        };

    private:
        const unsigned int count;
};

/*****************************************************************************/

class AliasPositionParser:
    public NumberListParser
{
    public:
        AliasPositionParser(const AliasMap &aliasMap):
            aliasMap(aliasMap) {}

    protected:
        int getMax() {
            AliasMap::const_iterator i;
            int maxPos = -1;

            for (i = aliasMap.begin(); i != aliasMap.end(); i++) {
                if (i->first > maxPos) {
                    maxPos = i->first;
                }
            }

            return maxPos;
        };

    private:
        const AliasMap &aliasMap;
};

/*****************************************************************************/

Command::Command(const string &name, const string &briefDesc):
    name(name),
    briefDesc(briefDesc),
    verbosity(Normal),
    emergency(false),
    force(false),
    reset(false)
{
}

/*****************************************************************************/

Command::~Command()
{
}

/*****************************************************************************/

void Command::setMasters(const string &m)
{
    masters = m;
};

/*****************************************************************************/

void Command::setVerbosity(Verbosity v)
{
    verbosity = v;
};

/*****************************************************************************/

void Command::setAliases(const string &a)
{
    aliases = a;
};

/*****************************************************************************/

void Command::setPositions(const string &p)
{
    positions = p;
};

/*****************************************************************************/

void Command::setDomains(const string &d)
{
    domains = d;
};

/*****************************************************************************/

void Command::setDataType(const string &t)
{
    dataType = t;
};

/*****************************************************************************/

void Command::setEmergency(bool e)
{
    emergency = e;
};

/*****************************************************************************/

void Command::setForce(bool f)
{
    force = f;
};

/*****************************************************************************/

void Command::setReset(bool r)
{
    reset = r;
};

/*****************************************************************************/

void Command::setOutputFile(const string &f)
{
    outputFile = f;
};

/*****************************************************************************/

void Command::setSkin(const string &s)
{
    skin = s;
};

/****************************************************************************/

bool Command::matches(const string &cmd) const
{
    return name == cmd;
}

/****************************************************************************/

bool Command::matchesSubstr(const string &cmd) const
{
    return name.substr(0, cmd.length()) == cmd;
}

/****************************************************************************/

bool Command::matchesAbbrev(const string &abb) const
{
    unsigned int i;
    size_t pos = 0;

    for (i = 0; i < abb.length(); i++) {
        pos = name.find(abb[i], pos);
        if (pos == string::npos)
            return false;
    }

    return true;
}

/*****************************************************************************/

string Command::numericInfo()
{
    stringstream str;

    str << "Numerical values can be specified either with decimal (no" << endl
        << "prefix), octal (prefix '0') or hexadecimal (prefix '0x') base."
        << endl;

    return str.str();
}

/*****************************************************************************/

void Command::throwInvalidUsageException(const stringstream &s)
{
    throw InvalidUsageException(s);
}

/*****************************************************************************/

void Command::throwCommandException(const string &msg)
{
    throw CommandException(msg);
}

/*****************************************************************************/

void Command::throwCommandException(const stringstream &s)
{
    throw CommandException(s);
}

/*****************************************************************************/

void Command::throwSingleSlaveRequired(unsigned int size) const
{
    stringstream err;

    err << "The slave selection matches " << size << " slaves. '"
        << name << "' requires a single slave.";

    throwInvalidUsageException(err);
}

/*****************************************************************************/

Command::MasterIndexList Command::getMasterIndices() const
{
    MasterIndexList indices;

    try {
        MasterIndexParser p;
        indices = p.parse(masters.c_str());
    } catch (MasterDeviceException &e) {
        stringstream err;
        err << "Failed to obtain number of masters: " << e.what();
        throwCommandException(err);
    } catch (runtime_error &e) {
        stringstream err;
        err << "Invalid master argument '" << masters << "': " << e.what();
        throwInvalidUsageException(err);
    }

    return indices;
}

/*****************************************************************************/

unsigned int Command::getSingleMasterIndex() const
{
    MasterIndexList masterIndices = getMasterIndices();

    if (masterIndices.size() != 1) {
        stringstream err;
        err << getName() << " requires to select a single master!";
        throwInvalidUsageException(err);
    }

    return masterIndices.front();
}

/*****************************************************************************/

Command::SlaveList Command::selectedSlaves(MasterDevice &m)
{
    ec_ioctl_master_t master;
    unsigned int i;
    ec_ioctl_slave_t slave;
    SlaveList list;

    m.getMaster(&master);

    if (aliases == "-") { // no alias given
        PositionParser pp(master.slave_count);
        NumberListParser::List posList = pp.parse(positions.c_str());
        NumberListParser::List::const_iterator pi;

        for (pi = posList.begin(); pi != posList.end(); pi++) {
            if (*pi < master.slave_count) {
                m.getSlave(&slave, *pi);
                list.push_back(slave);
            }
        }
    } else { // aliases given
        SlaveAliasParser ap(master, m);
        NumberListParser::List aliasList = ap.parse(aliases.c_str());
        NumberListParser::List::const_iterator ai;

        for (ai = aliasList.begin(); ai != aliasList.end(); ai++) {

            // gather slaves with that alias (and following)
            uint16_t lastAlias = 0;
            vector<ec_ioctl_slave_t> aliasSlaves;

            for (i = 0; i < master.slave_count; i++) {
                m.getSlave(&slave, i);
                if (slave.alias) {
                    if (lastAlias && lastAlias == *ai && slave.alias != *ai) {
                        // ignore multiple ocurrences of the same alias to
                        // assure consistency for the position argument
                        break;
                    }
                    lastAlias = slave.alias;
                }
                if (lastAlias == *ai) {
                    aliasSlaves.push_back(slave);
                }
            }

            PositionParser pp(aliasSlaves.size());
            NumberListParser::List posList = pp.parse(positions.c_str());
            NumberListParser::List::const_iterator pi;

            for (pi = posList.begin(); pi != posList.end(); pi++) {
                if (*pi < aliasSlaves.size()) {
                    list.push_back(aliasSlaves[*pi]);
                }
            }
        }
    }

    return list;
}

/*****************************************************************************/

bool operator<(
        const ec_ioctl_config_t &a,
        const ec_ioctl_config_t &b
        )
{
    return a.alias < b.alias
        || (a.alias == b.alias && a.position < b.position);
}

/*****************************************************************************/

Command::ConfigList Command::selectedConfigs(MasterDevice &m)
{
    unsigned int i;
    ec_ioctl_master_t master;
    ec_ioctl_config_t config;
    ConfigList list;
    stringstream err;

    m.getMaster(&master);

    if (aliases == "-" && positions == "-") { // shortcut
        for (i = 0; i < master.config_count; i++) {
            m.getConfig(&config, i);
            list.push_back(config);
        }
    } else { // take the long way home...
        ConfigMap configs;
        uint16_t maxAlias = 0;

        // fill cascaded map structure with all configs
        for (i = 0; i < master.config_count; i++) {
            m.getConfig(&config, i);
            AliasMap &aliasMap = configs[config.alias];
            aliasMap[config.position] = config;
            if (config.alias > maxAlias) {
                maxAlias = config.alias;
            }
        }

        ConfigAliasParser ap(maxAlias);
        NumberListParser::List aliasList = ap.parse(aliases.c_str());
        NumberListParser::List::const_iterator ai;

        for (ai = aliasList.begin(); ai != aliasList.end(); ai++) {

            ConfigMap::iterator ci = configs.find(*ai);
            if (ci == configs.end()) {
                continue;
            }

            AliasMap &aliasMap = configs[*ai];
            AliasPositionParser pp(aliasMap);
            NumberListParser::List posList = pp.parse(positions.c_str());
            NumberListParser::List::const_iterator pi;

            for (pi = posList.begin(); pi != posList.end(); pi++) {
                AliasMap::const_iterator ci;

                ci = aliasMap.find(*pi);
                if (ci != aliasMap.end()) {
                    list.push_back(ci->second);
                }
            }
        }
    }

    list.sort();
    return list;
}

/****************************************************************************/

Command::DomainList Command::selectedDomains(MasterDevice &m,
        const ec_ioctl_master_t &io)
{
    DomainList list;

    PositionParser pp(io.domain_count);
    NumberListParser::List domList = pp.parse(domains.c_str());
    NumberListParser::List::const_iterator di;

    for (di = domList.begin(); di != domList.end(); di++) {
        if (*di < io.domain_count) {
            ec_ioctl_domain_t d;
            m.getDomain(&d, *di);
            list.push_back(d);
        }
    }

    return list;
}

/****************************************************************************/

int Command::emergencySlave() const
{
    unsigned int ret;

    stringstream str;
    str << positions;
    str >> ret;

    return ret;
}

/****************************************************************************/

string Command::alStateString(uint8_t state)
{
    string ret;

    switch (state & EC_SLAVE_STATE_MASK) {
        case 1: ret = "INIT"; break;
        case 2: ret = "PREOP"; break;
        case 3: ret = "BOOT"; break;
        case 4: ret = "SAFEOP"; break;
        case 8: ret = "OP"; break;
        default: ret = "???";
    }

    if (state & EC_SLAVE_STATE_ACK_ERR) {
        ret += "+ERROR";
    }

    return ret;
}

/****************************************************************************/
