/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <list>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace std;

#include "globals.h"

/*****************************************************************************/

const char *help_config =
    "[OPTIONS]\n"
    "\n"
    "Output information about the slave configurations supplied by the\n"
    "application.\n"
    "\n"
    "Without the --verbose option, each line of output shows one slave\n"
    "configuration. Example:\n"
    "\n"
    "1001:0  0x0000003b/0x02010000  -  -\n"
    "|       |                      |  |\n"
    "|       |                      |  \\- Slave is operational.\n"
    "|       |                      \\- Slave has been found.\n"
    "|       \\- Hexadecimal vendor ID and product code, separated by a\n"
    "|          slash.\n"
    "\\- Decimal alias and position, separated by a colon.\n"
    "\n"
    "With the --verbose option given, the configured Pdos and Sdos are\n"
    "additionally printed.\n"
    "\n"
    "Command-specific options:\n"
    "  --verbose  -v  Show detailed configurations.\n";

/*****************************************************************************/

struct ConfigInfo {
    string alias;
    string pos;
    string ident;
    string att;
    string op;
};

typedef list<ec_ioctl_config_t> ConfigList;

void showDetailedConfigs(const ConfigList &configList);
void listConfigs(const ConfigList &configList);

/*****************************************************************************/

bool operator<(const ec_ioctl_config_t &a, const ec_ioctl_config_t &b)
{
    return a.alias < b.alias
        || (a.alias == b.alias && a.position < b.position);
}

/*****************************************************************************/

/** Lists the bus configuration.
 */
void command_config(void)
{
    ec_ioctl_master_t master;
    unsigned int i;
    ec_ioctl_config_t config;
    ConfigList configList;

    masterDev.open(MasterDevice::Read);
    masterDev.getMaster(&master);

    for (i = 0; i < master.config_count; i++) {
        masterDev.getConfig(&config, i);
        configList.push_back(config);
    }

    configList.sort();

    if (verbosity == Verbose) {
        showDetailedConfigs(configList);
    } else {
        listConfigs(configList);
    }
}

/*****************************************************************************/

/** Lists the complete bus configuration.
 */
void showDetailedConfigs(const ConfigList &configList)
{
    ConfigList::const_iterator configIter;
    unsigned int j, k, l;
    ec_ioctl_config_pdo_t pdo;
    ec_ioctl_config_pdo_entry_t entry;
    ec_ioctl_config_sdo_t sdo;

    for (configIter = configList.begin();
            configIter != configList.end();
            configIter++) {

        cout << "Alias: "
            << dec << configIter->alias << endl
            << "Position: " << configIter->position << endl
            << "Vendor Id: 0x"
            << hex << setfill('0')
            << setw(8) << configIter->vendor_id << endl
            << "Product code: 0x"
            << setw(8) << configIter->product_code << endl
            << "Attached: " << (configIter->attached ? "yes" : "no") << endl
            << "Operational: " << (configIter->operational ? "yes" : "no") << endl;

        for (j = 0; j < EC_MAX_SYNC_MANAGERS; j++) {
            if (configIter->syncs[j].pdo_count) {
                cout << "SM" << dec << j << " ("
                    << (configIter->syncs[j].dir == EC_DIR_INPUT
                            ? "Input" : "Output") << ")" << endl;
                for (k = 0; k < configIter->syncs[j].pdo_count; k++) {
                    masterDev.getConfigPdo(&pdo, configIter->config_index, j, k);

                    cout << "  Pdo 0x" << hex
                        << setw(4) << pdo.index
                        << " \"" << pdo.name << "\"" << endl;

                    for (l = 0; l < pdo.entry_count; l++) {
                        masterDev.getConfigPdoEntry(&entry,
                                configIter->config_index, j, k, l);

                        cout << "    Pdo entry 0x" << hex
                            << setw(4) << entry.index << ":"
                            << setw(2) << (unsigned int) entry.subindex
                            << ", " << dec << (unsigned int) entry.bit_length
                            << " bit, \"" << entry.name << "\"" << endl;
                    }
                }
            }
        }

        cout << "Sdo configuration:" << endl;
        if (configIter->sdo_count) {
            for (j = 0; j < configIter->sdo_count; j++) {
                masterDev.getConfigSdo(&sdo, configIter->config_index, j);

                cout << "  0x"
                    << hex << setfill('0')
                    << setw(4) << sdo.index << ":"
                    << setw(2) << (unsigned int) sdo.subindex
                    << ", " << dec << sdo.size << " byte: " << hex;

                switch (sdo.size) {
                    case 1:
                        cout << "0x" << setw(2)
                            << (unsigned int) *(uint8_t *) &sdo.data;
                        break;
                    case 2:
                        cout << "0x" << setw(4)
                            << le16tocpu(*(uint16_t *) &sdo.data);
                        break;
                    case 4:
                        cout << "0x" << setw(8)
                            << le32tocpu(*(uint32_t *) &sdo.data);
                        break;
                    default:
                        cout << "???";
                }

                cout << endl;
            }
        } else {
            cout << "  None." << endl;
        }

        cout << endl;
    }
}

/*****************************************************************************/

/** Lists the bus configuration.
 */
void listConfigs(const ConfigList &configList)
{
    ConfigList::const_iterator configIter;
    stringstream str;
    ConfigInfo info;
    typedef list<ConfigInfo> ConfigInfoList;
    ConfigInfoList list;
    ConfigInfoList::const_iterator iter;
    unsigned int maxAliasWidth = 0, maxPosWidth = 0,
                 maxAttWidth = 0, maxOpWidth = 0;

    for (configIter = configList.begin();
            configIter != configList.end();
            configIter++) {

        str << dec << configIter->alias;
        info.alias = str.str();
        str.clear();
        str.str("");

        str << configIter->position;
        info.pos = str.str();
        str.clear();
        str.str("");

        str << hex << setfill('0')
            << "0x" << setw(8) << configIter->vendor_id
            << "/0x" << setw(8) << configIter->product_code;
        info.ident = str.str();
        str.clear();
        str.str("");

        str << (configIter->attached ? "attached" : "-");
        info.att = str.str();
        str.clear();
        str.str("");

        str << (configIter->operational ? "operational" : "-");
        info.op = str.str();
        str.clear();
        str.str("");

        list.push_back(info);

        if (info.alias.length() > maxAliasWidth)
            maxAliasWidth = info.alias.length();
        if (info.pos.length() > maxPosWidth)
            maxPosWidth = info.pos.length();
        if (info.att.length() > maxAttWidth)
            maxAttWidth = info.att.length();
        if (info.op.length() > maxOpWidth)
            maxOpWidth = info.op.length();
    }

    for (iter = list.begin(); iter != list.end(); iter++) {
        cout << setfill(' ') << right
            << setw(maxAliasWidth) << iter->alias
            << ":" << left
            << setw(maxPosWidth) << iter->pos
            << "  "
            << iter->ident
            << "  "
            << setw(maxAttWidth) << iter->att << "  "
            << setw(maxOpWidth) << iter->op << "  "
            << endl;
    }
}

/*****************************************************************************/
