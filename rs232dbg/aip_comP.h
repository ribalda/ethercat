/**
 * rt_com
 * ======
 *
 * RT-Linux kernel module for communication across serial lines.
 *
 * Copyright (C) 1997 Jens Michaelsen
 * Copyright (C) 1997-2000 Jochen Kupper
 * Copyright (C) 1999 Hua Mao <hmao@nmt.edu>
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
 */



#ifndef AIP_COM_P_H
#define AIP_COM_P_H


#define RT_COM_NAME "rt_com(aip)"  //Hm, IgH

/* input/ouput buffer (FIFO) sizes */
#define RT_COM_BUF_SIZ        2048 //256	// MUST BE ONLY POWER OF 2 !!
/* amount of free space on input buffer for RTS reset */
#define RT_COM_BUF_LOW        (RT_COM_BUF_SIZ / 3)
/* amount of free space on input buffer for RTS set */
#define RT_COM_BUF_HI         (RT_COM_BUF_SIZ * 2 / 3)
/* limit of free space on input buffer for buffer full error */
#define RT_COM_BUF_FULL       20


/* usage flags */
#define RT_COM_PORT_FREE      0x00
#define RT_COM_PORT_INUSE     0x01
#define RT_COM_PORT_SETUP     0x02


/* Some hardware description */
#define RT_COM_BASE_BAUD      115200

/** Interrupt Service Routines
 * These are private functions */
static void rt_com0_isr( void );
static void rt_com1_isr( void );




/** Interrupt handling
 *
 * Define internal convinience macros for interrupt handling, so we
 * get rid of the system dependencies.
 */
//#define rt_com_irq_off( state )         do{}while(0) //rt_global_save_flags( &state ); rt_global_cli() schreiben und lesen sowieso in IRQ Hm
//#define rt_com_irq_on(state)            do{}while(0) //rt_global_restore_flags( state ) 
#define rt_com_irq_off( state )         rt_global_save_flags( &state ); rt_global_cli() 
#define rt_com_irq_on(state)            rt_global_restore_flags( state ) 
#define rt_com_request_irq( irq, isr )  rt_request_global_irq( irq, isr ); rt_enable_irq( irq );
#define rt_com_free_irq( irq )          rt_free_global_irq( irq )


/* port register offsets */
#define RT_COM_RXB  0x00
#define RT_COM_TXB  0x00
#define RT_COM_IER  0x01
#define RT_COM_IIR  0x02
#define RT_COM_FCR  0x02
#define RT_COM_LCR  0x03
#define RT_COM_MCR  0x04
#define RT_COM_LSR  0x05
#define RT_COM_MSR  0x06
#define RT_COM_DLL  0x00
#define RT_COM_DLM  0x01

/** MCR Modem Control Register masks */
#define MCR_DTR               0x01 // Data Terminal Ready
#define MCR_RTS               0x02 // Request To Send 
#define MCR_OUT1              0x04
#define MCR_OUT2              0x08
#define MCR_LOOP              0x10
#define MCR_AFE               0x20 // AutoFlow Enable

/** IER Interrupt Enable Register masks */
#define IER_ERBI              0x01  // Enable Received Data Available Interrupt
#define IER_ETBEI             0x02  // Enable Transmitter Holding Register
                                    // Empty Interrupt
#define IER_ELSI              0x04  // Enable Receiver Line Status Interrupt
#define IER_EDSSI             0x08  // Enable Modem Status Interrupt

/** MSR Modem Status Register masks */
#define MSR_DELTA_CTS         0x01
#define MSR_DELTA_DSR         0x02
#define MSR_TERI              0x04
#define MSR_DELTA_DCD         0x08
#define MSR_CTS               0x10
#define MSR_DSR               0x20
#define MSR_RI                0x40
#define MSR_DCD               0x80

/** LSR Line Status Register masks */
#define LSR_DATA_READY        0x01
#define LSR_OVERRUN_ERR       0x02
#define LSR_PARITY_ERR        0x04
#define LSR_FRAMING_ERR       0x08
#define LSR_BREAK             0x10
#define LSR_THRE              0x20	// Transmitter Holding Register
#define LSR_TEMT              0x40	// Transmitter Empty

/** FCR FIFO Control Register masks */
#define FCR_FIFO_ENABLE       0x01
#define FCR_INPUT_FIFO_RESET  0x02
#define FCR_OUTPUT_FIFO_RESET 0x04

/** data buffer
 *
 * Used for buffering of input and output data. Two buffers per port
 * (one for input, one for output). Organized as a FIFO */
struct rt_buf_struct{
    unsigned int  head;
    unsigned int  tail;
    char buf[ RT_COM_BUF_SIZ ];
};



/** Port data
 *
 * Internal information structure containing all data for a port. One
 * structure for every port.
 *
 * Contains all current setup parameters and all data currently
 * buffered by rt_com.
 *
 * mode (functioning mode)
 *   possible values:
 *   - RT_COM_DSR_ON_TX     - for standard functioning mode (DSR needed on TX)
 *   - RT_COM_NO_HAND_SHAKE - for comunication without hand shake signals
 *                            (only RXD-TXD-GND)
 *   - RT_COM_HW_FLOW       - for hardware flow control (RTS-CTS)
 *   Of course RT_COM_DSR_ON_TX and RT_COM_NO_HAND_SHAKE cannot be
 *   sppecified at the same time.
 *
 *   NOTE: When you select a mode that uses hand shake signals pay
 *   attention that no input signals (CTS,DSR,RI,DCD) must be
 *   floating.
 *
 * used (usage flag)
 *   possible values:
 *   - RT_COM_PORT_INUSE - port region requested by init_module
 *   - RT_COM_PORT_FREE  - port region requested by rt_com_set_param
 *   - RT_COM_PORT_SETUP - port parameters are setup,
 *                         don't specify at compile time !
 *
 * error
 *   last error detected
 *
 * ier (interrupt enable register)
 *   copy of IER chip register, last value set by rt_com.
 *
 * mcr (modem control register)
 *   copy of the MCR internal register
 */
struct rt_com_struct{
    int baud_base;
    int port;
    int irq;
    void (*isr)(void);
	int baud;
	unsigned int wordlength;
	unsigned int parity;
	unsigned int stopbits;
    int mode;
	int fifotrig;
    int used;
	int error;
    int type;
    int ier;
    int mcr;
    struct rt_buf_struct ibuf;
    struct rt_buf_struct obuf;
};


#endif /* RT_COM_P_H */



/**
 * Local Variables:
 * mode: C
 * c-file-style: "Stroustrup"
 * End:
 */
