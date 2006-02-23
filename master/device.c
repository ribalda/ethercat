/******************************************************************************
 *
 *  d e v i c e . c
 *
 *  Methoden für ein EtherCAT-Gerät.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/delay.h>

#include "device.h"

/*****************************************************************************/

/**
   EtherCAT-Geräte-Konstuktor.

   Initialisiert ein EtherCAT-Gerät, indem es die Variablen
   in der Struktur auf die Default-Werte setzt.

   @param ecd Zu initialisierendes EtherCAT-Gerät
*/

int ec_device_init(ec_device_t *ecd)
{
    ecd->dev = NULL;
    ecd->open = 0;
    ecd->tx_time = 0;
    ecd->rx_time = 0;
    ecd->state = EC_DEVICE_STATE_READY;
    ecd->rx_data_length = 0;
    ecd->isr = NULL;
    ecd->module = NULL;
    ecd->error_reported = 0;

    if ((ecd->tx_skb = dev_alloc_skb(ETH_HLEN + EC_MAX_FRAME_SIZE)) == NULL) {
        printk(KERN_ERR "EtherCAT: Error allocating device socket buffer!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   EtherCAT-Geräte-Destuktor.

   Gibt den dynamisch allozierten Speicher des
   EtherCAT-Gerätes (die beiden Socket-Buffer) wieder frei.

   @param ecd EtherCAT-Gerät
*/

void ec_device_clear(ec_device_t *ecd)
{
    if (ecd->open) ec_device_close(ecd);

    ecd->dev = NULL;

    if (ecd->tx_skb) {
        dev_kfree_skb(ecd->tx_skb);
        ecd->tx_skb = NULL;
    }
}

/*****************************************************************************/

/**
   Führt die open()-Funktion des Netzwerktreibers aus.

   Dies entspricht einem "ifconfig up". Vorher wird der Zeiger
   auf das EtherCAT-Gerät auf Gültigkeit geprüft und der
   Gerätezustand zurückgesetzt.

   @param ecd EtherCAT-Gerät

   @return 0 bei Erfolg, < 0: Ungültiger Zeiger, oder open()
   fehlgeschlagen
*/

int ec_device_open(ec_device_t *ecd)
{
    unsigned int i;

    if (!ecd) {
        printk(KERN_ERR "EtherCAT: Trying to open a NULL device!\n");
        return -1;
    }

    if (!ecd->dev) {
        printk(KERN_ERR "EtherCAT: No net_device to open!\n");
        return -1;
    }

    if (ecd->open) {
        printk(KERN_WARNING "EtherCAT: Device already opened!\n");
    }
    else {
        // Device could have received frames before
        for (i = 0; i < 4; i++) ec_device_call_isr(ecd);

        // Reset old device state
        ecd->state = EC_DEVICE_STATE_READY;

        if (ecd->dev->open(ecd->dev) == 0) ecd->open = 1;
    }

    return ecd->open ? 0 : -1;
}

/*****************************************************************************/

/**
   Führt die stop()-Funktion des net_devices aus.

   @param ecd EtherCAT-Gerät

   @return 0 bei Erfolg, < 0: Kein Gerät zum Schliessen oder
   Schliessen fehlgeschlagen.
*/

int ec_device_close(ec_device_t *ecd)
{
    if (!ecd->dev) {
        printk(KERN_ERR "EtherCAT: No device to close!\n");
        return -1;
    }

    if (!ecd->open) {
        printk(KERN_WARNING "EtherCAT: Device already closed!\n");
    }
    else {
        if (ecd->dev->stop(ecd->dev) == 0) ecd->open = 0;
    }

    return !ecd->open ? 0 : -1;
}

/*****************************************************************************/

/**
   Bereitet den geräteinternen Socket-Buffer auf den Versand vor.

   \return Zeiger auf den Speicher, in den die Frame-Daten sollen.
*/

uint8_t *ec_device_prepare(ec_device_t *ecd /**< EtherCAT-Gerät */)
{
    // Clear transmit socket buffer and reserve space for Ethernet-II header
    skb_trim(ecd->tx_skb, 0);
    skb_reserve(ecd->tx_skb, ETH_HLEN);

    // Erstmal Speicher für maximal langen Frame reservieren
    return skb_put(ecd->tx_skb, EC_MAX_FRAME_SIZE);
}

/*****************************************************************************/

/**
   Sendet einen Rahmen über das EtherCAT-Gerät.

   Kopiert die zu sendenden Daten in den statischen Socket-
   Buffer, fügt den Ethernat-II-Header hinzu und ruft die
   start_xmit()-Funktion der Netzwerkkarte auf.

   \return 0 bei Erfolg, < 0: Vorheriger Rahmen noch
   nicht empfangen, oder kein Speicher mehr vorhanden
*/

void ec_device_send(ec_device_t *ecd, /**< EtherCAT-Gerät */
                    unsigned int length /**< Länge der zu sendenden Daten */
                    )
{
    struct ethhdr *eth;

    // Framegroesse auf (jetzt bekannte) Laenge abschneiden
    skb_trim(ecd->tx_skb, length);

    // Ethernet-II-Header hinzufuegen
    eth = (struct ethhdr *) skb_push(ecd->tx_skb, ETH_HLEN);
    eth->h_proto = htons(0x88A4);
    memcpy(eth->h_source, ecd->dev->dev_addr, ecd->dev->addr_len);
    memset(eth->h_dest, 0xFF, ecd->dev->addr_len);

    ecd->state = EC_DEVICE_STATE_SENT;
    ecd->rx_data_length = 0;

    // Senden einleiten
    rdtscl(ecd->tx_time); // Get CPU cycles
    ecd->dev->hard_start_xmit(ecd->tx_skb, ecd->dev);
}

/*****************************************************************************/

/**
   Gibt die Anzahl der empfangenen Bytes zurück.

   \return Empfangene Bytes, oder 0, wenn kein Frame empfangen wurde.
*/

unsigned int ec_device_received(const ec_device_t *ecd)
{
    return ecd->rx_data_length;
}

/*****************************************************************************/

/**
   Gibt die empfangenen Daten zurück.

   \return Adresse auf empfangene Daten.
*/

uint8_t *ec_device_data(ec_device_t *ecd)
{
    return ecd->rx_data;
}

/*****************************************************************************/

/**
   Ruft die Interrupt-Routine der Netzwerkkarte auf.

   @param ecd EtherCAT-Gerät

   @return Anzahl der kopierten Bytes bei Erfolg, sonst < 0
*/

void ec_device_call_isr(ec_device_t *ecd)
{
    if (likely(ecd->isr)) ecd->isr(0, ecd->dev, NULL);
}

/*****************************************************************************/

/**
   Gibt alle Informationen über das Device-Objekt aus.

   @param ecd EtherCAT-Gerät
*/

void ec_device_print(ec_device_t *ecd)
{
    printk(KERN_DEBUG "---EtherCAT device information begin---\n");

    if (ecd)
    {
        printk(KERN_DEBUG "Assigned net_device: %X\n",
               (unsigned) ecd->dev);
        printk(KERN_DEBUG "Transmit socket buffer: %X\n",
               (unsigned) ecd->tx_skb);
        printk(KERN_DEBUG "Time of last transmission: %u\n",
               (unsigned) ecd->tx_time);
        printk(KERN_DEBUG "Time of last receive: %u\n",
               (unsigned) ecd->rx_time);
        printk(KERN_DEBUG "Actual device state: %i\n",
               (int) ecd->state);
        printk(KERN_DEBUG "Receive buffer: %X\n",
               (unsigned) ecd->rx_data);
        printk(KERN_DEBUG "Receive buffer fill state: %u/%u\n",
               (unsigned) ecd->rx_data_length, EC_MAX_FRAME_SIZE);
    }
    else
    {
        printk(KERN_DEBUG "Device is NULL!\n");
    }

    printk(KERN_DEBUG "---EtherCAT device information end---\n");
}

/******************************************************************************
 *
 * Treiberschnittstelle
 *
 *****************************************************************************/

void EtherCAT_dev_state(ec_device_t *ecd, ec_device_state_t state)
{
    ecd->state = state;
}

/*****************************************************************************/

int EtherCAT_dev_is_ec(ec_device_t *ecd, struct net_device *dev)
{
    return ecd && ecd->dev == dev;
}

/*****************************************************************************/

void EtherCAT_dev_receive(ec_device_t *ecd, void *data, unsigned int size)
{
    // Copy received data to ethercat-device buffer
    memcpy(ecd->rx_data, data, size);
    ecd->rx_data_length = size;
    ecd->state = EC_DEVICE_STATE_RECEIVED;
}

/*****************************************************************************/

EXPORT_SYMBOL(EtherCAT_dev_is_ec);
EXPORT_SYMBOL(EtherCAT_dev_state);
EXPORT_SYMBOL(EtherCAT_dev_receive);

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
