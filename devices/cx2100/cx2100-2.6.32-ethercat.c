/******************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2013  Graeme Foot, Kinetic Engineering Design Ltd.
 *                                   <graeme@touchcut.com>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License version 2, as published by the Free Software Foundation.
 * 
 *  This file is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  This file has been added to the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 *****************************************************************************/

/* 
 * Notes:
 * 
 * To transmit data the EtherCAT Master passes the data using reusable skb's
 * (socket data buffers).  Usually the skb's are allocated by the client and
 * released by the driver, but because they are reused, they are not freed.
 * 
 * In a general Ethernet situation skb's are also used to send received data
 * data back to the client, but instead we can send the data directly back
 * to the EtherCAT master and avoid extra memory copies.
 * 
 * This device is primarily for use with the IgH EtherCAT Master.  If it
 * is combined with an EK1110 module then it can also function as a general
 * Ethernet port.  (The EK1110 needs to be configured into forward mode.)
 * 
 * However, as this is an unusual use case for this hardware I have not enabled 
 * general Ethernet port functionality, so if it is not claimed by the EtherCAT 
 * master it will remain idle.
 * 
 * The following is the data for a frame that can be sent to the EK1110 to
 * set it into forward mode (untested):
 * 
 *  UINT8 frameForwardEthernetFrames[] = 
 *          { 0x01, 0x01, 0x05, 0x01, 0x00, 0x00, 
 *            0x00, 0x1b, 0x21, 0x36, 0x1b, 0xce, 
 *            0x88, 0xa4, 0x0e, 0x10,
 *            0x08,    
 *            0x00,  
 *            0x00, 0x00,
 *            0x00, 0x01,
 *            0x02, 0x00,
 *            0x00, 0x00,
 *            0x00, 0x00,
 *            0x00, 0x00 };
 */

#include <linux/pci.h>
#include <linux/module.h>
#include <linux/mman.h>

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/pci_ids.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/completion.h>
#include <linux/crc32.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/irq.h>

#include "../../globals.h"
#include "../ecdev.h"


MODULE_AUTHOR("Graeme Foot <graemef@touchcut.com>");
MODULE_DESCRIPTION("Beckhoff CX2100-0004 PCI Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(EC_MASTER_VERSION);

static int verbosityLevel = 0;
static int verbosityCalls = -1;
module_param(verbosityLevel, int, S_IRUGO);
module_param(verbosityCalls, int, S_IRUGO);



#define VENDOR_ID   0x15ec
#define DEVICE_ID   0x5000
#define DRIVER_NAME "ec_cx2100"


#define FRAME_PAGE_SIZE 0x1000

/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT      (6*HZ)
#define TX_BUF_TOT_LEN  


/* Note: the max Ethernet frame length is approx 1514 (ETH_FRAME_LEN)
 * the is the MTU (max transmission unit) + 10 (header) + 4 (CRC)
 * 0x800 = 2048 so we are home and hosed to cover the frame + DMA header
 *         and then also have a nice alignment */
#define MAX_FRAME_SIZE      0x800
#define TX_FRAME_DATA_SIZE  MAX_FRAME_SIZE - sizeof(listItem_s) - sizeof(txHeader_s)
#define RX_FRAME_DATA_SIZE  MAX_FRAME_SIZE - sizeof(rxDMAHeader_s) - sizeof(rxHeader_s)


/* verbosity levels
 * Level 0: no verbosity messages
 * Level 1: major verbosity messages
 *   ..
 * Level 9: detail verbosity messages
 */
#define VERBOSITY_MAJOR  1
#define VERBOSITY_DETAIL 9

#define VERBOSITY_DEVICEINFO   1
#define VERBOSITY_FUNCTIONCALL 5
#define VERBOSITY_RETVALUE     7


#define MSG_INFO(fmt, args...) printk(KERN_INFO DRIVER_NAME ": " fmt, ##args)
#define MSG_ERR(fmt, args...)  printk(KERN_ERR DRIVER_NAME " ERROR: " fmt, ##args)
#define MSG_WARN(fmt, args...) printk(KERN_WARNING DRIVER_NAME " WARNING: " fmt, ##args)
#define MSG_VERBOSE(lvl, fmt, args...) if (lvl <= verbosityLevel) \
{ \
  printk(KERN_INFO DRIVER_NAME ": " fmt, ##args); \
  if      (verbosityCalls > 0) verbosityCalls--; \
  else if (verbosityCalls == 0) verbosityLevel = 0; \
}



/** list item and helpers for tx frames
 */
typedef struct _listItem_s listItem_s;
struct _listItem_s
{
  listItem_s *prev;             /**< link to previous list item */
  listItem_s *next;             /**< link to next list item */
};


/** initialize a list
 */
static inline void initList(listItem_s *head)
{
  head->prev = head;
  head->next = head;
}


/** check if a list is empty (ie the list only points to itself)
 */
static inline int isListEmpty(listItem_s *head)
{
  return (head->next == head);
}


/** return this first item in the list
 */
static inline listItem_s *listFirstItem(listItem_s *head)
{
  return head != head->next ? head->next : NULL;
}


/** remove an item from its list
 */
static inline listItem_s *listRemoveItem(listItem_s *item)
{
  listItem_s* prevItem = item->prev;
  listItem_s* nextItem = item->next;
  
  prevItem->next = nextItem;
  nextItem->prev = prevItem;
  
  item->prev = NULL;
  item->next = NULL;

  return item;
}


/** remove the first item from the list and return it
 */
static inline listItem_s *listRemoveFirstItem(listItem_s *head)
{
  return listRemoveItem(head->next);
}


/** insert an item at the end of the list
 */
static inline void listAddTail(listItem_s *head, listItem_s *item)
{
  // insert between the old last item and the head
  listItem_s *prev = head->prev;
  listItem_s *next = head;
  
  prev->next = item;
  next->prev = item;

  item->prev = prev;
  item->next = next;
}



/** CX2100 base function information block */
typedef struct _infoBlock_s {
  u16   fnType;
  u16   fnRevision;
  union
  {
    struct
    {
      u32   fnParam;
      u32   fnAddrOffset;
      u32   fnSize;
    } _base;
    struct
    {
      u8    fnBlockCount;
      u8    creationDay;
      u8    creationMonth;
      u8    creationYear;
      u32   id1;
      u32   id2;
    } _info;
    struct
    {
      u8    txDMAChannel;
      u8    rxDMAChannel;
      u16   reserved1;
      u32   fnAddrOffset;
      u32   fnSize;
    } _ecMasterDMA;
  };
} infoBlock_s;

#define CCAT_ID1 0x000088a4
#define CCAT_ID2 0x54414343  /* "CCAT" */



/** EtherCAT master function block */
typedef struct _ecMasterFnBlock_s 
{
  u32   infoBlock;
  u32   miiOffset;
  u32   txFifoOffset;
  u32   macRegisterOffset;
  u32   rxWindowOffset;
  u32   txMemoryOffset;
  u8    physicalConnectionActive : 1;
  u8    reserved1                : 7;
  u8    defaultFieldbus;
  u16   supportedFieldbus;
} ecMasterFnBlock_s;



/** EtherCAT master MII */
typedef struct _mii_s 
{
  u16   miCycle       : 1;
  u16   reserved1     : 6;
  u16   cmdValid      : 1;
  u16   cmdRegister   : 2;
  u16   reserved2     : 6;
  
  u16   phyAddress    : 5;
  u16   reserved3     : 3;
  u16   addressOfReg  : 5;
  u16   reserved4     : 3;
  
  u16   phyWriteData;
  u16   phyReadData;
} mii_s;



/** EtherCAT master MAC address filter */
typedef struct _macFilter_s 
{
  u8    macAddr[6];
  
  u8    macFilterEnable   : 1;
  u8    reserved1         : 7;
  
  u8    linkStatus        : 1;
  u8    reserved2         : 7;
} macFilter_s;


/** EtherCAT master Management Functions */
typedef struct _ecManagement_s 
{
  mii_s        mii;
  macFilter_s  macFilter;
} ecManagement_s;



/** EtherCAT master tx header 
 */
typedef struct _txHeader_s 
{
  u16   length;           /**< not used for dma */
  
  u8    port0     : 1;    /**< sending port 0, one-hot coded */
  u8    port1     : 1;    /**< sending port 0, one-hot coded */
  u8    reserved1 : 6;
  
  u8    tsEnable  : 1;    /**< sending if timestamp is reached */
  u8    sync0     : 1;    /**< sending on sync 0 event */
  u8    sync1     : 1;    /**< sending on sync 1 event (doc says sync 0???) */
  u8    reserved2 : 5;
  
  u32   readAck   : 1;    /**< read acknowledge (1 if read by CX2100, ie sent) */
  u32   reserved3 : 31;
  
  u64   timestamp;        /**< frame timestamp */
} txHeader_s;



/** EtherCAT master dma tx frame 
 */
typedef struct _txDMAFrame_s 
{
  listItem_s   listItem;                 /**< user defined data, must be 64bit */
  txHeader_s   header;                   /**< frame header */
  u8           data[TX_FRAME_DATA_SIZE]; /**< frame data */
} txDMAFrame_s;



/* forward declaration */
typedef struct _rxDMAFrame_s rxDMAFrame_s;


/** EtherCAT master dma rx header 
 */
typedef struct _rxDMAHeader_s 
{
  rxDMAFrame_s *nextFrame;

  u32   received  : 1;
  u32   reserved1 : 7;
  u32   nextValid : 1;
  u32   reserved2 : 23;
} rxDMAHeader_s;
  

/** EtherCAT master dma rx data header 
 */
typedef struct _rxHeader_s 
{
  u16   length      : 12;
  u16   reserved3   : 4;
  
  u16   port        : 8;
  u16   reserved4   : 8;
  
  u32   reserved5;
  
  u64   timestamp;
} rxHeader_s;


/** EtherCAT master dma rx frame 
 */
struct _rxDMAFrame_s 
{
  rxDMAHeader_s   dmaHeader;                /**< frame DMA header */
  rxHeader_s      header;                   /**< frame data header */
  u8              data[RX_FRAME_DATA_SIZE]; /**< frame data */
};



/** tx fifo */
typedef struct _txFifo_s
{
  union
  {
    struct
    {
      u32   queueAddr   : 24;         /**< offset addr of current frame relative to DMA mem 
                                        *  set to address of the frame to add to the fifo */
      u32   queueLen64  : 8;          /**< length of frame in 64bit words (ie (len + 8) / 8) */
    };
    u32   queueFrame;                 /**< use queueFrame to set the queueAddr and queueLen64
                                       *   at the same time */
  };
  u32   reserved1;
  u32   fifoReset   : 8;              /**< reset fifo data (by writing any value) */
  u32   reserved2   : 24;
  u32   reserved3;
} txFifo_s;



/** rx fifo */
typedef struct _rxFifo_s
{
  union
  {
    struct
    {
      u32   queueAddr   : 24;         /**< offset addr of current frame relative to DMA mem
                                        *  set to address of the frame to add to the fifo */
      u32   reserved1   : 7;
      u32   queueValid  : 1;          /**< is the queue addr valid? */
    };
    u32   queueFrame;                 /**< use queueFrame to set the queueAddr and queueValid
                                       *   at the same time */
  };
  union
  {
    struct
    {
      u32   lastAddr    : 24;         /**< ??? doc says this is reserved */
      u32   reserved2   : 8;
    };
    u32   reserved3;
  };
  u32   fifoLevel   : 24;             /**< ??? no doc? fifo reset? */
  u32   bufferLevel : 8;              /**< ??? no doc? */
  u32   nextAddr;                     /**< ??? next address used for received frame */
}  rxFifo_s;



/** private data allocated and stored by the netDev */
typedef struct _cx2100_private_s 
{
  void __iomem       *bar0_addr;
  void __iomem       *bar2_addr;
  struct pci_dev     *pci_dev;
  struct napi_struct  napi;
  struct net_device  *netDev;

  unsigned char      *rxVirtMem;            /**< was rx_ring, rx dma virtual memory */
  dma_addr_t          rxPhysAddr;           /**< device tx dma physical address */
  size_t              rxDMAMemSize;         /**< size of the dma virtual memory allocated */
  int                 rxFrameCnt;           /**< number of available tx frames */
  rxDMAFrame_s       *rxFrames;             /**< first rx frame, can be used as array of frames */
  rxFifo_s           *rxFifo;
  rxDMAFrame_s       *rxHead;
  rxDMAFrame_s       *rxTail;

  unsigned char      *txVirtMem;            /**< was tx_buff, tx dma virtual memory */
  dma_addr_t          txPhysAddr;           /**< device tx dma physical address */
  size_t              txDMAMemSize;         /**< size of the dma virtual memory allocated */
  int                 txFrameCnt;           /**< number of available tx frames */
  txDMAFrame_s       *txFrames;             /**< first tx frame, can be used as array of frames */
  txFifo_s           *txFifo;
  listItem_s          txListFree;
  listItem_s          txListPend;
  int                 txOK;                 /**< are the transmit frames available */
  
  int                 linkOK;

  infoBlock_s        *ecMasterInfoBlock;    /**< The EtherCAT Master Information Block (in BAR0 function list) */
  ecMasterFnBlock_s  *ecMasterFnBlock;      /**< The EtherCAT Master Function Block (in BAR0) */
  ecManagement_s     *ecManagement;         /**< The ec Master Management interface and information */

  ec_device_t        *ecdev;
} cx2100_private_s;




/* forward declarations */
static int cx2100_recycleRxFrame(cx2100_private_s *tp, rxDMAFrame_s *rxFrame);
static int cx2100_getLinkStatus(cx2100_private_s *tp, int forceUpdate);



/** read bar memory at the given offset 
 */
static int readBarMem(
        void __iomem *in_barAddr,
        size_t in_offset,
        size_t in_size,
        void *out_data
        )
{
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "readBarMem");
  
  if (!in_barAddr) return -ENOMEM;
  
  mb();

  switch (in_size)
  {
    case 1 : { *((u8 *)out_data)  = (u8)ioread8(in_barAddr + in_offset); } break;
    case 2 : { *((u16 *)out_data) = (u16)ioread16(in_barAddr + in_offset); } break;
    case 4 : { *((u32 *)out_data) = (u32)ioread32(in_barAddr + in_offset); } break;
    default :
    {
      memcpy(out_data, (void *)(in_barAddr + in_offset), in_size);
    }
  }
  
  return 0;
}


/** write bar memory at the given offset 
 */
static int writeBarMem(
        void __iomem *in_barAddr,
        size_t in_offset,
        size_t in_size,
        void *in_data
        )
{
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "writeBarMem");
  
  if (!in_barAddr) return -ENOMEM;
  
  switch (in_size)
  {
    case 1 : { iowrite8(*(u8 *)in_data, in_barAddr + in_offset); } break;
    case 2 : { iowrite16(*(u16 *)in_data, in_barAddr + in_offset); } break;
    case 4 : { iowrite32(*(u32 *)in_data, in_barAddr + in_offset); } break;
    default :
    {
      memcpy((void *)(in_barAddr + in_offset), in_data, in_size);
    }
  }
  
  mb();

  return 0;
}




/** initialise dma memory
 * 
 * This allocates consistent virtual memory mapped to the DMA memory.
 * First we query the size of the DMA so we know how much memory needs to be 
 * allocated
 */
static int cx2100_initDmaMem(
        cx2100_private_s *tp,         /**< in: private data */
        u8 dmaChannel,                /**< in: the dma channel number */
        unsigned char **dmaVirtMem,   /**< out: pc side virtual memory address */
        dma_addr_t *dmaPhysAddr,      /**< out: hardware side physical address */
        size_t *dmaMemSize,           /**< out: size of coherent memory allocated */
        int *frameCnt,                /**< out: the number of frames that will fit in the memory */
        void **frameHead              /**< out: pointer to first frame in virtual mem */
        )
{
  u32 data   = 0xFFFFFFFF;
  u32 offset = 0x1000 + sizeof(u64) * dmaChannel;
  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_initDmaMem");
  
  // calculate available memory size by writing/reading bar2 dma physical address
  if ( (writeBarMem(tp->bar2_addr, offset, sizeof(data), &data) == 0) &&
       (readBarMem(tp->bar2_addr, offset, sizeof(data), &data) == 0) )
  {
    // calc mem size available
    u32 memTranslate = data & 0xfffffffc;
    u32 memSize      = (~memTranslate) + 1;
    *dmaMemSize      = 2*memSize - FRAME_PAGE_SIZE;

    if (memSize > 0)
    {
      *dmaVirtMem = dma_alloc_coherent(&tp->pci_dev->dev, *dmaMemSize,
                                       dmaPhysAddr, GFP_KERNEL);
    }
    else
    {
      *dmaVirtMem = NULL;
    }
    
    mb();

    if ( (*dmaVirtMem != NULL) && (*dmaPhysAddr != 0) )
    {
      // success, translate memory address
      u32 translateAddr = (*dmaPhysAddr + memSize - FRAME_PAGE_SIZE) & memTranslate;
      u64 addr          = translateAddr;

      // clear dma memory
      memset(*dmaVirtMem, 0, *dmaMemSize);

      // write translated physical address to bar2
      writeBarMem(tp->bar2_addr, offset, sizeof(addr), &addr);

      // calc number of available tx frames
      *frameCnt = memSize / sizeof(txDMAFrame_s);
      
      // set pointer to first frame;
      *frameHead = (void *)(((u32)*dmaVirtMem) + translateAddr - *dmaPhysAddr);
      
      // all good
      return 0;
    }
  }
  
  return -ENOMEM;
}



/** initialise the rx and tx lists
 * 
 * We keep a list of tx frames and send them to the txFifo when we want to 
 * send something.  Once the data is sent we recycle them back into the free list
 * 
 * The rx frames are all sent to the rxFifo so that it can send them to us when
 * there is data to be read.  Once we have read it we send it straight back
 * to the rxFifo
 */
static int cx2100_initRxTxLists(
        cx2100_private_s *tp            /**< in: private data */
        )
{
  int i;
  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_initRxTxLists");
  
  // init ok flags
  tp->txOK = 1;
  
  // calc fifo pointers
  tp->txFifo = ((void *)tp->ecMasterFnBlock) + tp->ecMasterFnBlock->txFifoOffset;
  tp->rxFifo = ((void *)tp->ecMasterFnBlock) + tp->ecMasterFnBlock->txFifoOffset + 0x10;
       
  // reset tx fifo
  tp->txFifo->fifoReset  = 0;
  tp->txFifo->fifoReset  = 1;
  
  // reset rx fifo
  tp->rxFifo->queueFrame = 0;
  tp->rxFifo->fifoLevel  = 0;

  
  // init tx frame lists
  initList(&tp->txListFree);
  initList(&tp->txListPend);

  
  // add all tx frames to the free list 
  for (i = 0; i < tp->txFrameCnt; i++)
  {
    listAddTail(&tp->txListFree, (listItem_s *)&tp->txFrames[i]);
  }
  
  
  // add all rx frames to the rx ring
  for (i = 0; i < tp->rxFrameCnt; i++)
  {
    cx2100_recycleRxFrame(tp, &tp->rxFrames[i]);
  }

  
  return 0;
}



/** open the port, allocate the DMA memory and set up the frames
 */
static int cx2100_open(
        struct net_device *netDev
        )
{
  cx2100_private_s *tp = netdev_priv(netDev);
  int               res;

  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_open");
  
  
  // init tx dma mem
  res = cx2100_initDmaMem(tp, tp->ecMasterInfoBlock->_ecMasterDMA.txDMAChannel,
                          &tp->txVirtMem, &tp->txPhysAddr, &tp->txDMAMemSize, 
                          &tp->txFrameCnt, (void **)&tp->txFrames);
  if (res) return res;

  
  // init rx dma mem
  res = cx2100_initDmaMem(tp, tp->ecMasterInfoBlock->_ecMasterDMA.rxDMAChannel,
                          &tp->rxVirtMem, &tp->rxPhysAddr, &tp->rxDMAMemSize,
                          &tp->rxFrameCnt, (void **)&tp->rxFrames);
  if (res) return res;
  
  
  // init the rx and tx list
  res = cx2100_initRxTxLists(tp);
  if (res) return res;
  
  
  // disable the mac filter to ensure it isn't writable
  tp->ecManagement->macFilter.macFilterEnable = 0;
  
  
  // get initial link status
  cx2100_getLinkStatus(tp, 1);

  
  return 0;
}



/** close the port
 */
static int cx2100_close(
        struct net_device *netDev
        )
{
  cx2100_private_s *tp = netdev_priv(netDev);
  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_close");
  

  // free the dma memory
  if (tp->rxVirtMem)
  {
    dma_free_coherent(&tp->pci_dev->dev, tp->rxDMAMemSize,
                      tp->rxVirtMem, tp->rxPhysAddr);
    tp->rxVirtMem = NULL;
  }
  if (tp->txVirtMem)
  {
    dma_free_coherent(&tp->pci_dev->dev, tp->txDMAMemSize,
                      tp->txVirtMem, tp->txPhysAddr);
    tp->txVirtMem = NULL;
  }
  
  
  // reset tx fifo
  if (tp->txFifo)
  {
    tp->txFifo->fifoReset  = 0;
    tp->txFifo->fifoReset  = 1;
  }
  
  // reset rx fifo
  if (tp->rxFifo)
  {
    tp->rxFifo->queueFrame = 0;
    tp->rxFifo->fifoLevel  = 0;
  }
  
  
  // reset the lists
  tp->txListFree.next = NULL;
  tp->txListFree.prev = NULL;
  
  tp->rxHead = NULL;
  tp->rxTail = NULL;


  return 0;
}


  
/** request a tx frame from the free tx frames list
 * 
 * We send the pointer to it back in terms of the data section
 */
static int cx2100_requestTxFrame(
        cx2100_private_s *tp,
        unsigned int len,
        txDMAFrame_s **txFrame
        )
{
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_requestTxFrame");
  
  if ( (len <= TX_FRAME_DATA_SIZE) && !isListEmpty(&tp->txListFree) )
  {
    // get a free frame
    *txFrame = (txDMAFrame_s *)listRemoveFirstItem(&tp->txListFree);

    MSG_VERBOSE(VERBOSITY_RETVALUE, "Get a free txFrame: 0x%08x", (u32)(*txFrame));
  
    
    // set port 0 (don't know why)
    (*txFrame)->header.port0 = 1;
    
    // txFrame available
    tp->txOK = 1;
    
    return 0;
  }
  else
  {
    if (tp->txOK)
    {
      // only show message on first failure
      tp->txOK = 0;
      
      if (len <= TX_FRAME_DATA_SIZE)
      {
        MSG_ERR("Communication Error: No more transmit frames are available");
      }
    }
  
    *txFrame = NULL;
    
    return -ENOMEM;
  }
}



// Not currently used
///** recycle an unused tx frame (re-adds to free list)
// */
//static int cx2100_recycleTxFrame(
//        cx2100_private_s *tp,
//        void **data
//        )
//{
//  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_recycleTxFrame");
//  
//  // offset from data pointer back to frame pointer and
//  // fill in the fram length
//  txDMAFrame_s *txFrame = (txDMAFrame_s *)((void *)data - offsetof(txDMAFrame_s, data));
//
//  // readd to the free list
//  listAddTail(&tp->txListFree, &txFrame->listItem);
//
//
//  return 0;
//}



/** send a tx frame, queues it to the txFifo and adds it to the pending list
 */
static int cx2100_sendTxFrame(
        cx2100_private_s *tp,
        unsigned int len,
        txDMAFrame_s *txFrame
        )
{
  // calc addr offset to start of frame header (ignoring dma header)
  u32 frameAddr = (u32)&txFrame->header - (u32)tp->txFrames;
  
  // calc the frame length (including header) in 64bit words
  // shift it to the top byte
  // Note: blindly add 8 bits to ensure we are rounding up
  u32 len64Word = ((len + sizeof(txHeader_s) + 8) / 8) << 24;
  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_sendTxFrame");

  MSG_VERBOSE(VERBOSITY_DETAIL, "TX Frame info: txFrame: 0x%08x, frameAddr: %u, frameLen: %u, frameLen64: %u", 
              (u32)txFrame, frameAddr, len, len64Word);
  
  
  mb();

  // fill in the frame length
  txFrame->header.length = (u16)len;

  mb();

  // set start address of frame-header and length in 64 bit words
  // note: using the above memory barrier to ensure these params
  //       are not out of order with this cmd
  tp->txFifo->queueFrame = frameAddr + len64Word;
  
  
  // set some stats
  tp->netDev->stats.tx_bytes   += len;
  tp->netDev->stats.tx_packets++;


  // add the frame to the pending list
  listAddTail(&tp->txListPend, &txFrame->listItem);

  
  return 0;
}


/** clean up any frames that have been sent and are still in the pending list
 * 
 * If the link is down then the fifo will be auto reset, so clear out any
 * pend items regardless of being read
 * 
 * We reset the readAck flag to let the txFifo know that we know the data has
 * been read, and to get it ready for the next use
 */
static void cx2100_cleanupSentTxFrames(
        cx2100_private_s *tp
        )
{
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_cleanupSentTxFrames");
  
  while (!isListEmpty(&tp->txListPend))
  {
    // get first item
    txDMAFrame_s *txFrame = (txDMAFrame_s *)listFirstItem(&tp->txListPend);
    
    MSG_VERBOSE(VERBOSITY_DETAIL, "Check cleanup frame: txFrame: 0x%08x", (u32)txFrame);

    // is it ready to be recycled? (frame has been read or link error)
    if (txFrame->header.readAck || !tp->linkOK)
    {
      MSG_VERBOSE(VERBOSITY_DETAIL, "Recycle frame: txFrame: 0x%08x", (u32)txFrame);
  
      // clear readAck bit
      txFrame->header.readAck = 0;

      // add back to the free list
      listRemoveItem(&txFrame->listItem);
      listAddTail(&tp->txListFree, &txFrame->listItem);
    }
    else
    {
      // frame not sent yet
      MSG_VERBOSE(VERBOSITY_DETAIL, "Frame not sent yet: txFrame: 0x%08x", (u32)txFrame);

      break;
    }
  }
}



/** driver callback function to transmit a frame
 * 
 * The data is passed via a socket buffer.  In the normal course of events
 * the client allocates the buffer and we free it.  However, the ethercat master
 * allocated the buffers once and reuses them, so don't free them.
 */
static netdev_tx_t cx2100_start_xmit(struct sk_buff *skb,
        struct net_device *netDev
        )
{
  cx2100_private_s *tp  = netdev_priv(netDev);
  unsigned int      len = skb->len;
  int               res;
  txDMAFrame_s     *txFrame;
  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_start_xmit");
  

  // get a frame, not if the len is too long then the frame is not returned
  res = cx2100_requestTxFrame(tp, len, &txFrame);
  if (unlikely(res)) 
  {
    MSG_VERBOSE(VERBOSITY_DETAIL, "txFrame not available");
  
    netDev->stats.tx_dropped++;
    return NETDEV_TX_OK;
  }
  
  
  // if the data is less then the min frame len then pre-zero the mem
  if (len < ETH_ZLEN)
  {
    memset(&txFrame->data, 0, ETH_ZLEN);
  }
  
  // copy from skb to frame data
  skb_copy_and_csum_dev(skb, (void *)&txFrame->data);
    
  
  // send the frame
  res = cx2100_sendTxFrame(tp, len, txFrame);
  
  
  netDev->trans_start = jiffies;


  return NETDEV_TX_OK;
}



/** return the received rx frame back to the rxFifo for reuse
 * 
 * We need to set the received flag to let the fifo know we have read the data
 */
static int cx2100_recycleRxFrame(
        cx2100_private_s *tp, 
        rxDMAFrame_s *rxFrame
        )
{  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_recycleRxFrame");
  
  // update last frame to point to this frame and set that it is a valid pointer
  if (tp->rxTail)
  {
    tp->rxTail->dmaHeader.nextFrame = rxFrame;
    tp->rxTail->dmaHeader.nextValid = 1;
  }
  
  // reset the received flag
  rxFrame->dmaHeader.received = 0;
  
  mb();
  
  // tell the fifo to use this frame
  // Note: we are setting the frame offset addr and valid flag in one go
  tp->rxFifo->queueFrame = 0x80000000 | ((u32)rxFrame - (u32)tp->rxFrames);
  
  if (tp->rxHead == NULL)
  {
    // init head
    tp->rxHead = rxFrame;
  }
  
  // set new tail
  tp->rxTail = rxFrame;
  
  
  return 0;
}



/** check for received frames
 */
static int cx2100_rx(
        cx2100_private_s *tp
        )
{ 
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_rx");
  
  // process received frames
  while (tp->rxHead && tp->rxHead->dmaHeader.received)
  {
    rxDMAFrame_s *rxFrame = tp->rxHead;

    // calc frames data length (frame length - data header - CRC)
    // Note: the frame length value does not include the DMA user header
    void   *data = (u8 *)&rxFrame->data;
    size_t  len  = rxFrame->header.length - sizeof(rxHeader_s) - 4;
    
    rmb();
    
    // update the rx head to the next list item
    if (tp->rxHead->dmaHeader.nextValid) tp->rxHead = tp->rxHead->dmaHeader.nextFrame;
    else                                 tp->rxHead = NULL;
    rxFrame->dmaHeader.nextValid = 0;
    

    // let the ec master know we have data
    ecdev_receive(tp->ecdev, data, len);
    
    // update stats
    tp->netDev->last_rx          = jiffies;
    tp->netDev->stats.rx_bytes  += len;
    tp->netDev->stats.rx_packets++;
    
    
    // recycle the frame to the free list
    cx2100_recycleRxFrame(tp, rxFrame);
  }
  
  
  return 0;
}



/** updates whether the link status is connected and return the status
 * 
 * also informs the ec master of any change
 */
static int cx2100_getLinkStatus(
        cx2100_private_s *tp,
        int forceUpdate
        )
{
  int linkOK;
  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_getLinkStatus");
  

  linkOK = (tp->ecManagement->macFilter.linkStatus != 0);
  if ( (tp->linkOK != linkOK) || (forceUpdate) )
  {
    MSG_INFO("Link Status change: %s", linkOK ? "Connected" : "Disconnected");
    tp->linkOK = linkOK;

    // inform the ec master of the new status
    ecdev_set_link(tp->ecdev, linkOK ? 1 : 0);
  }
  
  return linkOK;
}



/** The poller is called by the ec master and does all of the rx fifo work 
 * and cleans up after the tx fifo. 
 */
static void cx2100_poll(struct net_device *netDev)
{
  cx2100_private_s *tp = netdev_priv(netDev);

  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_poll");
  

  if (cx2100_getLinkStatus(tp, 0))
  {
    // check for received frames
    cx2100_rx(tp); 
  }

  // always clean up after any sent tx frames
  // Note: if link is down then the fifo is auto reset and we need to clean 
  // house anyway
  cx2100_cleanupSentTxFrames(tp);
}



/** ensure we have a Beckhoff device with an EtherCAT Master with DMA
 */
static void __devinit cx2100_listBlockInfo(
        infoBlock_s *baseInfoBlock 
        )
{
  int          i;
  infoBlock_s *ecMasterInfoBlock;
  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_listBlockInfo");
  
  
  ecMasterInfoBlock = baseInfoBlock+1;
  for (i = 0; i < baseInfoBlock->_info.fnBlockCount-1; i++)
  {
    switch (ecMasterInfoBlock->fnType)
    {
      case 0x0001 :
      {
        MSG_INFO("  %d: 0x%04x - Base Information Block (unexpected), Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0002 :
      {
        MSG_INFO("  %d: 0x%04x - EtherCAT Slave, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0003 :
      {
        MSG_INFO("  %d: 0x%04x - EtherCAT Master without DMA, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0004 :
      {
        MSG_INFO("  %d: 0x%04x - Ethernet MAC without DMA, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0005 :
      {
        MSG_INFO("  %d: 0x%04x - Ethernet Switch, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0006 :
      {
        MSG_INFO("  %d: 0x%04x - Sercos III, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0007 :
      {
        MSG_INFO("  %d: 0x%04x - Profibus, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0008 :
      {
        MSG_INFO("  %d: 0x%04x - CAN Controller, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0009 :
      {
        MSG_INFO("  %d: 0x%04x - KBUS Master, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x000a :
      {
        MSG_INFO("  %d: 0x%04x - IP-Link Master, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x000b :
      {
        MSG_INFO("  %d: 0x%04x - SPI Master, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x000c :
      {
        MSG_INFO("  %d: 0x%04x - I2C Master, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x000d :
      {
        MSG_INFO("  %d: 0x%04x - GPIO, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x000e :
      {
        MSG_INFO("  %d: 0x%04x - Drive, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x000f :
      {
        MSG_INFO("  %d: 0x%04x - CCAT Update, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0010 :
      {
        MSG_INFO("  %d: 0x%04x - System time, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0011 :
      {
        MSG_INFO("  %d: 0x%04x - Interrupt Controller, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0012 :
      {
        MSG_INFO("  %d: 0x%04x - EEPROM Controller, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0013 :
      {
        MSG_INFO("  %d: 0x%04x - DMA Controller, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0014 :
      {
        MSG_INFO("  %d: 0x%04x - EtherCAT Master with DMA, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0015 :
      {
        MSG_INFO("  %d: 0x%04x - Ethernet MAC with DMA, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0016 :
      {
        MSG_INFO("  %d: 0x%04x - SRAM Interface, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      case 0x0017 :
      {
        MSG_INFO("  %d: 0x%04x - Internal Copy block, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      } break;
      
      default :
      {
        MSG_INFO("  %d: 0x%04x - Unknown function type, Revision: 0x%04x\n", 
                 i, ecMasterInfoBlock->fnType, ecMasterInfoBlock->fnRevision);
      }
    }

    
    // next
    ecMasterInfoBlock++;
  }
}


/** ensure we have a Beckhoff device with an EtherCAT Master with DMA
 */
static int __devinit cx2100_matchDevice(
        infoBlock_s *baseInfoBlock, 
        infoBlock_s **ecMasterInfoBlock
        )
{
  int i;

  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_matchDevice");
  

  // ensure BAR0 type is 0x0001 and that the ID information is correct
  if ( (baseInfoBlock->fnType != 0x0001) ||
       (baseInfoBlock->_info.id1 != 0x000088a4) ||
       (baseInfoBlock->_info.id2 != 0x54414343) )
  {
    // invalid information
    if (baseInfoBlock->fnType != 0x0001)
    {
      MSG_ERR("Invalid base information type (Type: 0x%04x, ID1: 0x%08x, ID2: 0x%08x)\n",
              baseInfoBlock->fnType, baseInfoBlock->_info.id1, baseInfoBlock->_info.id2);
    }
    else if (baseInfoBlock->_info.id1 != 0x000088a4)
    {
      MSG_ERR("Invalid base information id 1 (Type: 0x%04x, ID1: 0x%08x, ID2: 0x%08x)\n",
              baseInfoBlock->fnType, baseInfoBlock->_info.id1, baseInfoBlock->_info.id2);
    }
    else if (baseInfoBlock->_info.id2 != 0x54414343)
    {
      MSG_ERR("Invalid base information id 2 (Type: 0x%04x, ID1: 0x%08x, ID2: 0x%08x)\n",
              baseInfoBlock->fnType, baseInfoBlock->_info.id1, baseInfoBlock->_info.id2);
    }
    
    if (baseInfoBlock->fnType == 0xffff)
    {
      uint8_t *data = (uint8_t *)baseInfoBlock;
      
      MSG_ERR("Invalid base information, Raw data:\n");
      MSG_ERR("  0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
              data[0],
              data[1],
              data[2],
              data[3],
              data[4],
              data[5],
              data[6],
              data[7]);
      MSG_ERR("  0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
              data[8],
              data[9],
              data[10],
              data[11],
              data[12],
              data[13],
              data[14],
              data[15]);
    }
    
    return -ENODEV;
  }

  
  // check the available functions, we are looking for an EtherCAT Master with DMA function
  *ecMasterInfoBlock = baseInfoBlock+1;
  for (i = 0; i < baseInfoBlock->_info.fnBlockCount-1; i++)
  {
    // is it an EtherCAT Master with DMA?
    if ((*ecMasterInfoBlock)->fnType == 0x0014)
    {
      MSG_INFO("Found EtherCAT Master (0x0014), Revision: 0x%04x, txDMA: 0x%02x, rxDMA: 0x%02x\n", 
               (*ecMasterInfoBlock)->fnRevision,
               (*ecMasterInfoBlock)->_ecMasterDMA.txDMAChannel,
               (*ecMasterInfoBlock)->_ecMasterDMA.rxDMAChannel);
      
      if (verbosityLevel >= VERBOSITY_DEVICEINFO)
      {
        cx2100_listBlockInfo(baseInfoBlock);
      }
      
      return 0;
    }
    
    // next
    (*ecMasterInfoBlock)++;
  }
  
  
  // no EtherCAT Master found, list supported functions
  MSG_ERR("EtherCAT master function not found.  This device supports the following functions:\n");
  
  cx2100_listBlockInfo(baseInfoBlock);
  
  
  return -ENODEV;
}
  
  

/** initialise the device and setup the net device and private info etc 
 */
static __devinit struct net_device * cx2100_initNetDev (
        struct pci_dev *dev 
        )
{
  struct net_device *netDev;
  cx2100_private_s  *tp;

  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_initNetDev");
  

  /* netDev and priv zeroed in alloc_etherdev */
  netDev = alloc_etherdev(sizeof (*tp));
  if (netDev == NULL)
  {
    dev_err(&dev->dev, "Unable to alloc new net device\n");
    return ERR_PTR(-ENOMEM);
  }
  SET_NETDEV_DEV(netDev, &dev->dev);

  
  // initialise some values
  netDev->base_addr = pci_resource_start(dev, 0);

  tp = netdev_priv(netDev);
  tp->pci_dev = dev;
  tp->netDev  = netDev;
  

  return netDev;
}


/** netDev operation callback functions */
static const struct net_device_ops cx2100_netdev_ops = {
  .ndo_open          = cx2100_open,
  .ndo_stop          = cx2100_close,
  .ndo_start_xmit    = cx2100_start_xmit,
};

static const struct ethtool_ops cx2100_ethtool_ops = {};



/** probe (install) the device
 * 
 * We check that we have the correct hardware because there are a few Beckhoff
 * devices with the same pci device ids
 */
static int __devinit cx2100_probe(
        struct pci_dev *dev, 
        const struct pci_device_id *id
        )
{
  cx2100_private_s  *tp;
  infoBlock_s       *ecMasterInfoBlock;
  void __iomem      *ioaddr;
  struct net_device *netDev;
  int                wasEnabled;
  int                res;
  int                i;
  res = 0;
  
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_probe");
  
  
  // enable the device so we can talk to it
  // check if the device was already enabled
  wasEnabled = pci_is_enabled(dev);
  if (!wasEnabled)
  {
    res = pci_enable_device(dev);
    if (res) return res;
  }
  
  // request information from the device
  res = pci_request_regions(dev, DRIVER_NAME);
  if (res) goto err_disable;
  
  
  // check bar 0 that we have the correct hardware
  ioaddr = pci_iomap(dev, 0, 0);
  if (!ioaddr)
  {
    dev_err(&dev->dev, "cannot map BAR0, aborting\n");
    res = -EIO;
    goto err_regions;
  }
  
  
  // enable PCI bus-mastering (dma)
  pci_set_master(dev);
  
  
  // check that we have the correct device type (ie contains an EC Master)
  // and return its information block
  res = cx2100_matchDevice(ioaddr, &ecMasterInfoBlock);
  if (res) goto err_bar0;
  
  
  // initialise the net device structure and private data
  netDev = cx2100_initNetDev(dev);
  if (IS_ERR(netDev))
  {
    res = PTR_ERR(netDev);
    goto err_bar0;
  }

  // fill in netdev info
  // Note: we will be using skb_copy_and_csum_dev to calc the checksum
  netDev->netdev_ops      = &cx2100_netdev_ops;
  netDev->ethtool_ops     = &cx2100_ethtool_ops;
  netDev->watchdog_timeo  = TX_TIMEOUT;
  netDev->features       |= NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_HIGHDMA;
  netDev->irq             = dev->irq;

  
  // set up the EtherCAT master
  tp = netdev_priv(netDev);
  tp->bar0_addr         = ioaddr;
  tp->ecMasterInfoBlock = ecMasterInfoBlock;
  tp->ecMasterFnBlock   = tp->bar0_addr + ecMasterInfoBlock->_ecMasterDMA.fnAddrOffset;
  tp->ecManagement      = ((void *)tp->ecMasterFnBlock) + tp->ecMasterFnBlock->miiOffset;
  tp->linkOK            = (tp->ecManagement->macFilter.linkStatus != 0);

  
  // fill in netDev mac address
  for (i = 0; i < 6; i++)
  {
    ((u8 *)(netDev->dev_addr))[i] = tp->ecManagement->macFilter.macAddr[i];
  }
  memcpy(netDev->perm_addr, netDev->dev_addr, netDev->addr_len);
  MSG_INFO("EtherCAT Master, MAC address: %02x:%02x:%02x:%02x:%02x:%02x, Link: %s\n", 
           tp->ecManagement->macFilter.macAddr[0],
           tp->ecManagement->macFilter.macAddr[1],
           tp->ecManagement->macFilter.macAddr[2],
           tp->ecManagement->macFilter.macAddr[3],
           tp->ecManagement->macFilter.macAddr[4],
           tp->ecManagement->macFilter.macAddr[5],
           tp->linkOK ? "Connected" : "Disconnected");
  
  
  // map bar2 IO
  tp->bar2_addr = pci_iomap(dev, 2, 0);
  if (!tp->bar2_addr)
  {
    dev_err(&dev->dev, "cannot map BAR2, aborting\n");
    res = -EIO;
    goto err_netDev;
  }
  
  
  // offer device to EtherCAT master module
  tp->ecdev = ecdev_offer(netDev, cx2100_poll, THIS_MODULE);
  if (!tp->ecdev)
  {
    MSG_WARN("EtherCAT Master device %s (%p) not utilized as an EtherCAT master\n", netDev->name, netDev);
    res = -EBUSY;
    goto err_bar2;
  }
  
  
  // open the device
  if (ecdev_open(tp->ecdev))
  {
    goto err_noMaster;
  }
  

  // set netDev ref in pci device
  pci_set_drvdata(dev, netDev);


  MSG_INFO("module inited\n");
  
  
  // success
  return 0;
  

err_noMaster :
  // not opened by the ec master
  ecdev_withdraw(tp->ecdev);
  tp->ecdev = NULL;
             
err_bar2 :
  // unmap bar2
  pci_iounmap(dev, tp->bar2_addr);
  tp->bar2_addr = NULL;
  tp->bar0_addr = NULL;

err_netDev :
  free_netdev(netDev);

err_bar0 :
  // release dma master
  pci_clear_master(dev); 
  // unmap bar0
  pci_iounmap(dev, ioaddr);
  
err_regions :
  // release regions
  pci_release_regions(dev);
  
err_disable :
  // failure, re-disable the device
  if (!wasEnabled) pci_disable_device(dev);

  
  // return error
  return res;
}



/** remove (shutdown) the device
 */
static void __devexit cx2100_remove(
        struct pci_dev *dev
        )
{
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_remove");
  
  if (dev)
  {
    struct net_device *netDev = pci_get_drvdata(dev);
    
    if (netDev)
    {
      cx2100_private_s *tp = netdev_priv(netDev);
      
    	flush_scheduled_work();

      if (tp)
      {
        if (tp->ecdev)
        {
          ecdev_close(tp->ecdev);
          ecdev_withdraw(tp->ecdev);
        }
        if (tp->bar0_addr)
        {
          pci_iounmap(dev, tp->bar0_addr);
        }
      }
      
      free_netdev(netDev);
    	pci_set_drvdata(dev, NULL);

      // clean up and disable
      pci_release_regions(dev);
      pci_clear_master(dev);  
      pci_disable_device(dev);
    }
  }
  
  MSG_INFO("module removed\n");
}




/** devices the driver handles
 */
struct pci_device_id cx2100_ids[] =
{
  {VENDOR_ID, DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
  {}  // end of list
};



/** driver struct
 */
struct pci_driver cx2100 = 
{
  .name     = DRIVER_NAME,
  .id_table = cx2100_ids,
  .probe    = cx2100_probe,
  .remove   = __devexit_p(cx2100_remove)
};



/** Initialize and register the driver 
 */
static int __init cx2100_init_module(void)
{
  MSG_INFO("EtherCAT-capable Beckhoff CX2100 EtherCAT Network Driver (Revision 2)\n");
  
  
  // check out if we should be in verbose mode
  if (verbosityLevel <= 0)
  {
    MSG_INFO("CX2100 Verbose Mode Off\n");
  }
  else
  {
    MSG_INFO("CX2100 Verbose Mode On, Level %d\n", verbosityLevel);
  }

  
  return pci_register_driver(&cx2100);
}



/** Unregister the driver
 */
static void __exit cx2100_cleanup_module(void)
{
  MSG_VERBOSE(VERBOSITY_FUNCTIONCALL, "Function Call: %s", "cx2100_cleanup_module");
  
  pci_unregister_driver(&cx2100);
}



module_init(cx2100_init_module);
module_exit(cx2100_cleanup_module);

