/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <stdexcept>
#include <vector>
using namespace std;

#include "MasterDevice.h"

/*****************************************************************************/

extern unsigned int masterIndex;
extern int slavePosition;
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

        enum Verbosity {
            Quiet,
            Normal,
            Verbose
        };
        void setVerbosity(Verbosity);

        const string &getName() const;
        const string &getBriefDescription() const;
		Verbosity getVerbosity() const;

        bool matchesSubstr(const string &) const;
        bool matchesAbbrev(const string &) const;

        virtual string helpString() const = 0;

        typedef vector<string> StringVector;
        virtual void execute(MasterDevice &, const StringVector &) = 0;

        static string numericInfo();

    protected:
        void throwInvalidUsageException(const stringstream &);
        void throwCommandException(const stringstream &);

		enum {BreakAfterBytes = 16};
        
        static string alStateString(uint8_t);

    private:
		string name;
        string briefDesc;
        Verbosity verbosity;

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

#endif
