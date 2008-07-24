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

void Command::throwInvalidUsageException(const stringstream &s)
{
    throw InvalidUsageException(s);
}

/*****************************************************************************/

void Command::throwCommandException(const stringstream &s)
{
    throw CommandException(s);
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

/****************************************************************************/
