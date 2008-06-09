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

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define le16tocpu(x) x
#define le32tocpu(x) x

#elif __BYTE_ORDER == __BIG_ENDIAN

#define le16tocpu(x) \
        ((uint16_t)( \
        (((uint16_t)(x) & 0x00ffU) << 8) | \
        (((uint16_t)(x) & 0xff00U) >> 8) ))
#define le32tocpu(x) \
        ((uint32_t)( \
        (((uint32_t)(x) & 0x000000ffUL) << 24) | \
        (((uint32_t)(x) & 0x0000ff00UL) <<  8) | \
        (((uint32_t)(x) & 0x00ff0000UL) >>  8) | \
        (((uint32_t)(x) & 0xff000000UL) >> 24) ))

#endif

/****************************************************************************/

struct CoEDataType {
    const char *name;
    uint16_t coeCode;
    unsigned int byteSize;
};

static const CoEDataType dataTypes[] = {
    {"int8",   0x0002, 1},
    {"int16",  0x0003, 2},
    {"int32",  0x0004, 4},
    {"uint8",  0x0005, 1},
    {"uint16", 0x0006, 2},
    {"uint32", 0x0007, 4},
    {"string", 0x0009, 0},
    {"raw",    0xffff, 0},
    {}
};

/****************************************************************************/

const CoEDataType *findDataType(const string &str)
{
    const CoEDataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (str == d->name)
            return d;

    return NULL;
}

/****************************************************************************/

const CoEDataType *findDataType(uint16_t code)
{
    const CoEDataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (code == d->coeCode)
            return d;

    return NULL;
}

/****************************************************************************/

Master::Master()
{
    index = 0;
    fd = -1;
    currentPermissions = Read;
}

/****************************************************************************/

Master::~Master()
{
    close();
}

/****************************************************************************/

void Master::setIndex(unsigned int i)
{
    index = i;
}

/****************************************************************************/

void Master::outputData(int domainIndex)
{
    open(Read);

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

    open(ReadWrite);

    if (ioctl(fd, EC_IOCTL_SET_DEBUG, debugLevel) < 0) {
        stringstream err;
        err << "Failed to set debug level: " << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::showDomains(int domainIndex)
{
    open(Read);

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
    unsigned int numSlaves, i;
    ec_ioctl_slave_t slave;
    uint16_t lastAlias, aliasIndex;
    
    open(Read);

    numSlaves = slaveCount();

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
    
    open(Read);
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

void Master::listPdos(int slavePosition, bool quiet)
{
    open(Read);

    if (slavePosition == -1) {
        unsigned int numSlaves = slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            listSlavePdos(i, quiet, true);
        }
    } else {
        listSlavePdos(slavePosition, quiet, false);
    }
}

/****************************************************************************/

void Master::listSdos(int slavePosition, bool quiet)
{
    open(Read);

    if (slavePosition == -1) {
        unsigned int numSlaves = slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            listSlaveSdos(i, quiet, true);
        }
    } else {
        listSlaveSdos(slavePosition, quiet, false);
    }
}

/****************************************************************************/

void Master::sdoUpload(
        int slavePosition,
        const string &dataTypeStr,
        const vector<string> &commandArgs
        )
{
    stringstream strIndex, strSubIndex;
    int number, sval;
    ec_ioctl_sdo_upload_t data;
    unsigned int i, uval;
    const CoEDataType *dataType = NULL;

    if (slavePosition < 0) {
        stringstream err;
        err << "'sdo_upload' requires a slave! Please specify --slave.";
        throw MasterException(err.str());
    }
    data.slave_position = slavePosition;

    if (commandArgs.size() != 2) {
        stringstream err;
        err << "'sdo_upload' takes two arguments!";
        throw MasterException(err.str());
    }

    strIndex << commandArgs[0];
    strIndex >> hex >> number;
    if (strIndex.fail() || number < 0x0000 || number > 0xffff) {
        stringstream err;
        err << "Invalid Sdo index '" << commandArgs[0] << "'!";
        throw MasterException(err.str());
    }
    data.sdo_index = number;

    strSubIndex << commandArgs[1];
    strSubIndex >> hex >> number;
    if (strSubIndex.fail() || number < 0x00 || number > 0xff) {
        stringstream err;
        err << "Invalid Sdo subindex '" << commandArgs[1] << "'!";
        throw MasterException(err.str());
    }
    data.sdo_entry_subindex = number;

    if (dataTypeStr != "") { // data type specified
        if (!(dataType = findDataType(dataTypeStr))) {
            stringstream err;
            err << "Invalid data type '" << dataTypeStr << "'!";
            throw MasterException(err.str());
        }
    } else { // no data type specified: fetch from dictionary
        ec_ioctl_sdo_entry_t entry;
        unsigned int entryByteSize;

        open(Read);

        try {
            getSdoEntry(&entry, slavePosition,
                    data.sdo_index, data.sdo_entry_subindex);
        } catch (MasterException &e) {
            stringstream err;
            err << "Failed to determine Sdo entry data type. "
                << "Please specify --type.";
            throw MasterException(err.str());
        }
        if (!(dataType = findDataType(entry.data_type))) {
            stringstream err;
            err << "Pdo entry has unknown data type 0x"
                << hex << setfill('0') << setw(4) << entry.data_type << "!"
                << " Please specify --type.";
            throw MasterException(err.str());
        }
    }

    if (dataType->byteSize) {
        data.target_size = dataType->byteSize;
    } else {
        data.target_size = DefaultTargetSize;
    }

    data.target = new uint8_t[data.target_size + 1];

    open(Read);

    if (ioctl(fd, EC_IOCTL_SDO_UPLOAD, &data) < 0) {
        stringstream err;
        err << "Failed to upload Sdo: " << strerror(errno);
        delete [] data.target;
        close();
        throw MasterException(err.str());
    }

    close();

    if (dataType->byteSize && data.data_size != dataType->byteSize) {
        stringstream err;
        err << "Data type mismatch. Expected " << dataType->name
            << " with " << dataType->byteSize << " byte, but got "
            << data.data_size << " byte.";
        throw MasterException(err.str());
    }

    cout << setfill('0');
    switch (dataType->coeCode) {
        case 0x0002: // int8
            sval = *(int8_t *) data.target;
            cout << sval << " 0x" << hex << setw(2) << sval << endl;
            break;
        case 0x0003: // int16
            sval = le16tocpu(*(int16_t *) data.target);
            cout << sval << " 0x" << hex << setw(4) << sval << endl;
            break;
        case 0x0004: // int32
            sval = le32tocpu(*(int32_t *) data.target);
            cout << sval << " 0x" << hex << setw(8) << sval << endl;
            break;
        case 0x0005: // uint8
            uval = (unsigned int) *(uint8_t *) data.target;
            cout << uval << " 0x" << hex << setw(2) << uval << endl;
            break;
        case 0x0006: // uint16
            uval = le16tocpu(*(uint16_t *) data.target);
            cout << uval << " 0x" << hex << setw(4) << uval << endl;
            break;
        case 0x0007: // uint32
            uval = le32tocpu(*(uint32_t *) data.target);
            cout << uval << " 0x" << hex << setw(8) << uval << endl;
            break;
        case 0x0009: // string
            cout << string((const char *) data.target, data.data_size)
                << endl;
            break;
        default:
            printRawData(data.target, data.data_size);
            break;
    }

    delete [] data.target;
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

    open(ReadWrite);

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
    open(Read);

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

void Master::open(Permissions perm)
{
    stringstream deviceName;

    if (fd != -1) { // already open
        if (currentPermissions < perm) { // more permissions required
            close();
        } else {
            return;
        }
    }
    
    deviceName << "/dev/EtherCAT" << index;
    
    if ((fd = ::open(deviceName.str().c_str(),
                    perm == ReadWrite ? O_RDWR : O_RDONLY)) == -1) {
        stringstream err;
        err << "Failed to open master device " << deviceName.str() << ": "
            << strerror(errno);
        throw MasterException(err.str());
    }

    currentPermissions = perm;
}

/****************************************************************************/

void Master::close()
{
    if (fd == -1)
        return;

    ::close(fd);
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

void Master::listSlavePdos(
        uint16_t slavePosition,
        bool quiet,
        bool withHeader
        )
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

            if (quiet)
                continue;

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

void Master::listSlaveSdos(
        uint16_t slavePosition,
        bool quiet,
        bool withHeader
        )
{
    ec_ioctl_slave_t slave;
    ec_ioctl_sdo_t sdo;
    ec_ioctl_sdo_entry_t entry;
    unsigned int i, j, k;
    const CoEDataType *d;
    
    getSlave(&slave, slavePosition);

    if (withHeader)
        cout << "=== Slave " << slavePosition << " ===" << endl;

    for (i = 0; i < slave.sdo_count; i++) {
        getSdo(&sdo, slavePosition, i);

        cout << "Sdo "
            << hex << setfill('0') << setw(4) << sdo.sdo_index
            << ", \"" << sdo.name << "\"" << endl;

        if (quiet)
            continue;

        for (j = 0; j <= sdo.max_subindex; j++) {
            getSdoEntry(&entry, slavePosition, -i, j);

            cout << "  " << hex << setfill('0') << setw(2)
                << (unsigned int) entry.sdo_entry_subindex
                << ", ";

            if ((d = findDataType(entry.data_type))) {
                cout << d->name;
            } else {
                cout << "type " << setw(4) << entry.data_type;
            }

            cout << ", " << dec << entry.bit_length << " bit, \""
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
        int sdoSpec,
        uint8_t entrySubindex
        )
{
    entry->slave_position = slaveIndex;
    entry->sdo_spec = sdoSpec;
    entry->sdo_entry_subindex = entrySubindex;

    if (ioctl(fd, EC_IOCTL_SDO_ENTRY, entry)) {
        stringstream err;
        err << "Failed to get Sdo entry: ";
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

void Master::printRawData(
        const uint8_t *data,
        unsigned int size)
{
    cout << hex << setfill('0');
    while (size--) {
        cout << "0x" << setw(2) << (unsigned int) *data++;
        if (size)
            cout << " ";
    }
    cout << endl;
}

/****************************************************************************/
