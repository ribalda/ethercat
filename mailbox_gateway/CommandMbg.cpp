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
 *  vim: expandtab
 *
 ****************************************************************************/

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

#include <map>
#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandMbg.h"
#include "MasterDevice.h"
#include "NumberListParser.h"
#include "../master/globals.h"

/*****************************************************************************/

class MasterIndexParser:
    public NumberListParser
{
    protected:
        int getMax() {
            MasterDevice dev;
            dev.setIndex(0U);
            dev.open(MasterDevice::Read);
            return (int) dev.getMasterCount() - 1;
        };
};

/*****************************************************************************/

CommandMbg::CommandMbg():
    m_verbosity(Normal)
{
    m_terminate = 0;
}

/*****************************************************************************/

CommandMbg::~CommandMbg()
{
}

/*****************************************************************************/

void CommandMbg::setMasters(const string &m)
{
    m_masters = m;
};

/*****************************************************************************/

void CommandMbg::setVerbosity(Verbosity v)
{
    m_verbosity = v;
};

/*****************************************************************************/

void CommandMbg::throwInvalidUsageException(const stringstream &s)
{
    throw InvalidUsageException(s);
}

/*****************************************************************************/

void CommandMbg::throwCommandException(const string &msg)
{
    throw CommandException(msg);
}

/*****************************************************************************/

void CommandMbg::throwCommandException(const stringstream &s)
{
    throw CommandException(s);
}

/*****************************************************************************/

CommandMbg::MasterIndexList CommandMbg::getMasterIndices() const
{
    MasterIndexList indices;

    try {
        MasterIndexParser p;
        indices = p.parse(m_masters.c_str());
    } catch (MasterDeviceException &e) {
        stringstream err;
        err << "Failed to obtain number of masters: " << e.what();
        throwCommandException(err);
    } catch (runtime_error &e) {
        stringstream err;
        err << "Invalid master argument '" << m_masters << "': " << e.what();
        throwInvalidUsageException(err);
    }

    return indices;
}

/*****************************************************************************/

unsigned int CommandMbg::getSingleMasterIndex() const
{
    MasterIndexList masterIndices = getMasterIndices();

    if (masterIndices.size() != 1) {
        stringstream err;
        err << "Single master required!";
        throwInvalidUsageException(err);
    }

    return masterIndices.front();
}

/*****************************************************************************/

bool operator<(
        const ec_ioctl_config_t &a,
        const ec_ioctl_config_t &b
        )
{
    return a.alias < b.alias
        || (a.alias == b.alias && a.position < b.position);
}

/****************************************************************************/

void CommandMbg::printBuff(uint8_t *in_buffer, size_t in_nbytes)
{
    size_t i;
    
    for (i = 0; i < in_nbytes; i++) {
        if (i % 16 == 0) {
            // add 2 space prefix
            cout << "  ";
        }
        
        cout << std::setfill('0') << std::setw(2) 
             << std::hex << (int)(*in_buffer);
        
        if (i % 16 == 15) {
            // add endl after 16 bytes
            cout << endl;
        } else if (i % 2 == 1) {
            // add space after 2 bytes
            cout << " ";
        }
        
        in_buffer++;
    }
    
    // add final endl?
    if ( (in_nbytes > 0) && (in_nbytes % 16 != 0) ) {
        cout << endl;
    }
    
    cout << std::dec;
}

/****************************************************************************/

int CommandMbg::processMessage(uint8_t *in_buffer, size_t &inout_nbytes)
{
    uint8_t                 *buff = in_buffer;
    ec_ioctl_mbox_gateway_t  ioctl;
    
    if (m_verbosity >= CommandMbg::Debug) {
        cout << "Received packet (size: " << inout_nbytes 
             << " bytes):" << endl;
        printBuff(in_buffer, inout_nbytes);
    }
    
    // check there's enough room for the EtherCAT Header
    // and get the length of the following data
    if (inout_nbytes < EC_FRAME_HEADER_SIZE) {
        if (m_verbosity >= CommandMbg::Normal) {
            cout << "Message error, received bytes < EtherCAT Header size" << endl;
        }
        return -1;
    }
    ioctl.data_size = EC_READ_U16(buff) & 0x7FF;
    ioctl.buff_size = MAX_BUFF_SIZE - EC_FRAME_HEADER_SIZE;
    buff           += EC_FRAME_HEADER_SIZE;
    
    // check we have all the data we expect
    if (inout_nbytes < EC_FRAME_HEADER_SIZE + ioctl.data_size) {
        if (m_verbosity >= CommandMbg::Normal) {
            cout << "Message error, received bytes (" << inout_nbytes
                 << ") < packet size (" << EC_FRAME_HEADER_SIZE + ioctl.data_size
                 << ")" << endl;
        }
        return -1;
    } else if ( (inout_nbytes > EC_FRAME_HEADER_SIZE + ioctl.data_size) &&
                (m_verbosity >= CommandMbg::Verbose) ) {
        cout << "Message warning, received bytes (" << inout_nbytes
             << ") > packet size (" << EC_FRAME_HEADER_SIZE + ioctl.data_size
             << "), ignoring extra data" << endl;
    }
    
    m_masterDev->open(MasterDevice::ReadWrite);
    
    // send the data
    ioctl.data = buff;
    if (m_masterDev->processMessage(&ioctl) < 0) {
        if (m_verbosity >= CommandMbg::Normal) {
            cout << "Message failed with code: " << strerror(errno)
                 << ", Check ethercat logs for more information" << endl;
        }
        return -1;
    }

    m_masterDev->close();

    // update EtherCAT header length and returned byte count
    EC_WRITE_U16(in_buffer, (ioctl.data_size & 0x7FF) | (EC_READ_U16(in_buffer) & 0xF800));
    inout_nbytes = EC_FRAME_HEADER_SIZE + ioctl.data_size;
    
    if (m_verbosity >= CommandMbg::Debug) {
        cout << "ECat master replied with (size: " << inout_nbytes
             << " bytes):" << endl;
        printBuff(in_buffer, inout_nbytes);
    }
    
    
    return 0;
}

/****************************************************************************/

void CommandMbg::execute(const StringVector &args)
{
    int                 tcpSockFd;
    int                 udpSockFd;
    int                 clientFd;
    int                 sockFd;
    struct sockaddr_in  serverAddr;
    struct sockaddr_in  clientAddr;
    socklen_t           addrlen;
    int                 retries;
    fd_set              masterSet;
    fd_set              sockSet;
    int                 minFd;
    int                 maxFd;
    int                 tmpFd;
    int                 limitFd;
    struct timeval      loopTimeout;
    int                 nready;
    uint8_t             buffer[MAX_BUFF_SIZE]; 
    size_t              nbytes;

    
    // ensure a single master
    m_masterDev = new MasterDevice(getSingleMasterIndex());


    // create TCP socket
    tcpSockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSockFd == -1) {
        throwCommandException("Unable to create TCP socket");
    } else if (m_verbosity >= CommandMbg::Verbose) {
        cout << "TCP socket created" << endl;
    }

    // set up the socket to reuse address so that it should bind immediately to
    // a previously closed port
    int yes = 1;
    if (setsockopt(tcpSockFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        throwCommandException("Unable to configure TCP socket (for reuse)");
    } else if (m_verbosity >= CommandMbg::Verbose) {
        cout << "TCP socket configured for reuse" << endl;
    }

    // set up the socket to disable Nagle (ie combining of small packet)
    // so that we avoid delay
    if (setsockopt(tcpSockFd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) == -1) {
        throwCommandException("Unable to configure TCP socket (TCP_NODELAY)");
    } else if (m_verbosity >= CommandMbg::Verbose) {
        cout << "TCP socket configured for no Nagle" << endl;
    }

    // try to bind the TCP socket, retry a few times
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_port        = htons(SVR_PORT);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    retries = 0;
    while ( bind(tcpSockFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) ) {
        if (m_verbosity >= CommandMbg::Verbose) {
            cout << "Unable to bind TCP socket on port " << SVR_PORT << endl;
        }

        if (++retries >= 30) {
            stringstream err;
            err << "Unable to bind TCP socket on port " << SVR_PORT;
            throwCommandException(err);
        }
        
        sleep(10);
    }
    if (m_verbosity >= CommandMbg::Verbose) {
        cout << "TCP socket bound" << endl;
    }

    // set up listening for client TCP connections
    if (listen(tcpSockFd, 10)) {
        throwCommandException("Error listening for TCP socket connections");
    } else if (m_verbosity >= CommandMbg::Normal) {
        cout << "TCP socket listening for connections on port " << SVR_PORT << endl;
    }


    // create UDP socket
    udpSockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSockFd == -1) {
        throwCommandException("Unable to create UDP socket");
    } else if (m_verbosity >= CommandMbg::Verbose) {
        cout << "UDP socket created" << endl;
    }

    // try to bind the UDP socket, retry a few times
    retries = 0;
    while ( bind(udpSockFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) ) {
        if (m_verbosity >= CommandMbg::Verbose) {
            cout << "Unable to bind UDP socket on port " << SVR_PORT << endl;
        }

        if (++retries >= 30) {
            stringstream err;
            err << "Unable to bind UDP socket on port " << SVR_PORT;
            throwCommandException(err);
        }
        
        sleep(10);
    }
    if (m_verbosity >= CommandMbg::Verbose) {
        cout << "UDP socket bound" << endl;
    }
    if (m_verbosity >= CommandMbg::Normal) {
        cout << "UDP socket listening for connections on port " << SVR_PORT << endl;
    }


    // clear the socket set
    FD_ZERO(&masterSet);

    // set the TCP and UDP flags in the socket read set 
    FD_SET(tcpSockFd, &masterSet);
    FD_SET(udpSockFd, &masterSet);
    
    // get the socket fd range
    minFd = (tcpSockFd < udpSockFd) ? tcpSockFd : udpSockFd;
    maxFd = (tcpSockFd > udpSockFd) ? tcpSockFd : udpSockFd;
    
    // get the tcp connection limit
    limitFd = maxFd + MAX_CONNECTIONS;

    if (m_verbosity >= CommandMbg::Debug) {
        cout << "tcpSockFd " << tcpSockFd
             << ", udpSockFd " << udpSockFd
             << ", minFd " << minFd
             << ", maxFd " << maxFd
             << ", limitFd " << limitFd << endl;
    }

    // run the server until
    while (!m_terminate) {
        // wait for a socket request (new connection or client request)
        // Note: uses timeout of 5 seconds to allow checking of terminate flag
        // Note: copy masterSet (active socket fd's) so that select can return
        //   the ready sockets to sockSet
        loopTimeout.tv_sec  = 5;
        loopTimeout.tv_usec = 0;
        sockSet = masterSet;
        nready = select(maxFd+1, &sockSet, NULL, NULL, &loopTimeout);
        
        // check for errors
        if (nready == -1) {
            int selectErr = errno;
            if (selectErr == EINTR) {
                // server interrupted, loop
                continue;
            } else {
                stringstream err;
                err << "Error during select " << errno 
                    << ", " << strerror(selectErr);
                throwCommandException(err);
            }
        }
        
        // run through the existing connections looking for data to be read
        // Note: cache the maxFd for the for loop to avoid looping too far
        //   if a new clientFd is accepted
        tmpFd = maxFd;
        for (sockFd = minFd; sockFd <= tmpFd; sockFd++)
        {
            // skip if fd is not in set
            if (!FD_ISSET(sockFd, &sockSet)) {
                continue;
            }
            
            if (m_verbosity >= CommandMbg::Debug) {
                cout << "Socket " << sockFd
                     << " selected" << endl;
            }
            
            if (sockFd == udpSockFd) {
                // UDP socket datagram
                addrlen = sizeof(clientAddr);
                nbytes = recvfrom(udpSockFd, buffer, MAX_BUFF_SIZE, 0,
                                  (struct sockaddr*)&clientAddr, &addrlen);
                if (nbytes <= 0) {
                    // got error or connection closed by client
                    if ((nbytes == 0) || (errno == ECONNRESET)) {
                        if (m_verbosity >= CommandMbg::Verbose) {
                            cout << "UDP client connection closed on " << sockFd << endl;
                        }
                    } else {
                        if (m_verbosity >= CommandMbg::Normal) {
                            cout << "UDP client read error " << errno 
                                 << " " << strerror(errno) 
                                 << ", on " << sockFd << endl;
                        }
                    }
                } else {
                    // process the frame
                    if (processMessage(buffer, nbytes) == 0) {
                        // send the reply
                        if (sendto(udpSockFd, buffer, nbytes, 0,
                                   (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) {
                            // send error
                            if (m_verbosity >= CommandMbg::Normal) {
                                cout << "UDP client send error " << errno 
                                     << " " << strerror(errno) 
                                     << ", on " << sockFd << endl;
                            }
                        }
                    }
                }
            } else if (sockFd == tcpSockFd) {
                // new TCP socket connection
                // accept the connection
                addrlen  = sizeof(clientAddr);
                clientFd = accept(tcpSockFd, (struct sockaddr*)&clientAddr, &addrlen);
                
                // check for error
                if ( (clientFd == -1) && (m_verbosity >= CommandMbg::Verbose) ) {
                    cout << "Error accepting TCP client connection" << endl;
                // check conneciton count
                } else if (clientFd > limitFd) {
                    shutdown(clientFd, SHUT_RDWR);
                    if (m_verbosity >= CommandMbg::Verbose) {
                        cout << "TCP client connection rejected, too many connections" << endl;
                    }
                } else {
                    if (m_verbosity >= CommandMbg::Verbose) {
                        cout << "New TCP client connection on " << clientFd << endl;
                    }
                    
                    // add the connection to the master sock set (and min/maxFd's)
                    FD_SET(clientFd, &masterSet);
                    if (clientFd < minFd) minFd = clientFd;
                    if (clientFd > maxFd) maxFd = clientFd;
                    
                    if (m_verbosity >= CommandMbg::Debug) {
                        cout << "minFd " << minFd
                             << ", maxFd " << maxFd << endl;
                    }
                }
            } else {
                // TCP client connection
                nbytes = recv(sockFd, buffer, MAX_BUFF_SIZE, 0);
                if (nbytes <= 0) {
                    // got error or connection closed by client
                    if ((nbytes == 0) || (errno == ECONNRESET)) {
                       if (m_verbosity >= CommandMbg::Verbose) {
                            cout << "TCP client connection closed on " << sockFd << endl;
                        }
                    } else {
                        if (m_verbosity >= CommandMbg::Normal) {
                            cout << "TCP client read error " << errno 
                                 << " " << strerror(errno) 
                                 << ", connection closed on " << sockFd << endl;
                        }
                    }

                    // close the socket
                    close(sockFd);

                    // remove from master sock set
                    FD_CLR(sockFd, &masterSet);
                } else {
                    // process the frame
                    if (processMessage(buffer, nbytes) == 0) {
                        // reply
                        if (write(sockFd, buffer, nbytes) < 0) {
                            // write error
                            if (m_verbosity >= CommandMbg::Normal) {
                                cout << "TCP client write error " << errno 
                                     << " " << strerror(errno) 
                                     << ", connection closed on " << sockFd << endl;
                            }

                            // close the socket
                            close(sockFd);

                            // remove from master sock set
                            FD_CLR(sockFd, &masterSet);
                        }
                    }
                }
            }
        }
    }
    
    if (m_verbosity >= CommandMbg::Normal) {
        cout << "Server exiting" << endl;
    }
}

/*****************************************************************************/

void CommandMbg::terminate()
{
    m_terminate = 1;
};

/****************************************************************************/
