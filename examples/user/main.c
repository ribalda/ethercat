/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include "ecrt.h"

/****************************************************************************/

int main(int argc, char **argv)
{
	ec_master_t *master;
	ec_domain_t *domain;
    
    master = ecrt_request_master(0);
	if (!master)
		return -1;

    domain = ecrt_master_create_domain(master);
    if (!domain)
        return -1;

	while (1) {
		sleep(1);
	}

	return 0;
}

/****************************************************************************/
