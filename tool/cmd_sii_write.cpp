/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <fstream>
using namespace std;

#include "globals.h"
#include "sii_crc.h"

/****************************************************************************/

// FIXME
const char *help_sii_write =
    "[OPTIONS]\n"
    "\n"
    "\n"
    "Command-specific options:\n";

/****************************************************************************/

void command_sii_write(void)
{
    stringstream err;
    ec_ioctl_slave_sii_t data;
    ifstream file;
    unsigned int byte_size;
    const uint16_t *categoryHeader;
    uint16_t categoryType, categorySize;
    uint8_t crc;

    if (slavePosition < 0) {
        err << "'sii_write' requires a slave! Please specify --slave.";
        throw MasterDeviceException(err.str());
    }
    data.slave_position = slavePosition;

    if (commandArgs.size() != 1) {
        err << "'ssi_write' takes exactly one argument!";
        throw MasterDeviceException(err.str());
    }

    file.open(commandArgs[0].c_str(), ifstream::in | ifstream::binary);
    if (file.fail()) {
        err << "Failed to open '" << commandArgs[0] << "'!";
        throw MasterDeviceException(err.str());
    }

    // get length of file
    file.seekg(0, ios::end);
    byte_size = file.tellg();
    file.seekg(0, ios::beg);

    if (!byte_size || byte_size % 2) {
        stringstream err;
        err << "Invalid file size! Must be non-zero and even.";
        throw MasterDeviceException(err.str());
    }

    data.nwords = byte_size / 2;
    if (data.nwords < 0x0041 && !force) {
        err << "SII data too short (" << data.nwords << " words)! Mimimum is"
                " 40 fixed words + 1 delimiter. Use --force to write anyway.";
        throw MasterDeviceException(err.str());
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
            throw MasterDeviceException(err.str());
        }

        // cycle through categories to detect corruption
        categoryHeader = data.words + 0x0040U;
        categoryType = le16tocpu(*categoryHeader);
        while (categoryType != 0xffff) {
            if (categoryHeader + 1 > data.words + data.nwords) {
                err << "SII data seem to be corrupted! "
                    << "Use --force to write anyway.";
                throw MasterDeviceException(err.str());
            }
            categorySize = le16tocpu(*(categoryHeader + 1));
            if (categoryHeader + 2 + categorySize + 1
                    > data.words + data.nwords) {
                err << "SII data seem to be corrupted! "
                    "Use --force to write anyway.";
                throw MasterDeviceException(err.str());
            }
            categoryHeader += 2 + categorySize;
            categoryType = le16tocpu(*categoryHeader);
        }
    }

    // send data to master
    masterDev.open(MasterDevice::ReadWrite);
    data.offset = 0;
	masterDev.writeSii(&data);
}

/*****************************************************************************/
