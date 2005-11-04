/** rt_com
 *  ======
 *
 * RT-Linux kernel module for communication across serial lines.
 *
 * Copyright (C) 1997 Jens Michaelsen
 * Copyright (C) 1997-2000 Jochen Kupper
 * Copyright (C) 1999 Hua Mao <hmao@nmt.edu>
 * Copyright (C) 1999 Roberto Finazzi
 * Copyright (C) 2000-2002 Giuseppe Renoldi
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
 * $Id: aip_com.c,v 1.1 2005/09/16 14:16:31 hm Exp hm $ */
#define VERSION "0.6.pre2-rtaicvs (modified by Hm, IgH for aip)"   //Hm, IgH

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

#include <asm/system.h>
#include <asm/io.h>

#ifdef _RTAI
#include <rtai.h>
#endif

#include "aip_com.h"
#include "aip_comP.h"

/* Hm, IgH
MODULE_AUTHOR("Jochen Kuepper");
MODULE_DESCRIPTION("real-time serial port driver");
MODULE_LICENSE("GPL");
*/


/* size of internal queues
* This is the default value. */
unsigned int            rt_com_buffersize = RT_COM_BUF_SIZ;

/** Default: mode=0 - no hw flow control
 *           used=0 - port and irq setting by rt_com_hwparam.
 *                    If you want to work like
 *                    a standard rt_com you can set used=1. */
struct rt_com_struct    rt_com_table[] = { 
       {    // ttyS0 - COM1
        RT_COM_BASE_BAUD,   // int baud_base;
        0x3f8,          // int port;
        4,              // int irq;
        rt_com0_isr,    // void (*isr)(void);
        115200,  // int baud;
        8,  // unsigned int wordlength;
        RT_COM_PARITY_NONE, // unsigned int parity;
        1,  // stopbits;
        RT_COM_NO_HAND_SHAKE,   // int mode;
        RT_COM_FIFO_SIZE_8,     // int fifotrig;
        1 //Hm, IgH   // int used;
    }, {    // ttyS1 - COM2
        RT_COM_BASE_BAUD,   // int baud_base;
        0x2f8,          // int port;
        3,              // int irq;
        rt_com1_isr,    // void (*isr)(void);
        0,  // int baud;
        8,  // unsigned int wordlength;
        RT_COM_PARITY_NONE, // unsigned int parity;
        1,  // stopbits;
        RT_COM_NO_HAND_SHAKE,   // int mode;
        RT_COM_FIFO_SIZE_8,     // int fifotrig;
        0   // int used;
    }
};

/** Number and descriptions of serial ports to manage.  You also need
 * to create an ISR ( rt_comN_isr() ) for each port.  */
#define RT_COM_CNT  (sizeof(rt_com_table) / sizeof(struct rt_com_struct))

/** Internal: Remaining free space of buffer
 *
 * @return amount of free space remaining in a buffer (input or output)
 *
 * @author Jochen Kupper
 * @version 2000/03/10 */
static inline unsigned int rt_com_buffer_free(unsigned int head, unsigned int tail)
{
    return(head < tail) ? (tail - head) : (rt_com_buffersize - (head - tail));
}

/** Clear input buffer.
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @return       0 if all right, -ENODEV or -EPERM on error.
 *
 * @author Roberto Finazzi, Jochen Kupper
 * @version 2000/03/12 */
int rt_com_clear_input(unsigned int ttyS)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else if (0 >= (rt_com_table[ttyS]).used) {
        return(-EPERM);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        struct rt_buf_struct    *b = &(p->ibuf);
        long                    state;
        rt_com_irq_off(state);
        b->tail = b->head;
        if (p->fifotrig)
            outb(inb(p->port + RT_COM_FCR) | FCR_INPUT_FIFO_RESET, p->port + RT_COM_FCR);
        rt_com_irq_on(state);
        if (p->mode & RT_COM_HW_FLOW) {
            /* with hardware flow set RTS */
            p->mcr |= MCR_RTS;
            outb(p->mcr, p->port + RT_COM_MCR);
        }

        return(0);
    }
}

/** Clear output buffer.
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @return       0 if all right, negative error conditions otherwise
 *
 * @author Roberto Finazzi, Jochen Kupper
 * @version 2000/03/12
 */
int rt_com_clear_output(unsigned int ttyS)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        if (0 >= (rt_com_table[ttyS]).used) {
            return(-EPERM);
        }
        else {
            struct rt_buf_struct    *b = &(p->obuf);
            long                    state;
            rt_com_irq_off(state);
            p->ier &= ~IER_ETBEI;
            outb(p->ier, p->port | RT_COM_IER);
            b->tail = b->head;
            if (p->fifotrig)
                outb(inb(p->port + RT_COM_FCR) | FCR_OUTPUT_FIFO_RESET, p->port + RT_COM_FCR);
            rt_com_irq_on(state);
            return(0);
        }
    }
}

/** Set functioning mode.
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @param mode   functioning mode 
 * @return       0 if all right, negative on error
 *
 * @author Roberto Finazzi, Jochen Kupper
 * @version 2000/03/12
 */
int rt_com_set_mode(unsigned int ttyS, int mode)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        if (0 >= p->used) {
            return(-EPERM);
        }
        else {
            p->mode = mode;
            if (p->used & RT_COM_PORT_SETUP) {
                /* setup done */
                if (mode == RT_COM_NO_HAND_SHAKE) {
                    /* if no hw signals disable modem interrupts */
                    p->ier &= ~IER_EDSSI;
                }
                else {
                    /* else enable it */
                    p->ier |= IER_EDSSI;
                }

                outb(p->ier, p->port + RT_COM_IER);
            }

            return(0);
        }
    }
}

/** Set receiver fifo trigger level.
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @param fifotrig   receiver fifo trigger level 
 * @return           0 if all right, negative on error
 *
 * @author Roberto Finazzi, Jochen Kupper
 * @version 2000/03/12
 */
int rt_com_set_fifotrig(unsigned int ttyS, int fifotrig)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        p->fifotrig = fifotrig;
        if (p->used & RT_COM_PORT_SETUP) {
            /* setup done */
            if (p->fifotrig)
                outb(FCR_FIFO_ENABLE | p->fifotrig, p->port + RT_COM_FCR);  // enable fifo
            else
                outb(0, p->port + RT_COM_FCR);  // disable fifo	
        }
    }

    return(0);
}

/** Set output signal for modem control (DTR, RTS).
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @param signal Output signals: RT_COM_DTR or RT_COM_RTS.
 * @param value  Status: 0 or 1. 
 * @return       0 if all right, negative error code otherwise
 *
 * @author Roberto Finazzi, Jochen Kupper
 * @version 2000/03/12 */
int rt_com_write_modem(unsigned int ttyS, int signal, int value)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else if (value &~0x01) {
        return(-EINVAL);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        if (0 >= p->used) {
            return(-EPERM);
        }
        else {
            if (value)
                p->mcr |= signal;
            else
                p->mcr &= ~signal;
            outb(p->mcr, p->port + RT_COM_MCR);
            return(0);
        }
    }
}

/** Read input signal from modem (CTS, DSR, RI, DCD).
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @param signal Input signals: RT_COM_CTS, RT_COM_DSR, RT_COM_RI, RT_COM_DCD
 *               or any combination.  
 * @return       input signal status; that is the bitwise-OR of the signal
 *               argument and the MSR register.
 *
 * @author Roberto Finazzi, Jochen Kupper
 * @version 2000/03/12 */
int rt_com_read_modem(unsigned int ttyS, int signal)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else if (signal & 0xf) {
        return(-EINVAL);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        if (0 >= p->used) {
            return(-EPERM);
        }
        else {
            return(inb(p->port + RT_COM_MSR) & signal);
        }
    }
}

/** Return last error detected.
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @return       bit 0 :1 = Input buffer overflow
 *               bit 1 :1 = Receive data overrun 
 *               bit 2 :1 = Parity error 
 *               bit 3 :1 = Framing error
 *               bit 4 :1 = Break detected 
 *
 * @author Roberto Finazzi
 * @version 2000/03/12
 */
int rt_com_error(unsigned int ttyS)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        int                     tmp = p->error;
        p->error = 0;
        return(tmp);
    }
}

/** Write data to a line.
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @param buffer Address of data.
 * @param count  Number of bytes to write. If negative, send byte only if 
 *               possible to send them all together.
 * @return       Number of bytes not written. Negative values are error
 *               conditions.
 *
 * @author Jens Michaelsen, Jochen Kupper, Giuseppe Renoldi
 * @version 2000/03/12 */
int rt_com_write(unsigned int ttyS, char *buffer, int count)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        if (!(p->used & RT_COM_PORT_SETUP)) {
            return(-EPERM);
        }
        else {
            struct rt_buf_struct    *b = &(p->obuf);
            long                    state;
            int                     bytestosend;
            if (count == 0)
                return(0);
            bytestosend = rt_com_buffer_free(b->head, b->tail);
            if (count < 0) {
                count = -count;
                if (count > bytestosend)
                    return(count);
                bytestosend = count;
            }
            else {
                if (count <= bytestosend)
                    bytestosend = count;
            }

            rt_com_irq_off(state);  
            while (bytestosend-- > 0) {
                /* put byte into buffer, move pointers to next elements */
                b->buf[b->head++] = *buffer++;
                if (b->head >= rt_com_buffersize)
                    b->head = 0;
                --count;
            }

            p->ier |= IER_ETBEI;
            outb(p->ier, p->port + RT_COM_IER);
            rt_com_irq_on(state);
            return(count);
        }
    }
}

/** Read data we got from a line.
 *
 * @param ttyS   Port to use corresponding to internal numbering scheme.
 * @param buffer Address of data buffer. Needs to be of size > cnt !
 * @param count  Number of bytes to read.
 * @return       Number of bytes actually read.
 *
 * @author Jens Michaelsen, Jochen Kupper
 * @version 2000/03/17 */
int rt_com_read(unsigned int ttyS, char *buffer, int count)
{
    if (0 > count) {
        return(-EINVAL);
    }
    else if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        if (!(p->used & RT_COM_PORT_SETUP)) {
            return(-EPERM);
        }
        else {
            struct rt_buf_struct    *b = &(p->ibuf);
            int                     done = 0;
            long                    state;
            rt_com_irq_off(state);
            while ((b->head != b->tail) && (--count >= 0)) {
                done++;
                *buffer++ = b->buf[b->tail++];
                b->tail &= (RT_COM_BUF_SIZ - 1);
            }

            rt_com_irq_on(state);
            if ((p->mode & RT_COM_HW_FLOW) && (rt_com_buffer_free(b->head, b->tail) > RT_COM_BUF_HI)) {
                /* if hardware flow and enough free space on input buffer
				   then set RTS */
                p->mcr |= MCR_RTS;
                outb(p->mcr, p->port + RT_COM_MCR);
            }

            return(done);
        }
    }
}

/** Get first byte from the write buffer.
 *
 * @param p  rt_com_struct of the line we are writing to.
 * @param c  Address to put the char in.
 * @return   Number of characters we actually got.
 * 
 * @author Jens Michaelsen, Jochen Kupper
 * @version 1999/10/01
 */
static inline int rt_com_irq_get(struct rt_com_struct *p, unsigned char *c)
{
    struct rt_buf_struct    *b = &(p->obuf);
    if (b->head != b->tail) {
        *c = b->buf[b->tail++];
        b->tail &= (RT_COM_BUF_SIZ - 1);
        return(1);
    }

    return(0);
}

/** Concatenate a byte to the read buffer.
 *
 * @param p   rt_com_struct of the line we are writing to.
 * @param ch  Byte to put into buffer.
 *
 * @author Jens Michaelsen, Jochen Kupper
 * @version 1999/07/20 */
static inline void rt_com_irq_put(struct rt_com_struct *p, unsigned char ch)
{
    struct rt_buf_struct    *b = &(p->ibuf);
    b->buf[b->head++] = ch;
    b->head &= (RT_COM_BUF_SIZ - 1);
}

/** Real interrupt handler.
 *
 * This one is called by the registered ISRs and does the actual work.
 *
 * @param ttyS  Port to use corresponding to internal numbering scheme.
 *
 * @author Jens Michaelsen, Jochen Kupper, Hua Mao, Roberto Finazzi
 * @version 2000/03/17 */
static inline int rt_com_isr(unsigned int ttyS)
{
    struct rt_com_struct    *p = &(rt_com_table[ttyS]);
    struct rt_buf_struct    *b = &(p->ibuf);
    unsigned int            base = p->port;
    int                     buff, data_to_tx;
    int                     loop = 4;
    int                     toFifo = 16;
    unsigned char           data, msr, lsr, iir;

    do {
        //iir=inb(base + RT_COM_IIR);
        //rt_printk("iir=0x%02x\n",iir);
        /* get available data from port */
        lsr = inb(base + RT_COM_LSR);
        if (0x1e & lsr)
            p->error = lsr & 0x1e;
        while (LSR_DATA_READY & lsr) {
            data = inb(base + RT_COM_RXB);

            //rt_printk("[%02x<- ",data);	
            rt_com_irq_put(p, data);
            lsr = inb(base + RT_COM_LSR);
            if (0x1e & lsr)
                p->error = 0x1e & lsr;
        }

        /* controls on buffer full and RTS clear on hardware flow
           control */
        buff = rt_com_buffer_free(b->head, b->tail);
        if (buff < RT_COM_BUF_FULL)
            p->error = RT_COM_BUFFER_FULL;

        if ((p->mode & RT_COM_HW_FLOW) && (buff < RT_COM_BUF_LOW)) {
            p->mcr &= ~MCR_RTS;
            outb(p->mcr, p->port + RT_COM_MCR);
        }

        /* if possible, put data to port */
        msr = inb(base + RT_COM_MSR);
        if
        (
            (p->mode == RT_COM_NO_HAND_SHAKE) ||
            ((p->mode & RT_COM_DSR_ON_TX) && (MSR_DSR & msr) && (p->mode & RT_COM_HW_FLOW) && (MSR_CTS & msr))
        ) {
            /* (DSR && (CTS || Mode==no hw flow)) or Mode==no handshake */
            // if (THRE==1) i.e. transmitter empty
            if ((lsr = inb(base + RT_COM_LSR)) & LSR_THRE) {
                // if there are data to transmit
                if ((data_to_tx = rt_com_irq_get(p, &data)) != 0) {
                    do {
                        //rt_printk("->%02x] ",data);	
                        outb(data, base + RT_COM_TXB);
                    } while ((--toFifo > 0) && (data_to_tx = rt_com_irq_get(p, &data) != 0));
                }

                if (!data_to_tx) {
                    /* no data in output buffer, disable Transmitter
	               	   Holding Register Empty Interrupt */
                    p->ier &= ~IER_ETBEI;
                    outb(p->ier, base + RT_COM_IER);
                }
            }
        }

        /* check the low nibble of IIR wheather there is another pending
           interrupt.  bit 0 = 0 if interrupt pending, bits 1,2,3 
		   are the interrupt ID */
        iir = inb(base + RT_COM_IIR);
    } while (((iir & 0x0F) != 1) && (--loop > 0));

#if defined RTLINUX_V2
    rtl_hard_enable_irq(p->irq);
#endif
    return 0;
}

/** Interrupt Service Routines
 *
 * These are the Interrupt Service Routines to be registered with the
 * OS. They simply call the genereric interrupt handler for the
 * current port to do the work.
 *
 * @author Jens Michaelsen, Jochen Kupper, Hua Mao
 * @version 1999/11/11 */
static void rt_com0_isr(void)
{
    //rt_printk("rt_com0_isr\n");
    rt_com_isr(0);
}

static void rt_com1_isr(void)
{
    //rt_printk("rt_com1_isr\n");
    rt_com_isr(1);
}

/** Setup one port
 *
 * Calls from init_module + cleanup_module have baud == 0; in these
 * cases we only do some cleanup.
 *
 * To allocate a port, give usefull setup parameter, to deallocate
 * give negative baud.
 *
 * @param ttyS       Number corresponding to internal port numbering scheme.
 *                   This is esp. the index of the rt_com_table to use.
 * @param baud       Data transmission rate to use [Byte/s]. If negative,
 *                   deallocate port instead. Called with baud == 0 from
 *                   init_module for basic initialization. Needs to be called
 *                   by user-task again before use !
 * @param mode       see rt_com_set_mode docs for now
 * @param parity     Parity for transmission protocol.
 *                   (RT_COM_PARITY_EVEN, RT_COM_PARITY_ODD, RT_COM_PARITY_NONE)
 * @param stopbits   Number of stopbits to use. 1 gives you one stopbit, 2
 *                   actually gives really two stopbits for wordlengths of
 *                   6 - 8 bit, but 1.5 stopbits for a wordlength of 5 bits.
 * @param wordlength Number of bits per word (5 - 8 bits).
 * @param fifotrig   if <0 set trigger fifo using default value set
 *                   in rt_com_table[], otherwise set trigger fifo accordingly
 *                   to the parameter
 * @return           0       - all right
 *                   -ENODEV - no entry for that ttyS in rt_com_table
 *                   -EPERM  - get hardware resources first (the port needs to
 *                             be setup hardware-wise first, that means you have
 *                             to specify a positive used flag at compile time
 *                             or call rt_com_set_hwparm first.)
 *
 * @author Jens Michaelsen, Jochen Kupper, Roberto Finazzi
 * @version 2000/03/12 */
int rt_com_setup
(
    unsigned int    ttyS,
    int             baud,
    int             mode,
    unsigned int    parity,
    unsigned int    stopbits,
    unsigned int    wordlength,
    int             fifotrig			
)
{
    if (ttyS >= RT_COM_CNT) {
        return(-ENODEV);
    }
    else {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        if (0 == p->used) {
            /*  */
            return(-EPERM);
        }
        else {
            unsigned int    base = p->port;

            /* Stop everything, set DLAB */
            outb(0x00, base + RT_COM_IER);
            outb(0x80, base + RT_COM_LCR);

            /* clear irqs */
            inb(base + RT_COM_IIR);
            inb(base + RT_COM_LSR);
            inb(base + RT_COM_RXB);
            inb(base + RT_COM_MSR);

            /* initialize error code */
            p->error = 0;

            /* if 0 == baud, nothing else to do ! */
            if (baud == 0)
                return(0);

            if (0 > baud) {
                /* free the port */
                /* disable interrupts */
                outb(0, base + RT_COM_IER);
                //MOD_DEC_USE_COUNT;  Hm, IgH
                return(0);
            }
            else {
                /* allocate and set-up the port */
                unsigned int    divider = p->baud_base / baud;

                //MOD_INC_USE_COUNT; Hm, IgH

                /* set transfer rate */
                outb(divider % 256, base + RT_COM_DLL);
                outb(divider / 256, base + RT_COM_DLM);

                /* bits 3,4 + 5 determine parity, mask away anything else */
                parity &= 0x38;

                /* set transmission parameters and clear DLAB */
                outb((wordlength - 5) | ((stopbits - 1) << 2) | parity, base + RT_COM_LCR);

                /* set-up MCR value and write it */
                p->mcr = MCR_DTR | MCR_RTS | MCR_OUT1 | MCR_OUT2;
                outb(p->mcr, base + RT_COM_MCR);

                /* set-up IER value and write it */
                p->mode = mode;
                if (p->mode == RT_COM_NO_HAND_SHAKE) {
                    /* if no handshaking signals enable only receiver interrupts  */
                    p->ier = IER_ERBI | IER_ELSI;
                }
                else {
                    /* enable receiver and modem interrupts */
                    p->ier = IER_ERBI | IER_ELSI | IER_EDSSI;
                }

                outb(p->ier, base + RT_COM_IER);
		if (fifotrig>=0)
		    p->fifotrig = fifotrig;	
                outb(FCR_FIFO_ENABLE | p->fifotrig, base + RT_COM_FCR); // enable fifo
                /* mark setup done */
                p->used |= RT_COM_PORT_SETUP;
                return(0);
            }
        }

        return(0);
    }
}

/** Set hardware parameter for a specific port.
 *
 * Change port address and irq setting for a specified port. The port
 * must have an entry in rt_com_table beforehand.
 *
 * To allow the specification of additional ports we would need to
 * dynamically allocate memory, that's not really feasible within a
 * real-time context, although we could preallocate a few entries in
 * init_module. However, it doesn't make too much sense, as you can
 * specify all ports that really exist (in hardware) at compile time
 * and enable only these you want to use.
 *
 * @param ttyS   Port to use; corresponding to internal numbering scheme.
 * @param port   port address, if zero, use standard value from rt_com_table
 * @param irq    irq address, if zero, use standard value from rt_com_table
 * @return        0       everything all right,
 *               -ENODEV  no entry in rt_com_table for that device,
 *               -EBUSY   port-region is used already.
 *
 * @author Roberto Finazzi, Jochen Kupper
 * @version 2000/03/10 */
int rt_com_hwsetup(unsigned int ttyS, int port, int irq)
{
    if (ttyS < RT_COM_CNT) {
        struct rt_com_struct    *p = &(rt_com_table[ttyS]);
        if (0 == p->used) {
            if (0 != port)
                p->port = port;
            if (0 != irq)
                p->irq = irq;
            if (-EBUSY == check_region(p->port, 8)) {
                return(-EBUSY);
            }

            request_region(p->port, 8, RT_COM_NAME);
            rt_com_request_irq(p->irq, p->isr);
            p->used = 1;
            rt_com_setup(ttyS, p->baud, p->mode, p->parity, p->stopbits, p->wordlength, p->fifotrig);
            return(0);
        }
        else {
            if (port >= 0)
                return(-EBUSY);
            rt_com_setup(ttyS, 0, 0, 0, 0, 0, 0);
            rt_com_free_irq(p->irq);
            release_region(p->port, 8);
            p->used = 0;
            return(0);
        }
    }

    return(-ENODEV);
}

/** Initialization
 *
 * For all ports that have a used flag greater than 0, request port
 * memory and register ISRs. If we cannot get the memory of all these
 * ports, release all already requested ports and return an error.
 *
 * @return Success status, zero on success. With a non-zero return
 * value of this routine, insmod will fail to load the module !
 *
 * @author Jochen Kupper, Hua Mao, Roberto Finazzi
 * @version 2000/03/10 */
//int init_module(void)   //Hm, IgH
int init_aip_com(void)
{
    int             errorcode = 0;
    unsigned int    i, j;

    printk(KERN_INFO "rt_com: Loading real-time serial port driver (version "VERSION ").\n");

    for (i = 0; i < RT_COM_CNT; i++) {
        struct rt_com_struct    *p = &(rt_com_table[i]);

        // if used set default values
        if (p->used > 0) {
	    printk(KERN_WARNING "RS232 testing %d\n",i);
            if (-EBUSY == check_region(p->port, 8)) {
                errorcode = -EBUSY;
                printk(KERN_WARNING "rt_com: error %d: cannot request port region %x\n", errorcode, p->port);
                break;
            }

            request_region(p->port, 8, "rt_com");
            rt_com_request_irq(p->irq, p->isr);
	    printk(KERN_WARNING "RS232 Request IRQ: %d\n",p->irq);
            rt_com_setup(i, p->baud, p->mode, p->parity, p->stopbits, p->wordlength, p->fifotrig);
        }
    }

    if (0 != errorcode) {
        printk(KERN_WARNING "rt_com: giving up.\n");
        for (j = 0; j < i; j++) {
            struct rt_com_struct    *p = &(rt_com_table[j]);
            if (0 < p->used) {
                rt_com_free_irq(p->irq);
                release_region(p->port, 8);
            }
        }
    }
    else {
        printk(KERN_INFO "rt_com: sucessfully loaded.\n");
    }

    return(errorcode);
}

/** Cleanup
 *
 * Unregister ISR and releases memory for all ports
 *
 * @author Jochen Kupper
 * @version 1999/10/01 */
//void cleanup_module(void)  Hm, IgH
void cleanup_aip_com(void)
{
    unsigned int i;
    for (i = 0; i < RT_COM_CNT; i++) {
        struct rt_com_struct    *p = &(rt_com_table[i]);
        if (0 < p->used) {
            rt_com_free_irq(p->irq);
            rt_com_setup(i, 0, 0, 0, 0, 0, 0);
            release_region(p->port, 8);
        }
    }

    printk(KERN_INFO "rt_com: unloaded.\n");
}

/*
EXPORT_SYMBOL(rt_com_buffersize);
EXPORT_SYMBOL(rt_com_clear_input);
EXPORT_SYMBOL(rt_com_clear_output);
EXPORT_SYMBOL(rt_com_error);
EXPORT_SYMBOL(rt_com_hwsetup);
EXPORT_SYMBOL(rt_com_read);
EXPORT_SYMBOL(rt_com_read_modem);
EXPORT_SYMBOL(rt_com_setup);
EXPORT_SYMBOL(rt_com_table);
EXPORT_SYMBOL(rt_com_write);
EXPORT_SYMBOL(rt_com_write_modem);
EXPORT_SYMBOL(rt_com_set_mode);
EXPORT_SYMBOL(rt_com_set_fifotrig);
*/

/**
 * Local Variables:
 * mode: C
 * c-file-style: "Stroustrup"
 * End:
 */
