/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <getopt.h>

#include <iostream>
#include <string>
using namespace std;

#include "Master.h"

/*****************************************************************************/

#define DEFAULT_MASTER 0
#define DEFAULT_COMMAND "slaves"
#define DEFAULT_SLAVESPEC ""

static unsigned int masterIndex = DEFAULT_MASTER;
static int slavePosition = -1;
static string command = DEFAULT_COMMAND;

/*****************************************************************************/

void printUsage()
{
    cerr
        << "Usage: ethercat <COMMAND> [OPTIONS]" << endl
		<< "Commands:" << endl
        << "  list (ls, slaves)  List all slaves (former 'lsec')." << endl
        << "  pdos               List Pdo mapping of given slaves." << endl
		<< "Global options:" << endl
        << "  --master  -m <master>  Index of the master to use. Default: "
		<< DEFAULT_MASTER	<< endl
        << "  --slave   -s <slave>   Numerical ring position. Default: All "
        "slaves." << endl
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
        {"help",   no_argument,       NULL, 'h'},
        {}
    };

    do {
        c = getopt_long(argc, argv, "m:s:h", longOptions, &optionIndex);

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
                number = strtoul(optarg, &remainder, 0);
                if (remainder == optarg || *remainder
                        || number < 0 || number > 0xFFFF) {
                    cerr << "Invalid slave position " << optarg << "!" << endl;
                    printUsage();
                    exit(1);
                }
				slavePosition = number;
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
}

/****************************************************************************/

int main(int argc, char **argv)
{
    Master master;
    
	getOptions(argc, argv);

    try {
        master.open(masterIndex);

        if (command == "list" || command == "ls" || command == "slaves") {
            master.listSlaves();

        } else if (command == "pdos") {
            master.listPdos(slavePosition);
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
