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

#ifndef __MASTER_DEVICE_H__
#define __MASTER_DEVICE_H__

#include <stdexcept>
#include <sstream>
using namespace std;

#include "ecrt.h"
#include "ioctl.h"

/****************************************************************************/

class MasterDeviceException:
    public runtime_error
{
    friend class MasterDevice;

    protected:
        /** Constructor with string parameter. */
        MasterDeviceException(
                const string &s /**< Message. */
                ): runtime_error(s) {}

        /** Constructor with stringstream parameter. */
        MasterDeviceException(
                const stringstream &s /**< Message. */
                ): runtime_error(s.str()) {}
};

/****************************************************************************/

class MasterDevice
{
    public:
        MasterDevice(unsigned int = 0U);
        ~MasterDevice();

        void setIndex(unsigned int);
        unsigned int getIndex() const;

        enum Permissions {Read, ReadWrite};
        void open(Permissions);
        void close();

        int getFD() const;

        void getModule(ec_ioctl_module_t *);
        
        int processMessage(ec_ioctl_mbox_gateway_t *);

        unsigned int getMasterCount() const {return m_masterCount;}

    private:
        unsigned int m_index;
        unsigned int m_masterCount;
        int          m_fd;
};

/****************************************************************************/

inline unsigned int MasterDevice::getIndex() const
{
    return m_index;
}

/****************************************************************************/

inline int MasterDevice::getFD() const
{
    return m_fd;
}

/****************************************************************************/

#endif
