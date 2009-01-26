/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandDownload.h"

/*****************************************************************************/

CommandDownload::CommandDownload():
    SdoCommand("download", "Write an SDO entry to a slave.")
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
        << "This command requires a single slave to be selected." << endl
    	<< endl
    	<< "The data type of the SDO entry is taken from the SDO" << endl
		<< "dictionary by default. It can be overridden with the" << endl
		<< "--type option. If the slave does not support the SDO" << endl
		<< "information service or the SDO is not in the dictionary," << endl
		<< "the --type option is mandatory." << endl
    	<< endl
    	<< "These are the valid SDO entry data types:" << endl
    	<< "  int8, int16, int32, uint8, uint16, uint32, string," << endl
        << "  octet_string." << endl
    	<< endl
    	<< "Arguments:" << endl
    	<< "  INDEX    is the SDO index and must be an unsigned" << endl
		<< "           16 bit number." << endl
    	<< "  SUBINDEX is the SDO entry subindex and must be an" << endl
		<< "           unsigned 8 bit number." << endl
    	<< "  VALUE    is the value to download and must correspond" << endl
		<< "           to the SDO entry datatype (see above)." << endl
    	<< endl
    	<< "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
    	<< "  --type     -t <type>   SDO entry data type (see above)." << endl
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
    const DataType *dataType = NULL;
    SlaveList slaves;

    if (args.size() != 3) {
        err << "'" << getName() << "' takes 3 arguments!";
        throwInvalidUsageException(err);
    }

    strIndex << args[0];
    strIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> data.sdo_index;
    if (strIndex.fail()) {
        err << "Invalid SDO index '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }

    strSubIndex << args[1];
    strSubIndex
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> number;
    if (strSubIndex.fail() || number > 0xff) {
        err << "Invalid SDO subindex '" << args[1] << "'!";
        throwInvalidUsageException(err);
    }
    data.sdo_entry_subindex = number;

    m.open(MasterDevice::ReadWrite);
    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    data.slave_position = slaves.front().position;

    if (!getDataType().empty()) { // data type specified
        if (!(dataType = findDataType(getDataType()))) {
            err << "Invalid data type '" << getDataType() << "'!";
            throwInvalidUsageException(err);
        }
    } else { // no data type specified: fetch from dictionary
        ec_ioctl_slave_sdo_entry_t entry;

        try {
            m.getSdoEntry(&entry, data.slave_position,
                    data.sdo_index, data.sdo_entry_subindex);
        } catch (MasterDeviceException &e) {
            err << "Failed to determine SDO entry data type. "
                << "Please specify --type.";
            throwCommandException(err);
        }
        if (!(dataType = findDataType(entry.data_type))) {
            err << "PDO entry has unknown data type 0x"
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
                    *(int16_t *) data.data = cpu_to_le16(val);
                    break;
                }
            case 0x0004: // int32
                {
                    int32_t val;
                    strValue >> val;
                    *(int32_t *) data.data = cpu_to_le32(val);
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
                    *(uint16_t *) data.data = cpu_to_le16(val);
                    break;
                }
            case 0x0007: // uint32
                {
                    uint32_t val;
                    strValue >> val;
                    *(uint32_t *) data.data = cpu_to_le32(val);
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
            case 0x000a: // octet_string
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

	try {
        m.sdoDownload(&data);
	} catch (MasterDeviceSdoAbortException &e) {
        delete [] data.data;
        err << "SDO transfer aborted with code 0x"
            << setfill('0') << hex << setw(8) << e.abortCode
            << ": " << abortText(e.abortCode);
        throwCommandException(err);
	} catch(MasterDeviceException &e) {
        delete [] data.data;
        throw e;
    }

    delete [] data.data;
}

/*****************************************************************************/
