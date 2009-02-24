/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <string.h>

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandFoeRead.h"
#include "foe.h"

/*****************************************************************************/

CommandFoeRead::CommandFoeRead():
    FoeCommand("foe_read", "Read a file from a slave via FoE.")
{
}

/*****************************************************************************/

string CommandFoeRead::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <SOURCEFILE>" << endl
    	<< endl
    	<< getBriefDescription() << endl
    	<< endl
        << "This command requires a single slave to be selected." << endl
    	<< endl
        << "Arguments:" << endl
        << "  SOURCEFILE is the name of the source file on the slave." << endl
        << endl
    	<< "Command-specific options:" << endl
        << "  --output-file -o <file>   Local target filename. If" << endl
        << "                            '-' (default), data are" << endl
        << "                            printed to stdout." << endl
        << "  --alias       -a <alias>  " << endl
        << "  --position    -p <pos>    Slave selection. See the help" << endl
        << "                            of the 'slaves' command." << endl
    	<< endl
		<< numericInfo();

	return str.str();
}

/****************************************************************************/

void CommandFoeRead::execute(MasterDevice &m, const StringVector &args)
{
    SlaveList slaves;
    ec_ioctl_slave_t *slave;
    ec_ioctl_slave_foe_t data;
    unsigned int i;
    stringstream err;

    if (args.size() != 1) {
        err << "'" << getName() << "' takes exactly one argument!";
        throwInvalidUsageException(err);
    }

    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);

    if (slaves.size() != 1) {
        throwSingleSlaveRequired(slaves.size());
    }
    slave = &slaves.front();
    data.slave_position = slave->position;

    /* FIXME: No good idea to have a fixed buffer size.
     * Read in chunks and fill a buffer instead.
     */
    data.offset = 0;
    data.buffer_size = 0x8800;
    data.buffer = new uint8_t[data.buffer_size];

    strncpy(data.file_name, args[0].c_str(), sizeof(data.file_name));

	try {
		m.readFoe(&data);
	} catch (MasterDeviceException &e) {
        delete [] data.buffer;
        if (data.result) {
            if (data.result == FOE_OPCODE_ERROR) {
                err << "FoE read aborted with error code 0x"
                    << setw(8) << setfill('0') << hex << data.error_code
                    << ": " << errorText(data.error_code);
            } else {
                err << "Failed to write via FoE: "
                    << resultText(data.result);
            }
            throwCommandException(err);
        } else {
            throw e;
        }
	}

    // TODO --output-file
	for (i = 0; i < data.data_size; i++) {
		uint8_t *w = data.buffer + i;
		cout << *(uint8_t *) w ;
	}

    delete [] data.buffer;
}

/*****************************************************************************/
