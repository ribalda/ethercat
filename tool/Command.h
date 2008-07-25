/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <stdexcept>
#include <vector>
#include <list>
using namespace std;

#include "MasterDevice.h"

/*****************************************************************************/

extern unsigned int masterIndex;
extern int domainIndex;
extern string dataTypeStr;
extern bool force;

/****************************************************************************/

class InvalidUsageException:
    public runtime_error
{
    friend class Command;

    protected:
        /** Constructor with stringstream parameter. */
        InvalidUsageException(
                const stringstream &s /**< Message. */
                ): runtime_error(s.str()) {}
};

/****************************************************************************/

class CommandException:
    public runtime_error
{
    friend class Command;

    protected:
        /** Constructor with stringstream parameter. */
        CommandException(
                const stringstream &s /**< Message. */
                ): runtime_error(s.str()) {}
};

/****************************************************************************/

class Command
{
    public:
        Command(const string &, const string &);
		virtual ~Command();

        const string &getName() const;
        const string &getBriefDescription() const;

        enum Verbosity {
            Quiet,
            Normal,
            Verbose
        };
        void setVerbosity(Verbosity);
		Verbosity getVerbosity() const;
        void setAlias(int);
        int getAlias() const;
        void setPosition(int);
        int getPosition() const;

        bool matchesSubstr(const string &) const;
        bool matchesAbbrev(const string &) const;

        virtual string helpString() const = 0;

        typedef vector<string> StringVector;
        virtual void execute(MasterDevice &, const StringVector &) = 0;

        static string numericInfo();

    protected:
		enum {BreakAfterBytes = 16};

        void throwInvalidUsageException(const stringstream &);
        void throwCommandException(const stringstream &);

        typedef list<ec_ioctl_slave_t> SlaveList;
        SlaveList selectedSlaves(MasterDevice &);

        static string alStateString(uint8_t);

    private:
		string name;
        string briefDesc;
        Verbosity verbosity;
        int alias;
        int position;

        Command();
};

/****************************************************************************/

inline const string &Command::getName() const
{
    return name;
}

/****************************************************************************/

inline const string &Command::getBriefDescription() const
{
    return briefDesc;
}

/****************************************************************************/

inline Command::Verbosity Command::getVerbosity() const
{
    return verbosity;
}

/****************************************************************************/

inline int Command::getAlias() const
{
    return alias;
}

/****************************************************************************/

inline int Command::getPosition() const
{
    return position;
}

/****************************************************************************/

#endif
