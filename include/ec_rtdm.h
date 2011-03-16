/******************************************************************************
 *
 *  $Id$
 *
 *  ec_rtdm.h	Copyright (C) 2009-2010  Moehwald GmbH B.Benner
 *			                      2011       IgH Andreas Stewering-Bone
 *
 *  This file is used for Prisma RT to interface to EtherCAT devices
 *								  
 *  This file is part of ethercatrtdm interface to IgH EtherCAT master 
 *  
 *  The Moehwald ethercatrtdm interface is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation; version 2.1
 *  of the License.
 *
 *  The IgH EtherCAT master userspace library is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with the IgH EtherCAT master userspace library. If not, see
 *  <http://www.gnu.org/licenses/>.
 *  
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/
#ifndef __ECRT_RTDM_H
#define __ECRT_RTDM_H


//
// Basefilename of RTDM device
//
#define EC_RTDM_DEV_FILE_NAME  "ec_rtdm"

//
// IOCTRL Values for RTDM_EXTENSION
//
// Realtime IOCTRL function
#define EC_RTDM_MSTRATTACH       1   // attach to a running master
#define EC_RTDM_MSTRGETMUTNAME   2   // return the mutexname
#define EC_RTDM_MSTRRECEIVE      3   // call the master receive
#define EC_RTDM_MSTRSEND         4   // call the master send
#define EC_RTDM_DOMAINSTATE	     5   // get domain state
#define EC_RTDM_MASTERSTATE	     6   // get master state
#define EC_RTDM_MASTER_APP_TIME  7	
#define EC_RTDM_SYNC_REF_CLOCK   8
#define EC_RTDM_SYNC_SLAVE_CLOCK 9

typedef struct _CstructMstrAttach
{
  unsigned int        domainindex;
  unsigned int        masterindex;
} CstructMstrAttach;


#define ecrt_rtdm_master_attach(X,Y)             rt_dev_ioctl(X, EC_RTDM_MSTRATTACH, Y)
#define ecrt_rtdm_master_recieve(X)              rt_dev_ioctl(X, EC_RTDM_MSTRRECEIVE)
#define ecrt_rtdm_master_send(X)                 rt_dev_ioctl(X, EC_RTDM_MSTRSEND)
#define ecrt_rtdm_domain_state(X,Y)              rt_dev_ioctl(X, EC_RTDM_DOMAINSTATE, Y)
#define ecrt_rtdm_master_state(X,Y)              rt_dev_ioctl(X, EC_RTDM_MASTERSTATE, Y)
#define ecrt_rtdm_master_application_time(X,Y)   rt_dev_ioctl(X, EC_RTDM_MASTER_APP_TIME, Y)
#define ecrt_rtdm_master_sync_reference_clock(X) rt_dev_ioctl(X, EC_RTDM_SYNC_REF_CLOCK)
#define ecrt_rtdm_master_sync_slave_clocks(X)    rt_dev_ioctl(X, EC_RTDM_SYNC_SLAVE_CLOCK);

#endif


