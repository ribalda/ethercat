/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <getopt.h>
#include <libgen.h> // basename()
#include <stdlib.h>

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandAlias.h"
#include "CommandConfig.h"
#include "CommandData.h"
#include "CommandDebug.h"
#include "CommandDomains.h"
#include "CommandDownload.h"
#include "CommandFoeRead.h"
#include "CommandFoeWrite.h"
#include "CommandMaster.h"
#include "CommandPdos.h"
#include "CommandPhyRead.h"
#include "CommandPhyWrite.h"
#include "CommandSdos.h"
#include "CommandSiiRead.h"
#include "CommandSiiWrite.h"
#include "CommandSlaves.h"
#include "CommandStates.h"
#include "CommandUpload.h"
#include "CommandVersion.h"
#include "CommandXml.h"

/*****************************************************************************/

typedef list<Command *> CommandList;
CommandList commandList;

MasterDevice masterDev;

string binaryBaseName;
string commandName;
Command::StringVector commandArgs;

// option variables
unsigned int masterIndex = 0;
int slavePosition = -1;
int slaveAlias = -1;
int domainIndex = -1;
string dataTypeStr;
Command::Verbosity verbosity = Command::Normal;
bool force = false;
bool helpRequested = false;
string outputFile;

/*****************************************************************************/

string usage()
{
    stringstream str;
    CommandList::const_iterator ci;
    size_t maxWidth = 0;

    for (ci = commandList.begin(); ci != commandList.end(); ci++) {
        if ((*ci)->getName().length() > maxWidth) {
            maxWidth = (*ci)->getName().length();
        }
    }

    str << "Usage: " << binaryBaseName << " <COMMAND> [OPTIONS] [ARGUMENTS]"
        << endl << endl
		<< "Commands (can be abbreviated):" << endl;

    str << left;
    for (ci = commandList.begin(); ci != commandList.end(); ci++) {
        str << "  " << setw(maxWidth) << (*ci)->getName()
            << "  " << (*ci)->getBriefDescription() << endl;
    }

    str << endl
		<< "Global options:" << endl
        << "  --master  -m <master>  Index of the master to use. Default: 0."
		<< endl
        << "  --force   -f           Force a command." << endl
        << "  --quiet   -q           Output less information." << endl
        << "  --verbose -v           Output more information." << endl
        << "  --help    -h           Show this help." << endl
        << endl
        << Command::numericInfo()
        << endl
        << "Call '" << binaryBaseName
        << " <COMMAND> --help' for command-specific help." << endl
        << endl
        << "Send bug reports to " << PACKAGE_BUGREPORT << "." << endl;

    return str.str();
}

/*****************************************************************************/

void getOptions(int argc, char **argv)
{
    int c, argCount;
    stringstream str;

    static struct option longOptions[] = {
        //name,         has_arg,           flag, val
        {"master",      required_argument, NULL, 'm'},
        {"alias",       required_argument, NULL, 'a'},
        {"position",    required_argument, NULL, 'p'},
        {"domain",      required_argument, NULL, 'd'},
        {"type",        required_argument, NULL, 't'},
        {"output-file", required_argument, NULL, 'o'},
        {"force",       no_argument,       NULL, 'f'},
        {"quiet",       no_argument,       NULL, 'q'},
        {"verbose",     no_argument,       NULL, 'v'},
        {"help",        no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "m:a:p:d:t:o:fqvh", longOptions, NULL);

        switch (c) {
            case 'm':
                str.clear();
                str.str("");
                str << optarg;
                str >> resetiosflags(ios::basefield) // guess base from prefix
                    >> masterIndex;
                if (str.fail() || masterIndex < 0) {
                    cerr << "Invalid master number " << optarg << "!" << endl
                        << endl << usage();
                    exit(1);
                }
                break;

            case 'a':
                str.clear();
                str.str("");
                str << optarg;
                str >> resetiosflags(ios::basefield) // guess base from prefix
                    >> slaveAlias;
                if (str.fail() || slaveAlias < 0 || slaveAlias > 0xFFFF) {
                    cerr << "Invalid slave alias " << optarg << "!" << endl
                        << endl << usage();
                    exit(1);
                }
                break;

            case 'p':
                str.clear();
                str.str("");
                str << optarg;
                str >> resetiosflags(ios::basefield) // guess base from prefix
                    >> slavePosition;
                if (str.fail()
                        || slavePosition < 0 || slavePosition > 0xFFFF) {
                    cerr << "Invalid slave position " << optarg << "!" << endl
                        << endl << usage();
                    exit(1);
                }
                break;

            case 'd':
                str.clear();
                str.str("");
                str << optarg;
                str >> resetiosflags(ios::basefield) // guess base from prefix
                    >> domainIndex;
                if (str.fail() || domainIndex < 0) {
                    cerr << "Invalid domain index " << optarg << "!" << endl
                        << endl << usage();
                    exit(1);
                }
                break;

            case 't':
                dataTypeStr = optarg;
                break;

            case 'o':
                outputFile = optarg;
                break;

            case 'f':
                force = true;
                break;

            case 'q':
                verbosity = Command::Quiet;
                break;

            case 'v':
                verbosity = Command::Verbose;
                break;

            case 'h':
                helpRequested = true;
                break;

            case '?':
                cerr << endl << usage();
                exit(1);

            default:
                break;
        }
    }
    while (c != -1);

	argCount = argc - optind;

    if (!argCount) {
        if (helpRequested) {
            cout << usage();
            exit(0);
        } else {
            cerr << "Please specify a command!" << endl
                << endl << usage();
            exit(1);
        }
	}

    commandName = argv[optind];
    while (++optind < argc)
        commandArgs.push_back(string(argv[optind]));
}

/****************************************************************************/

list<Command *> getMatchingCommands(const string &cmdStr)
{
    CommandList::iterator ci;
    list<Command *> res;

    // find matching commands from beginning of the string
    for (ci = commandList.begin(); ci != commandList.end(); ci++) {
        if ((*ci)->matchesSubstr(cmdStr)) {
            res.push_back(*ci);
        }
    }

    if (!res.size()) { // nothing found
        // find /any/ matching commands
        for (ci = commandList.begin(); ci != commandList.end(); ci++) {
            if ((*ci)->matchesAbbrev(cmdStr)) {
                res.push_back(*ci);
            }
        }
    }

    return res;
}

/****************************************************************************/

int main(int argc, char **argv)
{
    int retval = 0;
    list<Command *> matchingCommands;
    list<Command *>::const_iterator ci;
    Command *cmd;

    binaryBaseName = basename(argv[0]);

    commandList.push_back(new CommandAlias());
    commandList.push_back(new CommandConfig());
    commandList.push_back(new CommandData());
    commandList.push_back(new CommandDebug());
    commandList.push_back(new CommandDomains());
    commandList.push_back(new CommandDownload());
    commandList.push_back(new CommandFoeRead());
    commandList.push_back(new CommandFoeWrite());
    commandList.push_back(new CommandMaster());
    commandList.push_back(new CommandPdos());
    commandList.push_back(new CommandPhyRead());
    commandList.push_back(new CommandPhyWrite());
    commandList.push_back(new CommandSdos());
    commandList.push_back(new CommandSiiRead());
    commandList.push_back(new CommandSiiWrite());
    commandList.push_back(new CommandSlaves());
    commandList.push_back(new CommandStates());
    commandList.push_back(new CommandUpload());
    commandList.push_back(new CommandVersion());
    commandList.push_back(new CommandXml());

	getOptions(argc, argv);

    matchingCommands = getMatchingCommands(commandName);
    masterDev.setIndex(masterIndex);

    if (matchingCommands.size()) {
        if (matchingCommands.size() == 1) {
            cmd = matchingCommands.front();
            if (!helpRequested) {
                try {
                    cmd->setVerbosity(verbosity);
                    cmd->setAlias(slaveAlias);
                    cmd->setPosition(slavePosition);
                    cmd->setDomain(domainIndex);
                    cmd->setDataType(dataTypeStr);
                    cmd->setOutputFile(outputFile);
                    cmd->setForce(force);
                    cmd->execute(masterDev, commandArgs);
                } catch (InvalidUsageException &e) {
                    cerr << e.what() << endl << endl;
                    cerr << binaryBaseName << " " << cmd->helpString();
                    retval = 1;
                } catch (CommandException &e) {
                    cerr << e.what() << endl;
                    retval = 1;
                } catch (MasterDeviceException &e) {
                    cerr << e.what() << endl;
                    retval = 1;
                }
            } else {
                cout << binaryBaseName << " " << cmd->helpString();
            }
        } else {
            cerr << "Ambiguous command abbreviation! Matching:" << endl;
            for (ci = matchingCommands.begin();
                    ci != matchingCommands.end();
                    ci++) {
                cerr << (*ci)->getName() << endl;
            }
            cerr << endl << usage();
            retval = 1;
        }
    } else {
        cerr << "Unknown command " << commandName << "!" << endl
            << endl << usage();
        retval = 1;
    }

	return retval;
}

/****************************************************************************/
