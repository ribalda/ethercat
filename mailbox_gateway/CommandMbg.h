/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2019  Florian Pose, Ingenieurgemeinschaft IgH
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

#ifndef __MBG_COMMAND_H__
#define __MBG_COMMAND_H__

#include <stdexcept>
#include <vector>
#include <list>
#include <sstream>
using namespace std;

#include "../master/ioctl.h"


/** server port number (34980) */
#define SVR_PORT          0x88A4

/** the maximum tcp connection count */
#define MAX_CONNECTIONS   16

/** the maximum EtherCAT Mailbox Gateway data packet size */
#define MAX_BUFF_SIZE     1500


/****************************************************************************/

class MasterDevice;

/****************************************************************************/

class InvalidUsageException:
    public runtime_error
{
    friend class CommandMbg;

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
    friend class CommandMbg;

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

class CommandMbg
{
    public:
        CommandMbg();
        virtual ~CommandMbg();

        typedef list<unsigned int> MasterIndexList;
        void setMasters(const string &);
        MasterIndexList getMasterIndices() const;
        unsigned int getSingleMasterIndex() const;

        enum Verbosity {
            Quiet,
            Normal,
            Verbose,
            Debug
        };
        void setVerbosity(Verbosity);
        Verbosity getVerbosity() const;

        typedef vector<string> StringVector;
        void execute(const StringVector &);
        
        void terminate();

    protected:
        enum {BreakAfterBytes = 16};

        static void throwInvalidUsageException(const stringstream &);
        static void throwCommandException(const string &);
        static void throwCommandException(const stringstream &);

        int processMessage(uint8_t *, size_t &);

        void printBuff(uint8_t *, size_t);
        
    private:
        string        m_masters;
        Verbosity     m_verbosity;

        int           m_terminate;
        MasterDevice *m_masterDev;
};

/****************************************************************************/

inline CommandMbg::Verbosity CommandMbg::getVerbosity() const
{
    return m_verbosity;
}

/****************************************************************************/

#endif
