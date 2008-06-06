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
#include <cctype> // toupper()
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

void Master::outputData(int domainIndex)
{
    if (domainIndex == -1) {
        unsigned int numDomains = domainCount(), i;

        for (i = 0; i < numDomains; i++) {
            outputDomainData(i);
        }
    } else {
        outputDomainData(domainIndex);
    }
}

/****************************************************************************/

void Master::setDebug(const vector<string> &commandArgs)
{
    stringstream str;
    int debugLevel;
    
    if (commandArgs.size() != 1) {
        stringstream err;
        err << "'debug' takes exactly one argument!";
        throw MasterException(err.str());
    }

    str << commandArgs[0];
    str >> debugLevel;

    if (str.fail()) {
        stringstream err;
        err << "Invalid debug level '" << commandArgs[0] << "'!";
        throw MasterException(err.str());
    }

    if (ioctl(fd, EC_IOCTL_SET_DEBUG, debugLevel) < 0) {
        stringstream err;
        err << "Failed to set debug level: " << strerror(errno);
        throw MasterException(err.str());
    }
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

void Master::showMaster()
{
    ec_ioctl_master_t data;
    stringstream err;
    unsigned int i;
    
    getMaster(&data);

    cout
        << "Master" << index << endl
        << "  State: ";

    switch (data.mode) {
        case 0: cout << "Waiting for device..."; break;
        case 1: cout << "Idle"; break;
        case 2: cout << "Operation"; break;
        default:
                err << "Invalid master state " << data.mode;
                throw MasterException(err.str());
    }

    cout << endl
        << "  Slaves: " << data.slave_count << endl;

    for (i = 0; i < 2; i++) {
        cout << "  Device" << i << ": ";
        if (data.devices[i].address[0] == 0x00
                && data.devices[i].address[1] == 0x00
                && data.devices[i].address[2] == 0x00
                && data.devices[i].address[3] == 0x00
                && data.devices[i].address[4] == 0x00
                && data.devices[i].address[5] == 0x00) {
            cout << "None.";
        } else {
            cout << hex << setfill('0')
                << setw(2) << (unsigned int) data.devices[i].address[0] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[1] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[2] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[3] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[4] << ":"
                << setw(2) << (unsigned int) data.devices[i].address[5] << " ("
                << (data.devices[i].attached ? "attached" : "waiting...")
                << ")" << endl << dec
                << "    Tx count: " << data.devices[i].tx_count << endl
                << "    Rx count: " << data.devices[i].rx_count;
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

void Master::listSdos(int slavePosition)
{
    if (slavePosition == -1) {
        unsigned int numSlaves = slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            listSlaveSdos(i, true);
        }
    } else {
        listSlaveSdos(slavePosition, false);
    }
}

/****************************************************************************/

void Master::requestStates(
        int slavePosition,
        const vector<string> &commandArgs
        )
{
    string stateStr;
    uint8_t state;
    
    if (commandArgs.size() != 1) {
        stringstream err;
        err << "'state' takes exactly one argument!";
        throw MasterException(err.str());
    }

    stateStr = commandArgs[0];
    transform(stateStr.begin(), stateStr.end(),
            stateStr.begin(), (int (*) (int)) std::toupper);

    if (stateStr == "INIT") {
        state = 0x01;
    } else if (stateStr == "PREOP") {
        state = 0x02;
    } else if (stateStr == "SAFEOP") {
        state = 0x04;
    } else if (stateStr == "OP") {
        state = 0x08;
    } else {
        stringstream err;
        err << "Invalid state '" << commandArgs[0] << "'!";
        throw MasterException(err.str());
    }

    if (slavePosition == -1) {
        unsigned int i, numSlaves = slaveCount();
        for (i = 0; i < numSlaves; i++)
            requestState(i, state);
    } else {
        requestState(slavePosition, state);
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

void Master::outputDomainData(unsigned int domainIndex)
{
    ec_ioctl_domain_t domain;
    ec_ioctl_data_t data;
    unsigned char *processData;
    unsigned int i;
    
    getDomain(&domain, domainIndex);

    if (!domain.data_size)
        return;

    processData = new unsigned char[domain.data_size];

    try {
        getData(&data, domainIndex, domain.data_size, processData);
    } catch (MasterException &e) {
        delete [] processData;
        throw e;
    }

    for (i = 0; i < data.data_size; i++)
        cout << processData[i];
    cout.flush();

    delete [] processData;
}

/****************************************************************************/

void Master::showDomain(unsigned int domainIndex)
{
    ec_ioctl_domain_t domain;
    unsigned char *processData;
    ec_ioctl_data_t data;
    unsigned int i, j;
    ec_ioctl_domain_fmmu_t fmmu;
    unsigned int dataOffset;
    
    getDomain(&domain, domainIndex);

	cout << "Domain" << domainIndex << ":"
		<< " LogBaseAddr 0x"
		<< hex << setfill('0') << setw(8) << domain.logical_base_address
		<< ", Size " << dec << setfill(' ') << setw(3) << domain.data_size
		<< ", WorkingCounter "
		<< dec << domain.working_counter << "/"
        << domain.expected_working_counter << endl;

    if (!domain.data_size)
        return;

    processData = new unsigned char[domain.data_size];

    try {
        getData(&data, domainIndex, domain.data_size, processData);
    } catch (MasterException &e) {
        delete [] processData;
        throw e;
    }

    for (i = 0; i < domain.fmmu_count; i++) {
        getFmmu(&fmmu, domainIndex, i);

        cout << "  SlaveConfig "
            << fmmu.slave_config_alias << ":" << fmmu.slave_config_position
            << ", Dir "
            << setfill(' ') << setw(3) << (fmmu.fmmu_dir ? "In" : "Out")
            << ", LogAddr 0x" 
            << hex << setfill('0') << setw(8) << fmmu.logical_address
            << ", Size " << dec << fmmu.data_size << endl;

        dataOffset = fmmu.logical_address - domain.logical_base_address;
        if (dataOffset + fmmu.data_size > domain.data_size) {
            stringstream err;
            err << "Fmmu information corrupted!";
            delete [] processData;
            throw MasterException(err.str());
        }

        cout << "    " << hex << setfill('0');
        for (j = 0; j < fmmu.data_size; j++) {
            cout << setw(2)
                << (unsigned int) *(processData + dataOffset + j) << " ";
        }
        cout << endl;
    }

    delete [] processData;
}

/****************************************************************************/

void Master::listSlavePdos(uint16_t slavePosition, bool withHeader)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_sync_t sync;
    ec_ioctl_pdo_t pdo;
    ec_ioctl_pdo_entry_t entry;
    unsigned int i, j, k;
    
    getSlave(&slave, slavePosition);

    if (withHeader)
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

void Master::listSlaveSdos(uint16_t slavePosition, bool withHeader)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_sdo_t sdo;
    ec_ioctl_sdo_entry_t entry;
    unsigned int i, j, k;
    
    getSlave(&slave, slavePosition);

    if (withHeader)
        cout << "=== Slave " << slavePosition << " ===" << endl;

    for (i = 0; i < slave.sdo_count; i++) {
        getSdo(&sdo, slavePosition, i);

        cout << "Sdo 0x"
            << hex << setfill('0') << setw(4) << sdo.sdo_index
            << ", \"" << sdo.name << "\"" << endl;

        for (j = 0; j <= sdo.max_subindex; j++) {
            getSdoEntry(&entry, slavePosition, i, j);

            cout << "  Entry 0x"
                << hex << setfill('0') << setw(2)
                << (unsigned int) entry.sdo_entry_subindex
                << ", type 0x" << setw(4) << entry.data_type
                << ", " << dec << entry.bit_length << " bit, \""
                << entry.description << "\"" << endl;
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
    ec_ioctl_master_t data;

    getMaster(&data);
    return data.slave_count;
}

/****************************************************************************/

void Master::getMaster(ec_ioctl_master_t *data)
{
    if (ioctl(fd, EC_IOCTL_MASTER, data) < 0) {
        stringstream err;
        err << "Failed to get master information: " << strerror(errno);
        throw MasterException(err.str());
    }
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

void Master::getData(ec_ioctl_data_t *data, unsigned int domainIndex,
        unsigned int dataSize, unsigned char *mem)
{
    data->domain_index = domainIndex;
    data->data_size = dataSize;
    data->target = mem;

    if (ioctl(fd, EC_IOCTL_DATA, data) < 0) {
        stringstream err;
        err << "Failed to get domain data: " << strerror(errno);
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

void Master::getFmmu(
        ec_ioctl_domain_fmmu_t *fmmu,
        unsigned int domainIndex,
        unsigned int fmmuIndex
        )
{
    fmmu->domain_index = domainIndex;
    fmmu->fmmu_index = fmmuIndex;

    if (ioctl(fd, EC_IOCTL_DOMAIN_FMMU, fmmu)) {
        stringstream err;
        err << "Failed to get domain FMMU: ";
        if (errno == EINVAL)
            err << "Either domain " << domainIndex << " does not exist, "
                << "or it contains less than " << (unsigned int) fmmuIndex + 1
                << " FMMus!";
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
                << "or it contains less than " << (unsigned int) syncIndex + 1
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
                << "or it contains less than " << (unsigned int) syncIndex + 1
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
                << "or it contains less than " << (unsigned int) syncIndex + 1
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

void Master::getSdo(
        ec_ioctl_sdo_t *sdo,
        uint16_t slaveIndex,
        uint16_t sdoPosition
        )
{
    sdo->slave_position = slaveIndex;
    sdo->sdo_position = sdoPosition;

    if (ioctl(fd, EC_IOCTL_SDO, sdo)) {
        stringstream err;
        err << "Failed to get Sdo: ";
        if (errno == EINVAL)
            err << "Either slave " << slaveIndex << " does not exist, "
                << "or it contains less than " << sdoPosition + 1 << " Sdos!"
                << endl;
        else
            err << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::getSdoEntry(
        ec_ioctl_sdo_entry_t *entry,
        uint16_t slaveIndex,
        uint16_t sdoPosition,
        uint8_t entrySubindex
        )
{
    entry->slave_position = slaveIndex;
    entry->sdo_position = sdoPosition;
    entry->sdo_entry_subindex = entrySubindex;

    if (ioctl(fd, EC_IOCTL_SDO_ENTRY, entry)) {
        stringstream err;
        err << "Failed to get Sdo entry: ";
        if (errno == EINVAL)
            err << "Either slave " << slaveIndex << " does not exist, "
                << "or it contains less than " << sdoPosition + 1
                << " Sdos, or the Sdo at position " << sdoPosition
                << " contains less than " << (unsigned int) entrySubindex + 1
                << " entries!" << endl;
        else
            err << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::requestState(
        uint16_t slavePosition,
        uint8_t state
        )
{
    ec_ioctl_slave_state_t data;

    data.slave_position = slavePosition;
    data.requested_state = state;
    
    if (ioctl(fd, EC_IOCTL_SLAVE_STATE, &data)) {
        stringstream err;
        err << "Failed to request slave state: ";
        if (errno == EINVAL)
            err << "Slave " << slavePosition << " does not exist!";
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
