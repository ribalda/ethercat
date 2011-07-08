/******************************************************************************
 *
 *  $Id$
 *
 *  main.c	        Copyright (C) 2011       IgH Andreas Stewering-Bone
 *
 *  This file is part of ethercatrtdm interface to IgH EtherCAT master 
 *  
 *  The IgH EtherCAT master is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation; version 2.1
 *  of the License.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT master userspace library is distributed in the hope that
 *  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with the IgH EtherCAT master userspace library. If not, see
 *  <http://www.gnu.org/licenses/>.
 *  
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/


#include <errno.h>
#include <mqueue.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>

#include <rtdm/rtdm.h>
#include <rtdk.h>

#include "../../include/ecrt.h"
#include "../../include/ec_rtdm.h"

#define NSEC_PER_SEC 1000000000

static unsigned int cycle = 1000; /* 1 ms */

static pthread_t cyclicthread;

int rt_fd = -1;
int run=0;

unsigned int sync_ref_counter = 0;

CstructMstrAttach MstrAttach;

/****************************************************************************/




// Optional features
#define CONFIGURE_PDOS  1


/****************************************************************************/

// EtherCAT
static ec_master_t *master = NULL;
static ec_master_state_t master_state = {};

static ec_domain_t *domain1 = NULL;
static ec_domain_state_t domain1_state = {};


/****************************************************************************/
static uint8_t *domain1_pd = NULL;

// process data

#define BusCoupler01_Pos    0, 0
#define DigOutSlave01_Pos   0, 1
#define DigOutSlave02_Pos   0, 2
#define DigInSlave01_Pos    0, 3
#define AnaOutSlave01_Pos   0, 4
#define AnaInSlave01_Pos    0, 5
#define BusCoupler02_Pos    0, 6
#define AnaInSlave02_Pos    0, 7


#define Beckhoff_EK1100 0x00000002, 0x044c2c52
#define Beckhoff_EL1014 0x00000002, 0x03f63052
#define Beckhoff_EL2004 0x00000002, 0x07d43052
#define Beckhoff_EL4132 0x00000002, 0x10243052
#define Beckhoff_EL3102 0x00000002, 0x0c1e3052
#define Beckhoff_EL4102 0x00000002, 0x10063052
#define Beckhoff_EL6731 0x00000002, 0x1a4b3052
#define Beckhoff_EL6600 0x00000002, 0x19c93052
#define Beckhoff_EL3602 0x00000002, 0x0e123052
#define Beckhoff_EL5151 0x00000002, 0x141f3052


// offsets for PDO entries
static unsigned int off_dig_out0      = 0;
static unsigned int off_dig_out1      = 0;
static unsigned int off_dig_out2      = 0;
static unsigned int off_dig_out3      = 0;
static unsigned int off_dig_in0       = 0;
static unsigned int off_ana_out0      = 0;
static unsigned int off_ana_out1      = 0;
static unsigned int off_ana_in0_status = 0;
static unsigned int off_ana_in0_value  = 0;
static unsigned int off_ana_in1_status = 0;
static unsigned int off_ana_in1_value  = 0;


// process data
unsigned int bit_position0=0; /* Pointer to a variable to store a bit */
unsigned int bit_position1=0; /* Pointer to a variable to store a bit */
unsigned int bit_position2=0; /* Pointer to a variable to store a bit */
unsigned int bit_position3=0; /* Pointer to a variable to store a bit */

const static ec_pdo_entry_reg_t domain1_regs[] = {
   {DigOutSlave01_Pos, Beckhoff_EL2004, 0x7000, 0x01, &off_dig_out0, &bit_position0},
   {DigOutSlave01_Pos, Beckhoff_EL2004, 0x7010, 0x01, &off_dig_out1, &bit_position1},
   {DigOutSlave01_Pos, Beckhoff_EL2004, 0x7020, 0x01, &off_dig_out2, &bit_position2},
   {DigOutSlave01_Pos, Beckhoff_EL2004, 0x7030, 0x01, &off_dig_out3, &bit_position3},
   {DigInSlave01_Pos,  Beckhoff_EL1014, 0x6000, 0x01, &off_dig_in0},
   {AnaOutSlave01_Pos, Beckhoff_EL4132, 0x3001, 0x01, &off_ana_out0},
   {AnaOutSlave01_Pos, Beckhoff_EL4132, 0x3002, 0x01, &off_ana_out1},
   {AnaInSlave01_Pos,  Beckhoff_EL3102, 0x3101, 0x01, &off_ana_in0_status},
   {AnaInSlave01_Pos,  Beckhoff_EL3102, 0x3101, 0x02, &off_ana_in0_value},
   {AnaInSlave01_Pos,  Beckhoff_EL3102, 0x3102, 0x01, &off_ana_in1_status},
   {AnaInSlave01_Pos,  Beckhoff_EL3102, 0x3102, 0x02, &off_ana_in1_value},
   {}
};

char rt_dev_file[64];
static unsigned int blink = 0;

static ec_slave_config_t *sc_dig_out_01 = NULL;

static ec_slave_config_t *sc_dig_out_02 = NULL;

static ec_slave_config_t *sc_dig_in_01 = NULL;

static ec_slave_config_t *sc_ana_out_01 = NULL;

static ec_slave_config_t *sc_ana_in_01 = NULL;

static ec_slave_config_t *sc_ana_in_02 = NULL;

/*****************************************************************************/

/* Slave 1, "EL2004"
 * Vendor ID:       0x00000002
 * Product code:    0x07d43052
 * Revision number: 0x00100000
 */

ec_pdo_entry_info_t slave_1_pdo_entries[] = {
   {0x7000, 0x01, 1}, /* Output */
   {0x7010, 0x01, 1}, /* Output */
   {0x7020, 0x01, 1}, /* Output */
   {0x7030, 0x01, 1}, /* Output */
};

ec_pdo_info_t slave_1_pdos[] = {
   {0x1600, 1, slave_1_pdo_entries + 0}, /* Channel 1 */
   {0x1601, 1, slave_1_pdo_entries + 1}, /* Channel 2 */
   {0x1602, 1, slave_1_pdo_entries + 2}, /* Channel 3 */
   {0x1603, 1, slave_1_pdo_entries + 3}, /* Channel 4 */
};

ec_sync_info_t slave_1_syncs[] = {
   {0, EC_DIR_OUTPUT, 4, slave_1_pdos + 0, EC_WD_ENABLE},
   {0xff}
};

/* Slave 2, "EL2004"
 * Vendor ID:       0x00000002
 * Product code:    0x07d43052
 * Revision number: 0x00100000
 */

ec_pdo_entry_info_t slave_2_pdo_entries[] = {
   {0x7000, 0x01, 1}, /* Output */
   {0x7010, 0x01, 1}, /* Output */
   {0x7020, 0x01, 1}, /* Output */
   {0x7030, 0x01, 1}, /* Output */
};

ec_pdo_info_t slave_2_pdos[] = {
   {0x1600, 1, slave_2_pdo_entries + 0}, /* Channel 1 */
   {0x1601, 1, slave_2_pdo_entries + 1}, /* Channel 2 */
   {0x1602, 1, slave_2_pdo_entries + 2}, /* Channel 3 */
   {0x1603, 1, slave_2_pdo_entries + 3}, /* Channel 4 */
};

ec_sync_info_t slave_2_syncs[] = {
   {0, EC_DIR_OUTPUT, 4, slave_2_pdos + 0, EC_WD_ENABLE},
   {0xff}
};

/* Slave 3, "EL1014"
 * Vendor ID:       0x00000002
 * Product code:    0x03f63052
 * Revision number: 0x00100000
 */

ec_pdo_entry_info_t slave_3_pdo_entries[] = {
   {0x6000, 0x01, 1}, /* Input */
   {0x6010, 0x01, 1}, /* Input */
   {0x6020, 0x01, 1}, /* Input */
   {0x6030, 0x01, 1}, /* Input */
};

ec_pdo_info_t slave_3_pdos[] = {
   {0x1a00, 1, slave_3_pdo_entries + 0}, /* Channel 1 */
   {0x1a01, 1, slave_3_pdo_entries + 1}, /* Channel 2 */
   {0x1a02, 1, slave_3_pdo_entries + 2}, /* Channel 3 */
   {0x1a03, 1, slave_3_pdo_entries + 3}, /* Channel 4 */
};

ec_sync_info_t slave_3_syncs[] = {
   {0, EC_DIR_INPUT, 4, slave_3_pdos + 0, EC_WD_DISABLE},
   {0xff}
};

/* Slave 4, "EL4132"
 * Vendor ID:       0x00000002
 * Product code:    0x10243052
 * Revision number: 0x03f90000
 */

ec_pdo_entry_info_t slave_4_pdo_entries[] = {
   {0x3001, 0x01, 16}, /* Output */
   {0x3002, 0x01, 16}, /* Output */
};

ec_pdo_info_t slave_4_pdos[] = {
   {0x1600, 1, slave_4_pdo_entries + 0}, /* RxPDO 01 mapping */
   {0x1601, 1, slave_4_pdo_entries + 1}, /* RxPDO 02 mapping */
};

ec_sync_info_t slave_4_syncs[] = {
   {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {2, EC_DIR_OUTPUT, 2, slave_4_pdos + 0, EC_WD_DISABLE},
   {3, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {0xff}
};

/* Slave 5, "EL3102"
 * Vendor ID:       0x00000002
 * Product code:    0x0c1e3052
 * Revision number: 0x00000000
 */

ec_pdo_entry_info_t slave_5_pdo_entries[] = {
   {0x3101, 0x01, 8}, /* Status */
   {0x3101, 0x02, 16}, /* Value */
   {0x3102, 0x01, 8}, /* Status */
   {0x3102, 0x02, 16}, /* Value */
};

ec_pdo_info_t slave_5_pdos[] = {
   {0x1a00, 2, slave_5_pdo_entries + 0}, /* TxPDO 001 mapping */
   {0x1a01, 2, slave_5_pdo_entries + 2}, /* TxPDO 002 mapping */
};

ec_sync_info_t slave_5_syncs[] = {
   {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {2, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {3, EC_DIR_INPUT, 2, slave_5_pdos + 0, EC_WD_DISABLE},
   {0xff}
};

/* Slave 6, "EL6731-0010"
 * Vendor ID:       0x00000002
 * Product code:    0x1a4b3052
 * Revision number: 0x0011000a
 */

ec_sync_info_t slave_6_syncs[] = {
   {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {2, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {3, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
};


/* Slave 7, "EL6601"
 * Vendor ID:       0x00000002
 * Product code:    0x19c93052
 * Revision number: 0x00110000
 */
/*
ec_sync_info_t slave_7_syncs[] = {
   {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {2, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {3, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {0xff}
};
*/

/* Master 0, Slave 7, "EL3602"
 * Vendor ID:       0x00000002
 * Product code:    0x0e123052
 * Revision number: 0x00100000
 */
ec_pdo_entry_info_t slave_7_pdo_entries[] = {
   {0x6000, 0x01, 1}, /* Underrange */
   {0x6000, 0x02, 1}, /* Overrange */
   {0x6000, 0x03, 2}, /* Limit 1 */
   {0x6000, 0x05, 2}, /* Limit 2 */
   {0x6000, 0x07, 1}, /* Error */
   {0x0000, 0x00, 7}, /* Gap */
   {0x1800, 0x07, 1},
   {0x1800, 0x09, 1},
   {0x6000, 0x11, 32}, /* Value */
   {0x6010, 0x01, 1}, /* Underrange */
   {0x6010, 0x02, 1}, /* Overrange */
   {0x6010, 0x03, 2}, /* Limit 1 */
   {0x6010, 0x05, 2}, /* Limit 2 */
   {0x6010, 0x07, 1}, /* Error */
   {0x0000, 0x00, 7}, /* Gap */
   {0x1801, 0x07, 1},
   {0x1801, 0x09, 1},
   {0x6010, 0x11, 32}, /* Value */
};

ec_pdo_info_t slave_7_pdos[] = {
   {0x1a00, 9, slave_7_pdo_entries + 0}, /* AI TxPDO-Map Inputs Ch.1 */
   {0x1a01, 9, slave_7_pdo_entries + 9}, /* AI TxPDO-Map Inputs Ch.2 */
};

ec_sync_info_t slave_7_syncs[] = {
   {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {2, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {3, EC_DIR_INPUT, 2, slave_7_pdos + 0, EC_WD_DISABLE},
   {0xff}
};

/* Master 0, Slave 8, "EL5151"
 * Vendor ID:       0x00000002
 * Product code:    0x141f3052
 * Revision number: 0x00130000
 */

ec_pdo_entry_info_t slave_8_pdo_entries[] = {
   {0x6000, 0x01, 1},
   {0x6000, 0x02, 1},
   {0x6000, 0x03, 1},
   {0x0000, 0x00, 4}, /* Gap */
   {0x6000, 0x08, 1},
   {0x6000, 0x09, 1},
   {0x6000, 0x0a, 1},
   {0x6000, 0x0b, 1},
   {0x0000, 0x00, 1}, /* Gap */
   {0x6000, 0x0d, 1},
   {0x1c32, 0x20, 1},
   {0x0000, 0x00, 1}, /* Gap */
   {0x1800, 0x09, 1},
   {0x6000, 0x11, 32},
   {0x6000, 0x12, 32},
   {0x6000, 0x14, 32},
};

ec_pdo_info_t slave_8_pdos[] = {
   {0x0000, 0, NULL},
   {0x1a00, 15, slave_8_pdo_entries + 0},
   {0x1a02, 1, slave_8_pdo_entries + 15},
};

ec_sync_info_t slave_8_syncs[] = {
   {0, EC_DIR_OUTPUT, 0, NULL, EC_WD_DISABLE},
   {1, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
   {2, EC_DIR_OUTPUT, 1, slave_8_pdos + 0, EC_WD_DISABLE},
   {3, EC_DIR_INPUT, 2, slave_8_pdos + 1, EC_WD_DISABLE},
   {0xff}
};


/*****************************************************************************/


void rt_check_domain_state(void)
{
    ec_domain_state_t ds;

    if (rt_fd>=0)
      {
          ecrt_rtdm_domain_state(rt_fd,&ds);  
      }

    if (ds.working_counter != domain1_state.working_counter)
     {
        rt_printf("Domain1: WC %u.\n", ds.working_counter);
     }
    if (ds.wc_state != domain1_state.wc_state)
     {
    	rt_printf("Domain1: State %u.\n", ds.wc_state);
     }

    domain1_state = ds;
}

void rt_check_master_state(void)
{
    ec_master_state_t ms;

    if (rt_fd>=0)
      {
          ecrt_rtdm_master_state(rt_fd,&ms);
      }

    if (ms.slaves_responding != master_state.slaves_responding)
    {
        rt_printf("%u slave(s).\n", ms.slaves_responding);
    }
    if (ms.al_states != master_state.al_states)
    {
        rt_printf("AL states: 0x%02X.\n", ms.al_states);
    }
    if (ms.link_up != master_state.link_up)
    {
        rt_printf("Link is %s.\n", ms.link_up ? "up" : "down");
    }
    master_state = ms;
}




void rt_sync()
{
  struct timespec now;
  uint64_t now_ns;
  clock_gettime(CLOCK_REALTIME,&now);

  now_ns = 1000000000LL*now.tv_sec + now.tv_nsec;

  if (rt_fd>=0)
  {
      ecrt_rtdm_master_application_time(rt_fd, &now_ns);
  }

  if (sync_ref_counter) {
     sync_ref_counter--;
  } else {
     sync_ref_counter = 9;
     if (rt_fd>=0)
     {
         ecrt_rtdm_master_sync_reference_clock(rt_fd);
     }
  }
  if (rt_fd>=0)
  {
      ecrt_rtdm_master_sync_slave_clocks(rt_fd) ;
  }
}

/*****************************************************************************/

/**********************************************************/
void cleanup_all(void)
{
    run = 0;   
}

void catch_signal(int sig)
{
    cleanup_all();    
}





void *my_thread(void *arg)
{
    struct timespec next_period;

    int counter = 0;
    int divcounter = 0;
    int divider = 10;


    clock_gettime(CLOCK_REALTIME, &next_period);
    while(1) {
        next_period.tv_nsec += cycle * 1000;
        while (next_period.tv_nsec >= NSEC_PER_SEC) {
                next_period.tv_nsec -= NSEC_PER_SEC;
                next_period.tv_sec++;
                }

        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_period, NULL);


        counter++;
        if (counter>60000) {
            run=0;
            return NULL;
        }

        if(run ==  0) {
            return NULL;
        }

        
        // receive ethercat
        ecrt_rtdm_master_recieve(rt_fd);
        ecrt_rtdm_domain_process(rt_fd);
        
        rt_check_domain_state();
        
        if (divcounter ==0)
            {
                divcounter=divider;
                rt_check_master_state();
            }
        divcounter--;
        if ((counter % 200)==0)
            {
                blink = !blink;
                
            }
      

        EC_WRITE_U8(domain1_pd + off_dig_out0, blink ? 0x0 : 0x0F);
        EC_WRITE_U16(domain1_pd + off_ana_out0, blink ? 0x0: 0xfff);
        
        //sync DC
        rt_sync();
        
        // send process data
        ecrt_rtdm_domain_queque(rt_fd);
        ecrt_rtdm_master_send(rt_fd);   
    }
    return NULL;
}



int main(int argc, char *argv[])
{
    ec_slave_config_t *sc;
    int rtstatus;



    signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);
    signal(SIGHUP, catch_signal);

    mlockall(MCL_CURRENT|MCL_FUTURE);



    MstrAttach.masterindex = 0;
    
    printf("request master\n");
    master = ecrt_request_master(MstrAttach.masterindex);
    if (!master)
        return -1;
    
    
    domain1 = ecrt_master_create_domain(master);
    if (!domain1)
        return -1;
    
    
#ifdef CONFIGURE_PDOS

    printf("Configuring PDOs...\n");
    
    printf("Get Configuring el2004...\n");
    sc_dig_out_01 = ecrt_master_slave_config(master, DigOutSlave01_Pos, Beckhoff_EL2004);
    if (!sc_dig_out_01) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }
    
    printf("Configuring EL2004...\n");
    if (ecrt_slave_config_pdos(sc_dig_out_01, EC_END, slave_1_syncs))
        {
            fprintf(stderr, "Failed to configure PDOs.\n");
            return -1;
        }
    
    printf("Get Configuring el2004...\n");
    sc_dig_out_02 = ecrt_master_slave_config(master, DigOutSlave02_Pos, Beckhoff_EL2004);
    if (!sc_dig_out_02) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    printf("Configuring EL2004...\n");
    if (ecrt_slave_config_pdos(sc_dig_out_02, EC_END, slave_2_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }
    
    printf("Get Configuring el1014...\n");
    sc_dig_in_01 = ecrt_master_slave_config(master, DigInSlave01_Pos, Beckhoff_EL1014);
    if (!sc_dig_in_01) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }
    
    printf("Configuring EL1014...\n");
    if (ecrt_slave_config_pdos(sc_dig_in_01, EC_END, slave_3_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

    printf("Get Configuring EL4132...\n");
    sc_ana_out_01 = ecrt_master_slave_config(master, AnaOutSlave01_Pos, Beckhoff_EL4132);
    if (!sc_ana_out_01) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    printf("Configuring EL4132...\n");
    if (ecrt_slave_config_pdos(sc_ana_out_01, EC_END, slave_4_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

    printf("Get Configuring EL3102...\n");
    sc_ana_in_01 = ecrt_master_slave_config(master, AnaInSlave01_Pos, Beckhoff_EL3102);
    if (!sc_ana_in_01) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
    }

    printf("Configuring EL3102...\n");
    if (ecrt_slave_config_pdos(sc_ana_in_01, EC_END, slave_5_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
    }

    printf("Get Configuring EL3602...\n");
	sc_ana_in_02 = ecrt_master_slave_config(master, AnaInSlave02_Pos, Beckhoff_EL3602);
	if (!sc_ana_in_02) {
        fprintf(stderr, "Failed to get slave configuration.\n");
        return -1;
	}
    

	printf("Configuring EL3602...\n");
	if (ecrt_slave_config_pdos(sc_ana_in_02, EC_END, slave_7_syncs)) {
        fprintf(stderr, "Failed to configure PDOs.\n");
        return -1;
	}
    
#endif
    
    // Create configuration for bus coupler
    sc = ecrt_master_slave_config(master, BusCoupler01_Pos, Beckhoff_EK1100);
    if (!sc)
        return -1;
    
#ifdef CONFIGURE_PDOS
    if (ecrt_domain_reg_pdo_entry_list(domain1, domain1_regs)) {
        fprintf(stderr, "PDO entry registration failed!\n");
        return -1;
    }
#endif


        
    sprintf(&rt_dev_file[0],"%s%u",EC_RTDM_DEV_FILE_NAME,0);
    
    
    rt_fd = rt_dev_open( &rt_dev_file[0], 0);
    if (rt_fd < 0) {
        printf("can't open %s\n", &rt_dev_file[0]);
        return -1;
    }
    
    MstrAttach.domainindex = ecrt_domain_index(domain1);
    
    // attach the master over rtdm driver
    rtstatus=ecrt_rtdm_master_attach(rt_fd, &MstrAttach);
    if (rtstatus < 0)
        {
            printf("cannot attach to master over rtdm\n");
            return -1;
        }
    
    printf("Activating master...\n");
    if (ecrt_master_activate(master))
        return -1;
    
    if (!(domain1_pd = ecrt_domain_data(domain1))) {
        return -1;
    }
    fprintf(stderr, "domain1_pd:  0x%.6x\n", (unsigned int)domain1_pd);
    
    
    
    int ret;
    run=1;

    /* Create cyclic RT-thread */
    struct sched_param param = { .sched_priority = 82 };
    pthread_attr_t thattr;
    pthread_attr_init(&thattr);
    pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setinheritsched(&thattr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&thattr, SCHED_FIFO);
    pthread_setschedparam(cyclicthread, SCHED_FIFO, &param);
    pthread_set_name_np(cyclicthread, "ec_xenomai_posix_test");
    ret = pthread_create(&cyclicthread, &thattr, &my_thread, NULL);
    if (ret) {
        fprintf(stderr, "%s: pthread_create cyclic task failed\n",
                strerror(-ret));
        goto failure;
    }




    while (run)
      {
    	sched_yield();
      }






    if (rt_fd >= 0)
        {
            printf("closing rt device %s\n", &rt_dev_file[0]);
            
            rt_dev_close(rt_fd);
            
        }
    
    printf("End of Program\n");
    ecrt_release_master(master);

    return 0;

 failure:
    pthread_kill(cyclicthread, SIGHUP);
    pthread_join(cyclicthread, NULL);


    return 1;
}

