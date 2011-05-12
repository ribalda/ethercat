/******************************************************************************
 *
 *  $Id$
 *
 *  ec_rtdm.c	Copyright (C) 2009-2010  Moehwald GmbH B.Benner
 *                            2011       IgH Andreas Stewering-Bone
 *								  
 *								  
 *  This file is part of the IgH EtherCAT master 
 *  
 *  The IgH EtherCAT master is free software; you can
 *  redistribute it and/or modify it under the terms of the GNU Lesser General
 *  Public License as published by the Free Software Foundation; version 2.1
 *  of the License.
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

#include <linux/module.h>
#include <linux/mman.h>


#ifdef ENABLE_XENOMAI
#include <native/task.h>
#include <native/sem.h>
#include <native/mutex.h>
#include <native/timer.h>
#endif

#ifdef ENABLE_RTAI
#include <rtai_sched.h>
#include <rtai_sem.h>
#endif


#include <rtdm/rtdm_driver.h>

#include "../include/ecrt.h"
#include "../include/ec_rtdm.h"

#ifdef ENABLE_XENOMAI
#define my_mutex_create(X,Y)  rt_mutex_create(X, Y)
#define my_mutex_acquire(X,Y) rt_mutex_acquire(X,Y)
#define my_mutex_release(X)   rt_mutex_release(X)
#define my_mutex_delete(X)    rt_mutex_delete(X)
#endif

#ifdef ENABLE_RTAI
#define my_mutex_create(X,Y)  rt_sem_init(X, 1)
#define my_mutex_acquire(X,Y) rt_sem_wait(X)
#define my_mutex_release(X)   rt_sem_signal(X)
#define my_mutex_delete(X)    rt_sem_delete(X)
#define TM_INFINITE
#endif




#define EC_RTDM_MAX_MASTERS 5 /**< Maximum number of masters. */

#define EC_RTDM_GINFO(fmt, args...) \
    rtdm_printk(KERN_INFO "EtherCATrtdm: " fmt,  ##args)

#define EC_RTDM_GERR(fmt, args...) \
    rtdm_printk(KERN_ERR "EtherCATrtdm ERROR: " fmt, ##args)

#define EC_RTDM_GWARN(fmt, args...) \
    rtdm_printk(KERN_WARNING "EtherCATrtdm WARNING: " fmt, ##args)


#define EC_RTDM_INFO(devno, fmt, args...) \
    rtdm_printk(KERN_INFO "EtherCATrtdm %u: " fmt, devno, ##args)

#define EC_RTDM_ERR(devno, fmt, args...) \
    rtdm_printk(KERN_ERR "EtherCATrtdm %u ERROR: " fmt, devno, ##args)

#define EC_RTDM_WARN(devno, fmt, args...) \
    rtdm_printk(KERN_WARNING "EtherCATrtdm %u WARNING: " fmt, devno, ##args)




typedef struct _EC_RTDM_DRV_STRUCT {
    unsigned int	    isattached;
    ec_master_t *	    master;
    ec_domain_t *	    domain;	
#ifdef ENABLE_XENOMAI			   
    RT_MUTEX           masterlock;
#endif
#ifdef ENABLE_RTAI
    SEM                masterlock;
#endif
    unsigned int	    sendcnt;
    unsigned int	    reccnt;
    unsigned int	    sendcntlv;
    unsigned int	    reccntlv;
    char                    mutexname[64];
    unsigned int            masterno;
} EC_RTDM_DRV_STRUCT;


static EC_RTDM_DRV_STRUCT ec_rtdm_masterintf[EC_RTDM_MAX_MASTERS];


/* import from ethercat */
ec_master_t *ecrt_attach_master(unsigned int master_index /**< Index of the master to request. */
        );

// driver context struct: used for storing various information
typedef struct _EC_RTDM_DRV_CONTEXT {
    int                       dev_id;
    EC_RTDM_DRV_STRUCT*  pdrvstruc;
} EC_RTDM_DRV_CONTEXT;



/**********************************************************/
/*            Utilities                                   */
/**********************************************************/

static int _atoi(const char* text)
{
  char  b;
  int wd=-1;
  int nfak=1;

  wd=0;

  while ((*text==' ') || (*text=='\t')) text++;
  if (*text=='-') 
    {
      nfak=-1;
	  text++;
    }
  if (*text=='+') 
    {
      text++;
    }
  while (*text!=0)
    {
 	  b = *text;

	  if ( (b>='0') && (b<='9') )
		{
		   b=b-'0';
		   wd=wd*10+b;
		}
	  text++;
	}
  return (nfak*wd);
}


/**********************************************************/
/*            DRIVER sendcallback                         */
/**********************************************************/
void send_callback(void *cb_data)
{
    EC_RTDM_DRV_STRUCT * pdrvstruc;
    
    pdrvstruc = (EC_RTDM_DRV_STRUCT*)cb_data;
    if (pdrvstruc->master)
        {
            my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
            ecrt_master_send_ext(pdrvstruc->master);
            my_mutex_release(&pdrvstruc->masterlock);
      }
}

/*****************************************************************************/

void receive_callback(void *cb_data)
{
    EC_RTDM_DRV_STRUCT * pdrvstruc;

    pdrvstruc = (EC_RTDM_DRV_STRUCT*)cb_data;
    if (pdrvstruc->master)
      {
          my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
          ecrt_master_receive(pdrvstruc->master);
          my_mutex_release(&pdrvstruc->masterlock);      
    }
}




void detach_master(EC_RTDM_DRV_STRUCT * pdrvstruc)
{

  if (pdrvstruc->isattached)
      {
          EC_RTDM_INFO(pdrvstruc->masterno,"reseting callbacks!\n");
          ecrt_master_callbacks(pdrvstruc->master,NULL,NULL,NULL);
          EC_RTDM_INFO(pdrvstruc->masterno,"deleting mutex!\n");
          my_mutex_delete(&pdrvstruc->masterlock);
          pdrvstruc->master = NULL;
          pdrvstruc->isattached=0;
          EC_RTDM_INFO(pdrvstruc->masterno,"master detach done!\n");
      }
}




/**********************************************************/
/*            DRIVER OPEN                                 */
/**********************************************************/
int ec_rtdm_open_rt(struct rtdm_dev_context    *context,
                 rtdm_user_info_t           *user_info,
                 int                        oflags)
{
    EC_RTDM_DRV_CONTEXT* my_context;
    EC_RTDM_DRV_STRUCT * pdrvstruc;
    const char * p;
    int dev_no;
    unsigned int namelen;

    //int ret;
    int dev_id;

    // get the context for our driver - used to store driver info
    my_context = (EC_RTDM_DRV_CONTEXT*)context->dev_private;

    dev_no = -1;
    namelen   = strlen(context->device->driver_name);
    p = &context->device->driver_name[namelen-1];
    if (p!=&context->device->driver_name[0])
      {
	  while ((*p>='0') && (*p<='9')) 
	    {
	       p--;
	       if (p==&context->device->driver_name[0]) break;
   	    }
	  dev_no=_atoi(p);
	  if  ((dev_no!=-1) && (dev_no<EC_RTDM_MAX_MASTERS))
	    { 
		dev_id    = context->device->device_id;
    		pdrvstruc = (EC_RTDM_DRV_STRUCT*)&ec_rtdm_masterintf[dev_no];

    		my_context->dev_id         = dev_id;
    		my_context->pdrvstruc      = pdrvstruc;
		 
    		// enable interrupt in RTDM
    		return 0;	
	    }	
      }
   EC_RTDM_GERR("open - Cannot detect master device no\n");
   return -EFAULT;
}

/**********************************************************/
/*            DRIVER CLOSE                                */
/**********************************************************/
int ec_rtdm_close_rt(struct rtdm_dev_context   *context,
                  rtdm_user_info_t          *user_info)
{
    EC_RTDM_DRV_CONTEXT* my_context;
    EC_RTDM_DRV_STRUCT * pdrvstruc;

    // get the context
    my_context = (EC_RTDM_DRV_CONTEXT*)context->dev_private;

    pdrvstruc =  my_context->pdrvstruc;
    EC_RTDM_INFO(pdrvstruc->masterno,"close called!\n");
    detach_master(pdrvstruc);
    return 0;
	
}

/**********************************************************/
/*            DRIVER IOCTL                                */
/**********************************************************/
int ec_rtdm_ioctl_rt(struct rtdm_dev_context   *context,
                  rtdm_user_info_t          *user_info,
                  int                       request,
                  void                      *arg)
{
    EC_RTDM_DRV_CONTEXT* my_context;
    EC_RTDM_DRV_STRUCT * pdrvstruc;
    int ret;
    unsigned int l_ioctlvalue[]={0,0,0,0,0,0,0,0};
    ec_domain_state_t ds;
    ec_master_state_t ms;
    uint64_t app_time;


    ret = 0;

    // get the context
    my_context = (EC_RTDM_DRV_CONTEXT*)context->dev_private;
    pdrvstruc =  my_context->pdrvstruc;
    
    switch (request) {
    case EC_RTDM_MASTERSTATE:
    {
		if (!pdrvstruc->isattached)
            {
                return -EFAULT;
            }
		if (pdrvstruc->master)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                
                ecrt_master_state(pdrvstruc->master, &ms);
                
                my_mutex_release(&pdrvstruc->masterlock);
                
            }
        if  (rtdm_rw_user_ok(user_info, arg, sizeof(ms)))
            {
                // copy data to user
                if (rtdm_copy_to_user(user_info, arg, &ms,sizeof(ms)))
                    {
                        return -EFAULT;
                    }
            }
        
    }
    break;
    case EC_RTDM_DOMAINSTATE:
    {
		if (!pdrvstruc->isattached)
            {
                return -EFAULT;
            }
	   	if (pdrvstruc->domain)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                
                ecrt_domain_state(pdrvstruc->domain, &ds);
                
                my_mutex_release(&pdrvstruc->masterlock);
            }
        if  (rtdm_rw_user_ok(user_info, arg, sizeof(ds)))
            {
                // copy data to user
                if (rtdm_copy_to_user(user_info, arg, &ds,sizeof(ds)))
                    {
                        return -EFAULT;
                    }
            }
    }
    break;
    case EC_RTDM_MASTER_RECEIVE:
    {	       
        if (pdrvstruc->isattached)
            {
                if (pdrvstruc->master)
                    {
                        my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                        ecrt_master_receive(pdrvstruc->master);
                        pdrvstruc->reccnt++;
                        my_mutex_release(&pdrvstruc->masterlock);
                    }
            }
    }
    break;
    case EC_RTDM_DOMAIN_PROCESS:
    {	       
        if (pdrvstruc->isattached)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                ecrt_domain_process(pdrvstruc->domain);
                my_mutex_release(&pdrvstruc->masterlock);
            }
    }
    break;
    case EC_RTDM_MASTER_SEND:
    {
        
        if (pdrvstruc->isattached)
            {
                if (pdrvstruc->master)
                    {
                        my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                        ecrt_master_send(pdrvstruc->master);
                        pdrvstruc->sendcnt++;
                        my_mutex_release(&pdrvstruc->masterlock);
                    }
            }
    }
    break;
    case EC_RTDM_DOMAIN_QUEQUE:
    {	       
        if (pdrvstruc->isattached)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                ecrt_domain_queue(pdrvstruc->domain);
                my_mutex_release(&pdrvstruc->masterlock);
            }
    }
    break;

    case EC_RTDM_MASTER_APP_TIME:
    {
		if (!pdrvstruc->isattached)
            {
                rtdm_printk("ERROR : No Master attached\n");
                return -EFAULT;
            }
        if (rtdm_safe_copy_from_user(user_info, &app_time, arg, sizeof(app_time)))
            {
                rtdm_printk("ERROR : can't copy data to driver\n");
                return -EFAULT;
            }
            
        if (pdrvstruc->master)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                
                ecrt_master_application_time(pdrvstruc->master, app_time);
                my_mutex_release(&pdrvstruc->masterlock);
                
            }
    }
    break;
    case EC_RTDM_SYNC_REF_CLOCK:
    {
		if (!pdrvstruc->isattached)
            {
                return -EFAULT;
            }
        if (pdrvstruc->master)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                
                ecrt_master_sync_reference_clock(pdrvstruc->master);
                
                my_mutex_release(&pdrvstruc->masterlock);
                
            }
    }
    break;
    case EC_RTDM_SYNC_SLAVE_CLOCK:
    {
		if (!pdrvstruc->isattached)
            {
                return -EFAULT;
            }
        if (pdrvstruc->master)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                
                ecrt_master_sync_slave_clocks(pdrvstruc->master);
                
                my_mutex_release(&pdrvstruc->masterlock);
                
            }
    }
    break;
    case EC_RTDM_MASTER_SYNC_MONITOR_QUEQUE:
    {
		if (!pdrvstruc->isattached)
            {
                return -EFAULT;
            }
        if (pdrvstruc->master)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                ecrt_master_sync_monitor_queue(pdrvstruc->master);
                my_mutex_release(&pdrvstruc->masterlock);
            }
    }
    break;
    case EC_RTDM_MASTER_SYNC_MONITOR_PROCESS:
    {
        uint32_t ret;
		if (!pdrvstruc->isattached)
            {
                return -EFAULT;
            }
        if (pdrvstruc->master)
            {
                my_mutex_acquire(&pdrvstruc->masterlock,TM_INFINITE);
                ret = ecrt_master_sync_monitor_process(pdrvstruc->master);
                my_mutex_release(&pdrvstruc->masterlock);
                if (rtdm_safe_copy_to_user(user_info, arg, &ret, sizeof(ret)))
                    {
                        EC_RTDM_ERR(pdrvstruc->masterno,"copy to user param failed!\n");
                        ret=-EFAULT;
                    }
            }
    }
    break;
    case EC_RTDM_MSTRATTACH:
    {
        unsigned int mstridx;
        
        mstridx = 0;
        ret = 0;
        
        EC_RTDM_INFO(pdrvstruc->masterno,"Master attach start!\n");
        if (user_info) 
            {
                if (rtdm_read_user_ok(user_info, arg, sizeof(unsigned int)))
                    {
                        if (rtdm_copy_from_user(user_info, &l_ioctlvalue[0], arg,sizeof(unsigned int))==0)
                            {
                                pdrvstruc->domain = (ec_domain_t*)l_ioctlvalue[0];
                            }
                        else
                            {
                                EC_RTDM_ERR(pdrvstruc->masterno,"copy user param failed!\n");
                                ret=-EFAULT;
                            }		
                    }
                else
                    { 
                        EC_RTDM_ERR(pdrvstruc->masterno,"user parameter domain missing!\n");
                        ret=-EFAULT;
                    }	
            }
		if (ret!=0) 
            {
                return ret;
            }
        
		if ( (pdrvstruc->master) && (pdrvstruc->isattached))
            // master is allready attached
            {
                // master is allready attached
                EC_RTDM_ERR(pdrvstruc->masterno,"Master is allready attached!\n");
                ret = -EFAULT;
            }
	    else
            {
                //mstr=ecrt_request_master(0);
                mstridx = pdrvstruc->masterno;
	        	
                pdrvstruc->master=ecrt_attach_master(mstridx);
                
                if (pdrvstruc->master)
                    {
                        // Ok
                        EC_RTDM_INFO(pdrvstruc->masterno,"Master searching for domain!\n");
                        pdrvstruc->domain = ecrt_master_find_domain(pdrvstruc->master,l_ioctlvalue[0]);
                        if (!pdrvstruc->domain)
                            {
                                //
                                EC_RTDM_ERR(pdrvstruc->masterno,"Cannot find domain from index %u!\n",l_ioctlvalue[0]);
                                ret = -EFAULT;
                            }
                        else
                            {
                                
                                // set device name
                                snprintf(&pdrvstruc->mutexname[0],sizeof(pdrvstruc->mutexname)-1,"ETHrtdmLOCK%d",pdrvstruc->masterno);
                                EC_RTDM_INFO(pdrvstruc->masterno,"Creating Master mutex %s!\n",&pdrvstruc->mutexname[0]);
                                my_mutex_create(&pdrvstruc->masterlock,&pdrvstruc->mutexname[0]);
                                //ecrt_release_master(mstr);
                                ecrt_master_callbacks(pdrvstruc->master, send_callback, receive_callback, pdrvstruc);
                                EC_RTDM_INFO(pdrvstruc->masterno,"MSTR ATTACH done domain=%u!\n",(unsigned int)pdrvstruc->domain);
                                pdrvstruc->isattached=1;
                                ret = 0;
                            }
                        
                    }
                else
                    {
                        EC_RTDM_ERR(pdrvstruc->masterno,"Master attach failed!\n");
                        pdrvstruc->master = NULL;
                        ret = -EFAULT;
                    }
            }
    }
    break;
    default:
        ret = -ENOTTY;
    }
    return ret;
}


/**********************************************************/
/*            DRIVER READ                                 */
/**********************************************************/
int ec_rtdm_read_rt(struct rtdm_dev_context *context,
                    rtdm_user_info_t *user_info, void *buf, size_t nbyte)
{
    int                     ret;
#if defined(USE_THIS)
    EC_RTDM_DRV_CONTEXT* my_context;
    char                    *out_pos;
    int                     dev_id;
    rtdm_toseq_t            timeout_seq;
    int                     ret;

    out_pos = (char *)buf;
    
    my_context = (EC_RTDM_DRV_CONTEXT*)context->dev_private;
    
    // zero bytes requested ? return!
    if (nbyte == 0)
        return 0;

    // check if R/W actions to user-space are allowed
    if (user_info && !rtdm_rw_user_ok(user_info, buf, nbyte))
        return -EFAULT;

    dev_id = my_context->dev_id;

    // in case we need to check if reading is allowed (locking)
/*    if (test_and_set_bit(0, &ctx->in_lock))
        return -EBUSY;
*/
/*  // if we need to do some stuff with preemption disabled:
    rtdm_lock_get_irqsave(&ctx->lock, lock_ctx);
    // stuff here
    rtdm_lock_put_irqrestore(&ctx->lock, lock_ctx);
*/

    // wait: if ctx->timeout = 0, it will block infintely until
    //       rtdm_event_signal(&ctx->irq_event); is called from our
    //       interrupt routine
    //ret = rtdm_event_timedwait(&ctx->irq_event, ctx->timeout, &timeout_seq);

    // now write the requested stuff to user-space
    if (rtdm_copy_to_user(user_info, out_pos,
                          dummy_buffer, BUFSIZE) != 0) {
        ret = -EFAULT;
    } else {
        ret = BUFSIZE;
    }
#else
    ret = -EFAULT;
#endif
    return ret;
}

/**********************************************************/
/*            DRIVER WRITE                                */
/**********************************************************/
int ec_rtdm_write_rt(struct rtdm_dev_context *context,
                   rtdm_user_info_t *user_info,
                   const void *buf, size_t nbyte)
{
    int                     ret;

#if defined(USE_THIS)
    int                     dev_id;
    char                    *in_pos = (char *)buf;

    EC_RTDM_DRV_CONTEXT* my_context;
    

    my_context = (EC_RTDM_DRV_CONTEXT*)context->dev_private;
    

    if (nbyte == 0)
        return 0;
    if (user_info && !rtdm_read_user_ok(user_info, buf, nbyte))
        return -EFAULT;

    dev_id = my_context->dev_id;

    if (rtdm_copy_from_user(user_info, dummy_buffer,
                             in_pos, BUFSIZE) != 0) {
        ret = -EFAULT;
    } else {
       ret = BUFSIZE;
    }
#else
    ret = -EFAULT;
#endif
    // used when it is atomic
//   rtdm_mutex_unlock(&ctx->out_lock);
    return ret;
}

/**********************************************************/
/*            DRIVER OPERATIONS                           */
/**********************************************************/

// Template

static struct rtdm_device ec_rtdm_device_t = {
    struct_version:     RTDM_DEVICE_STRUCT_VER,

    device_flags:       RTDM_NAMED_DEVICE,
    context_size:   	sizeof(EC_RTDM_DRV_CONTEXT),
    device_name:        EC_RTDM_DEV_FILE_NAME,

/* open and close functions are not real-time safe due kmalloc
   and kfree. If you do not use kmalloc and kfree, and you made
   sure that there is no syscall in the open/close handler, you
   can declare the open_rt and close_rt handler.
*/
    open_rt:            NULL,
    open_nrt:           ec_rtdm_open_rt,

    ops: {
        close_rt:       NULL,
        close_nrt:      ec_rtdm_close_rt,

        ioctl_rt:       ec_rtdm_ioctl_rt,
        ioctl_nrt:      ec_rtdm_ioctl_rt, // rtdm_mmap_to_user is not RT safe

        read_rt:        ec_rtdm_read_rt,
        read_nrt:       NULL,

        write_rt:       ec_rtdm_write_rt,
        write_nrt:      NULL,

        recvmsg_rt:     NULL,
        recvmsg_nrt:    NULL,

        sendmsg_rt:     NULL,
        sendmsg_nrt:    NULL,
    },

    device_class:       RTDM_CLASS_EXPERIMENTAL,
    device_sub_class:   222,
    driver_name:        EC_RTDM_DEV_FILE_NAME,
    driver_version:     RTDM_DRIVER_VER(1,0,1),
    peripheral_name:    EC_RTDM_DEV_FILE_NAME,
    provider_name:      "EtherLab Community",
//    proc_name:          ethcatrtdm_device.device_name,
};


static struct rtdm_device ec_rtdm_devices[EC_RTDM_MAX_MASTERS];


/**********************************************************/
/*            INIT DRIVER                                 */
/**********************************************************/
int init_module(void)
{
	unsigned int i;
    int ret;

    ret = 0; 	
    
    EC_RTDM_GINFO("Initlializing EtherCAT RTDM Interface to Igh EtherCAT Master\n");
    memset(&ec_rtdm_masterintf[0],0,sizeof(ec_rtdm_masterintf));
    for (i=0;( (i<EC_RTDM_MAX_MASTERS) && (ret==0) ) ;i++)
      {	 
    	// master no to struct
    	ec_rtdm_masterintf[i].masterno = i;
        // copy from template
        memcpy(&ec_rtdm_devices[i],&ec_rtdm_device_t,sizeof(ec_rtdm_devices[0]));

        // set device name
        snprintf(&ec_rtdm_devices[i].device_name[0],RTDM_MAX_DEVNAME_LEN,"%s%d",EC_RTDM_DEV_FILE_NAME,i);
        // set proc_name
	    ec_rtdm_devices[i].proc_name = &ec_rtdm_devices[i].device_name[0];
	    ec_rtdm_devices[i].driver_name = &ec_rtdm_devices[i].device_name[0];
		ec_rtdm_devices[i].peripheral_name = &ec_rtdm_devices[i].device_name[0];
	
		EC_RTDM_GINFO("Registering device %s!\n",ec_rtdm_devices[i].driver_name);
		ret = rtdm_dev_register(&ec_rtdm_devices[i]);

      }	
    if (ret!=0)
      {	
    	// register m
        EC_RTDM_GERR("Initialization of EtherCAT RTDM Interface failed\n");
      }		
    return ret;
}

/**********************************************************/
/*            CLEANUP DRIVER                              */
/**********************************************************/
void cleanup_module(void)
{
    unsigned int i;

    EC_RTDM_GINFO("Cleanup EtherCAT RTDM Interface \n");
    for (i=0;i<EC_RTDM_MAX_MASTERS;i++)
      {
         if (ec_rtdm_masterintf[i].isattached)
           {
	          detach_master(&ec_rtdm_masterintf[i]);
	       }
 		 EC_RTDM_GINFO("Unregistering device %s!\n",ec_rtdm_devices[i].driver_name);
         rtdm_dev_unregister(&ec_rtdm_devices[i],1000);
    }
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EtherCAT RTDM Interface");
