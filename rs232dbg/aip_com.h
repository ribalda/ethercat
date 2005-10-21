/**************************************************************************************************
*
*                          aip_com.h
*
*           Macros für Kommunikation über serielle Schnittstelle
*           Basiert auf rt_com.h von rtai !! (siehe unten)
*
*           
*           Autor: Wilhelm Hagemeister
*
*           (C) Copyright IgH 2002
*           Ingenieurgemeinschaft IgH
*           Heinz-Bäcker Str. 34
*           D-45356 Essen
*           Tel.: +49 201/61 99 31
*           Fax.: +49 201/61 98 36
*           E-mail: hm@igh-essen.com
*
*
*           $RCSfile: aip_com.h,v $
*           $Revision: 1.1 $
*           $Author: hm $
*           $Date: 2004/09/30 15:50:32 $
*           $State: Exp $
*
*
*
*
*
*
*
*
*
**************************************************************************************************/

/** rt_com
 *  ======
 *
 * RT-Linux kernel module for communication across serial lines.
 *
 * Copyright (C) 1997 Jens Michaelsen
 * Copyright (C) 1997-2000 Jochen Kupper
 * Copyright (C) 2002 Giuseppe Renoldi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file License. if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307, USA.
 *
 * $Id: aip_com.h,v 1.1 2004/09/30 15:50:32 hm Exp $ */



#ifndef AIP_COM_H
#define AIP_COM_H

/** This is the interface definition of the plain rt_com API.
 *
 * This should be all you need to use rt_com within your real-time
 * kernel module.
 *
 * (When POSIX is done, we will reference the appropriate header
 * here - probably depending on a flag.) */

int init_aip_com(void);              //Hm, IgH
void cleanup_aip_com(void);          //Hm, IgH


/** specify hardware parameters, set-up communication parameters */
extern int rt_com_hwsetup( unsigned int ttyS, int base, int irq );
extern int rt_com_setup( unsigned int ttyS, int baud, int mode,
			 unsigned int parity, unsigned int stopbits,
			 unsigned int wordlength, int fifotrig );

/** read/write from/to input/output buffer */
extern int rt_com_read( unsigned int, char *, int );
extern int rt_com_write( unsigned int ttyS, char *buffer, int count );

/** clear input or output buffer */
extern int rt_com_clear_input( unsigned int ttyS );
extern int rt_com_clear_output( unsigned int ttyS );

/** read input signal from modem (CTS,DSR,RI,DCD), set output signal
 * for modem control (DTR,RTS) */
extern int rt_com_read_modem( unsigned int ttyS, int signal );
extern int rt_com_write_modem( unsigned int ttyS, int signal, int value );

/** functioning mode and fifo trigger setting */
extern int rt_com_set_mode( unsigned int ttyS, int mode);
extern int rt_com_set_fifotrig( unsigned int ttyS, int fifotrig);

/** return last error detected */
extern int rt_com_error( unsigned int ttyS );


/** size of internal queues, this is constant during module lifetime */
extern unsigned int rt_com_buffersize;

#define rt_com_set_param rt_com_hwsetup
#define rt_com_setup_old(a,b,c,d,e) rt_com_setup((a),(b),0,(c),(d),(e),-1)


/** functioning modes */
#define RT_COM_NO_HAND_SHAKE  0x00
#define RT_COM_DSR_ON_TX      0x01
#define RT_COM_HW_FLOW	      0x02

/** parity flags */
#define RT_COM_PARITY_EVEN    0x18
#define RT_COM_PARITY_NONE    0x00
#define RT_COM_PARITY_ODD     0x08
#define RT_COM_PARITY_HIGH    0x28
#define RT_COM_PARITY_LOW     0x38

/* FIFO Control */
#define RT_COM_FIFO_DISABLE   0x00
#define RT_COM_FIFO_SIZE_1    0x00
#define RT_COM_FIFO_SIZE_4    0x40
#define RT_COM_FIFO_SIZE_8    0x80
#define RT_COM_FIFO_SIZE_14   0xC0

/** rt_com_write_modem  masks */
#define RT_COM_DTR            0x01
#define RT_COM_RTS            0x02 

/** rt_com_read_modem masks */
#define RT_COM_CTS            0x10
#define RT_COM_DSR            0x20
#define RT_COM_RI             0x40
#define RT_COM_DCD            0x80

/** rt_com_error masks */
#define RT_COM_BUFFER_FULL    0x01
#define RT_COM_OVERRUN_ERR    0x02
#define RT_COM_PARITY_ERR     0x04
#define RT_COM_FRAMING_ERR    0x08
#define RT_COM_BREAK          0x10


#endif /* RT_COM_H */



/**
 * Local Variables:
 * mode: C
 * c-file-style: "Stroustrup"
 * End:
 */
