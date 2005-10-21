/**************************************************************************************************
*
*                          msr_io.c
*
*           Verwaltung der IO-Karten
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
*           E-mail: sp@igh-essen.com
*
*
*           $RCSfile: msr_io.c,v $
*           $Revision: 1.9 $
*           $Author: ha $
*           $Date: 2005/06/24 20:06:56 $
*           $State: Exp $
*
*
*           $Log: msr_io.c,v $
*           Revision 1.9  2005/06/24 20:06:56  ha
*           *** empty log message ***
*
*           Revision 1.8  2005/06/24 17:39:05  ha
*           *** empty log message ***
*
*
*
*
*
*
**************************************************************************************************/


/*--includes-------------------------------------------------------------------------------------*/

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/delay.h> 	/* mdelay() */
#include <linux/spinlock.h> 
#include <linux/param.h>	/* HZ */
#include <linux/sched.h> 	/* jiffies */
#include <linux/fs.h>     /* everything... */
#include <rtai_fifos.h>

#include "msr_io.h"

#include <msr_messages.h>


#include "aim_globals.h"

spinlock_t data_lock = SPIN_LOCK_UNLOCKED;

#include "cif-rtai-io.h"

/*--defines--------------------------------------------------------------------------------------*/


/*--external functions---------------------------------------------------------------------------*/


/*--external data--------------------------------------------------------------------------------*/


/*--public data----------------------------------------------------------------------------------*/

#define PB_CARDS 4
struct {
	unsigned int fd;
	unsigned int timestamp;
	unsigned int fault;
	unsigned int active;
	void *in_buf;
	void *out_buf;
	size_t in_buf_len;
	size_t out_buf_len;

	unsigned int reset_timeout;
} card[PB_CARDS];


/*
***************************************************************************************************
*
* Function: msr_io_init
*
* Beschreibung: Initialisieren der I/O-Karten
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/
void x_PB_io(unsigned long card_no) {
	int rv = 0;
	unsigned int flags;

	spin_lock_irqsave(&data_lock, flags);

	switch (card_no) {
	case 0:
		rv = cif_exchange_io(card[0].fd,card[0].in_buf,card[0].out_buf);
		if (!rv)
			card[0].timestamp = jiffies;
		break;
	case 1:
		rv = cif_exchange_io(card[1].fd,card[1].in_buf,card[1].out_buf);
//		rv = cif_read_io(card[1].fd,card[1].in_buf);
//		IMO.to_dSPACE.P101.HX_Stat = 51;
//		IMO.from_dSPACE.P101.HX_Control |= IMO.to_dSPACE.P101.HX_Stat<<4;
//		rv += cif_write_io(card[1].fd,card[1].out_buf);
		if (!rv)
			card[1].timestamp = jiffies;
		break;
	/*
	case 2:
		rv = cif_exchange_io(card[2].fd,&cif_in.P201,&cif_out.P201);
		break;
	*/
	case 3:
		rv = cif_exchange_io(card[3].fd,card[3].in_buf,card[3].out_buf);
		if (!rv)
			card[3].timestamp = jiffies;
		break;
	}

	if (rv) {
			msr_print_error("Error during exchange_io %i %i", 
					card_no, rv );
	}

	spin_unlock_irqrestore(&data_lock, flags);

}

int msr_io_init()
{   
    int rv;

    memset(card, 0, sizeof(card));

#define FIFO_BUF 10000

    if ((rv = rtf_create(0, FIFO_BUF)) < 0) {
	    msr_print_error("Could not open FIFO %i", rv);
	    return -1;
    }

#ifndef _SIMULATION
/*
    card[0].in_buf_len = sizeof(IMO.from_dSPACE);
    card[0].out_buf_len = sizeof(IMO.to_dSPACE);
    card[0].in_buf = &IMO.from_dSPACE;
    card[0].out_buf = &IMO.to_dSPACE;
    card[0].active = 1;
    if (!(card[0].fd = cif_open_card(0, card[0].in_buf_len,
				    card[0].out_buf_len, x_PB_io, 0))) {
	msr_print_error("Cannot open CIF card PB01");
	return -1;
    }
 

    card[1].in_buf_len = sizeof(IMO.to_dSPACE.P101);
    card[1].out_buf_len = sizeof(IMO.from_dSPACE.P101);
    card[1].in_buf = &IMO.to_dSPACE.P101;
    card[1].out_buf = &IMO.from_dSPACE.P101;
    card[1].active = 1;
    if (!(card[1].fd = cif_open_card(1, card[1].in_buf_len, 
				    card[1].out_buf_len, x_PB_io, 1))) {
	msr_print_error("Cannot open CIF card P101");
	return -1;
    }
 
    card[2].in_buf_len = sizeof(IMO.to_dSPACE.P201);
    card[2].out_buf_len = sizeof(IMO.from_dSPACE.P201);
    card[2].in_buf = &IMO.to_dSPACE.P201;
    card[2].out_buf = &IMO.from_dSPACE.P201;
    if (!(card[2].fd = cif_open_card(2, card[2].in_buf_len, 
				    card[2].out_buf_len, x_PB_io, 2))) {
	msr_print_error("Cannot open CIF card P201");
	return -1;
    }
 
*/
    card[3].in_buf_len = sizeof(dSPACE.in);
    card[3].out_buf_len = sizeof(dSPACE.out);
    card[3].in_buf = &dSPACE.in;
    card[3].out_buf = &dSPACE.out;
    card[3].active = 1;
    if (!(card[3].fd = cif_open_card(0,  card[3].in_buf_len, 
				    card[3].out_buf_len, x_PB_io,3))) {
	msr_print_error("Cannot open CIF card P301");
	return -1;
    }
 
    //msr_reg_chk_failure(&int_cif_io_fail,TINT,T_CHK_HIGH,0,T_CRIT,"CIF Card was not ready to exchange data");


#endif

    return 0;
}

/*
***************************************************************************************************
*
* Function: msr_io_register
*
* Beschreibung: Rohdaten als Kanaele registrieren
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/

int msr_io_register()
{

#ifndef _SIMULATION

#endif

    return 0;
}


/*
***************************************************************************************************
*
* Function: msr_io_write
*
* Beschreibung: Schreiben der Werte
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/
int msr_io_write()
{
    static int return_value = 0;
    int rv;
    int i = 0;
    unsigned int flags;
    unsigned int com_check_timestamp = 0;
    static int COM_Up = 1;

    if (jiffies - com_check_timestamp > HZ/20) {

	    if ( rtf_put_if(0,&IMO,sizeof(IMO)) != sizeof(IMO)) {
		    //msr_print_error("Could not output data");
	    }

	    com_check_timestamp = jiffies;

	    spin_lock_irqsave(&data_lock, flags);
	    for ( i=0; i < PB_CARDS; i++) {
		    // Ignore inactive and cards that already have a fault
		    if (!card[i].active || card[i].fault)
			    continue;

		    // For active cards, check timestamp value. Mark card
		    // as faulty if there was no data exchange in the last
		    // 50ms
		    if (jiffies - card[i].timestamp > HZ/20) {
			    COM_Up = 0;
			    card[i].fault = 1;
			    card[i].reset_timeout = jiffies;
			    msr_print_error("Card %i timed out", i);
		    }

	    }
    
	    spin_unlock_irqrestore(&data_lock, flags);
    
	    for ( i = 0; i < PB_CARDS; i++ ) {
		    if (!card[i].active || (card[i].active && !card[i].fault))
			    continue;

		    switch (card[i].fault) {
		    case 1:
			    rv = cif_write_io(card[i].fd,card[i].out_buf);

			    if (!rv) {
				    msr_print_error("Card %i online", i);
				    card[i].fault = 0;
				    card[i].timestamp = jiffies;
				    break;
			    }

			    msr_print_error("rv of cif_write_io(%i) = %i", 
					    i, rv);

			    card[i].fault = 2;
			    cif_set_host_state(card[i].fd,0);
			    card[i].reset_timeout = jiffies;

		    case 2:
			    if (cif_card_ready(card[i].fd)) {
				    cif_set_host_state(card[i].fd,1);
				    card[i].fault = 0;
				    break;
			    }
			    if (jiffies < card[i].reset_timeout)
				    break;

			    rv = cif_reset_card(card[i].fd,10,1);
			    msr_print_error("rv of cif_reset_card(%i) = %i", 
					    i, rv);

			    // Reset again in 10 seconds
			    card[i].reset_timeout += 10*HZ;
		    }
	    }
    }

    if (COM_Up)
	    IMO.to_dSPACE.Status = IMO.from_dSPACE.WatchDog;

//    if (return_value)
//	int_cif_io_fail = 1;

    return return_value;
}

/*
***************************************************************************************************
*
* Function: msr_io_read
*
* Beschreibung: Lesen der Werte
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/
int msr_io_read()
{
    int return_value = 0;

#ifndef _SIMULATION

    int_cif_io_fail = 0;
    /*
    return_value = cif_exchange_io(fd_PB01,
		    &cif_out,&cif_in,
		    sizeof(cif_out),sizeof(cif_in)
		    );
		    */
/*     if (return_value) */
/* 	int_cif_io_fail = 1; */
//    printk("%i\n", return_value);

#endif
    return return_value;
}


/*
***************************************************************************************************
*
* Function: msr_io_cleanup
*
* Beschreibung: Aufräumen
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/
void msr_io_cleanup()
{
/*
    cif_set_host_state(card[0].fd,0);
    cif_close_card(card[0].fd);

    cif_set_host_state(card[1].fd,0);
    cif_close_card(card[1].fd);

    cif_set_host_state(card[2].fd,0);
    cif_close_card(card[2].fd);


*/
    cif_set_host_state(card[3].fd,0);
    cif_close_card(card[3].fd);

    rtf_destroy(0);
}


