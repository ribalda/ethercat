/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <sstream>
using namespace std;

#include "globals.h"

/*****************************************************************************/

const char *help_alias =
    "[OPTIONS] <ALIAS>\n"
    "\n"
    "Write the secondary slave address (alias) for either\n"
    "one or for multiple slaves.\n"
    "\n"
    "Arguments:\n"
    "  ALIAS must be a 16 bit unsigned integer, specified\n"
    "        either in decimal (no prefix), octal (prefix '0')\n"
    "        or hexadecimal (prefix '0x').\n"
    "\n"
    "Command-specific options:\n"
    "  -s <SLAVE>  Write the alias of the slave with the given\n"
    "              ring position. If this option is not\n"
    "              specified, the alias of all slaves is set.\n"
    "              The --force option is required in this\n"
    "              case.\n";

/*****************************************************************************/

void writeSlaveAlias(uint16_t, uint16_t);

/*****************************************************************************/

/** Writes the Secondary slave address (alias) to the slave's SII.
 */
void command_alias(void)
{
    uint16_t alias;
    stringstream err, strAlias;
    int number;
    unsigned int numSlaves, i;

    if (commandArgs.size() != 1) {
        err << "'" << command << "' takes exactly one argument!";
        throw InvalidUsageException(err);
    }

    strAlias << commandArgs[0];
    strAlias
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> number;
    if (strAlias.fail() || number < 0x0000 || number > 0xffff) {
        err << "Invalid alias '" << commandArgs[0] << "'!";
        throw InvalidUsageException(err);
    }
    alias = number;

    if (slavePosition == -1) {
        if (!force) {
            err << "This will write the alias addresses of all slaves to "
                << alias << "! Please specify --force to proceed.";
            throw ExecutionFailureException(err);
        }

        masterDev.open(MasterDevice::ReadWrite);
        numSlaves = masterDev.slaveCount();

        for (i = 0; i < numSlaves; i++) {
            writeSlaveAlias(i, alias);
        }
    } else {
        masterDev.open(MasterDevice::ReadWrite);
        writeSlaveAlias(slavePosition, alias);
    }
}

/*****************************************************************************/

/** Calculates the SII checksum field.
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
uint8_t calcSiiCrc(
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

/** Writes the Secondary slave address (alias) to the slave's SII.
 */
void writeSlaveAlias(
        uint16_t slavePosition,
        uint16_t alias
        )
{
    ec_ioctl_slave_sii_t data;
    ec_ioctl_slave_t slave;
    stringstream err;
    uint8_t crc;

    masterDev.getSlave(&slave, slavePosition);

    if (slave.sii_nwords < 8) {
        err << "Current SII contents are too small to set an alias "
            << "(" << slave.sii_nwords << " words)!";
        throw ExecutionFailureException(err);
    }

    // read first 8 SII words
    data.slave_position = slavePosition;
    data.offset = 0;
    data.nwords = 8;
    data.words = new uint16_t[data.nwords];

    try {
        masterDev.readSii(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.words;
        err << "Failed to read SII: " << e.what();
        throw ExecutionFailureException(err);
    }

    // write new alias address in word 4
    data.words[4] = cputole16(alias);

    // calculate checksum over words 0 to 6
    crc = calcSiiCrc((const uint8_t *) data.words, 14);

    // write new checksum into first byte of word 7
    *(uint8_t *) (data.words + 7) = crc;

    // write first 8 words with new alias and checksum
    try {
        masterDev.writeSii(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.words;
        err << "Failed to read SII: " << e.what();
        throw ExecutionFailureException(err);
    }

    delete [] data.words;
}

/*****************************************************************************/
