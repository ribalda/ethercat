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

void Master::showDomains(int domainIndex)
{
    if (domainIndex == -1) {
        unsigned int numDomains = domainCount(), i;

        for (i = 0; i < numDomains; i++) {
            showDomain(i);
        }
    } else {
        showDomain(domainIndex);
    }
}

/****************************************************************************/

void Master::listSlaves()
{
    unsigned int numSlaves = slaveCount(), i;
    ec_ioctl_slave_t slave;
    uint16_t lastAlias, aliasIndex;
    
    lastAlias = 0;
    aliasIndex = 0;
    for (i = 0; i < numSlaves; i++) {
        getSlave(&slave, i);
        cout << setw(2) << i << "  ";

        if (slave.alias) {
            lastAlias = slave.alias;
            aliasIndex = 0;
        }
        if (lastAlias) {
            cout << setw(10) << "#" << lastAlias << ":" << aliasIndex;
        }

        cout << "  " << slaveState(slave.state) << "  ";

        if (strlen(slave.name)) {
            cout << slave.name;
        } else {
            cout << "0x" << hex << setfill('0') << slave.vendor_id
                << ":0x" << slave.product_code;
        }

        cout << endl;
    }
}

/****************************************************************************/

void Master::listPdos(int slavePosition)
{
    if (slavePosition == -1) {
        unsigned int numSlaves = slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            listSlavePdos(i, true);
        }
    } else {
        listSlavePdos(slavePosition, false);
    }
}

/****************************************************************************/

void Master::generateXml(int slavePosition)
{
    if (slavePosition == -1) {
        unsigned int numSlaves = slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            generateSlaveXml(i);
        }
    } else {
        generateSlaveXml(slavePosition);
    }
}

/****************************************************************************/

void Master::showDomain(unsigned int domainIndex)
{
    ec_ioctl_domain_t data;
    
    getDomain(&data, domainIndex);

	unsigned int data_size;
	uint32_t logical_base_address;
	uint16_t working_counter;
	uint16_t expected_working_counter;

	cout << "Domain" << domainIndex << ":"
		<< " LogBaseAddr 0x"
		<< hex << setfill('0') << setw(8) << data.logical_base_address
		<< ", Size " << dec << data.data_size
		<< ", WorkingCounter "
		<< dec << data.working_counter
		<< " of " << data.expected_working_counter
		<< endl;
}

/****************************************************************************/

void Master::listSlavePdos(uint16_t slavePosition, bool printSlave)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_sync_t sync;
    ec_ioctl_pdo_t pdo;
    ec_ioctl_pdo_entry_t entry;
    unsigned int i, j, k;
    
    getSlave(&slave, slavePosition);

    if (printSlave)
        cout << "=== Slave " << slavePosition << " ===" << endl;

    for (i = 0; i < slave.sync_count; i++) {
        getSync(&sync, slavePosition, i);

        cout << "SM" << i << ":"
            << " PhysAddr 0x"
            << hex << setfill('0') << setw(4) << sync.physical_start_address
            << ", DefaultSize "
            << dec << setfill(' ') << setw(4) << sync.default_size
            << ", ControlRegister 0x"
            << hex << setfill('0') << setw(2)
            << (unsigned int) sync.control_register
            << ", Enable " << dec << (unsigned int) sync.enable
            << endl;

        for (j = 0; j < sync.pdo_count; j++) {
            getPdo(&pdo, slavePosition, i, j);

            cout << "  " << (pdo.dir ? "T" : "R") << "xPdo 0x"
                << hex << setfill('0') << setw(4) << pdo.index
                << " \"" << pdo.name << "\"" << endl;

            for (k = 0; k < pdo.entry_count; k++) {
                getPdoEntry(&entry, slavePosition, i, j, k);

                cout << "    Pdo entry 0x"
                    << hex << setfill('0') << setw(4) << entry.index
                    << ":" << hex << setfill('0') << setw(2)
                    << (unsigned int) entry.subindex
                    << ", " << dec << (unsigned int) entry.bit_length
                    << " bit, \"" << entry.name << "\"" << endl;
            }
        }
    }
}

/****************************************************************************/

void Master::generateSlaveXml(uint16_t slavePosition)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_sync_t sync;
    ec_ioctl_pdo_t pdo;
    ec_ioctl_pdo_entry_t entry;
    unsigned int i, j, k;
    
    getSlave(&slave, slavePosition);

    cout
        << "<?xml version=\"1.0\" ?>" << endl
        << "  <EtherCATInfo>" << endl
        << "    <!-- Slave " << slave.position << " -->" << endl
        << "    <Vendor>" << endl
        << "      <Id>" << slave.vendor_id << "</Id>" << endl
        << "    </Vendor>" << endl
        << "    <Descriptions>" << endl
        << "      <Devices>" << endl
        << "        <Device>" << endl
        << "          <Type ProductCode=\"#x"
        << hex << setfill('0') << setw(8) << slave.product_code
        << "\" RevisionNo=\"#x"
        << hex << setfill('0') << setw(8) << slave.revision_number
        << "\"/>" << endl;

    for (i = 0; i < slave.sync_count; i++) {
        getSync(&sync, slavePosition, i);

        for (j = 0; j < sync.pdo_count; j++) {
            getPdo(&pdo, slavePosition, i, j);

            cout
                << "          <" << (pdo.dir ? "T" : "R") << "xPdo>" << endl
                << "            <Index>#x"
                << hex << setfill('0') << setw(4) << pdo.index
                << "</Index>" << endl
                << "            <Name>" << pdo.name << "</Name>" << endl;

            for (k = 0; k < pdo.entry_count; k++) {
                getPdoEntry(&entry, slavePosition, i, j, k);

                cout
                    << "            <Entry>" << endl
                    << "              <Index>#x"
                    << hex << setfill('0') << setw(4) << entry.index
                    << "</Index>" << endl;
                if (entry.index)
                    cout
                        << "              <SubIndex>"
                        << dec << (unsigned int) entry.subindex
                        << "</SubIndex>" << endl;
                
                cout
                    << "              <BitLen>"
                    << (unsigned int) entry.bit_length
                    << "</BitLen>" << endl;

                if (entry.index) {
                    cout
                        << "              <Name>" << entry.name
                        << "</Name>" << endl
                        << "              <DataType>";

                    if (entry.bit_length == 1) {
                        cout << "BOOL";
                    } else if (!(entry.bit_length % 8)) {
                        if (entry.bit_length <= 64)
                            cout << "UINT" << (unsigned int) entry.bit_length;
                        else
                            cout << "STRING("
                                << (unsigned int) (entry.bit_length / 8)
                                << ")";
                    } else {
                        cerr << "Invalid bit length "
                            << (unsigned int) entry.bit_length << endl;
                    }

                        cout << "</DataType>" << endl;
                }

                cout << "            </Entry>" << endl;
            }

            cout
                << "          </" << (pdo.dir ? "T" : "R") << "xPdo>" << endl;
        }
    }

    cout
        << "        </Device>" << endl
        << "     </Devices>" << endl
        << "  </Descriptions>" << endl
        << "</EtherCATInfo>" << endl;
}

/****************************************************************************/

unsigned int Master::domainCount()
{
    int ret;

    if ((ret = ioctl(fd, EC_IOCTL_DOMAIN_COUNT, 0)) < 0) {
        stringstream err;
        err << "Failed to get number of domains: " << strerror(errno);
        throw MasterException(err.str());
    }

    return ret;
}

/****************************************************************************/

unsigned int Master::slaveCount()
{
    int ret;

    if ((ret = ioctl(fd, EC_IOCTL_SLAVE_COUNT, 0)) < 0) {
        stringstream err;
        err << "Failed to get number of slaves: " << strerror(errno);
        throw MasterException(err.str());
    }

    return ret;
}

/****************************************************************************/

void Master::getDomain(ec_ioctl_domain_t *data, unsigned int index)
{
    data->index = index;

    if (ioctl(fd, EC_IOCTL_DOMAIN, data)) {
        stringstream err;
        err << "Failed to get domain: ";
        if (errno == EINVAL)
            err << "Domain " << index << " does not exist!";
        else
            err << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::getSlave(ec_ioctl_slave_t *slave, uint16_t slaveIndex)
{
    slave->position = slaveIndex;

    if (ioctl(fd, EC_IOCTL_SLAVE, slave)) {
        stringstream err;
        err << "Failed to get slave: ";
        if (errno == EINVAL)
            err << "Slave " << slaveIndex << " does not exist!";
        else
            err << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::getSync(
        ec_ioctl_sync_t *sync,
        uint16_t slaveIndex,
        uint8_t syncIndex
        )
{
    sync->slave_position = slaveIndex;
    sync->sync_index = syncIndex;

    if (ioctl(fd, EC_IOCTL_SYNC, sync)) {
        stringstream err;
        err << "Failed to get sync manager: ";
        if (errno == EINVAL)
            err << "Either slave " << slaveIndex << " does not exist, "
                << "or contains less than " << (unsigned int) syncIndex + 1
                << " sync managers!";
        else
            err << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::getPdo(
        ec_ioctl_pdo_t *pdo,
        uint16_t slaveIndex,
        uint8_t syncIndex,
        uint8_t pdoPos
        )
{
    pdo->slave_position = slaveIndex;
    pdo->sync_index = syncIndex;
    pdo->pdo_pos = pdoPos;

    if (ioctl(fd, EC_IOCTL_PDO, pdo)) {
        stringstream err;
        err << "Failed to get Pdo: ";
        if (errno == EINVAL)
            err << "Either slave " << slaveIndex << " does not exist, "
                << "or contains less than " << (unsigned int) syncIndex + 1
                << " sync managers, or sync manager "
                << (unsigned int) syncIndex << " contains less than "
                << pdoPos + 1 << " Pdos!" << endl;
        else
            err << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::getPdoEntry(
        ec_ioctl_pdo_entry_t *entry,
        uint16_t slaveIndex,
        uint8_t syncIndex,
        uint8_t pdoPos,
        uint8_t entryPos
        )
{
    entry->slave_position = slaveIndex;
    entry->sync_index = syncIndex;
    entry->pdo_pos = pdoPos;
    entry->entry_pos = entryPos;

    if (ioctl(fd, EC_IOCTL_PDO_ENTRY, entry)) {
        stringstream err;
        err << "Failed to get Pdo entry: ";
        if (errno == EINVAL)
            err << "Either slave " << slaveIndex << " does not exist, "
                << "or contains less than " << (unsigned int) syncIndex + 1
                << " sync managers, or sync manager "
                << (unsigned int) syncIndex << " contains less than "
                << pdoPos + 1 << " Pdos, or the Pdo at position " << pdoPos
                << " contains less than " << (unsigned int) entryPos + 1
                << " entries!" << endl;
        else
            err << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

string Master::slaveState(uint8_t state)
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
