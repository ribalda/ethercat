/**************************************************************************************************
*
*                          msr_jitter.c
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

/*--Schutz vor mehrfachem includieren------------------------------------------------------------*/

#ifndef _MSR_JITTER_H_
#define _MSR_JITTER_H_

void msr_jitter_run(unsigned int hz);
void msr_jitter_init(void);

#endif
