/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <getopt.h>

#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include "Master.h"

/*****************************************************************************/

#define DEFAULT_MASTER 0

static unsigned int masterIndex = DEFAULT_MASTER;
static int slavePosition = -1;
static int domainIndex = -1;
static string command;
vector<string> commandArgs;
static bool quiet = false;
string dataTypeStr;
bool force = false;

/*****************************************************************************/

void printUsage()
{
    cerr
        << "Usage: ethercat <COMMAND> [OPTIONS]" << endl
		<< "Commands:" << endl
        << "  alias              Write alias address(es)." << endl
        << "  config             Show slave configurations." << endl
        << "  data               Output binary domain process data." << endl
        << "  debug              Set the master debug level." << endl
        << "  domain             Show domain information." << endl
        << "  list (ls)          List all slaves (former 'lsec')." << endl
        << "  master             Show master information." << endl
        << "  pdos               List Pdo mapping." << endl
        << "  sdos               List Sdo dictionaries." << endl
        << "  sdo_download (sd)  Write an Sdo entry." << endl
        << "  sdo_upload (su)    Read an Sdo entry." << endl
        << "  slave              Show slave information." << endl
        << "  sii_read (sr)      Output a slave's SII contents." << endl
        << "  sii_write (sw)     Write slave's SII contents." << endl
        << "  state              Request slave states." << endl
        << "  xml                Generate slave information xmls." << endl
		<< "Global options:" << endl
        << "  --master  -m <master>  Index of the master to use. Default: "
		<< DEFAULT_MASTER << endl
        << "  --slave   -s <index>   Positive numerical ring position,"
        << endl
        << "                         or 'all' for all slaves (default)."
        << endl
        << "  --domain  -d <index>   Positive numerical index,"
        << endl
        << "                         or 'all' for all domains (default)."
        << endl
        << "  --type    -t <type>    Forced Sdo data type." << endl
        << "  --force   -f           Force action." << endl
        << "  --quiet   -q           Show less output." << endl
        << "  --help    -h           Show this help." << endl;
}

/*****************************************************************************/

void getOptions(int argc, char **argv)
{
    int c, argCount, optionIndex, number;
	char *remainder;

    static struct option longOptions[] = {
        //name,    has_arg,           flag, val
        {"master", required_argument, NULL, 'm'},
        {"slave",  required_argument, NULL, 's'},
        {"domain", required_argument, NULL, 'd'},
        {"type",   required_argument, NULL, 't'},
        {"force",  no_argument,       NULL, 'f'},
        {"quiet",  no_argument,       NULL, 'q'},
        {"help",   no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "m:s:d:t:fqh", longOptions, &optionIndex);

        switch (c) {
            case 'm':
                number = strtoul(optarg, &remainder, 0);
                if (remainder == optarg || *remainder || number < 0) {
                    cerr << "Invalid master number " << optarg << "!" << endl;
                    printUsage();
                    exit(1);
                }
				masterIndex = number;
                break;

            case 's':
                if (!strcmp(optarg, "all")) {
                    slavePosition = -1;
                } else {
                    number = strtoul(optarg, &remainder, 0);
                    if (remainder == optarg || *remainder
                            || number < 0 || number > 0xFFFF) {
                        cerr << "Invalid slave position "
                            << optarg << "!" << endl;
                        printUsage();
                        exit(1);
                    }
                    slavePosition = number;
                }
                break;

            case 'd':
                if (!strcmp(optarg, "all")) {
                    domainIndex = -1;
                } else {
                    number = strtoul(optarg, &remainder, 0);
                    if (remainder == optarg || *remainder || number < 0) {
                        cerr << "Invalid domain index "
							<< optarg << "!" << endl;
                        printUsage();
                        exit(1);
                    }
                    domainIndex = number;
                }
                break;

            case 't':
                dataTypeStr = optarg;
                break;

            case 'f':
                force = true;
                break;

            case 'q':
                quiet = true;
                break;

            case 'h':
            case '?':
                printUsage();
                exit(0);

            default:
                break;
        }
    }
    while (c != -1);

	argCount = argc - optind;

	if (!argCount) {
        cerr << "Please specify a command!" << endl;
		printUsage();
        exit(1);
	}

    command = argv[optind];
    while (++optind < argc)
        commandArgs.push_back(string(argv[optind]));
}

/****************************************************************************/

int main(int argc, char **argv)
{
    Master master;
    
	getOptions(argc, argv);

    try {
        master.setIndex(masterIndex);

        if (command == "alias") {
            master.writeAlias(slavePosition, force, commandArgs);
        } else if (command == "config") {
            master.showConfig();
        } else if (command == "data") {
            master.outputData(domainIndex);
        } else if (command == "debug") {
            master.setDebug(commandArgs);
        } else if (command == "domain") {
            master.showDomains(domainIndex);
		} else if (command == "list" || command == "ls") {
            master.listSlaves();
		} else if (command == "master") {
            master.showMaster();
        } else if (command == "pdos") {
            master.listPdos(slavePosition, quiet);
        } else if (command == "sdos") {
            master.listSdos(slavePosition, quiet);
        } else if (command == "sdo_download" || command == "sd") {
            master.sdoDownload(slavePosition, dataTypeStr, commandArgs);
        } else if (command == "sdo_upload" || command == "su") {
            master.sdoUpload(slavePosition, dataTypeStr, commandArgs);
		} else if (command == "slave") {
            master.showSlaves(slavePosition);
        } else if (command == "sii_read" || command == "sr") {
            master.siiRead(slavePosition);
        } else if (command == "sii_write" || command == "sw") {
            master.siiWrite(slavePosition, force, commandArgs);
        } else if (command == "state") {
            master.requestStates(slavePosition, commandArgs);
        } else if (command == "xml") {
            master.generateXml(slavePosition);
        } else {
            cerr << "Unknown command " << command << "!" << endl;
            printUsage();
            exit(1);
        }
    } catch (MasterException &e) {
        cerr << e.what() << endl;
        exit(1);
    }

	return 0;
}

/****************************************************************************/
