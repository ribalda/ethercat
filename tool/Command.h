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
 ****************************************************************************/

#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <stdexcept>
#include <vector>
#include <list>
#include <sstream>
using namespace std;

#include "../master/ioctl.h"

class MasterDevice;

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
        /** Constructor with char * parameter. */
        CommandException(
                const string &msg /**< Message. */
                ): runtime_error(msg) {}

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

        typedef list<unsigned int> MasterIndexList;
        void setMasters(const string &);
        MasterIndexList getMasterIndices() const;
		unsigned int getSingleMasterIndex() const;

        enum Verbosity {
            Quiet,
            Normal,
            Verbose
        };
        void setVerbosity(Verbosity);
        Verbosity getVerbosity() const;

        void setAliases(const string &);
        void setPositions(const string &);

        void setDomains(const string &);
        typedef list<unsigned int> DomainIndexList;
        DomainIndexList getDomainIndices() const;

        void setDataType(const string &);
        const string &getDataType() const;

        void setEmergency(bool);
        bool getEmergency() const;

        void setForce(bool);
        bool getForce() const;

        void setOutputFile(const string &);
        const string &getOutputFile() const;

        void setSkin(const string &);
        const string &getSkin() const;

        bool matchesSubstr(const string &) const;
        bool matchesAbbrev(const string &) const;

        virtual string helpString(const string &) const = 0;

        typedef vector<string> StringVector;
        virtual void execute(const StringVector &) = 0;

        static string numericInfo();

    protected:
        enum {BreakAfterBytes = 16};

        static void throwInvalidUsageException(const stringstream &);
        static void throwCommandException(const string &);
        static void throwCommandException(const stringstream &);
        void throwSingleSlaveRequired(unsigned int) const;

        typedef list<ec_ioctl_slave_t> SlaveList;
        SlaveList selectedSlaves(MasterDevice &);
        typedef list<ec_ioctl_config_t> ConfigList;
        ConfigList selectedConfigs(MasterDevice &);
        typedef list<ec_ioctl_domain_t> DomainList;
        DomainList selectedDomains(MasterDevice &, const ec_ioctl_master_t &);
        int emergencySlave() const;

        static string alStateString(uint8_t);

    private:
        string name;
        string briefDesc;
        string masters;
        Verbosity verbosity;
        string aliases;
        string positions;
        string domains;
        string dataType;
        bool emergency;
        bool force;
        string outputFile;
        string skin;

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

inline const string &Command::getDataType() const
{
    return dataType;
}

/****************************************************************************/

inline bool Command::getEmergency() const
{
    return emergency;
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

inline const string &Command::getSkin() const
{
    return skin;
}

/****************************************************************************/

#endif
