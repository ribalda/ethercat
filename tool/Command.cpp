/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include "Command.h"

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
    switch (state) {
        case 1: return "INIT";
        case 2: return "PREOP";
        case 3: return "BOOT";
        case 4: return "SAFEOP";
        case 8: return "OP";
        default: return "???";
    }
}

/****************************************************************************/
