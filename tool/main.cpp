/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
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
#include "CommandCrc.h"
#include "CommandCStruct.h"
#include "CommandData.h"
#include "CommandDebug.h"
#include "CommandDiag.h"
#include "CommandDomains.h"
#include "CommandDownload.h"
#ifdef EC_EOE
#include "CommandEoe.h"
#include "CommandEoeAddIf.h"
#include "CommandEoeDelIf.h"
#endif
#include "CommandFoeRead.h"
#include "CommandFoeWrite.h"
#include "CommandGraph.h"
#ifdef EC_EOE
# include "CommandIp.h"
#endif
#include "CommandMaster.h"
#include "CommandPdos.h"
#include "CommandRegRead.h"
#include "CommandRegWrite.h"
#include "CommandRegReadWrite.h"
#include "CommandReboot.h"
#include "CommandRescan.h"
#include "CommandSdos.h"
#include "CommandSiiRead.h"
#include "CommandSiiWrite.h"
#include "CommandSlaves.h"
#include "CommandSoeRead.h"
#include "CommandSoeWrite.h"
#include "CommandStates.h"
#include "CommandUpload.h"
#include "CommandVersion.h"
#include "CommandXml.h"

#include "MasterDevice.h"

/*****************************************************************************/

typedef list<Command *> CommandList;
CommandList commandList;

string binaryBaseName;
string commandName;
Command::StringVector commandArgs;

// option variables
string masters = "-"; // all masters
string positions = "-"; // all positions
string aliases = "-"; // all aliases
string domains = "-"; // all domains
string dataTypeStr;
Command::Verbosity verbosity = Command::Normal;
bool force = false;
bool emergency = false;
bool helpRequested = false;
bool reset = false;
string outputFile;
string skin;

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
        << "  --master  -m <master>  Comma separated list of masters" << endl
        << "                         to select, ranges are allowed." << endl
        << "                         Examples: '1,3', '5-7,9', '-3'." << endl
        << "                         Default: '-' (all)."
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
        {"skin",        required_argument, NULL, 's'},
        {"emergency",   no_argument,       NULL, 'e'},
        {"force",       no_argument,       NULL, 'f'},
        {"reset",       no_argument,       NULL, 'r'},
        {"quiet",       no_argument,       NULL, 'q'},
        {"verbose",     no_argument,       NULL, 'v'},
        {"help",        no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "m:a:p:d:t:o:s:efrqvh", longOptions, NULL);

        switch (c) {
            case 'm':
                masters = optarg;
                break;

            case 'a':
                aliases = optarg;
                break;

            case 'p':
                positions = optarg;
                break;

            case 'd':
                domains = optarg;
                break;

            case 't':
                dataTypeStr = optarg;
                break;

            case 'o':
                outputFile = optarg;
                break;

            case 's':
                skin = optarg;
                break;

            case 'e':
                emergency = true;
                break;

            case 'f':
                force = true;
                break;

            case 'r':
                reset = true;
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

    // see if there's an exact match
    for (ci = commandList.begin(); ci != commandList.end(); ci++) {
        if ((*ci)->matches(cmdStr)) {
            res.push_back(*ci);
            break;
        }
    }
    
    if (!res.size()) { // nothing found
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
    commandList.push_back(new CommandCrc());
    commandList.push_back(new CommandCStruct());
    commandList.push_back(new CommandData());
    commandList.push_back(new CommandDebug());
    commandList.push_back(new CommandDiag());
    commandList.push_back(new CommandDomains());
    commandList.push_back(new CommandDownload());
#ifdef EC_EOE
    commandList.push_back(new CommandEoe());
    commandList.push_back(new CommandEoeAddIf());
    commandList.push_back(new CommandEoeDelIf());
#endif
    commandList.push_back(new CommandFoeRead());
    commandList.push_back(new CommandFoeWrite());
    commandList.push_back(new CommandGraph());
#ifdef EC_EOE
    commandList.push_back(new CommandIp());
#endif
    commandList.push_back(new CommandMaster());
    commandList.push_back(new CommandPdos());
    commandList.push_back(new CommandRegRead());
    commandList.push_back(new CommandRegWrite());
    commandList.push_back(new CommandRegReadWrite());
    commandList.push_back(new CommandReboot());
    commandList.push_back(new CommandRescan());
    commandList.push_back(new CommandSdos());
    commandList.push_back(new CommandSiiRead());
    commandList.push_back(new CommandSiiWrite());
    commandList.push_back(new CommandSlaves());
    commandList.push_back(new CommandSoeRead());
    commandList.push_back(new CommandSoeWrite());
    commandList.push_back(new CommandStates());
    commandList.push_back(new CommandUpload());
    commandList.push_back(new CommandVersion());
    commandList.push_back(new CommandXml());

    getOptions(argc, argv);

    matchingCommands = getMatchingCommands(commandName);

    if (matchingCommands.size()) {
        if (matchingCommands.size() == 1) {
            cmd = matchingCommands.front();
            if (!helpRequested) {
                try {
                    cmd->setMasters(masters);
                    cmd->setVerbosity(verbosity);
                    cmd->setAliases(aliases);
                    cmd->setPositions(positions);
                    cmd->setDomains(domains);
                    cmd->setDataType(dataTypeStr);
                    cmd->setOutputFile(outputFile);
                    cmd->setSkin(skin);
                    cmd->setEmergency(emergency);
                    cmd->setForce(force);
                    cmd->setReset(reset);
                    cmd->execute(commandArgs);
                } catch (InvalidUsageException &e) {
                    cerr << e.what() << endl << endl;
                    cerr << cmd->helpString(binaryBaseName);
                    retval = 1;
                } catch (CommandException &e) {
                    cerr << e.what() << endl;
                    retval = 1;
                } catch (MasterDeviceException &e) {
                    cerr << e.what() << endl;
                    retval = 1;
                }
            } else {
                cout << cmd->helpString(binaryBaseName);
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
