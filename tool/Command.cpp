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

    err << "The slave selection matches " << size << "slaves. '"
        << name << "' requires a single slave.";

    throwInvalidUsageException(err);
}

/*****************************************************************************/

Command::SlaveList Command::selectedSlaves(MasterDevice &m)
{
    unsigned int numSlaves = m.slaveCount(), i, aliasIndex;
    uint16_t lastAlias;
    ec_ioctl_slave_t slave;
    SlaveList list;

    if (alias == -1) { // no alias given
        if (position == -1) { // no alias and position given
            // all items
            for (i = 0; i < numSlaves; i++) {
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
            for (i = 0; i < numSlaves; i++) {
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
            for (i = 0; i < numSlaves; i++) {
                m.getSlave(&slave, i);
                if (slave.alias) { // FIXME 'lock' first alias
                    lastAlias = slave.alias;
                    aliasIndex = 0;
                }
                if (lastAlias == (uint16_t) alias
                        && aliasIndex == (unsigned int) position) {
                    list.push_back(slave);
                }
                aliasIndex++;
            }
        }
    }

    return list;
}

/****************************************************************************/

string Command::alStateString(uint8_t state)
{
    switch (state) {
        case 1: return "INIT";
        case 2: return "PREOP";
        case 4: return "SAFEOP";
        case 8: return "OP";
        default: return "???";
    }
}

/****************************************************************************/
