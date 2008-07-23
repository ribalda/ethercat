/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "globals.h"

/****************************************************************************/

const char *help_sii_read =
    "[OPTIONS]\n"
    "\n"
    "Outputs the SII (EEPROM) contents of a slave.\n"
    "\n"
    "Without the --verbose option, binary SII contents are output. They can\n"
    "be piped to a tool like hexdump, for example:\n"
    "\n"
    "  ethercat sii_read -s2 | hexdump -C\n"
    "\n"
    "With the --verbose option given, a textual representation of the data\n"
    "is output, that is separated by SII category names.\n"
    "\n"
    "Command-specific options:\n"
    "  --slave   -s <index>  Positive numerical ring position (mandatory).\n"
    "  --verbose -v          Output textual data with category names.\n"
    "\n"
    "Numerical values can be specified either with decimal (no prefix),\n"
    "octal (prefix '0') or hexadecimal (prefix '0x') base.\n";

/****************************************************************************/

struct CategoryName {
    uint16_t type;
    const char *name;
};

static const CategoryName categoryNames[] = {
    {0x000a, "STRINGS"},
    {0x0014, "DataTypes"},
    {0x001e, "General"},
    {0x0028, "FMMU"},
    {0x0029, "SyncM"},
    {0x0032, "TXPDO"},
    {0x0033, "RXPDO"},
    {0x003c, "DC"},
    {}
};

/****************************************************************************/

const char *getCategoryName(uint16_t type)
{
    const CategoryName *cn = categoryNames;

    while (cn->type) {
        if (cn->type == type) {
            return cn->name;
        }
        cn++;
    }

    return "unknown";
}

/****************************************************************************/

void command_sii_read(void)
{
    ec_ioctl_slave_sii_t data;
    ec_ioctl_slave_t slave;
    unsigned int i;
    const uint16_t *categoryHeader;
    uint16_t categoryType, categorySize;
    stringstream err;

    if (slavePosition < 0) {
        err << "'" << commandName << "' requires a slave! "
            << "Please specify --slave.";
        throw InvalidUsageException(err);
    }
    data.slave_position = slavePosition;

    masterDev.open(MasterDevice::Read);

    masterDev.getSlave(&slave, slavePosition);

    if (!slave.sii_nwords)
        return;

    data.offset = 0;
    data.nwords = slave.sii_nwords;
    data.words = new uint16_t[data.nwords];

	try {
		masterDev.readSii(&data);
	} catch (MasterDeviceException &e) {
        delete [] data.words;
		throw e;
	}

    if (verbosity == Verbose) {
        cout << "SII Area:" << hex << setfill('0');
        for (i = 0; i < min(data.nwords, 0x0040U) * 2; i++) {
            if (i % BreakAfterBytes) {
                cout << " ";
            } else {
                cout << endl << "  ";
            }
            cout << setw(2) << (unsigned int) *((uint8_t *) data.words + i);
        }
        cout << endl;

        if (data.nwords > 0x0040U) {
            // cycle through categories
            categoryHeader = data.words + 0x0040U;
            categoryType = le16tocpu(*categoryHeader);
            while (categoryType != 0xffff) {
                cout << "SII Category 0x" << hex
                    << setw(4) << categoryType
                    << " (" << getCategoryName(categoryType) << ")" << flush;

                if (categoryHeader + 1 > data.words + data.nwords) {
                    err << "SII data seem to be corrupted!";
                    throw CommandException(err);
                }
                categorySize = le16tocpu(*(categoryHeader + 1));
                cout << ", " << dec << categorySize << " words" << flush;

                if (categoryHeader + 2 + categorySize
                        > data.words + data.nwords) {
                    err << "SII data seem to be corrupted!";
                    throw CommandException(err);
                }

                cout << hex;
                for (i = 0; i < categorySize * 2U; i++) {
                    if (i % BreakAfterBytes) {
                        cout << " ";
                    } else {
                        cout << endl << "  ";
                    }
                    cout << setw(2) << (unsigned int)
                        *((uint8_t *) (categoryHeader + 2) + i);
                }
                cout << endl;

                if (categoryHeader + 2 + categorySize + 1
                        > data.words + data.nwords) {
                    err << "SII data seem to be corrupted!"; 
                    throw CommandException(err);
                }
                categoryHeader += 2 + categorySize;
                categoryType = le16tocpu(*categoryHeader);
            }
        }
    } else {
        for (i = 0; i < data.nwords; i++) {
            uint16_t *w = data.words + i;
            cout << *(uint8_t *) w << *((uint8_t *) w + 1);
        }
    }

    delete [] data.words;
}

/*****************************************************************************/
