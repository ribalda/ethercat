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
        void setDomain(int);
        int getDomain() const;
        void setDataType(const string &);
        const string &getDataType() const;
		void setForce(bool);
		bool getForce() const;
		void setOutputFile(const string &);
		const string &getOutputFile() const;

        bool matchesSubstr(const string &) const;
        bool matchesAbbrev(const string &) const;

        virtual string helpString() const = 0;

        typedef vector<string> StringVector;
        virtual void execute(MasterDevice &, const StringVector &) = 0;

        static string numericInfo();

    protected:
		enum {BreakAfterBytes = 16};

        void throwInvalidUsageException(const stringstream &) const;
        void throwCommandException(const stringstream &) const;
        void throwSingleSlaveRequired(unsigned int) const;

        typedef list<ec_ioctl_slave_t> SlaveList;
        SlaveList selectedSlaves(MasterDevice &);
        typedef list<ec_ioctl_config_t> ConfigList;
        ConfigList selectedConfigs(MasterDevice &);
        typedef list<ec_ioctl_domain_t> DomainList;
        DomainList selectedDomains(MasterDevice &);

        static string alStateString(uint8_t);

    private:
		string name;
        string briefDesc;
        Verbosity verbosity;
        int alias;
        int position;
		int domain;
		string dataType;
		bool force;
		string outputFile;

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

inline int Command::getDomain() const
{
    return domain;
}

/****************************************************************************/

inline const string &Command::getDataType() const
{
    return dataType;
}

/****************************************************************************/

inline bool Command::getForce() const
{
    return force;
}

/****************************************************************************/

inline const string &Command::getOutputFile() const
{
    return outputFile;
}

/****************************************************************************/

#endif
