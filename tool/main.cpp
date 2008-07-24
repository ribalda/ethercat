/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <getopt.h>
#include <libgen.h> // basename()

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <list>
using namespace std;

#include "globals.h"

/*****************************************************************************/

string binaryBaseName;
unsigned int masterIndex = 0;
int slavePosition = -1;
int domainIndex = -1;
string commandName;
vector<string> commandArgs;
Verbosity verbosity = Normal;
string dataTypeStr;
bool force = false;

bool helpRequested = false;

MasterDevice masterDev;

/*****************************************************************************/

struct Command {
    const char *name;
    void (*func)(void);
    const char *helpString;
    const char *briefDesc;

    int execute(void) const;
    string getHelpString(void) const;
};

/*****************************************************************************/

#define DEFINE_EXTERN_COMMAND(name) \
    void command_##name(void); \
    extern const char *help_##name

#define INIT_COMMAND(name, desc) \
    {#name, command_##name, help_##name, desc}

DEFINE_EXTERN_COMMAND(alias);
DEFINE_EXTERN_COMMAND(config);
DEFINE_EXTERN_COMMAND(data);
DEFINE_EXTERN_COMMAND(debug);
DEFINE_EXTERN_COMMAND(domains);
DEFINE_EXTERN_COMMAND(master);
DEFINE_EXTERN_COMMAND(pdos);
DEFINE_EXTERN_COMMAND(sdos);
DEFINE_EXTERN_COMMAND(download);
DEFINE_EXTERN_COMMAND(upload);
DEFINE_EXTERN_COMMAND(slaves);
DEFINE_EXTERN_COMMAND(sii_read);
DEFINE_EXTERN_COMMAND(sii_write);
DEFINE_EXTERN_COMMAND(states);
DEFINE_EXTERN_COMMAND(xml);

static const Command commands[] = {
    INIT_COMMAND(alias, "Write alias addresses."),
    INIT_COMMAND(config, "Show bus configuration."),
    INIT_COMMAND(data, "Output binary domain process data."),
    INIT_COMMAND(debug, "Set the master's debug level."),
    INIT_COMMAND(domains, "Show domain information."),
    INIT_COMMAND(master, "Show master information."),
    INIT_COMMAND(pdos, "List Pdo assignment/mapping."),
    INIT_COMMAND(sdos, "List Sdo dictionaries."),
    INIT_COMMAND(download, "Write an Sdo entry."),
    INIT_COMMAND(upload, "Read an Sdo entry."),
    INIT_COMMAND(slaves, "Show slaves."),
    INIT_COMMAND(sii_read, "Output a slave's SII contents."),
    INIT_COMMAND(sii_write, "Write slave's SII contents."),
    INIT_COMMAND(states, "Request slave states."),
    INIT_COMMAND(xml, "Generate slave information XML."),
};

static const Command *cmdEnd = commands + sizeof(commands) / sizeof(Command);

/*****************************************************************************/

void printUsage()
{
    const Command *cmd;
    size_t maxWidth = 0;

    for (cmd = commands; cmd < cmdEnd; cmd++) {
        if (strlen(cmd->name) > maxWidth) {
            maxWidth = strlen(cmd->name);
        }
    }

    cerr
        << "Usage: " << binaryBaseName << " <COMMAND> [OPTIONS] [ARGUMENTS]"
        << endl << endl
		<< "Commands (can be abbreviated):" << endl;

    cerr << left;
    for (cmd = commands; cmd < cmdEnd; cmd++) {
        cerr << "  " << setw(maxWidth) << cmd->name
            << " " << cmd->briefDesc << endl;
    }

    cerr
        << endl
		<< "Global options:" << endl
        << "  --master  -m <master>  Index of the master to use. Default: 0."
		<< endl
        << "  --force   -f           Force a command." << endl
        << "  --quiet   -q           Output less information." << endl
        << "  --verbose -v           Output more information." << endl
        << "  --help    -h           Show this help." << endl
        << endl
        << "Numerical values can be specified either with decimal "
        << "(no prefix)," << endl
        << "octal (prefix '0') or hexadecimal (prefix '0x') base." << endl
        << endl
        << "Call '" << binaryBaseName
        << " <COMMAND> --help' for command-specific help." << endl
        << endl
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

    commandName = argv[optind];
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
        cerr << getHelpString();
        return 1;
    } catch (CommandException &e) {
        cerr << e.what() << endl;
        return 1;
    } catch (MasterDeviceException &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}

/****************************************************************************/

string Command::getHelpString() const
{
    stringstream help;
    help << binaryBaseName << " " << commandName << " " << helpString;
    return help.str();
}

/****************************************************************************/

bool substrMatch(const string &abb, const string &full)
{
    return full.substr(0, abb.length()) == abb;
}
    
/****************************************************************************/

bool abbrevMatch(const string &abb, const string &full)
{
    unsigned int abbIndex;
    size_t fullPos = 0;

    for (abbIndex = 0; abbIndex < abb.length(); abbIndex++) {
        fullPos = full.find(abb[abbIndex], fullPos);
        if (fullPos == string::npos)
            return false;
    }

    return true;
}
    
/****************************************************************************/

list<const Command *> getMatchingCommands(const string &cmdStr)
{
    const Command *cmd;
    list<const Command *> res;

    // find matching commands from beginning of the string
    for (cmd = commands; cmd < cmdEnd; cmd++) {
        if (substrMatch(cmdStr, cmd->name)) {
            res.push_back(cmd);
        }
    }

    if (!res.size()) { // nothing found
        // find /any/ matching commands
        for (cmd = commands; cmd < cmdEnd; cmd++) {
            if (abbrevMatch(cmdStr, cmd->name)) {
                res.push_back(cmd);
            }
        }
    }

    return res;
}

/****************************************************************************/

int main(int argc, char **argv)
{
    int retval = 0;
    list<const Command *> commands;
    list<const Command *>::const_iterator ci;
    const Command *cmd;

    binaryBaseName = basename(argv[0]);
	getOptions(argc, argv);

    commands = getMatchingCommands(commandName);

    if (commands.size()) {
        if (commands.size() == 1) {
            cmd = commands.front();
            commandName = cmd->name;
            if (!helpRequested) {
                masterDev.setIndex(masterIndex);
                retval = cmd->execute();
            } else {
                cout << cmd->getHelpString();
            }
        } else {
            cerr << "Ambiguous command abbreviation! Matching:" << endl;
            for (ci = commands.begin(); ci != commands.end(); ci++) {
                cerr << (*ci)->name << endl;
            }
            cerr << endl;
            printUsage();
            retval = 1;
        }
    } else {
        cerr << "Unknown command " << commandName << "!" << endl << endl;
        printUsage();
        retval = 1;
    }

	return retval;
}

/****************************************************************************/

void printRawData(
		const uint8_t *data,
		unsigned int size
		)
{
    cout << hex << setfill('0');
    while (size--) {
        cout << "0x" << setw(2) << (unsigned int) *data++;
        if (size)
            cout << " ";
    }
    cout << endl;
}

/****************************************************************************/
