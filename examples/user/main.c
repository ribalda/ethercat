/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "ecrt.h"

/****************************************************************************/

static unsigned int sig_alarms = 0;
static unsigned int user_alarms = 0;

/****************************************************************************/

void signal_handler(int signum) {
    switch (signum) {
        case SIGALRM:
            sig_alarms++;
            break;
    }
}

/****************************************************************************/

int main(int argc, char **argv)
{
	ec_master_t *master;
	ec_domain_t *domain;
	ec_slave_config_t *sc;
    struct sigaction sa;
    struct itimerval tv;
    
    master = ecrt_request_master(0);
	if (!master)
		return -1;

    domain = ecrt_master_create_domain(master);
    if (!domain)
        return -1;

    sc = ecrt_master_slave_config(master, 0, 0, 0x00000002, 0x044C2C52);
    if (!sc)
        return -1;

    if (ecrt_master_activate(master))
        return -1;

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, 0)) {
        fprintf(stderr, "Failed to install signal handler!\n");
        return -1;
    }

    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = 10000;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = 1;
    if (setitimer(ITIMER_REAL, &tv, NULL)) {
        fprintf(stderr, "Failed to start timer: %s\n", strerror(errno));
        return 1;
    }

	while (1) {
        sleep(1);

        while (sig_alarms != user_alarms) {

#if 0
            struct timeval t;
            gettimeofday(&t, NULL);
            printf("%u %u\n", t.tv_sec, t.tv_usec);
#endif
            ecrt_master_receive(master);
            ecrt_master_send(master);

            user_alarms++;
        }
	}

	return 0;
}

/****************************************************************************/
