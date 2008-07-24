/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandDownload.h"
#include "coe_datatypes.h"
#include "byteorder.h"

/*****************************************************************************/

CommandDownload::CommandDownload():
    Command("download", "Write an Sdo entry to a slave.")
{
}

/*****************************************************************************/

string CommandDownload::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <INDEX> <SUBINDEX> <VALUE>" << endl
    	<< endl
    	<< getBriefDescription() << endl
    	<< endl
    	<< "The data type of the Sdo entry is taken from the Sdo" << endl
		<< "dictionary by default. It can be overridden with the" << endl
		<< "--type option. If the slave does not support the Sdo" << endl
		<< "information service or the Sdo is not in the dictionary," << endl
		<< "the --type option is mandatory." << endl
    	<< endl
    	<< "These are the valid Sdo entry data types:" << endl
    	<< "  int8, int16, int32, uint8, uint16, uint32, string." << endl
    	<< endl
    	<< "Arguments:"
    	<< "  INDEX    is the Sdo index and must be an unsigned" << endl
		<< "           16 bit number." << endl
    	<< "  SUBINDEX is the Sdo entry subindex and must be an" << endl
		<< "           unsigned 8 bit number." << endl
    	<< "  VALUE    is the value to download and must correspond" << endl
		<< "           to the Sdo entry datatype (see above)." << endl
    	<< endl
    	<< "Command-specific options:" << endl
    	<< "  --slave -s <index>  Positive numerical ring position" << endl
		<< "                      (mandatory)." << endl
    	<< "  --type  -t <type>   Forced Sdo entry data type (see" << endl
		<< "                      above)." << endl
    	<< endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandDownload::execute(MasterDevice &m, const StringVector &args)
{
    stringstream strIndex, strSubIndex, strValue, err;
    ec_ioctl_slave_sdo_download_t data;
    unsigned int number;
    const CoEDataType *dataType = NULL;

    if (slavePosition < 0) {
        err << "'" << getName() << "' requires a slave! "
            << "Please specify --slave.";
        throwInvalidUsageException(err);
    }
    data.slave_position = slavePosition;

    if (args.size() != 3) {
        err << "'" << getName() << "' takes 3 arguments!";
        throwInvalidUsageException(err);
    }

    strIndex << args[0];
    strIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.sdo_index;
    if (strIndex.fail()) {
        err << "Invalid Sdo index '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

    strSubIndex << args[1];
    strSubIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> number;
    if (strSubIndex.fail() || number > 0xff) {
        err << "Invalid Sdo subindex '" << args[1] << "'!";
        throwInvalidUsageException(err);
    }
    data.sdo_entry_subindex = number;

    if (dataTypeStr != "") { // data type specified
        if (!(dataType = findDataType(dataTypeStr))) {
            err << "Invalid data type '" << dataTypeStr << "'!";
            throwInvalidUsageException(err);
        }
    } else { // no data type specified: fetch from dictionary
        ec_ioctl_slave_sdo_entry_t entry;

        m.open(MasterDevice::ReadWrite);

        try {
            m.getSdoEntry(&entry, slavePosition,
                    data.sdo_index, data.sdo_entry_subindex);
        } catch (MasterDeviceException &e) {
            err << "Failed to determine Sdo entry data type. "
                << "Please specify --type.";
            throwCommandException(err);
        }
        if (!(dataType = findDataType(entry.data_type))) {
            err << "Pdo entry has unknown data type 0x"
                << hex << setfill('0') << setw(4) << entry.data_type << "!"
                << " Please specify --type.";
            throwCommandException(err);
        }
    }

    if (dataType->byteSize) {
        data.data_size = dataType->byteSize;
    } else {
        data.data_size = DefaultBufferSize;
    }

    data.data = new uint8_t[data.data_size + 1];

    strValue << args[2];
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
                    throwCommandException(err);
                }
                data.data_size = strValue.str().size();
                strValue >> (char *) data.data;
                break;

            default:
                delete [] data.data;
                err << "Unknown data type 0x" << hex << dataType->coeCode;
                throwCommandException(err);
        }
    } catch (ios::failure &e) {
        delete [] data.data;
        err << "Invalid value argument '" << args[2]
            << "' for type '" << dataType->name << "'!";
        throwInvalidUsageException(err);
    }

    m.open(MasterDevice::ReadWrite);

	try {
			m.sdoDownload(&data);
	} catch(MasterDeviceException &e) {
        delete [] data.data;
        throw e;
    }

    delete [] data.data;
}

/*****************************************************************************/
