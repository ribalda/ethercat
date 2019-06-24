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

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

#include <sstream>
#include <iomanip>
using namespace std;

#include "MasterDevice.h"

/****************************************************************************/

MasterDevice::MasterDevice(unsigned int index):
    m_index(index),
    m_masterCount(0U),
    m_fd(-1)
{
}

/****************************************************************************/

MasterDevice::~MasterDevice()
{
    close();
}

/****************************************************************************/

void MasterDevice::setIndex(unsigned int i)
{
    m_index = i;
}

/****************************************************************************/

void MasterDevice::open(Permissions perm)
{
    stringstream deviceName;

    if (m_fd == -1) { // not already open
        ec_ioctl_module_t module_data;
        deviceName << "/dev/EtherCAT" << m_index;

        if ((m_fd = ::open(deviceName.str().c_str(),
                           perm == ReadWrite ? O_RDWR : O_RDONLY)) == -1) {
            stringstream err;
            err << "Failed to open master device " << deviceName.str() << ": "
                << strerror(errno);
            throw MasterDeviceException(err);
        }

        getModule(&module_data);
        if (module_data.ioctl_version_magic != EC_IOCTL_VERSION_MAGIC) {
            stringstream err;
            err << "ioctl() version magic is differing: "
                << deviceName.str() << ": " << module_data.ioctl_version_magic
                << ", ethercat tool: " << EC_IOCTL_VERSION_MAGIC;
            throw MasterDeviceException(err);
        }
        m_masterCount = module_data.master_count;
    }
}

/****************************************************************************/

void MasterDevice::close()
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
}

/****************************************************************************/

void MasterDevice::getModule(ec_ioctl_module_t *data)
{
    if (ioctl(m_fd, EC_IOCTL_MODULE, data) < 0) {
        stringstream err;
        err << "Failed to get module information: " << strerror(errno);
        throw MasterDeviceException(err);
    }
}

/****************************************************************************/

int MasterDevice::processMessage(ec_ioctl_mbox_gateway_t *data)
{
    return ioctl(m_fd, EC_IOCTL_MBOX_GATEWAY, data);
}

/*****************************************************************************/
