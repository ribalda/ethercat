/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2019  Florian Pose, Ingenieurgemeinschaft IgH
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
#include <unistd.h>
#include <signal.h>
#include <execinfo.h>
#include <string.h>

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandMbg.h"
#include "MasterDevice.h"

/*****************************************************************************/

string binaryBaseName;
CommandMbg::StringVector commandArgs;

// option variables
string                 masters = "-"; // all masters
CommandMbg::Verbosity  verbosity = CommandMbg::Normal;
bool                   helpRequested = false;
CommandMbg            *cmd;

/*****************************************************************************/

string usage()
{
    stringstream str;

    str << "Usage: " << binaryBaseName << " [OPTIONS]"
        << endl << endl;

    str << left
        << "Options:" << endl
        << "  --master  -m <master>    Specify the master to provide" << endl
        << "                           the gateway for if there is more" << endl
        << "                           than one master." << endl
        << "                           If there is only one master this" << endl
        << "                           option is not required." << endl
        << "  --quiet   -q             Output no information, unless" << endl
        << "                           there are parameter errors." << endl
        << "  --verbose -v             Output more information." << endl
        << "  --debug   -d             Output debug information." << endl
        << "  --help    -h             Show this help." << endl
        << endl
        << "Send bug reports to " << PACKAGE_BUGREPORT << "." << endl;

    return str.str();
}

/*****************************************************************************/

void getOptions(int argc, char **argv)
{
    int c;
    stringstream str;

    static struct option longOptions[] = {
        //name,         has_arg,           flag, val
        {"master",      required_argument, NULL, 'm'},
        {"quiet",       no_argument,       NULL, 'q'},
        {"verbose",     no_argument,       NULL, 'v'},
        {"debug",       no_argument,       NULL, 'd'},
        {"help",        no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "m:qvdh", longOptions, NULL);

        switch (c) {
            case 'm':
                masters = optarg;
                break;

            case 'q':
                verbosity = CommandMbg::Quiet;
                break;

            case 'v':
                verbosity = CommandMbg::Verbose;
                break;

            case 'd':
                verbosity = CommandMbg::Debug;
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

    while (++optind < argc)
        commandArgs.push_back(string(argv[optind]));
}

/****************************************************************************/

void halt(int sig)
{
    // halt execution immediately
    exit(128 + sig);
}

/****************************************************************************/

void terminate(int sig)
{
    if (verbosity > CommandMbg::Quiet) {
        cout << "Application Terminating" << endl;
    }

    // flag application to terminate
    if (cmd) {
        cmd->terminate();
    } else {
        // halt execution immediately
        exit(0);
    }
}

/****************************************************************************/

void debug(int sig)
{
    if (verbosity > CommandMbg::Quiet) {
        int   cnt;
        void *buffer[100];

        cerr << "Application Error: " << sig 
             << "(" << strsignal(sig) << ")" << endl << endl;
        
        // output backtrace to stderr
        cnt = backtrace(buffer, 100);
        backtrace_symbols_fd(buffer, cnt, STDERR_FILENO);
    }

    // exit
    exit(128 + sig);
}

/****************************************************************************/

int main(int argc, char **argv)
{
    binaryBaseName = basename(argv[0]);

    getOptions(argc, argv);

    if (helpRequested) {
        cout << usage();
        return 0;
    }
    
    // set up signal handlers
    signal(SIGINT, halt);
    signal(SIGKILL, halt);
    signal(SIGTERM, terminate);
    signal(SIGALRM, terminate);
    signal(SIGBUS, debug);
    signal(SIGFPE, debug);
    signal(SIGABRT, debug);
    signal(SIGSEGV, debug);
    
    
    // run the gateway until cancelled
    cmd = new CommandMbg();
    cmd->setMasters(masters);
    cmd->setVerbosity(verbosity);
    
    try {
        // execute server
        cmd->execute(commandArgs);
    } catch (CommandException &e) {
        // catch application errors and exit
        if (verbosity > CommandMbg::Quiet) {
            cerr << "Application Error: " << e.what() << endl;
        }
    } catch (runtime_error &e) {
        // catch unknown errors and exit
        if (verbosity > CommandMbg::Quiet) {
            cerr << "Unknown Error: " << e.what() << endl;
        }
    }

    
    return 0;
}

/****************************************************************************/
