/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  The right to use EtherCAT Technology is granted and comes free of
 *  charge under condition of compatibility of product made by
 *  Licensee. People intending to distribute/sell products based on the
 *  code, have to sign an agreement to guarantee that products using
 *  software based on IgH EtherCAT master stay compatible with the actual
 *  EtherCAT specification (which are released themselves as an open
 *  standard) as the (only) precondition to have the right to use EtherCAT
 *  Technology, IP and trade marks.
 *
 *****************************************************************************/

/**
   \file
   EtherCAT XML Daemon.
*/

/*****************************************************************************/

#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <syslog.h>

#include <libxml/parser.h>

#include <iostream>
#include <list>
using namespace std;

#include "slave_device.hpp"

/*****************************************************************************/

unsigned int sig_int_term = 0;
unsigned int sig_hangup = 0;
string xml_dir;
bool become_daemon = true;

list<SlaveDevice> slaveDevices;

/*****************************************************************************/

void parse_xml_file(const char *);
void parse_info(xmlDocPtr, xmlNodePtr);
void parse_descriptions(xmlDocPtr, xmlNodePtr);
void parse_devices(xmlDocPtr, xmlNodePtr);

void read_xml_dir();
void set_signal_handlers();
void get_options(int, char *[]);
void print_usage();
void signal_handler(int);
void init_daemon();

/*****************************************************************************/

int main(int argc, char *argv[])
{
    set_signal_handlers();
    get_options(argc, argv);

    read_xml_dir();

    if (become_daemon) init_daemon();

    openlog("ecxmld", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "EtherCAT XML daemon starting up.");

    return 0;
}

/*****************************************************************************/

void read_xml_dir()
{
    DIR *dir;
    struct dirent *dir_ent;
    string entry;

    if (!(dir = opendir(xml_dir.c_str()))) {
        cerr << "ERROR: Failed to open XML directory \"" << xml_dir << "\"!"
             << endl;
        exit(1);
    }

    while ((dir_ent = readdir(dir))) {
        entry = dir_ent->d_name;
        if (entry.size() < 4
            || entry.substr(entry.size() - 4) != ".xml") continue;

        parse_xml_file((xml_dir + "/" + entry).c_str());
    }

    // Verzeichnis schliessen
    closedir(dir);
}

/*****************************************************************************/

void parse_xml_file(const char *xml_file)
{
    xmlDocPtr doc;
    xmlNodePtr cur;

    cout << xml_file << endl;

    if (!(doc = xmlParseFile(xml_file))) {
        cerr << "ERROR: Parse error in document!" << endl;
        return;
    }

    if (!(cur = xmlDocGetRootElement(doc))) {
        cout << "Empty document!" << endl;
        xmlFreeDoc(doc);
        return;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "EtherCATInfo")) {
        cerr << "Document of the wrong type!" << endl;
        xmlFreeDoc(doc);
        return;
    }

    parse_info(doc, cur);
    xmlFreeDoc(doc);
}

/*****************************************************************************/

void parse_info(xmlDocPtr doc, xmlNodePtr cur)
{
    cout << "info" << endl;
    cur = cur->xmlChildrenNode;

    while (cur) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *) "Descriptions"))) {
            parse_descriptions(doc, cur);
        }
	cur = cur->next;
    }
}

/*****************************************************************************/

void parse_descriptions(xmlDocPtr doc, xmlNodePtr cur)
{
    cout << "desc" << endl;
    cur = cur->xmlChildrenNode;

    while (cur) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *) "Devices"))) {
            parse_devices(doc, cur);
        }
	cur = cur->next;
    }
}

/*****************************************************************************/

void parse_devices(xmlDocPtr doc, xmlNodePtr cur)
{
    cout << "devices" << endl;
    cur = cur->xmlChildrenNode;

    while (cur) {
        if ((!xmlStrcmp(cur->name, (const xmlChar *) "Device"))) {
            slaveDevices.push_back(SlaveDevice());
            slaveDevices.back().fromXml(doc, cur);
        }
	cur = cur->next;
    }
}

/*****************************************************************************/

void get_options(int argc, char *argv[])
{
    int c;

    while (1) {
        if ((c = getopt(argc, argv, "d:kh")) == -1) break;

            switch (c) {
            case 'd':
                xml_dir = optarg;
                break;

            case 'k':
                become_daemon = false;
                break;

            case 'h':
                print_usage();
                exit(0);

            default:
                print_usage();
                exit(1);
        }
    }

    if (optind < argc) {
        cerr << "ERROR: Too many arguments!" << endl;
        print_usage();
        exit(1);
    }

    if (xml_dir == "") {
        cerr << "ERROR: XML directory not set!" << endl;
        print_usage();
        exit(1);
    }
}

/*****************************************************************************/

void print_usage()
{
    cout << "Usage: ecxmld [OPTIONS]" << endl
         << "   -d DIR   Set XML directory (MANDATORY)." << endl
         << "   -k       Do not detach from console." << endl
         << "   -h       Show this help." << endl;
}

/*****************************************************************************/

void signal_handler(int sig)
{
    if (sig == SIGHUP) {
        sig_hangup++;
    }
    else if (sig == SIGINT || sig == SIGTERM) {
        sig_int_term++;
    }
}

/*****************************************************************************/

void set_signal_handlers()
{
    struct sigaction action;

    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGHUP, &action, 0);
    sigaction(SIGINT, &action, 0);
    sigaction(SIGTERM, &action, 0);
}

/*****************************************************************************/

void init_daemon()
{
    pid_t pid;

    if ((pid = fork()) < 0) {
        cerr << endl << "ERROR: fork() failed!" << endl << endl;
        exit(1);
    }

    if (pid) exit(0);

    if (setsid() == -1) {
        cerr << "ERROR: Failed to become session leader!" << endl;
        exit(1);
    }

    if (chdir("/") < 0) {
        cerr << "ERROR: Failed to change to file root!" << endl;
        exit(1);
    }

    umask(0);

    if (close(0) < 0) {
        cerr << "WARNING: Failed to close STDIN!" << endl;
    }

    if (close(1) < 0) {
        cerr << "WARNING: Failed to close STDOUT!" << endl;
    }

    if (close(2) < 0) {
        cerr << "WARNING: Failed to close STDERR!" << endl;
    }
}

/*****************************************************************************/
