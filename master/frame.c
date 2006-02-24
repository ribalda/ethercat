/******************************************************************************
 *
 *  f r a m e . c
 *
 *  Methoden für einen EtherCAT-Frame.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <linux/slab.h>
#include <linux/delay.h>

#include "../include/EtherCAT_si.h"
#include "frame.h"
#include "master.h"

/*****************************************************************************/

#define EC_FUNC_HEADER \
    frame->master = master; \
    frame->state = ec_frame_ready; \
    frame->index = 0; \
    frame->working_counter = 0;

#define EC_FUNC_WRITE_FOOTER \
    frame->data_length = length; \
    memcpy(frame->data, data, length);

#define EC_FUNC_READ_FOOTER \
    frame->data_length = length; \
    memset(frame->data, 0x00, length);

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-NPRD-Kommando.

   Node-adressed physical read.
*/

void ec_frame_init_nprd(ec_frame_t *frame,
                        /**< EtherCAT-Rahmen */
                        ec_master_t *master,
                        /**< EtherCAT-Master */
                        uint16_t node_address,
                        /**< Adresse des Knotens (Slaves) */
                        uint16_t offset,
                        /**< Physikalische Speicheradresse im Slave */
                        unsigned int length
                        /**< Länge der zu lesenden Daten */
                        )
{
    if (unlikely(node_address == 0x0000))
        printk(KERN_WARNING "EtherCAT: Warning - Using node address 0x0000!\n");

    EC_FUNC_HEADER;

    frame->type = ec_frame_type_nprd;
    frame->address.physical.slave = node_address;
    frame->address.physical.mem = offset;

    EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-NPWR-Kommando.

   Node-adressed physical write.
*/

void ec_frame_init_npwr(ec_frame_t *frame,
                        /**< EtherCAT-Rahmen */
                        ec_master_t *master,
                        /**< EtherCAT-Master */
                        uint16_t node_address,
                        /**< Adresse des Knotens (Slaves) */
                        uint16_t offset,
                        /**< Physikalische Speicheradresse im Slave */
                        unsigned int length,
                        /**< Länge der zu schreibenden Daten */
                        const uint8_t *data
                        /**< Zeiger auf Speicher mit zu schreibenden Daten */
                        )
{
    if (unlikely(node_address == 0x0000))
        printk(KERN_WARNING "EtherCAT: Warning - Using node address 0x0000!\n");

    EC_FUNC_HEADER;

    frame->type = ec_frame_type_npwr;
    frame->address.physical.slave = node_address;
    frame->address.physical.mem = offset;

    EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-APRD-Kommando.

   Autoincrement physical read.
*/

void ec_frame_init_aprd(ec_frame_t *frame,
                        /**< EtherCAT-Rahmen */
                        ec_master_t *master,
                        /**< EtherCAT-Master */
                        uint16_t ring_position,
                        /**< Position des Slaves im Bus */
                        uint16_t offset,
                        /**< Physikalische Speicheradresse im Slave */
                        unsigned int length
                        /**< Länge der zu lesenden Daten */
                        )
{
    EC_FUNC_HEADER;

    frame->type = ec_frame_type_aprd;
    frame->address.physical.slave = (int16_t) ring_position * (-1);
    frame->address.physical.mem = offset;

    EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-APWR-Kommando.

   Autoincrement physical write.
*/

void ec_frame_init_apwr(ec_frame_t *frame,
                        /**< EtherCAT-Rahmen */
                        ec_master_t *master,
                        /**< EtherCAT-Master */
                        uint16_t ring_position,
                        /**< Position des Slaves im Bus */
                        uint16_t offset,
                        /**< Physikalische Speicheradresse im Slave */
                        unsigned int length,
                        /**< Länge der zu schreibenden Daten */
                        const uint8_t *data
                        /**< Zeiger auf Speicher mit zu schreibenden Daten */
                        )
{
    EC_FUNC_HEADER;

    frame->type = ec_frame_type_apwr;
    frame->address.physical.slave = (int16_t) ring_position * (-1);
    frame->address.physical.mem = offset;

    EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-BRD-Kommando.

   Broadcast read.
*/

void ec_frame_init_brd(ec_frame_t *frame,
                       /**< EtherCAT-Rahmen */
                       ec_master_t *master,
                       /**< EtherCAT-Master */
                       uint16_t offset,
                       /**< Physikalische Speicheradresse im Slave */
                       unsigned int length
                       /**< Länge der zu lesenden Daten */
                       )
{
    EC_FUNC_HEADER;

    frame->type = ec_frame_type_brd;
    frame->address.physical.slave = 0x0000;
    frame->address.physical.mem = offset;

    EC_FUNC_READ_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-BWR-Kommando.

   Broadcast write.
*/

void ec_frame_init_bwr(ec_frame_t *frame,
                       /**< EtherCAT-Rahmen */
                       ec_master_t *master,
                       /**< EtherCAT-Master */
                       uint16_t offset,
                       /**< Physikalische Speicheradresse im Slave */
                       unsigned int length,
                       /**< Länge der zu schreibenden Daten */
                       const uint8_t *data
                       /**< Zeiger auf Speicher mit zu schreibenden Daten */
                       )
{
    EC_FUNC_HEADER;

    frame->type = ec_frame_type_bwr;
    frame->address.physical.slave = 0x0000;
    frame->address.physical.mem = offset;

    EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Initialisiert ein EtherCAT-LRW-Kommando.

   Logical read write.
*/

void ec_frame_init_lrw(ec_frame_t *frame,
                       /**< EtherCAT-Rahmen */
                       ec_master_t *master,
                       /**< EtherCAT-Master */
                       uint32_t offset,
                       /**< Logische Startadresse */
                       unsigned int length,
                       /**< Länge der zu lesenden/schreibenden Daten */
                       uint8_t *data
                       /**< Zeiger auf die Daten */
                       )
{
    EC_FUNC_HEADER;

    frame->type = ec_frame_type_lrw;
    frame->address.logical = offset;

    EC_FUNC_WRITE_FOOTER;
}

/*****************************************************************************/

/**
   Sendet einen einzelnen EtherCAT-Rahmen.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_frame_send(ec_frame_t *frame /**< Rahmen zum Senden */)
{
    unsigned int command_size, frame_size, i;
    uint8_t *data;

    if (unlikely(frame->master->debug_level > 0)) {
        printk(KERN_DEBUG "EtherCAT: ec_frame_send\n");
    }

    if (unlikely(frame->state != ec_frame_ready)) {
        printk(KERN_WARNING "EtherCAT: Frame not in \"ready\" state!\n");
    }

    command_size = frame->data_length + EC_COMMAND_HEADER_SIZE
        + EC_COMMAND_FOOTER_SIZE;
    frame_size = command_size + EC_FRAME_HEADER_SIZE;

    if (unlikely(frame_size > EC_MAX_FRAME_SIZE)) {
        printk(KERN_ERR "EtherCAT: Frame too long (%i)!\n", frame_size);
        return -1;
    }

    if (frame_size < EC_MIN_FRAME_SIZE) frame_size = EC_MIN_FRAME_SIZE;

    if (unlikely(frame->master->debug_level > 0)) {
        printk(KERN_DEBUG "EtherCAT: Frame length: %i\n", frame_size);
    }

    frame->index = frame->master->command_index;
    frame->master->command_index = (frame->master->command_index + 1) % 0x0100;

    if (unlikely(frame->master->debug_level > 0)) {
        printk(KERN_DEBUG "EtherCAT: Sending command index 0x%X\n",
               frame->index);
    }

    frame->state = ec_frame_sent;

    // Zeiger auf Socket-Buffer holen
    data = ec_device_prepare(&frame->master->device);

    // EtherCAT frame header
    EC_WRITE_U16(data, (command_size & 0x7FF) | 0x1000);
    data += EC_FRAME_HEADER_SIZE;

    // EtherCAT command header
    EC_WRITE_U8 (data,     frame->type);
    EC_WRITE_U8 (data + 1, frame->index);
    EC_WRITE_U32(data + 2, frame->address.logical);
    EC_WRITE_U16(data + 6, frame->data_length & 0x7FF);
    EC_WRITE_U16(data + 8, 0x0000);
    data += EC_COMMAND_HEADER_SIZE;

    // EtherCAT command data
    memcpy(data, frame->data, frame->data_length);
    data += frame->data_length;

    // EtherCAT command footer
    EC_WRITE_U16(data, frame->working_counter);
    data += EC_COMMAND_FOOTER_SIZE;

    // Pad with zeros
    for (i = EC_FRAME_HEADER_SIZE + EC_COMMAND_HEADER_SIZE
             + frame->data_length + EC_COMMAND_FOOTER_SIZE;
         i < EC_MIN_FRAME_SIZE; i++)
        EC_WRITE_U8(data++, 0x00);

    // Send frame
    ec_device_send(&frame->master->device, frame_size);

    return 0;
}

/*****************************************************************************/

/**
   Empfängt einen gesendeten Rahmen.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_frame_receive(ec_frame_t *frame /**< Gesendeter Rahmen */)
{
    unsigned int received_length, frame_length, data_length;
    uint8_t *data;
    uint8_t command_type, command_index;
    ec_device_t *device;

    if (unlikely(frame->state != ec_frame_sent)) {
        printk(KERN_ERR "EtherCAT: Frame was not sent!\n");
        return -1;
    }

    device = &frame->master->device;

    if (!(received_length = ec_device_received(device))) return -1;

    device->state = EC_DEVICE_STATE_READY;

    if (unlikely(received_length < EC_FRAME_HEADER_SIZE)) {
        printk(KERN_ERR "EtherCAT: Received frame with incomplete EtherCAT"
               " frame header!\n");
        ec_frame_print(frame);
        return -1;
    }

    data = ec_device_data(device);

    // Länge des gesamten Frames prüfen
    frame_length = EC_READ_U16(data) & 0x07FF;
    data += EC_FRAME_HEADER_SIZE;

    if (unlikely(frame_length > received_length)) {
        printk(KERN_ERR "EtherCAT: Received corrupted frame (length does"
               " not match)!\n");
        ec_frame_print(frame);
        return -1;
    }

    // Command header
    command_type =  EC_READ_U8(data);
    command_index = EC_READ_U8(data + 1);
    data_length =   EC_READ_U16(data + 6) & 0x07FF;
    data += EC_COMMAND_HEADER_SIZE;

    if (unlikely(EC_FRAME_HEADER_SIZE + EC_COMMAND_HEADER_SIZE
                 + data_length + EC_COMMAND_FOOTER_SIZE > received_length)) {
        printk(KERN_ERR "EtherCAT: Received frame with incomplete command"
               " data!\n");
        ec_frame_print(frame);
        return -1;
    }

    if (unlikely(frame->type != command_type
                 || frame->index != command_index
                 || frame->data_length != data_length))
    {
        printk(KERN_WARNING "EtherCAT: WARNING - Send/Receive anomaly!\n");
        ec_frame_print(frame); // FIXME uninteressant...
        ec_device_call_isr(device); // Empfangenes "vergessen"
        return -1;
    }

    frame->state = ec_frame_received;

    // Empfangene Daten in Kommandodatenspeicher kopieren
    memcpy(frame->data, data, data_length);
    data += data_length;

    // Working-Counter setzen
    frame->working_counter = EC_READ_U16(data);

    if (unlikely(frame->master->debug_level > 1)) {
        ec_frame_print(frame);
    }

    return 0;
}

/*****************************************************************************/

/**
   Sendet einen einzeln Rahmen und wartet auf dessen Empfang.

   \return 0 bei Erfolg, sonst < 0
*/

int ec_frame_send_receive(ec_frame_t *frame
                          /**< Rahmen zum Senden/Empfangen */
                          )
{
    unsigned int tries_left;

    if (unlikely(ec_frame_send(frame) < 0)) {
        printk(KERN_ERR "EtherCAT: Frame sending failed!\n");
        return -1;
    }

    tries_left = 20;
    do
    {
        udelay(1);
        ec_device_call_isr(&frame->master->device);
        tries_left--;
    }
    while (unlikely(!ec_device_received(&frame->master->device)
                    && tries_left));

    if (unlikely(!tries_left)) {
        printk(KERN_ERR "EtherCAT: Frame timeout!\n");
        return -1;
    }

    if (unlikely(ec_frame_receive(frame) < 0)) {
        printk(KERN_ERR "EtherCAT: Frame receiving failed!\n");
        return -1;
    }

    return 0;
}

/*****************************************************************************/

/**
   Gibt Frame-Inhalte zwecks Debugging aus.
*/

void ec_frame_print(const ec_frame_t *frame /**< EtherCAT-Frame */)
{
    unsigned int i;

    printk(KERN_DEBUG "EtherCAT: Frame contents (%i Bytes):\n",
           frame->data_length);

    printk(KERN_DEBUG);
    for (i = 0; i < frame->data_length; i++)
    {
        printk("%02X ", frame->data[i]);
        if ((i + 1) % 16 == 0) printk("\n" KERN_DEBUG);
    }
    printk("\n");
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
