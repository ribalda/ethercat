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
   EtherCAT Slave Database.
*/

/*****************************************************************************/

#ifndef __ECDB_H__
#define __ECDB_H__

/*****************************************************************************/

/** \cond */

#define Beckhoff_BK1120 0x00000002, 0x04602C22

#define Beckhoff_EL1004 0x00000002, 0x03EC3052
#define Beckhoff_EL1004_PDO_Inputs 0x3101, 1
#define Beckhoff_EL1004_Inputs Beckhoff_EL1004, Beckhoff_EL1004_PDO_Inputs

#define Beckhoff_EL1014 0x00000002, 0x03F63052
#define Beckhoff_EL1014_PDO_Inputs 0x3101, 1
#define Beckhoff_EL1014_Inputs Beckhoff_EL1014, Beckhoff_EL1014_PDO_Inputs 

#define Beckhoff_EL2004 0x00000002, 0x07D43052
#define Beckhoff_EL2004_PDO_Outputs 0x3001, 1
#define Beckhoff_EL2004_Outputs Beckhoff_EL2004, Beckhoff_EL2004_PDO_Outputs 

#define Beckhoff_EL2032 0x00000002, 0x07F03052
#define Beckhoff_EL2032_PDO_Outputs 0x3001, 1
#define Beckhoff_EL2032_Outputs Beckhoff_EL2032, Beckhoff_EL2032_PDO_Outputs

#define Beckhoff_EL3102 0x00000002, 0x0C1E3052
#define Beckhoff_EL3102_PDO_Status1 0x3101, 1
#define Beckhoff_EL3102_PDO_Input1  0x3101, 2
#define Beckhoff_EL3102_PDO_Status2 0x3102, 1
#define Beckhoff_EL3102_PDO_Input2  0x3102, 2
#define Beckhoff_EL3102_Status1 Beckhoff_EL3102, Beckhoff_EL3102_PDO_Status1
#define Beckhoff_EL3102_Input1  Beckhoff_EL3102, Beckhoff_EL3102_PDO_Input1 
#define Beckhoff_EL3102_Status2 Beckhoff_EL3102, Beckhoff_EL3102_PDO_Status2
#define Beckhoff_EL3102_Input2  Beckhoff_EL3102, Beckhoff_EL3102_PDO_Input2

#define Beckhoff_EL3152 0x00000002, 0x0C503052
#define Beckhoff_EL3152_PDO_Status1 0x3101, 1
#define Beckhoff_EL3152_PDO_Input1  0x3101, 2
#define Beckhoff_EL3152_PDO_Status2 0x3102, 1
#define Beckhoff_EL3152_PDO_Input2  0x3102, 2
#define Beckhoff_EL3152_Status1 Beckhoff_EL3152, Beckhoff_EL3152_PDO_Status1
#define Beckhoff_EL3152_Input1  Beckhoff_EL3152, Beckhoff_EL3152_PDO_Input1
#define Beckhoff_EL3152_Status2 Beckhoff_EL3152, Beckhoff_EL3152_PDO_Status2
#define Beckhoff_EL3152_Input2  Beckhoff_EL3152, Beckhoff_EL3152_PDO_Input2

#define Beckhoff_EL3162 0x00000002, 0x0C5A3052
#define Beckhoff_EL3162_PDO_Status1 0x3101, 1
#define Beckhoff_EL3162_PDO_Input1  0x3101, 2
#define Beckhoff_EL3162_PDO_Status2 0x3102, 1
#define Beckhoff_EL3162_PDO_Input2  0x3102, 2
#define Beckhoff_EL3162_Status1 Beckhoff_EL3162, Beckhoff_EL3162_PDO_Status1
#define Beckhoff_EL3162_Input1  Beckhoff_EL3162, Beckhoff_EL3162_PDO_Input1
#define Beckhoff_EL3162_Status2 Beckhoff_EL3162, Beckhoff_EL3162_PDO_Status2
#define Beckhoff_EL3162_Input2  Beckhoff_EL3162, Beckhoff_EL3162_PDO_Input2

#define Beckhoff_EL4102 0x00000002, 0x10063052
#define Beckhoff_EL4102_PDO_Output1 0x6411, 1
#define Beckhoff_EL4102_PDO_Output2 0x6411, 2
#define Beckhoff_EL4102_Output1 Beckhoff_EL4102, Beckhoff_EL4102_PDO_Output1
#define Beckhoff_EL4102_Output2 Beckhoff_EL4102, Beckhoff_EL4102_PDO_Output2

#define Beckhoff_EL4132 0x00000002, 0x10243052
#define Beckhoff_EL4132_PDO_Output1 0x6411, 1
#define Beckhoff_EL4132_PDO_Output2 0x6411, 2
#define Beckhoff_EL4132_Output1 Beckhoff_EL4132, Beckhoff_EL4132_PDO_Output1
#define Beckhoff_EL4132_Output2 Beckhoff_EL4132, Beckhoff_EL4132_PDO_Output2

#define Beckhoff_EL5001 0x00000002, 0x13893052
#define Beckhoff_EL5001_PDO_Status 0x3101, 1
#define Beckhoff_EL5001_PDO_Value  0x3101, 2
#define Beckhoff_EL5001_Status Beckhoff_EL5001, Beckhoff_EL5001_PDO_Status
#define Beckhoff_EL5001_Value  Beckhoff_EL5001, Beckhoff_EL5001_PDO_Value

#define Beckhoff_EL5101 0x00000002, 0x13ED3052
#define Beckhoff_EL5101_PDO_Status      0x6000, 1
#define Beckhoff_EL5101_PDO_Value       0x6000, 2
#define Beckhoff_EL5101_PDO_Latch       0x6000, 3
#define Beckhoff_EL5101_PDO_Frequency   0x6000, 4
#define Beckhoff_EL5101_PDO_Period      0x6000, 5
#define Beckhoff_EL5101_PDO_Window      0x6000, 6
#define Beckhoff_EL5101_PDO_Ctrl        0x7000, 1
#define Beckhoff_EL5101_PDO_OutputValue 0x7000, 2
#define Beckhoff_EL5101_Status      Beckhoff_EL5101, Beckhoff_EL5101_PDO_Status      
#define Beckhoff_EL5101_Value       Beckhoff_EL5101, Beckhoff_EL5101_PDO_Value       
#define Beckhoff_EL5101_Latch       Beckhoff_EL5101, Beckhoff_EL5101_PDO_Latch       
#define Beckhoff_EL5101_Frequency   Beckhoff_EL5101, Beckhoff_EL5101_PDO_Frequency   
#define Beckhoff_EL5101_Period      Beckhoff_EL5101, Beckhoff_EL5101_PDO_Period      
#define Beckhoff_EL5101_Window      Beckhoff_EL5101, Beckhoff_EL5101_PDO_Window      
#define Beckhoff_EL5101_Ctrl        Beckhoff_EL5101, Beckhoff_EL5101_PDO_Ctrl        
#define Beckhoff_EL5101_OutputValue Beckhoff_EL5101, Beckhoff_EL5101_PDO_OutputValue 

/** \endcond */

/*****************************************************************************/

#endif
