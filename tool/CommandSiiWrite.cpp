/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <fstream>
using namespace std;

#include "CommandSiiWrite.h"
#include "sii_crc.h"
#include "byteorder.h"

/*****************************************************************************/

CommandSiiWrite::CommandSiiWrite():
    Command("sii_write", "Write SII contents to a slave.")
{
}

/*****************************************************************************/

string CommandSiiWrite::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <FILENAME>" << endl
        << endl 
        << getBriefDescription() << endl
        << endl
        << "This command requires a single slave to be selected." << endl
    	<< endl
        << "The file contents are checked for validity and integrity." << endl
        << "These checks can be overridden with the --force option." << endl
        << endl
        << "Arguments:" << endl
        << "  FILENAME must be a path to a file that contains a" << endl
        << "           positive number of words. If it is '-'," << endl
        << "           data are read from stdin." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --force    -f          Override validity checks." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandSiiWrite::execute(MasterDevice &m, const StringVector &args)
{
    stringstream err;
    ec_ioctl_slave_sii_t data;
    ifstream file;
    SlaveList slaves;

    if (args.size() != 1) {
        err << "'" << getName() << "' takes exactly one argument!";
        throwInvalidUsageException(err);
    }

    if (args[0] == "-") {
        loadSiiData(&data, cin);
    } else {
        file.open(args[0].c_str(), ifstream::in | ifstream::binary);
        if (file.fail()) {
            err << "Failed to open '" << args[0] << "'!";
            throwCommandException(err);
        }
        loadSiiData(&data, file);
        file.close();
    }

    if (!getForce()) {
        try {
            checkSiiData(&data);
        } catch (CommandException &e) {
            delete [] data.words;
            throw e;
        }
    }

    try {
        m.open(MasterDevice::ReadWrite);
    } catch (MasterDeviceException &e) {
        delete [] data.words;
        throw e;
    }

    slaves = selectedSlaves(m);
    if (slaves.size() != 1) {
        delete [] data.words;
        throwSingleSlaveRequired(slaves.size());
    }
    data.slave_position = slaves.front().position;

    // send data to master
    data.offset = 0;
    try {
        m.writeSii(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.words;
        throw e;
    }

    if (getVerbosity() == Verbose) {
        cerr << "SII writing finished." << endl;
    }

    delete [] data.words;
}

/*****************************************************************************/

void CommandSiiWrite::loadSiiData(
        ec_ioctl_slave_sii_t *data,
        const istream &in
        )
{
    stringstream err;
    ostringstream tmp;

    tmp << in.rdbuf();
    string const &contents = tmp.str();

    if (getVerbosity() == Verbose) {
        cerr << "Read " << contents.size() << " bytes of SII data." << endl;
    }

    if (!contents.size() || contents.size() % 2) {
        err << "Invalid data size " << contents.size() << "! "
            << "Must be non-zero and even.";
        throwCommandException(err);
    }
    data->nwords = contents.size() / 2;

    // allocate buffer and read file into buffer
    data->words = new uint16_t[data->nwords];
    contents.copy((char *) data->words, contents.size());
}

/*****************************************************************************/

void CommandSiiWrite::checkSiiData(
        const ec_ioctl_slave_sii_t *data
        )
{
    stringstream err;
    const uint16_t *categoryHeader;
    uint16_t categoryType, categorySize;
    uint8_t crc;

    if (data->nwords < 0x0041) {
        err << "SII data too short (" << data->nwords << " words)! Mimimum is"
            " 40 fixed words + 1 delimiter. Use --force to write anyway.";
        throwCommandException(err);
    }

    // calculate checksum over words 0 to 6
    crc = calcSiiCrc((const uint8_t *) data->words, 14);
    if (crc != ((const uint8_t *) data->words)[14]) {
        err << "CRC incorrect. Must be 0x"
            << hex << setfill('0') << setw(2) << (unsigned int) crc
            << ". Use --force to write anyway.";
        throwCommandException(err);
    }

    // cycle through categories to detect corruption
    categoryHeader = data->words + 0x0040U;
    categoryType = le16tocpu(*categoryHeader);
    while (categoryType != 0xffff) {
        if (categoryHeader + 1 > data->words + data->nwords) {
            err << "SII data seem to be corrupted! "
                << "Use --force to write anyway.";
            throwCommandException(err);
        }
        categorySize = le16tocpu(*(categoryHeader + 1));
        if (categoryHeader + 2 + categorySize + 1
                > data->words + data->nwords) {
            err << "SII data seem to be corrupted! "
                "Use --force to write anyway.";
            throwCommandException(err);
        }
        categoryHeader += 2 + categorySize;
        categoryType = le16tocpu(*categoryHeader);
    }
}

/*****************************************************************************/
