/****************************************************************
 *
 *  e c _ d e v i c e . c
 *
 *  Methoden für ein EtherCAT-Gerät.
 *
 *  $Date$
 *  $Author$
 *
 ***************************************************************/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/delay.h>

#include "ec_device.h"

/***************************************************************/

/**
   EtherCAT-Geräte-Konstuktor.

   Initialisiert ein EtherCAT-Gerät, indem es die Variablen
   in der Struktur auf die Default-Werte setzt.

   @param ecd Zu initialisierendes EtherCAT-Gerät
*/

void EtherCAT_device_init(EtherCAT_device_t *ecd)
{
  ecd->dev = NULL;
  ecd->tx_skb = NULL;
  ecd->rx_skb = NULL;
  ecd->tx_time = 0;
  ecd->rx_time = 0;
  ecd->tx_intr_cnt = 0;
  ecd->rx_intr_cnt = 0;
  ecd->intr_cnt = 0;
  ecd->state = ECAT_DS_READY;
  ecd->rx_data_length = 0;
  ecd->isr = NULL;
  ecd->module = NULL;
}

/***************************************************************/

/**
   EtherCAT-Geräte-Destuktor.

   Gibt den dynamisch allozierten Speicher des
   EtherCAT-Gerätes (die beiden Socket-Buffer) wieder frei.

   @param ecd EtherCAT-Gerät
*/

void EtherCAT_device_clear(EtherCAT_device_t *ecd)
{
  ecd->dev = NULL;

  if (ecd->tx_skb)
  {
    dev_kfree_skb(ecd->tx_skb);
    ecd->tx_skb = NULL;
  }

  if (ecd->rx_skb)
  {
    dev_kfree_skb(ecd->rx_skb);
    ecd->rx_skb = NULL;
  }
}

/***************************************************************/

/**
   Weist einem EtherCAT-Gerät das entsprechende net_device zu.

   Prüft das net_device, allokiert Socket-Buffer in Sende- und
   Empfangsrichtung und weist dem EtherCAT-Gerät und den
   Socket-Buffern das net_device zu.

   @param ecd EtherCAT-Device
   @param dev net_device

   @return 0: Erfolg, < 0: Konnte Socket-Buffer nicht allozieren.
*/

int EtherCAT_device_assign(EtherCAT_device_t *ecd,
                           struct net_device *dev)
{
  if (!dev)
  {
    printk("EtherCAT: Device is NULL!\n");
    return -1;
  }

  if ((ecd->tx_skb = dev_alloc_skb(ECAT_FRAME_BUFFER_SIZE)) == NULL)
  {
    printk(KERN_ERR "EtherCAT: Could not allocate device tx socket buffer!\n");
    return -1;
  }

  if ((ecd->rx_skb = dev_alloc_skb(ECAT_FRAME_BUFFER_SIZE)) == NULL)
  {
    dev_kfree_skb(ecd->tx_skb);
    ecd->tx_skb = NULL;

    printk(KERN_ERR "EtherCAT: Could not allocate device rx socket buffer!\n");
    return -1;
  }

  ecd->dev = dev;
  ecd->tx_skb->dev = dev;
  ecd->rx_skb->dev = dev;

  printk("EtherCAT: Assigned Device %X.\n", (unsigned) dev);

  return 0;
}

/***************************************************************/

/**
   Führt die open()-Funktion des Netzwerktreibers aus.

   Dies entspricht einem "ifconfig up". Vorher wird der Zeiger
   auf das EtherCAT-Gerät auf Gültigkeit geprüft und der
   Gerätezustand zurückgesetzt.

   @param ecd EtherCAT-Gerät

   @return 0 bei Erfolg, < 0: Ungültiger Zeiger, oder open()
   fehlgeschlagen
*/

int EtherCAT_device_open(EtherCAT_device_t *ecd)
{
  if (!ecd)
  {
    printk(KERN_ERR "EtherCAT: Trying to open a NULL device!\n");
    return -1;
  }

  if (!ecd->dev)
  {
    printk(KERN_ERR "EtherCAT: No net_device to open!\n");
    return -1;
  }

  // Reset old device state
  ecd->state = ECAT_DS_READY;
  ecd->tx_intr_cnt = 0;
  ecd->rx_intr_cnt = 0;

  return ecd->dev->open(ecd->dev);
}

/***************************************************************/

/**
   Führt die stop()-Funktion des net_devices aus.

   @param ecd EtherCAT-Gerät

   @return 0 bei Erfolg, < 0: Kein Gerät zum Schliessen
*/

int EtherCAT_device_close(EtherCAT_device_t *ecd)
{
  if (!ecd->dev)
  {
    printk("EtherCAT: No device to close!\n");
    return -1;
  }

  printk("EtherCAT: Stopping device (txcnt: %u, rxcnt: %u)\n",
         (unsigned int) ecd->tx_intr_cnt,
         (unsigned int) ecd->rx_intr_cnt);

  return ecd->dev->stop(ecd->dev);
}

/***************************************************************/

/**
   Sendet einen Rahmen über das EtherCAT-Gerät.

   Kopiert die zu sendenden Daten in den statischen Socket-
   Buffer, fügt den Ethernat-II-Header hinzu und ruft die
   start_xmit()-Funktion der Netzwerkkarte auf.

   @param ecd EtherCAT-Gerät
   @param data Zeiger auf die zu sendenden Daten
   @param length Länge der zu sendenden Daten

   @return 0 bei Erfolg, < 0: Vorheriger Rahmen noch
   nicht empfangen, oder kein Speicher mehr vorhanden
*/

int EtherCAT_device_send(EtherCAT_device_t *ecd,
                         unsigned char *data,
                         unsigned int length)
{
  unsigned char *frame_data;
  struct ethhdr *eth;

  if (ecd->state == ECAT_DS_SENT)
  {
    printk(KERN_WARNING "EtherCAT: Trying to send frame while last was not received!\n");
  }

  skb_trim(ecd->tx_skb, 0); // Clear transmit socket buffer
  skb_reserve(ecd->tx_skb, ETH_HLEN); // Reserve space for Ethernet-II header

  // Copy data to socket buffer
  frame_data = skb_put(ecd->tx_skb, length);
  memcpy(frame_data, data, length);

  // Add Ethernet-II-Header
  if ((eth = (struct ethhdr *) skb_push(ecd->tx_skb, ETH_HLEN)) == NULL)
  {
    printk(KERN_ERR "EtherCAT: device_send - Could not allocate Ethernet-II header!\n");
    return -1;
  }

  eth->h_proto = htons(0x88A4); // Protocol type
  memcpy(eth->h_source, ecd->dev->dev_addr, ecd->dev->addr_len); // Hardware address
  memset(eth->h_dest, 0xFF, ecd->dev->addr_len); // Broadcast address

  rdtscl(ecd->tx_time); // Get CPU cycles

  // Start sending of frame
  ecd->state = ECAT_DS_SENT;
  ecd->dev->hard_start_xmit(ecd->tx_skb, ecd->dev);

  return 0;
}

/***************************************************************/

/**
   Holt einen empfangenen Rahmen von der Netzwerkkarte.

   Zuerst wird geprüft, ob überhaupt ein Rahmen empfangen
   wurde. Wenn ja, wird diesem mit Hilfe eines Spin-Locks
   in den angegebenen Speicherbereich kopiert.

   @param ecd EtherCAT-Gerät
   @param data Zeiger auf den Speicherbereich, in den die
               empfangenen Daten kopiert werden sollen

   @return Anzahl der kopierten Bytes bei Erfolg, sonst < 0
*/

int EtherCAT_device_receive(EtherCAT_device_t *ecd,
                            unsigned char *data)
{
  if (ecd->state != ECAT_DS_RECEIVED)
  {
    printk(KERN_ERR "EtherCAT: receive - Nothing received!\n");
    return -1;
  }

  if (ecd->rx_data_length > ECAT_FRAME_BUFFER_SIZE)
  {
    printk(KERN_ERR "EtherCAT: receive - Reveived frame too long (%i Bytes)!\n",
           ecd->rx_data_length);
    return -1;
  }

  memcpy(data, ecd->rx_data, ecd->rx_data_length);

  return ecd->rx_data_length;
}

/***************************************************************/

/**
   Ruft manuell die Interrupt-Routine der Netzwerkkarte auf.

   @param ecd EtherCAT-Gerät

   @return Anzahl der kopierten Bytes bei Erfolg, sonst < 0
*/

void EtherCAT_device_call_isr(EtherCAT_device_t *ecd)
{
    if (ecd->isr) ecd->isr(0, ecd->dev, NULL);
}

/***************************************************************/

/**
   Gibt alle Informationen über das Device-Objekt aus.

   @param ecd EtherCAT-Gerät
*/

void EtherCAT_device_debug(EtherCAT_device_t *ecd)
{
  printk(KERN_DEBUG "---EtherCAT device information begin---\n");

  if (ecd)
  {
    printk(KERN_DEBUG "Assigned net_device: %X\n", (unsigned) ecd->dev);
    printk(KERN_DEBUG "Transmit socket buffer: %X\n", (unsigned) ecd->tx_skb);
    printk(KERN_DEBUG "Receive socket buffer: %X\n", (unsigned) ecd->rx_skb);
    printk(KERN_DEBUG "Time of last transmission: %u\n", (unsigned) ecd->tx_time);
    printk(KERN_DEBUG "Time of last receive: %u\n", (unsigned) ecd->rx_time);
    printk(KERN_DEBUG "Number of transmit interrupts: %u\n", (unsigned) ecd->tx_intr_cnt);
    printk(KERN_DEBUG "Number of receive interrupts: %u\n", (unsigned) ecd->rx_intr_cnt);
    printk(KERN_DEBUG "Total Number of interrupts: %u\n", (unsigned) ecd->intr_cnt);
    printk(KERN_DEBUG "Actual device state: %i\n", (int) ecd->state);
    printk(KERN_DEBUG "Receive buffer: %X\n", (unsigned) ecd->rx_data);
    printk(KERN_DEBUG "Receive buffer fill state: %u/%u\n",
           (unsigned) ecd->rx_data_length, ECAT_FRAME_BUFFER_SIZE);
  }
  else
  {
    printk(KERN_DEBUG "Device is NULL!\n");
  }

  printk(KERN_DEBUG "---EtherCAT device information end---\n");
}

/***************************************************************/

EXPORT_SYMBOL(EtherCAT_device_init);
EXPORT_SYMBOL(EtherCAT_device_clear);
EXPORT_SYMBOL(EtherCAT_device_assign);
EXPORT_SYMBOL(EtherCAT_device_open);
EXPORT_SYMBOL(EtherCAT_device_close);

/***************************************************************/
