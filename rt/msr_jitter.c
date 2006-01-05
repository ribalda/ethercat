/**************************************************************************************************
*
*                          msr_jitter.c
*
*           
*           Autor: Wilhelm Hagemeister
*
*           (C) Copyright IgH 2002
*           Ingenieurgemeinschaft IgH
*           Heinz-BÅ‰cker Str. 34
*           D-45356 Essen
*           Tel.: +49 201/61 99 31
*           Fax.: +49 201/61 98 36
*           E-mail: hm@igh-essen.com
*
*
*           $RCSfile: msr_adeos_latency.c,v $
*           $Revision: 1.3 $
*           $Author: hm $
*           $Date: 2005/12/07 20:13:53 $
*           $State: Exp $
*
*
*           $Log: msr_adeos_latency.c,v $
*           Revision 1.3  2005/12/07 20:13:53  hm
*           *** empty log message ***
*
*           Revision 1.2  2005/12/07 15:56:13  hm
*           *** empty log message ***
*
*           Revision 1.1  2005/12/07 08:43:40  hm
*           Initial revision
*
*           Revision 1.5  2005/11/14 20:28:09  hm
*           *** empty log message ***
*
*           Revision 1.4  2005/11/13 10:34:07  hm
*           *** empty log message ***
*
*           Revision 1.3  2005/11/12 20:52:46  hm
*           *** empty log message ***
*
*           Revision 1.2  2005/11/12 20:51:27  hm
*           *** empty log message ***
*
*           Revision 1.1  2005/11/12 19:16:02  hm
*           Initial revision
*
*           Revision 1.13  2005/06/17 11:35:13  hm
*           *** empty log message ***
*
*
*
*
**************************************************************************************************/

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/msr.h> /* maschine-specific registers */
#include <linux/param.h> /* fuer HZ */

#include <msr_reg.h>
#include "msr_jitter.h"

/*--includes-------------------------------------------------------------------------------------*/
 

/*--external functions---------------------------------------------------------------------------*/

/*--external data--------------------------------------------------------------------------------*/

/*--public data----------------------------------------------------------------------------------*/

/*--local data-----------------------------------------------------------------------------------*/

#define NUMCLASSES 16

static int jittime[NUMCLASSES]={0,1,2,5,10,20,50,100,200,500,1000,2000,5000,10000,20000,50000}; //in usec
static int jitcount[NUMCLASSES];
static double jitpercent[NUMCLASSES];

static unsigned int tcount = 1;



static void msr_jit_read(void)
{
    int i;
    for(i=0;i<NUMCLASSES;i++) {
	if(tcount >100) {
	    jitpercent[i] = jitcount[i]*100.0/tcount;
	}
    }
}

void msr_jitter_init(void)
{
    msr_reg_int_list("/Taskinfo/Jitter/Classes","usec",&jittime[0],MSR_R,NUMCLASSES,NULL,NULL,NULL);
    msr_reg_int_list("/Taskinfo/Jitter/Count","",&jitcount[0],MSR_R,NUMCLASSES,NULL,NULL,NULL);
    msr_reg_dbl_list("/Taskinfo/Jitter/percent","%",&jitpercent[0],MSR_R,NUMCLASSES,NULL,NULL,&msr_jit_read);
}

/*
***************************************************************************************************
*
* Function: msr_jitter_run
*
* Beschreibung: 
*               
*
* Parameter: Zeiger auf msr_data
*
* RÅ¸ckgabe: 
*               
* Status: exp
*
***************************************************************************************************
*/

void msr_jitter_run(unsigned int hz) {

    int i,hit;
    static int firstrun = 1;
    static unsigned long k,j = 0;
    unsigned int dt,jitter;


    rdtscl(k);

    tcount++;

    //Zeitabstand zwischen zwei Interrupts in usec

    dt = ((unsigned long)(100000/HZ)*((unsigned long)(k-j)))/(current_cpu_data.loops_per_jiffy/10);

    jitter = (unsigned int)abs((int)dt-(int)1000000/hz); //jitter errechnet zum Sollabtastrate

    //in die Klassen einsortieren
    if(!firstrun) { //das erste mal nicht einsortieren
	hit = 0;
	for(i=0;i<NUMCLASSES-1;i++) {
	    if(jitter>=jittime[i] && jitter<jittime[i+1]) {
		jitcount[i]++;
		hit = 1;
		break;
	    }
	}
	if(hit == 0) //gréˆéﬂer als der letzte
	    jitcount[NUMCLASSES-1]++;

    }
    else
	firstrun = 0;

    j = k;


}

















