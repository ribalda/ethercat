/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <sstream>
using namespace std;

#include "CommandAlias.h"
#include "sii_crc.h"
#include "byteorder.h"

/*****************************************************************************/

CommandAlias::CommandAlias():
    Command("alias", "Write alias addresses.")
{
}

/*****************************************************************************/

string CommandAlias::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS] <ALIAS>" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Arguments:" << endl
        << "  ALIAS must be an unsigned 16 bit number. Zero means" << endl
        << "        removing an alias address." << endl
        << endl
        << "If multiple slaves are selected, the --force option" << endl
        << "is required." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection. See the help of" << endl
        << "                         the 'slaves' command." << endl
        << "  --force    -f          Acknowledge writing aliases of" << endl
        << "                         multiple slaves." << endl
        << endl
        << numericInfo();

    return str.str();
}

/*****************************************************************************/

/** Writes the Secondary slave address (alias) to the slave's SII.
 */
void CommandAlias::execute(MasterDevice &m, const StringVector &args)
{
    uint16_t alias;
    stringstream err, strAlias;
    int number;
    SlaveList slaves;
    SlaveList::const_iterator si;

    if (args.size() != 1) {
        err << "'" << getName() << "' takes exactly one argument!";
        throwInvalidUsageException(err);
    }

    strAlias << args[0];
    strAlias
        >> resetiosflags(ios::basefield) // guess base from prefix
        >> number;
    if (strAlias.fail() || number < 0x0000 || number > 0xffff) {
        err << "Invalid alias '" << args[0] << "'!";
        throwInvalidUsageException(err);
    }
    alias = number;

    m.open(MasterDevice::ReadWrite);
    slaves = selectedSlaves(m);
    
    if (slaves.size() > 1 && !force) {
        err << "This will write the alias addresses of "
            << slaves.size() << " slaves to " << alias
            << "! Please specify --force to proceed.";
        throwCommandException(err);
    }

    if (!slaves.size() && getVerbosity() != Quiet) {
        cerr << "Warning: Selection matches no slaves!" << endl;
    }

    for (si = slaves.begin(); si != slaves.end(); si++) {
        writeSlaveAlias(m, *si, alias);
    }
}

/*****************************************************************************/

/** Writes the Secondary slave address (alias) to the slave's SII.
 */
void CommandAlias::writeSlaveAlias(
        MasterDevice &m,
        const ec_ioctl_slave_t &slave,
        uint16_t alias
        )
{
    ec_ioctl_slave_sii_t data;
    stringstream err;
    uint8_t crc;

    if (slave.sii_nwords < 8) {
        err << "Current SII contents are too small to set an alias "
            << "(" << slave.sii_nwords << " words)!";
        throwCommandException(err);
    }

    // read first 8 SII words
    data.slave_position = slave.position;
    data.offset = 0;
    data.nwords = 8;
    data.words = new uint16_t[data.nwords];

    try {
        m.readSii(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.words;
        err << "Failed to read SII: " << e.what();
        throwCommandException(err);
    }

    // write new alias address in word 4
    data.words[4] = cputole16(alias);

    // calculate checksum over words 0 to 6
    crc = calcSiiCrc((const uint8_t *) data.words, 14);

    // write new checksum into first byte of word 7
    *(uint8_t *) (data.words + 7) = crc;

    // write first 8 words with new alias and checksum
    try {
        m.writeSii(&data);
    } catch (MasterDeviceException &e) {
        delete [] data.words;
        err << "Failed to read SII: " << e.what();
        throwCommandException(err);
    }

    delete [] data.words;
}

/*****************************************************************************/
