/******************************************************************************
 *
 * msr_jitter.c
 *
 * Autor: Wilhelm Hagemeister
 *
 * (C) Copyright IgH 2002
 * Ingenieurgemeinschaft IgH
 * Heinz-Bäcker Str. 34
 * D-45356 Essen
 * Tel.: +49 201/61 99 31
 * Fax.: +49 201/61 98 36
 * E-mail: hm@igh-essen.com
 *
 * $Id$
 *
 *****************************************************************************/

/*--Schutz vor mehrfachem includieren----------------------------------------*/

#ifndef _MSR_JITTER_H_
#define _MSR_JITTER_H_

void msr_jitter_run(unsigned int hz);
void msr_jitter_init(void);

#endif
