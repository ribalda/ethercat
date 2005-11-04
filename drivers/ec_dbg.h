#ifndef ECDBG_H_
#define ECDBG_H_


//#define DEBUG_SEND_RECEIVE
#define ECMASTER_DEBUG


//#include "../rs232dbg/rs232dbg.h"


#ifdef ECMASTER_DEBUG
/* note: prints function name for you */
//#  define EC_DBG(fmt, args...) SDBG_print(fmt, ## args)

//#define EC_DBG(fmt, args...) SDBG_print("%s: " fmt, __FUNCTION__ , ## args)
#define EC_DBG(fmt, args...) printk(fmt, ## args)
#else
#define EC_DBG(fmt, args...)
#endif


#endif
