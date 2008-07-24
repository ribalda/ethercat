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
        << "The file contents are checked for validity and integrity." << endl
        << "These checks can be overridden with the --force option." << endl
        << endl
        << "Arguments:" << endl
        << "  FILENAME must be a path to a file that contains a" << endl
        << "           positive number of words." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --slave -s <index>  Positive numerical ring position" << endl
        << "                      (mandatory)." << endl
        << "  --force -f          Override validity checks." << endl
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
    unsigned int byte_size;
    const uint16_t *categoryHeader;
    uint16_t categoryType, categorySize;
    uint8_t crc;

    if (slavePosition < 0) {
        err << "'" << getName() << "' requires a slave! "
            << "Please specify --slave.";
        throwInvalidUsageException(err);
    }
    data.slave_position = slavePosition;

    if (args.size() != 1) {
        err << "'" << getName() << "' takes exactly one argument!";
        throwInvalidUsageException(err);
    }

    file.open(args[0].c_str(), ifstream::in | ifstream::binary);
    if (file.fail()) {
        err << "Failed to open '" << args[0] << "'!";
        throwCommandException(err);
    }

    // get length of file
    file.seekg(0, ios::end);
    byte_size = file.tellg();
    file.seekg(0, ios::beg);

    if (!byte_size || byte_size % 2) {
        err << "Invalid file size! Must be non-zero and even.";
        throwCommandException(err);
    }

    data.nwords = byte_size / 2;
    if (data.nwords < 0x0041 && !force) {
        err << "SII data too short (" << data.nwords << " words)! Mimimum is"
                " 40 fixed words + 1 delimiter. Use --force to write anyway.";
        throwCommandException(err);
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
            throwCommandException(err);
        }

        // cycle through categories to detect corruption
        categoryHeader = data.words + 0x0040U;
        categoryType = le16tocpu(*categoryHeader);
        while (categoryType != 0xffff) {
            if (categoryHeader + 1 > data.words + data.nwords) {
                err << "SII data seem to be corrupted! "
                    << "Use --force to write anyway.";
                throwCommandException(err);
            }
            categorySize = le16tocpu(*(categoryHeader + 1));
            if (categoryHeader + 2 + categorySize + 1
                    > data.words + data.nwords) {
                err << "SII data seem to be corrupted! "
                    "Use --force to write anyway.";
                throwCommandException(err);
            }
            categoryHeader += 2 + categorySize;
            categoryType = le16tocpu(*categoryHeader);
        }
    }

    // send data to master
    m.open(MasterDevice::ReadWrite);
    data.offset = 0;
	m.writeSii(&data);
}

/*****************************************************************************/
