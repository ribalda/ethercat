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
#include <fstream>
#include <cctype> // toupper()
#include <list>
using namespace std;

#include "Master.h"

#define swap16(x) \
        ((uint16_t)( \
        (((uint16_t)(x) & 0x00ffU) << 8) | \
        (((uint16_t)(x) & 0xff00U) >> 8) ))
#define swap32(x) \
        ((uint32_t)( \
        (((uint32_t)(x) & 0x000000ffUL) << 24) | \
        (((uint32_t)(x) & 0x0000ff00UL) <<  8) | \
        (((uint32_t)(x) & 0x00ff0000UL) >>  8) | \
        (((uint32_t)(x) & 0xff000000UL) >> 24) ))

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define le16tocpu(x) x
#define le32tocpu(x) x

#define cputole16(x) x
#define cputole32(x) x

#elif __BYTE_ORDER == __BIG_ENDIAN

#define le16tocpu(x) swap16(x)
#define le32tocpu(x) swap32(x)

#define cputole16(x) swap16(x)
#define cputole32(x) swap32(x)

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

/*****************************************************************************/

/**
 * Writes the Secondary slave address (alias) to the slave's SII.
 */
void Master::writeAlias(
        int slavePosition,
        bool force,
        const vector<string> &commandArgs
        )
{
    ec_ioctl_sii_t data;
    ec_ioctl_slave_t slave;
    unsigned int i;
    uint16_t alias;
    stringstream err, strAlias;
    int number;

    if (commandArgs.size() != 1) {
        stringstream err;
        err << "'alias' takes exactly one argument!";
        throw MasterException(err.str());
    }

    strAlias << commandArgs[0];
    strAlias >> hex >> number;
    if (strAlias.fail() || number < 0x0000 || number > 0xffff) {
        err << "Invalid alias '" << commandArgs[0] << "'!";
        throw MasterException(err.str());
    }
    alias = number;

    if (slavePosition == -1) {
        unsigned int numSlaves, i;

        if (!force) {
            err << "This will write the alias addresses of all slaves to 0x"
                << hex << setfill('0') << setw(4) << alias << "! "
                << "Please specify --force to proceed.";
            throw MasterException(err.str());
        }

        open(ReadWrite);
        numSlaves = slaveCount();

        for (i = 0; i < numSlaves; i++) {
            writeSlaveAlias(i, alias);
        }
    } else {
        open(ReadWrite);
        writeSlaveAlias(slavePosition, alias);
    }
}

/*****************************************************************************/

/**
 * Lists the bus configuration.
 */
void Master::showConfig()
{
    ec_ioctl_master_t master;
    unsigned int i, j, k, l;
    ec_ioctl_config_t config;
    ec_ioctl_config_pdo_t pdo;
    ec_ioctl_config_pdo_entry_t entry;
    ec_ioctl_config_sdo_t sdo;

    open(Read);
    getMaster(&master);

    for (i = 0; i < master.config_count; i++) {
        getConfig(&config, i);

        cout << "Alias: 0x"
            << hex << setfill('0') << setw(4) << config.alias << endl
            << "Position: " << dec << config.position << endl
            << "Vendor Id: 0x"
            << hex << setw(8) << config.vendor_id << endl
            << "Product code: 0x"
            << hex << setw(8) << config.product_code << endl
            << "Attached: " << (config.attached ? "yes" : "no") << endl
            << "Operational: " << (config.operational ? "yes" : "no") << endl;

        for (j = 0; j < 16; j++) {
            if (config.syncs[j].pdo_count) {
                cout << "SM" << dec << j << " ("
                    << (config.syncs[j].dir == EC_DIR_INPUT
                            ? "Input" : "Output") << ")" << endl;
                for (k = 0; k < config.syncs[j].pdo_count; k++) {
                    getConfigPdo(&pdo, i, j, k);

                    cout << "  Pdo 0x"
                        << hex << setfill('0') << setw(4) << pdo.index
                        << " \"" << pdo.name << "\"" << endl;

                    for (l = 0; l < pdo.entry_count; l++) {
                        getConfigPdoEntry(&entry, i, j, k, l);

                        cout << "    Pdo entry 0x"
                            << hex << setfill('0') << setw(4) << entry.index
                            << ":" << setw(2) << (unsigned int) entry.subindex
                            << ", " << dec << (unsigned int) entry.bit_length
                            << " bit, \"" << entry.name << "\"" << endl;
                    }
                }
            }
        }

        if (config.sdo_count) {
            cout << "Sdo configuration:" << endl;
            for (j = 0; j < config.sdo_count; j++) {
                getConfigSdo(&sdo, i, j);

                cout << "  0x"
                    << hex << setfill('0') << setw(4) << sdo.index
                    << ":" << setw(2) << (unsigned int) sdo.subindex
                    << ", " << sdo.size << " byte: " << hex;

                switch (sdo.size) {
                    case 1:
                        cout << "0x" << setw(2)
                            << (unsigned int) *(uint8_t *) &sdo.data;
                        break;
                    case 2:
                        cout << "0x" << setw(4)
                            << le16tocpu(*(uint16_t *) &sdo.data);
                        break;
                    case 4:
                        cout << "0x" << setw(8)
                            << le32tocpu(*(uint32_t *) &sdo.data);
                        break;
                    default:
                        cout << "???";
                }

                cout << endl;
            }
        }

        cout << endl;
    }
}

/****************************************************************************/

void Master::outputData(int domainIndex)
{
    open(Read);

    if (domainIndex == -1) {
        unsigned int i;
        ec_ioctl_master_t master;

        getMaster(&master);

        for (i = 0; i < master.domain_count; i++) {
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
        unsigned int i;
        ec_ioctl_master_t master;

        getMaster(&master);

        for (i = 0; i < master.domain_count; i++) {
            showDomain(i);
        }
    } else {
        showDomain(domainIndex);
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
        << "  Phase: ";

    switch (data.phase) {
        case 0: cout << "Waiting for device..."; break;
        case 1: cout << "Idle"; break;
        case 2: cout << "Operation"; break;
        default:
                err << "Invalid master phase " << data.phase;
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

void Master::sdoDownload(
        int slavePosition,
        const string &dataTypeStr,
        const vector<string> &commandArgs
        )
{
    stringstream strIndex, strSubIndex, strValue, err;
    int number, sval;
    ec_ioctl_sdo_download_t data;
    unsigned int i, uval;
    const CoEDataType *dataType = NULL;

    if (slavePosition < 0) {
        err << "'sdo_download' requires a slave! Please specify --slave.";
        throw MasterException(err.str());
    }
    data.slave_position = slavePosition;

    if (commandArgs.size() != 3) {
        err << "'sdo_download' takes 3 arguments!";
        throw MasterException(err.str());
    }

    strIndex << commandArgs[0];
    strIndex >> hex >> number;
    if (strIndex.fail() || number < 0x0000 || number > 0xffff) {
        err << "Invalid Sdo index '" << commandArgs[0] << "'!";
        throw MasterException(err.str());
    }
    data.sdo_index = number;

    strSubIndex << commandArgs[1];
    strSubIndex >> hex >> number;
    if (strSubIndex.fail() || number < 0x00 || number > 0xff) {
        err << "Invalid Sdo subindex '" << commandArgs[1] << "'!";
        throw MasterException(err.str());
    }
    data.sdo_entry_subindex = number;

    if (dataTypeStr != "") { // data type specified
        if (!(dataType = findDataType(dataTypeStr))) {
            err << "Invalid data type '" << dataTypeStr << "'!";
            throw MasterException(err.str());
        }
    } else { // no data type specified: fetch from dictionary
        ec_ioctl_sdo_entry_t entry;
        unsigned int entryByteSize;

        open(ReadWrite);

        try {
            getSdoEntry(&entry, slavePosition,
                    data.sdo_index, data.sdo_entry_subindex);
        } catch (MasterException &e) {
            err << "Failed to determine Sdo entry data type. "
                << "Please specify --type.";
            throw MasterException(err.str());
        }
        if (!(dataType = findDataType(entry.data_type))) {
            err << "Pdo entry has unknown data type 0x"
                << hex << setfill('0') << setw(4) << entry.data_type << "!"
                << " Please specify --type.";
            throw MasterException(err.str());
        }
    }

    if (dataType->byteSize) {
        data.data_size = dataType->byteSize;
    } else {
        data.data_size = DefaultBufferSize;
    }

    data.data = new uint8_t[data.data_size + 1];

    strValue << commandArgs[2];

    switch (dataType->coeCode) {
        case 0x0002: // int8
            strValue >> sval;
            if ((uint32_t) sval > 0xff) {
                delete [] data.data;
                err << "Invalid value for type '"
                    << dataType->name << "'!";
                throw MasterException(err.str());
            }
            *data.data = (int8_t) sval;
            break;
        case 0x0003: // int16
            strValue >> sval;
            if ((uint32_t) sval > 0xffff) {
                delete [] data.data;
                err << "Invalid value for type '"
                    << dataType->name << "'!";
                throw MasterException(err.str());
            }
            *(int16_t *) data.data = cputole16(sval);
            break;
        case 0x0004: // int32
            strValue >> sval;
            *(int32_t *) data.data = cputole32(sval);
            break;
        case 0x0005: // uint8
            strValue >> uval;
            if ((uint32_t) uval > 0xff) {
                delete [] data.data;
                err << "Invalid value for type '"
                    << dataType->name << "'!";
                throw MasterException(err.str());
            }
            *data.data = (uint8_t) uval;
            break;
        case 0x0006: // uint16
            strValue >> uval;
            if ((uint32_t) uval > 0xffff) {
                delete [] data.data;
                err << "Invalid value for type '"
                    << dataType->name << "'!";
                throw MasterException(err.str());
            }
            *(uint16_t *) data.data = cputole16(uval);
            break;
        case 0x0007: // uint32
            strValue >> uval;
            *(uint32_t *) data.data = cputole32(uval);
            break;
        case 0x0009: // string
            if (strValue.str().size() >= data.data_size) {
                err << "String too big";
                throw MasterException(err.str());
            }
            data.data_size = strValue.str().size();
            strValue >> (char *) data.data;
            break;
        default:
            break;
    }

    if (strValue.fail()) {
        err << "Invalid value argument '" << commandArgs[2] << "'!";
        throw MasterException(err.str());
    }

    open(ReadWrite);

    if (ioctl(fd, EC_IOCTL_SDO_DOWNLOAD, &data) < 0) {
        stringstream err;
        err << "Failed to download Sdo: ";
        if (errno == EIO && data.abort_code) {
            err << "Abort code 0x" << hex << setfill('0') << setw(8)
                << data.abort_code;
        } else {
            err << strerror(errno);
        }
        delete [] data.data;
        throw MasterException(err.str());
    }

    delete [] data.data;
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
        data.target_size = DefaultBufferSize;
    }

    data.target = new uint8_t[data.target_size + 1];

    open(Read);

    if (ioctl(fd, EC_IOCTL_SDO_UPLOAD, &data) < 0) {
        stringstream err;
        err << "Failed to upload Sdo: ";
        if (errno == EIO && data.abort_code) {
            err << "Abort code 0x" << hex << setfill('0') << setw(8)
                << data.abort_code;
        } else {
            err << strerror(errno);
        }
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

void Master::showSlaves(int slavePosition, bool verbose)
{
    open(Read);

    if (verbose) {
        if (slavePosition == -1) {
            unsigned int numSlaves = slaveCount(), i;

            for (i = 0; i < numSlaves; i++) {
                showSlave(i);
            }
        } else {
            showSlave(slavePosition);
        }
    } else {
        listSlaves(slavePosition);
    }
}

/****************************************************************************/

void Master::siiRead(int slavePosition)
{
    ec_ioctl_sii_t data;
    ec_ioctl_slave_t slave;
    unsigned int i;

    if (slavePosition < 0) {
        stringstream err;
        err << "'sii_read' requires a slave! Please specify --slave.";
        throw MasterException(err.str());
    }
    data.slave_position = slavePosition;

    open(Read);

    getSlave(&slave, slavePosition);

    if (!slave.sii_nwords)
        return;

    data.offset = 0;
    data.nwords = slave.sii_nwords;
    data.words = new uint16_t[data.nwords];

    if (ioctl(fd, EC_IOCTL_SII_READ, &data) < 0) {
        stringstream err;
        delete [] data.words;
        err << "Failed to read SII: " << strerror(errno);
        throw MasterException(err.str());
    }

    for (i = 0; i < data.nwords; i++) {
        uint16_t *w = data.words + i;
        cout << *(uint8_t *) w << *((uint8_t *) w + 1);
    }

    delete [] data.words;
}

/****************************************************************************/

void Master::siiWrite(
        int slavePosition,
        bool force,
        const vector<string> &commandArgs
        )
{
    stringstream err;
    ec_ioctl_sii_t data;
    ifstream file;
    unsigned int byte_size;
    const uint16_t *categoryHeader;
    uint16_t categoryType, categorySize;
    uint8_t crc;

    if (slavePosition < 0) {
        err << "'sii_write' requires a slave! Please specify --slave.";
        throw MasterException(err.str());
    }
    data.slave_position = slavePosition;

    if (commandArgs.size() != 1) {
        err << "'ssi_write' takes exactly one argument!";
        throw MasterException(err.str());
    }

    file.open(commandArgs[0].c_str(), ifstream::in | ifstream::binary);
    if (file.fail()) {
        err << "Failed to open '" << commandArgs[0] << "'!";
        throw MasterException(err.str());
    }

    // get length of file
    file.seekg(0, ios::end);
    byte_size = file.tellg();
    file.seekg(0, ios::beg);

    if (!byte_size || byte_size % 2) {
        stringstream err;
        err << "Invalid file size! Must be non-zero and even.";
        throw MasterException(err.str());
    }

    data.nwords = byte_size / 2;
    if (data.nwords < 0x0041 && !force) {
        err << "SII data too short (" << data.nwords << " words)! Mimimum is"
                " 40 fixed words + 1 delimiter. Use --force to write anyway.";
        throw MasterException(err.str());
    }

    // allocate buffer and read file into buffer
    data.words = new uint16_t[data.nwords];
    file.read((char *) data.words, byte_size);
    file.close();

    if (!force) {
        // calculate checksum over words 0 to 6
        crc = calcSiiCrc((const uint8_t *) data.words, 14);
        if (crc != ((const uint8_t *) data.words)[14]) {
            err << "CRC incorrect. Must be 0x"
                << hex << setfill('0') << setw(2) << (unsigned int) crc
                << ". Use --force to write anyway.";
            throw MasterException(err.str());
        }

        // cycle through categories to detect corruption
        categoryHeader = data.words + 0x0040;
        categoryType = le16tocpu(*categoryHeader);
        while (categoryType != 0xffff) {
            if (categoryHeader + 1 > data.words + data.nwords) {
                err << "SII data seem to be corrupted! "
                    << "Use --force to write anyway.";
                throw MasterException(err.str());
            }
            categorySize = le16tocpu(*(categoryHeader + 1));
            if (categoryHeader + categorySize + 2 > data.words + data.nwords) {
                err << "SII data seem to be corrupted! "
                    "Use --force to write anyway.";
                throw MasterException(err.str());
            }
            categoryHeader += categorySize + 2;
            categoryType = le16tocpu(*categoryHeader);
        }
    }

    // send data to master
    open(ReadWrite);
    data.offset = 0;
    if (ioctl(fd, EC_IOCTL_SII_WRITE, &data) < 0) {
        stringstream err;
        err << "Failed to write SII: " << strerror(errno);
        throw MasterException(err.str());
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
        return;
    }
    
    deviceName << "/dev/EtherCAT" << index;
    
    if ((fd = ::open(deviceName.str().c_str(),
                    perm == ReadWrite ? O_RDWR : O_RDONLY)) == -1) {
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
    fd = -1;
}

/*****************************************************************************/

/**
 * Writes the Secondary slave address (alias) to the slave's SII.
 */
void Master::writeSlaveAlias(
        uint16_t slavePosition,
        uint16_t alias
        )
{
    ec_ioctl_sii_t data;
    ec_ioctl_slave_t slave;
    stringstream err;
    uint8_t crc;

    open(ReadWrite);

    getSlave(&slave, slavePosition);

    if (slave.sii_nwords < 8) {
        err << "Current SII contents are too small to set an alias "
            << "(" << slave.sii_nwords << " words)!";
        throw MasterException(err.str());
    }

    data.slave_position = slavePosition;
    data.offset = 0;
    data.nwords = 8;
    data.words = new uint16_t[data.nwords];

    // read first 8 SII words
    if (ioctl(fd, EC_IOCTL_SII_READ, &data) < 0) {
        delete [] data.words;
        err << "Failed to read SII: " << strerror(errno);
        throw MasterException(err.str());
    }

    // write new alias address in word 4
    data.words[4] = cputole16(alias);

    // calculate checksum over words 0 to 6
    crc = calcSiiCrc((const uint8_t *) data.words, 14);

    // write new checksum into first byte of word 7
    *(uint8_t *) (data.words + 7) = crc;

    // write first 8 words with new alias and checksum
    if (ioctl(fd, EC_IOCTL_SII_WRITE, &data) < 0) {
        delete [] data.words;
        err << "Failed to write SII: " << strerror(errno);
        throw MasterException(err.str());
    }

    delete [] data.words;
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
            << ", SM" << dec << (unsigned int) fmmu.sync_index << " ("
            << setfill(' ') << setw(3)
            << (fmmu.dir == EC_DIR_INPUT ? "Input" : "Output")
            << "), LogAddr 0x" 
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

            cout << "  " << (sync.control_register & 0x04 ? "R" : "T")
                << "xPdo 0x"
                << hex << setfill('0') << setw(4) << pdo.index
                << " \"" << pdo.name << "\"" << endl;

            if (quiet)
                continue;

            for (k = 0; k < pdo.entry_count; k++) {
                getPdoEntry(&entry, slavePosition, i, j, k);

                cout << "    Pdo entry 0x"
                    << hex << setfill('0') << setw(4) << entry.index
                    << ":" << setw(2) << (unsigned int) entry.subindex
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

            cout << "  " << hex << setfill('0')
                << setw(4) << sdo.sdo_index << ":" 
                << setw(2) << (unsigned int) entry.sdo_entry_subindex
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

struct SlaveInfo {
    string pos;
    string alias;
    string relPos;
    string state;
    string flag;
    string name;
};

void Master::listSlaves(int slavePosition)
{
    unsigned int numSlaves, i;
    ec_ioctl_slave_t slave;
    uint16_t lastAlias, aliasIndex;
    SlaveInfo slaveInfo;
    typedef list<SlaveInfo> SlaveInfoList;
    SlaveInfoList slaveInfoList;
    SlaveInfoList::const_iterator iter;
    stringstream str;
    unsigned int maxPosWidth = 0, maxAliasWidth = 0,
                 maxRelPosWidth = 0, maxStateWidth = 0;
    
    open(Read);

    numSlaves = slaveCount();

    lastAlias = 0;
    aliasIndex = 0;
    for (i = 0; i < numSlaves; i++) {
        getSlave(&slave, i);
        
        if (slave.alias) {
            lastAlias = slave.alias;
            aliasIndex = 0;
        }

        if (slavePosition == -1 || i == slavePosition) {
            str << dec << i;
            slaveInfo.pos = str.str();
            str.clear();
            str.str("");

            if (lastAlias) {
                str << "#" << hex << lastAlias;
                slaveInfo.alias = str.str();
                str.str("");
                str << ":" << dec << aliasIndex;
                slaveInfo.relPos = str.str();
                str.str("");
            } else {
                slaveInfo.alias = "";
                slaveInfo.relPos = "";
            }

            slaveInfo.state = slaveState(slave.state);
            slaveInfo.flag = (slave.error_flag ? 'E' : '+');

            if (strlen(slave.name)) {
                slaveInfo.name = slave.name;
            } else {
                str << hex << setfill('0')
                    << setw(8) << slave.vendor_id << ":"
                    << setw(8) << slave.product_code;
                slaveInfo.name = str.str();
                str.str("");
            }


            slaveInfoList.push_back(slaveInfo);

            if (slaveInfo.pos.length() > maxPosWidth)
                maxPosWidth = slaveInfo.pos.length();
            if (slaveInfo.alias.length() > maxAliasWidth)
                maxAliasWidth = slaveInfo.alias.length();
            if (slaveInfo.relPos.length() > maxRelPosWidth)
                maxRelPosWidth = slaveInfo.relPos.length();
            if (slaveInfo.state.length() > maxStateWidth)
                maxStateWidth = slaveInfo.state.length();
        }

        if (lastAlias)
            aliasIndex++;
    }

    for (iter = slaveInfoList.begin(); iter != slaveInfoList.end(); iter++) {
        cout << setfill(' ') << right
            << setw(maxPosWidth) << iter->pos << "  "
            << setw(maxAliasWidth) << iter->alias
            << left
            << setw(maxRelPosWidth) << iter->relPos << "  "
            << setw(maxStateWidth) << iter->state << "  "
            << iter->flag << "  "
            << iter->name << endl;
    }
}

/****************************************************************************/

void Master::showSlave(uint16_t slavePosition)
{
    ec_ioctl_slave_t slave;
    list<string> protoList;
    list<string>::const_iterator protoIter;
    
    getSlave(&slave, slavePosition);
        
    cout << "Slave " << dec << slavePosition << endl
        << "Alias: 0x" << hex << setfill('0') << setw(4) << slave.alias << endl
        << "State: " << slaveState(slave.state) << endl
        << "Flag: " << (slave.error_flag ? 'E' : '+') << endl
        << "Identity:" << endl
        << "  Vendor Id: 0x"
        << hex << setfill('0') << setw(8) << slave.vendor_id << endl
        << "  Product code: 0x"
        << setw(8) << slave.product_code << endl
        << "  Revision number: 0x"
        << setw(8) << slave.revision_number << endl
        << "  Serial number: 0x"
        << setw(8) << slave.serial_number << endl;

    if (slave.mailbox_protocols) {
        cout << "Mailboxes:" << endl
        << "  RX: 0x"
        << hex << setw(4) << slave.rx_mailbox_offset << "/"
        << dec << slave.rx_mailbox_size
        << ", TX: 0x"
        << hex << setw(4) << slave.tx_mailbox_offset << "/"
        << dec << slave.tx_mailbox_size << endl
        << "  Supported protocols: ";

        if (slave.mailbox_protocols & EC_MBOX_AOE) {
            protoList.push_back("AoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_EOE) {
            protoList.push_back("EoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_COE) {
            protoList.push_back("CoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_FOE) {
            protoList.push_back("FoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_SOE) {
            protoList.push_back("SoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_VOE) {
            protoList.push_back("VoE");
        }

        for (protoIter = protoList.begin(); protoIter != protoList.end();
                protoIter++) {
            if (protoIter != protoList.begin())
                cout << ", ";
            cout << *protoIter;
        }
        cout << endl;
    }

    if (slave.has_general_category) {
        cout << "General:" << endl
            << "  Name: " << slave.name << endl;

        if (slave.mailbox_protocols & EC_MBOX_COE) {
            cout << "  CoE details:" << endl
                << "    Enable Sdo: "
                << (slave.coe_details.enable_sdo ? "yes" : "no") << endl
                << "    Enable Sdo Info: "
                << (slave.coe_details.enable_sdo_info ? "yes" : "no") << endl
                << "    Enable Pdo Assign: "
                << (slave.coe_details.enable_pdo_assign
                        ? "yes" : "no") << endl
                << "    Enable Pdo Configuration: "
                << (slave.coe_details.enable_pdo_configuration
                        ? "yes" : "no") << endl
                << "    Enable Upload at startup: "
                << (slave.coe_details.enable_upload_at_startup
                        ? "yes" : "no") << endl
                << "    Enable Sdo complete access: "
                << (slave.coe_details.enable_sdo_complete_access
                        ? "yes" : "no") << endl;
        }

        cout << "  Flags:" << endl
            << "    Enable SafeOp: "
            << (slave.general_flags.enable_safeop ? "yes" : "no") << endl
            << "    Enable notLRW: "
            << (slave.general_flags.enable_not_lrw ? "yes" : "no") << endl
            << "  Current consumption: "
            << dec << slave.current_on_ebus << " mA" << endl;
    }
    cout << endl;
}

/****************************************************************************/

void Master::generateSlaveXml(uint16_t slavePosition)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_sync_t sync;
    ec_ioctl_pdo_t pdo;
    string pdoType;
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
            pdoType = (sync.control_register & 0x04 ? "R" : "T");
            pdoType += "xPdo";

            cout
                << "          <" << pdoType << ">" << endl
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
                    << dec << (unsigned int) entry.bit_length
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
                << "          </" << pdoType << ">" << endl;
        }
    }

    cout
        << "        </Device>" << endl
        << "     </Devices>" << endl
        << "  </Descriptions>" << endl
        << "</EtherCATInfo>" << endl;
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

void Master::getConfig(ec_ioctl_config_t *data, unsigned int index)
{
    data->config_index = index;

    if (ioctl(fd, EC_IOCTL_CONFIG, data) < 0) {
        stringstream err;
        err << "Failed to get slave configuration: " << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::getConfigPdo(
        ec_ioctl_config_pdo_t *data,
        unsigned int index,
        uint8_t sync_index,
        uint16_t pdo_pos
        )
{
    data->config_index = index;
    data->sync_index = sync_index;
    data->pdo_pos = pdo_pos;

    if (ioctl(fd, EC_IOCTL_CONFIG_PDO, data) < 0) {
        stringstream err;
        err << "Failed to get slave config Pdo: " << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::getConfigPdoEntry(
        ec_ioctl_config_pdo_entry_t *data,
        unsigned int index,
        uint8_t sync_index,
        uint16_t pdo_pos,
        uint8_t entry_pos
        )
{
    data->config_index = index;
    data->sync_index = sync_index;
    data->pdo_pos = pdo_pos;
    data->entry_pos = entry_pos;

    if (ioctl(fd, EC_IOCTL_CONFIG_PDO_ENTRY, data) < 0) {
        stringstream err;
        err << "Failed to get slave config Pdo entry: " << strerror(errno);
        throw MasterException(err.str());
    }
}

/****************************************************************************/

void Master::getConfigSdo(
        ec_ioctl_config_sdo_t *data,
        unsigned int index,
        unsigned int sdo_pos
        )
{
    data->config_index = index;
    data->sdo_pos = sdo_pos;

    if (ioctl(fd, EC_IOCTL_CONFIG_SDO, data) < 0) {
        stringstream err;
        err << "Failed to get slave config Sdo: " << strerror(errno);
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

/*****************************************************************************/

/**
 * Calculates the SII checksum field.
 *
 * The checksum is generated with the polynom x^8+x^2+x+1 (0x07) and an
 * initial value of 0xff (see IEC 61158-6-12 ch. 5.4).
 *
 * The below code was originally generated with PYCRC
 * http://www.tty1.net/pycrc
 *
 * ./pycrc.py --width=8 --poly=0x07 --reflect-in=0 --xor-in=0xff
 *   --reflect-out=0 --xor-out=0 --generate c --algorithm=bit-by-bit
 *
 * \return CRC8
 */
uint8_t Master::calcSiiCrc(
        const uint8_t *data, /**< pointer to data */
        size_t length /**< number of bytes in \a data */
        )
{
    unsigned int i;
    uint8_t bit, byte, crc = 0x48;

    while (length--) {
        byte = *data++;
        for (i = 0; i < 8; i++) {
            bit = crc & 0x80;
            crc = (crc << 1) | ((byte >> (7 - i)) & 0x01);
            if (bit) crc ^= 0x07;
        }
    }

    for (i = 0; i < 8; i++) {
        bit = crc & 0x80;
        crc <<= 1;
        if (bit) crc ^= 0x07;
    }

    return crc;
}

/*****************************************************************************/
