/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <getopt.h>
#include <libgen.h> // basename()

#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include "globals.h"

/*****************************************************************************/

string binaryBaseName;
unsigned int masterIndex = 0;
int slavePosition = -1;
int domainIndex = -1;
string command;
vector<string> commandArgs;
Verbosity verbosity = Normal;
string dataTypeStr;
bool force = false;

bool helpRequested = false;

MasterDevice masterDev;

/*****************************************************************************/

struct Command {
    void (*func)(void);
    const char *helpString;

    int execute(void) const;
    void displayHelp(void) const;
};

struct CommandAlias {
    const char *name;
    const Command *command;
};

/*****************************************************************************/

#define COMMAND(name) \
    void command_##name(void); \
    extern const char *help_##name; \
    const Command cmd_##name = {command_##name, help_##name};

COMMAND(alias);
COMMAND(config);

const CommandAlias commandAliases[] = {
    {"alias",  &cmd_alias},

    {"config", &cmd_config},
    {"conf",   &cmd_config},
    {"cf",     &cmd_config},
};

#if 0
        } else if (command == "data") {
            master.outputData(domainIndex);
        } else if (command == "debug") {
            master.setDebug(commandArgs);
        } else if (command == "domain") {
            master.showDomains(domainIndex);
		} else if (command == "master") {
            master.showMaster();
        } else if (command == "pdos") {
            master.listPdos(slavePosition);
        } else if (command == "sdos") {
            master.listSdos(slavePosition);
        } else if (command == "sdo_download" || command == "sd") {
            master.sdoDownload(slavePosition, dataTypeStr, commandArgs);
        } else if (command == "sdo_upload" || command == "su") {
            master.sdoUpload(slavePosition, dataTypeStr, commandArgs);
		} else if (command == "slave" || command == "slaves"
                || command == "list" || command == "ls") {
            master.showSlaves(slavePosition);
        } else if (command == "sii_read" || command == "sr") {
            master.siiRead(slavePosition);
        } else if (command == "sii_write" || command == "sw") {
            master.siiWrite(slavePosition, force, commandArgs);
        } else if (command == "state") {
            master.requestStates(slavePosition, commandArgs);
        } else if (command == "xml") {
            master.generateXml(slavePosition);
#endif

/*****************************************************************************/

void printUsage()
{
    cerr
        << "Usage: " << binaryBaseName << " <COMMAND> [OPTIONS]" << endl
		<< "Commands:" << endl
        << "  alias         Write alias addresses." << endl
        << "  config        Show bus configuration." << endl
        << "  data          Output binary domain process data." << endl
        << "  debug         Set the master's debug level." << endl
        << "  domain        Show domain information." << endl
        << "  master        Show master information." << endl
        << "  pdos          List Pdo assignment/mapping." << endl
        << "  sdo_download  Write an Sdo entry." << endl
        << "  sdos          List Sdo dictionaries." << endl
        << "  sdo_upload    Read an Sdo entry." << endl
        << "  sii_read      Output a slave's SII contents." << endl
        << "  sii_write     Write slave's SII contents." << endl
        << "  slaves        Show slaves." << endl
        << "  state         Request slave states." << endl
        << "  xml           Generate slave information xmls." << endl
		<< "Global options:" << endl
        << "  --master  -m <master>  Index of the master to use. Default: 0"
		<< endl
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
        << "  --quiet   -q           Output less information." << endl
        << "  --verbose -v           Output more information." << endl
        << "  --help    -h           Show this help." << endl
        << "Call '" << binaryBaseName
        << " <COMMAND> --help' for command-specific help." << endl
        << "Send bug reports to " << PACKAGE_BUGREPORT << "." << endl;
}

/*****************************************************************************/

void getOptions(int argc, char **argv)
{
    int c, argCount, optionIndex, number;
	char *remainder;

    static struct option longOptions[] = {
        //name,     has_arg,           flag, val
        {"master",  required_argument, NULL, 'm'},
        {"slave",   required_argument, NULL, 's'},
        {"domain",  required_argument, NULL, 'd'},
        {"type",    required_argument, NULL, 't'},
        {"force",   no_argument,       NULL, 'f'},
        {"quiet",   no_argument,       NULL, 'q'},
        {"verbose", no_argument,       NULL, 'v'},
        {"help",    no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "m:s:d:t:fqvh", longOptions, &optionIndex);

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
                verbosity = Quiet;
                break;

            case 'v':
                verbosity = Verbose;
                break;

            case 'h':
                helpRequested = true;
                break;

            case '?':
                printUsage();
                exit(1);

            default:
                break;
        }
    }
    while (c != -1);

	argCount = argc - optind;

    if (!argCount) {
        if (!helpRequested) {
            cerr << "Please specify a command!" << endl;
        }
        printUsage();
        exit(!helpRequested);
	}

    command = argv[optind];
    while (++optind < argc)
        commandArgs.push_back(string(argv[optind]));
}

/****************************************************************************/

int Command::execute() const
{
    try {
        func();
    } catch (InvalidUsageException &e) {
        cerr << e.what() << endl << endl;
        displayHelp();
        return 1;
    } catch (ExecutionFailureException &e) {
        cerr << e.what() << endl;
        return 1;
    } catch (MasterDeviceException &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}

/****************************************************************************/

void Command::displayHelp() const
{
    cerr << binaryBaseName << " " << command << " " << helpString;
}

/****************************************************************************/

int main(int argc, char **argv)
{
    int retval = 0;
    const CommandAlias *alias;
    const CommandAlias *endAlias =
        commandAliases + sizeof(commandAliases) / sizeof(CommandAlias);

    binaryBaseName = basename(argv[0]);
	getOptions(argc, argv);

    // search command alias in alias map
    for (alias = commandAliases; alias < endAlias; alias++) {
        if (command == alias->name)
            break;
    }

    if (alias < endAlias) { // command alias found
        if (!helpRequested) {
            masterDev.setIndex(masterIndex);
            retval = alias->command->execute();
        } else {
            alias->command->displayHelp();
        }
    } else { // command not found
        cerr << "Unknown command " << command << "!" << endl << endl;
        printUsage();
        retval = 1;
    }

	return retval;
}

/****************************************************************************/
