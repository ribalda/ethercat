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

#include "Command.h"
#include "MasterDevice.h"
#include "NumberListParser.h"

/*****************************************************************************/

class MasterIndexParser:
    public NumberListParser
{
    unsigned int getMax()
    {
        MasterDevice dev;
        dev.setIndex(0U);
        dev.open(MasterDevice::Read);
        return dev.getMasterCount() - 1;
    };
};

/*****************************************************************************/

Command::Command(const string &name, const string &briefDesc):
    name(name),
    briefDesc(briefDesc),
    verbosity(Normal)
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

void Command::setAlias(int a)
{
    alias = a;
};

/*****************************************************************************/

void Command::setPosition(int p)
{
    position = p;
};

/*****************************************************************************/

void Command::setDomain(int d)
{
    domain = d;
};

/*****************************************************************************/

void Command::setDataType(const string &t)
{
    dataType = t;
};

/*****************************************************************************/

void Command::setForce(bool f)
{
    force = f;
};

/*****************************************************************************/

void Command::setOutputFile(const string &f)
{
    outputFile = f;
};

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

void Command::throwInvalidUsageException(const stringstream &s) const
{
    throw InvalidUsageException(s);
}

/*****************************************************************************/

void Command::throwCommandException(const string &msg) const
{
    throw CommandException(msg);
}

/*****************************************************************************/

void Command::throwCommandException(const stringstream &s) const
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
    unsigned int i, aliasIndex;
    uint16_t lastAlias;
    ec_ioctl_slave_t slave;
    SlaveList list;

    m.getMaster(&master);

    if (alias == -1) { // no alias given
        if (position == -1) { // no alias and position given
            // all items
            for (i = 0; i < master.slave_count; i++) {
                m.getSlave(&slave, i);
                list.push_back(slave);
            }
        } else { // no alias, but position given
            // one item by position
            m.getSlave(&slave, position);
            list.push_back(slave);
        }
    } else { // alias given
        if (position == -1) { // alias, but no position given
            // take all items with a given alias
            lastAlias = 0;
            for (i = 0; i < master.slave_count; i++) {
                m.getSlave(&slave, i);
                if (slave.alias) {
                    lastAlias = slave.alias;
                }
                if (lastAlias == (uint16_t) alias) {
                    list.push_back(slave);
                }
            }
        } else { // alias and position given
            lastAlias = 0;
            aliasIndex = 0;
            for (i = 0; i < master.slave_count; i++) {
                m.getSlave(&slave, i);
                if (slave.alias && slave.alias == (uint16_t) alias) {
                    lastAlias = slave.alias;
                    aliasIndex = 0;
                }
                if (lastAlias && aliasIndex == (unsigned int) position) {
                    list.push_back(slave);
                }
                aliasIndex++;
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

    if (alias == -1) { // no alias given
        if (position == -1) { // no alias and position given
            // all items
            for (i = 0; i < master.config_count; i++) {
                m.getConfig(&config, i);
                list.push_back(config);
            }
        } else { // no alias, but position given
            for (i = 0; i < master.config_count; i++) {
                m.getConfig(&config, i);
                if (!config.alias && config.position == position) {
                    list.push_back(config);
                    break; // there can be at most one matching
                }
            }
        }
    } else { // alias given
        if (position == -1) { // alias, but no position given
            // take all items with a given alias
            for (i = 0; i < master.config_count; i++) {
                m.getConfig(&config, i);
                if (config.alias == alias) {
                    list.push_back(config);
                }
            }
        } else { // alias and position given
            for (i = 0; i < master.config_count; i++) {
                m.getConfig(&config, i);
                if (config.alias == alias && config.position == position) {
                    list.push_back(config);
                    break; // there can be at most one matching
                }
            }
        }
    }

    list.sort();
    return list;
}

/****************************************************************************/

Command::DomainList Command::selectedDomains(MasterDevice &m)
{
    ec_ioctl_domain_t d;
    DomainList list;

    if (domain == -1) {
        ec_ioctl_master_t master;
        unsigned int i;

        m.getMaster(&master);

        for (i = 0; i < master.domain_count; i++) {
            m.getDomain(&d, i);
            list.push_back(d);
        }
    } else {
        m.getDomain(&d, domain);
        list.push_back(d);
    }

    return list;
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
