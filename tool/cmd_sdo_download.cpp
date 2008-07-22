/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "globals.h"
#include "coe_datatypes.h"

/****************************************************************************/

// FIXME
const char *help_sdo_download =
    "[OPTIONS]\n"
    "\n"
    "\n"
    "Command-specific options:\n";

/****************************************************************************/

void command_sdo_download(void)
{
    stringstream strIndex, strSubIndex, strValue, err;
    ec_ioctl_slave_sdo_download_t data;
    unsigned int number;
    const CoEDataType *dataType = NULL;

    if (slavePosition < 0) {
        err << "'sdo_download' requires a slave! Please specify --slave.";
        throw MasterDeviceException(err.str());
    }
    data.slave_position = slavePosition;

    if (commandArgs.size() != 3) {
        err << "'sdo_download' takes 3 arguments!";
        throw MasterDeviceException(err.str());
    }

    strIndex << commandArgs[0];
    strIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.sdo_index;
    if (strIndex.fail()) {
        err << "Invalid Sdo index '" << commandArgs[0] << "'!";
        throw MasterDeviceException(err.str());
    }

    strSubIndex << commandArgs[1];
    strSubIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> number;
    if (strSubIndex.fail() || number > 0xff) {
        err << "Invalid Sdo subindex '" << commandArgs[1] << "'!";
        throw MasterDeviceException(err.str());
    }
    data.sdo_entry_subindex = number;

    if (dataTypeStr != "") { // data type specified
        if (!(dataType = findDataType(dataTypeStr))) {
            err << "Invalid data type '" << dataTypeStr << "'!";
            throw MasterDeviceException(err.str());
        }
    } else { // no data type specified: fetch from dictionary
        ec_ioctl_slave_sdo_entry_t entry;

        masterDev.open(MasterDevice::ReadWrite);

        try {
            masterDev.getSdoEntry(&entry, slavePosition,
                    data.sdo_index, data.sdo_entry_subindex);
        } catch (MasterDeviceException &e) {
            err << "Failed to determine Sdo entry data type. "
                << "Please specify --type.";
            throw ExecutionFailureException(err);
        }
        if (!(dataType = findDataType(entry.data_type))) {
            err << "Pdo entry has unknown data type 0x"
                << hex << setfill('0') << setw(4) << entry.data_type << "!"
                << " Please specify --type.";
            throw ExecutionFailureException(err);
        }
    }

    if (dataType->byteSize) {
        data.data_size = dataType->byteSize;
    } else {
        data.data_size = DefaultBufferSize;
    }

    data.data = new uint8_t[data.data_size + 1];

    strValue << commandArgs[2];
    strValue >> resetiosflags(ios::basefield); // guess base from prefix
    strValue.exceptions(ios::failbit);

    try {
        switch (dataType->coeCode) {
            case 0x0002: // int8
                {
                    int16_t val; // uint8_t is interpreted as char
                    strValue >> val;
                    if (val > 127 || val < -128)
                        throw ios::failure("Value out of range");
                    *data.data = val;
                    break;
                }
            case 0x0003: // int16
                {
                    int16_t val;
                    strValue >> val;
                    *(int16_t *) data.data = cputole16(val);
                    break;
                }
            case 0x0004: // int32
                {
                    int32_t val;
                    strValue >> val;
                    *(int32_t *) data.data = cputole32(val);
                    break;
                }
            case 0x0005: // uint8
                {
                    uint16_t val; // uint8_t is interpreted as char
                    strValue >> val;
                    if (val > 0xff)
                        throw ios::failure("Value out of range");
                    *data.data = val;
                    break;
                }
            case 0x0006: // uint16
                {
                    uint16_t val;
                    strValue >> val;
                    *(uint16_t *) data.data = cputole16(val);
                    break;
                }
            case 0x0007: // uint32
                {
                    uint32_t val;
                    strValue >> val;
                    *(uint32_t *) data.data = cputole32(val);
                    break;
                }
            case 0x0009: // string
                if (strValue.str().size() >= data.data_size) {
                    err << "String too large";
                    throw MasterDeviceException(err.str());
                }
                data.data_size = strValue.str().size();
                strValue >> (char *) data.data;
                break;

            default:
                delete [] data.data;
                err << "Unknown data type 0x" << hex << dataType->coeCode;
                throw MasterDeviceException(err.str());
        }
    } catch (ios::failure &e) {
        delete [] data.data;
        err << "Invalid value argument '" << commandArgs[2]
            << "' for type '" << dataType->name << "'!";
        throw MasterDeviceException(err.str());
    }

    masterDev.open(MasterDevice::ReadWrite);

	try {
		masterDev.sdoDownload(&data);
	} catch(MasterDeviceException &e) {
        delete [] data.data;
        throw e;
    }

    delete [] data.data;
}

/*****************************************************************************/
