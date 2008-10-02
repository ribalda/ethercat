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
	ec_slave_config_t *sc;
    
    master = ecrt_request_master(0);
	if (!master)
		return -1;

    domain = ecrt_master_create_domain(master);
    if (!domain)
        return -1;

    sc = ecrt_master_slave_config(master, 0, 0, 0x00000002, 0x044C2C52);
    if (!sc)
        return -1;

	while (1) {
		sleep(1);
	}

	return 0;
}

/****************************************************************************/
