/******************************************************************************
*
*           msr_io.c
*
*           Debugging über Serielle Schnittstelle
*           
*           Autoren: Wilhelm Hagemeister
*
*           $LastChangedDate: 2005-09-16 17:45:46 +0200 (Fri, 16 Sep 2005) $
*           $Author: hm $
*
*           (C) Copyright IgH 2005
*           Ingenieurgemeinschaft IgH
*           Heinz-Bäcker Str. 34
*           D-45356 Essen
*           Tel.: +49 201/61 99 31
*           Fax.: +49 201/61 98 36
*           E-mail: sp@igh-essen.com
*
* /bin/setserial /dev/ttyS0 uart none
* /bin/setserial /dev/ttyS1 uart none
*
*
******************************************************************************/

/*--Includes-----------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/tqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#include "aip_com.h"
#include "rs232dbg.h"
#include <rtai.h>

spinlock_t rs232wlock;


void SDBG_print(const char *format, ...) 
{
    va_list argptr;  
    static char buf[1024];                               
    int len;
    if(format != NULL) {
	va_start(argptr,format); 
	len = vsnprintf(buf, sizeof(buf), format, argptr);
        if (len > 0 && buf[len - 1] == '\n') len--; // fp
	rt_com_write(0,buf,len);
	rt_com_write(0,"\r\n",2);
	va_end(argptr); 
    }
}

/*
void SDBG_print(unsigned char *buf)
{
    static int counter = 0;
    unsigned char cbuf[20];
    unsigned long flags;

//    flags = rt_spin_lock_irqsave (&rs232wlock);

    sprintf(cbuf,"%0d -- ",counter);
    rt_com_write(0,cbuf,strlen(cbuf));
    rt_com_write(0,buf,strlen(buf));
    rt_com_write(0,"\r\n",2);
    counter++;
    counter %= 10; //did we miss frames ??
//    rt_spin_unlock_irqrestore (&rs232wlock,flags);
}
*/


/*
*******************************************************************************
*
* Function: msr_init
*
* Beschreibung: MSR initialisieren
*
* Parameter:
*
* Rückgabe: 
*               
* Status: exp
*
*******************************************************************************
*/

int msr_init(void)
{   
    spin_lock_init (&rs2323wlock);

    printk("starting RS232 Setup\n");
    if(init_aip_com())
    {
	printk("RS232 Setup failed\n");
	return -1;
    }

    SDBG_print("Hello Word, Serial Debugger started...");
    mdelay(10);
    return 0;
}

/*
*******************************************************************************
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
*******************************************************************************
*/

void msr_io_cleanup(void)
{
  cleanup_aip_com();
}

/*---Treiber-Einsprungspunkte etc.-------------------------------------------*/

MODULE_LICENSE("GPL");

module_init(msr_init);
module_exit(msr_io_cleanup);

/*---Ende--------------------------------------------------------------------*/
