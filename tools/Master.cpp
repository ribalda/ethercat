/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <iostream>
#include <iomanip>
#include <sstream>
using namespace std;

#include "Master.h"
#include "../master/ioctl.h"

/****************************************************************************/

Master::Master()
{
    index = 0;
    fd = -1;
}

/****************************************************************************/

Master::~Master()
{
    close();
}

/****************************************************************************/

void Master::open(unsigned int index)
{
    stringstream deviceName;
    
    Master::index = index;

    deviceName << "/dev/EtherCAT" << index;
    
    if ((fd = ::open(deviceName.str().c_str(), O_RDONLY)) == -1) {
        stringstream err;
        err << "Failed to open master device " << deviceName.str() << ": "
            << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::close()
{
    if (fd == -1)
        return;

    ::close(fd);
}

/****************************************************************************/

unsigned int Master::slaveCount()
{
    unsigned int numSlaves;

    if (ioctl(fd, EC_IOCTL_SLAVE_COUNT, &numSlaves)) {
        stringstream err;
        err << "Failed to get number of slaves: " << strerror(errno);
        throw MasterException(err.str());
    }

    return numSlaves;
}

/****************************************************************************/

void Master::listSlaves()
{
    unsigned int numSlaves = slaveCount(), i;
    struct ec_ioctl_slave_info *infos, *info;
    uint16_t lastAlias, aliasIndex;
    
    if (!numSlaves)
        return;
    
    infos = new struct ec_ioctl_slave_info[numSlaves];

    if (ioctl(fd, EC_IOCTL_SLAVE_INFO, infos)) {
        stringstream err;
        err << "Failed to get slave information: " << strerror(errno);
        throw MasterException(err.str());
    }

    lastAlias = 0;
    aliasIndex = 0;
    for (i = 0; i < numSlaves; i++) {
        info = &infos[i];
        cout << setw(2) << info->ring_position << "  ";

        if (info->alias) {
            lastAlias = info->alias;
            aliasIndex = 0;
        }
        if (lastAlias) {
            cout << setw(10) << "#" << lastAlias << ":" << aliasIndex;
        }

        cout << "  " << slaveState(info->state) << "  ";

        if (strlen(info->description)) {
            cout << info->description;
        } else {
            cout << "0x" << hex << setfill('0') << info->vendor_id
                << ":0x" << info->product_code;
        }

        cout << endl;
    }

    delete [] infos;
}

/****************************************************************************/

string Master::slaveState(uint8_t state) const
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
