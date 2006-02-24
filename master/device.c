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
#include "master.h"

/*****************************************************************************/

/**
   EtherCAT-Geräte-Konstuktor.
*/

int ec_device_init(ec_device_t *device, /**< EtherCAT-Gerät */
                   ec_master_t *master /**< Zugehöriger Master */
                   )
{
    device->master = master;
    device->dev = NULL;
    device->open = 0;
    device->tx_time = 0;
    device->rx_time = 0;
    device->state = EC_DEVICE_STATE_READY;
    device->rx_data_size = 0;
    device->isr = NULL;
    device->module = NULL;
    device->error_reported = 0;

    if ((device->tx_skb = dev_alloc_skb(ETH_HLEN + EC_MAX_FRAME_SIZE)) == NULL) {
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
*/

void ec_device_clear(ec_device_t *device /**< EtherCAT-Gerät */)
{
    if (device->open) ec_device_close(device);

    device->dev = NULL;

    if (device->tx_skb) {
        dev_kfree_skb(device->tx_skb);
        device->tx_skb = NULL;
    }
}

/*****************************************************************************/

/**
   Führt die open()-Funktion des Netzwerktreibers aus.

   Dies entspricht einem "ifconfig up". Vorher wird der Zeiger
   auf das EtherCAT-Gerät auf Gültigkeit geprüft und der
   Gerätezustand zurückgesetzt.

   \return 0 bei Erfolg, < 0: Ungültiger Zeiger, oder open()
           fehlgeschlagen
*/

int ec_device_open(ec_device_t *device /**< EtherCAT-Gerät */)
{
    unsigned int i;

    if (!device) {
        printk(KERN_ERR "EtherCAT: Trying to open a NULL device!\n");
        return -1;
    }

    if (!device->dev) {
        printk(KERN_ERR "EtherCAT: No net_device to open!\n");
        return -1;
    }

    if (device->open) {
        printk(KERN_WARNING "EtherCAT: Device already opened!\n");
    }
    else {
        // Device could have received frames before
        for (i = 0; i < 4; i++) ec_device_call_isr(device);

        // Reset old device state
        device->state = EC_DEVICE_STATE_READY;

        if (device->dev->open(device->dev) == 0) device->open = 1;
    }

    return device->open ? 0 : -1;
}

/*****************************************************************************/

/**
   Führt die stop()-Funktion des net_devices aus.

   \return 0 bei Erfolg, < 0: Kein Gerät zum Schliessen oder
   Schliessen fehlgeschlagen.
*/

int ec_device_close(ec_device_t *device /**< EtherCAT-Gerät */)
{
    if (!device->dev) {
        printk(KERN_ERR "EtherCAT: No device to close!\n");
        return -1;
    }

    if (!device->open) {
        printk(KERN_WARNING "EtherCAT: Device already closed!\n");
    }
    else {
        if (device->dev->stop(device->dev) == 0) device->open = 0;
    }

    return !device->open ? 0 : -1;
}

/*****************************************************************************/

/**
   Bereitet den geräteinternen Socket-Buffer auf den Versand vor.

   \return Zeiger auf den Speicher, in den die Frame-Daten sollen.
*/

uint8_t *ec_device_prepare(ec_device_t *device /**< EtherCAT-Gerät */)
{
    // Clear transmit socket buffer and reserve space for Ethernet-II header
    skb_trim(device->tx_skb, 0);
    skb_reserve(device->tx_skb, ETH_HLEN);

    // Erstmal Speicher für maximal langen Frame reservieren
    return skb_put(device->tx_skb, EC_MAX_FRAME_SIZE);
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

void ec_device_send(ec_device_t *device, /**< EtherCAT-Gerät */
                    unsigned int length /**< Länge der zu sendenden Daten */
                    )
{
    struct ethhdr *eth;

    // Framegroesse auf (jetzt bekannte) Laenge abschneiden
    skb_trim(device->tx_skb, length);

    // Ethernet-II-Header hinzufuegen
    eth = (struct ethhdr *) skb_push(device->tx_skb, ETH_HLEN);
    eth->h_proto = htons(0x88A4);
    memcpy(eth->h_source, device->dev->dev_addr, device->dev->addr_len);
    memset(eth->h_dest, 0xFF, device->dev->addr_len);

    device->state = EC_DEVICE_STATE_SENT;
    device->rx_data_size = 0;

    if (unlikely(device->master->debug_level > 1)) {
        printk(KERN_DEBUG "EtherCAT: Sending frame:\n");
        ec_data_print(device->tx_skb->data + ETH_HLEN, device->tx_skb->len);
    }

    // Senden einleiten
    rdtscl(device->tx_time); // Get CPU cycles
    device->dev->hard_start_xmit(device->tx_skb, device->dev);
}

/*****************************************************************************/

/**
   Gibt die Anzahl der empfangenen Bytes zurück.

   \return Empfangene Bytes, oder 0, wenn kein Frame empfangen wurde.
*/

unsigned int ec_device_received(const ec_device_t *device)
{
    return device->rx_data_size;
}

/*****************************************************************************/

/**
   Gibt die empfangenen Daten zurück.

   \return Adresse auf empfangene Daten.
*/

uint8_t *ec_device_data(ec_device_t *device)
{
    return device->rx_data;
}

/*****************************************************************************/

/**
   Ruft die Interrupt-Routine der Netzwerkkarte auf.
*/

void ec_device_call_isr(ec_device_t *device /**< EtherCAT-Gerät */)
{
    if (likely(device->isr)) device->isr(0, device->dev, NULL);
}

/*****************************************************************************/

/**
   Gibt alle Informationen über das Device-Objekt aus.
*/

void ec_device_print(ec_device_t *device /**< EtherCAT-Gerät */)
{
    printk(KERN_DEBUG "---EtherCAT device information begin---\n");

    if (device)
    {
        printk(KERN_DEBUG "Assigned net_device: %X\n",
               (unsigned) device->dev);
        printk(KERN_DEBUG "Transmit socket buffer: %X\n",
               (unsigned) device->tx_skb);
        printk(KERN_DEBUG "Time of last transmission: %u\n",
               (unsigned) device->tx_time);
        printk(KERN_DEBUG "Time of last receive: %u\n",
               (unsigned) device->rx_time);
        printk(KERN_DEBUG "Actual device state: %i\n",
               (int) device->state);
        printk(KERN_DEBUG "Receive buffer: %X\n",
               (unsigned) device->rx_data);
        printk(KERN_DEBUG "Receive buffer fill state: %u/%u\n",
               (unsigned) device->rx_data_size, EC_MAX_FRAME_SIZE);
    }
    else
    {
        printk(KERN_DEBUG "Device is NULL!\n");
    }

    printk(KERN_DEBUG "---EtherCAT device information end---\n");
}

/*****************************************************************************/

/**
   Gibt das letzte Rahmenpaar aus.
*/

void ec_device_debug(const ec_device_t *device /**< EtherCAT-Gerät */)
{
    printk(KERN_DEBUG "EtherCAT: >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    ec_data_print(device->tx_skb->data + ETH_HLEN, device->tx_skb->len);
    printk(KERN_DEBUG "------------------------------------------------\n");
    ec_data_print_diff(device->tx_skb->data + ETH_HLEN, device->rx_data,
                       device->rx_data_size);
    printk(KERN_DEBUG "EtherCAT: <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus.
*/

void ec_data_print(const uint8_t *data /**< Daten */,
                   size_t size /**< Anzahl Bytes */
                   )
{
    size_t i;

    printk(KERN_DEBUG);
    for (i = 0; i < size; i++) {
        printk("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printk("\n" KERN_DEBUG);
    }
    printk("\n");
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus, differentiell.
*/

void ec_data_print_diff(const uint8_t *d1, /**< Daten 1 */
                        const uint8_t *d2, /**< Daten 2 */
                        size_t size /** Anzahl Bytes */
                        )
{
    size_t i;

    printk(KERN_DEBUG);
    for (i = 0; i < size; i++) {
        if (d1[i] == d2[i]) printk(".. ");
        else printk("%02X ", d2[i]);
        if ((i + 1) % 16 == 0) printk("\n" KERN_DEBUG);
    }
    printk("\n");
}

/******************************************************************************
 *
 * Treiberschnittstelle
 *
 *****************************************************************************/

/**
   Setzt den Zustand des EtherCAT-Gerätes.
*/

void EtherCAT_dev_state(ec_device_t *device,  /**< EtherCAT-Gerät */
                        ec_device_state_t state /**< Neuer Zustand */
                        )
{
    device->state = state;
}

/*****************************************************************************/

/**
   Prüft, ob das Net-Device \a dev zum registrierten EtherCAT-Gerät gehört.
*/

int EtherCAT_dev_is_ec(const ec_device_t *device,  /**< EtherCAT-Gerät */
                       const struct net_device *dev /**< Net-Device */
                       )
{
    return device && device->dev == dev;
}

/*****************************************************************************/

void EtherCAT_dev_receive(ec_device_t *device, const void *data, size_t size)
{
    // Copy received data to ethercat-device buffer
    memcpy(device->rx_data, data, size);
    device->rx_data_size = size;
    device->state = EC_DEVICE_STATE_RECEIVED;

    if (unlikely(device->master->debug_level > 1)) {
        printk(KERN_DEBUG "EtherCAT: Received frame:\n");
        ec_data_print_diff(device->tx_skb->data + ETH_HLEN, device->rx_data,
                           device->rx_data_size);
    }
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
