/**************************************************************************************************
*
*                          msr_io.h
*
*           Verwaltung der IO-Karten

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
*           $RCSfile: msr_io.h,v $
*           $Revision: 1.5 $
*           $Author: ha $
*           $Date: 2005/06/24 20:08:15 $
*           $State: Exp $
*
*
*           $Log: msr_io.h,v $
*           Revision 1.5  2005/06/24 20:08:15  ha
*           *** empty log message ***
*
*           Revision 1.4  2005/06/24 17:39:05  ha
*           *** empty log message ***
*
*           Revision 1.3  2005/02/28 17:11:48  hm
*           *** empty log message ***
*
*           Revision 1.1  2005/02/10 16:34:24  hm
*           Initial revision
*
*           Revision 1.4  2004/12/21 22:03:54  hm
*           *** empty log message ***
*
*           Revision 1.3  2004/12/16 15:44:01  hm
*           *** empty log message ***
*
*           Revision 1.2  2004/12/01 17:07:49  hm
*           *** empty log message ***
*
*           Revision 1.1  2004/11/26 15:14:21  hm
*           Initial revision
*
*           Revision 1.1  2004/11/01 11:05:20  hm
*           Initial revision
*
*           Revision 1.1  2004/10/21 12:09:23  hm
*           Initial revision
*
*           Revision 1.3  2004/09/21 18:10:58  hm
*           *** empty log message ***
*
*           Revision 1.2  2004/07/22 17:28:02  hm
*           *** empty log message ***
*
*           Revision 1.1  2004/06/21 08:46:52  hm
*           Initial revision
*
*           Revision 1.4  2004/06/02 20:38:42  hm
*           *** empty log message ***
*
*           Revision 1.3  2004/06/02 20:38:18  hm
*           *** empty log message ***
*
*           Revision 1.2  2004/06/02 12:15:17  hm
*           *** empty log message ***
*
*           Revision 1.5  2003/02/20 17:33:37  hm
*           *** empty log message ***
*
*           Revision 1.4  2003/02/14 18:17:28  hm
*           *** empty log message ***
*
*           Revision 1.3  2003/02/13 17:11:12  hm
*           *** empty log message ***
*
*           Revision 1.2  2003/01/30 15:05:58  hm
*           *** empty log message ***
*
*           Revision 1.1  2003/01/24 20:40:09  hm
*           Initial revision
*
*           Revision 1.1  2003/01/22 15:55:40  hm
*           Initial revision
*
*           Revision 1.1  2002/08/13 16:26:27  hm
*           Initial revision
*
*           Revision 1.4  2002/07/04 13:34:27  sp
*           *** empty log message ***
*
*           Revision 1.3  2002/07/04 12:08:34  sp
*           *** empty log message ***
*
*           Revision 1.2  2002/07/04 08:44:19  sp
*           Änderung des Autors :) und des Datums
*
*           Revision 1.1  2002/07/04 08:25:26  sp
*           Initial revision
*
*
*
*
*
*
*
**************************************************************************************************/

/*--Schutz vor mehrfachem includieren------------------------------------------------------------*/

#ifndef _MSR_IO_H_
#define _MSR_IO_H_

/*--includes-------------------------------------------------------------------------------------*/

//#include "msr_control.h"

/*--defines--------------------------------------------------------------------------------------*/


struct cif_in_t {			/* Von Feld nach dSPACE */
	uint8_t CIM_stat;
	uint8_t P101[91];
	uint8_t P201[72];
	uint8_t P301[72];
} __attribute__ ((packed));

struct cif_out_t {			/* Von dSPACE zum Feld */
	uint8_t WatchDog;
	uint8_t P101[39];
	uint8_t P201[32];
	uint8_t P301[32];
} __attribute__ ((packed));

/*--external functions---------------------------------------------------------------------------*/

/*--external data--------------------------------------------------------------------------------*/

/*--public data----------------------------------------------------------------------------------*/

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
int msr_io_init();

/*
***************************************************************************************************
*
* Function: msr_io_register
*
* Beschreibung: Kanaele oder Parameter registrieren
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/

int msr_io_register();

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
int msr_io_write();

/*
***************************************************************************************************
*
* Function: msr_io_write
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
int msr_io_read();

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
void msr_io_cleanup();

#endif


