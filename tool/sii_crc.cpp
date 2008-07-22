/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include "sii_crc.h"

/*****************************************************************************/

/** Calculates the SII checksum field.
 *
 * The checksum is generated with the polynom x^8+x^2+x+1 (0x07) and an
 * initial value of 0xff (see IEC 61158-6-12 ch. 5.4).
 *
 * The below code was originally generated with PYCRC
 * http://www.tty1.net/pycrc
 *
 * ./pycrc.py --width=8 --poly=0x07 --reflect-in=0 --xor-in=0xff
 *   --reflect-out=0 --xor-out=0 --generate c --algorithm=bit-by-bit
 *
 * \return CRC8
 */
uint8_t calcSiiCrc(
        const uint8_t *data, /**< pointer to data */
        size_t length /**< number of bytes in \a data */
        )
{
    unsigned int i;
    uint8_t bit, byte, crc = 0x48;

    while (length--) {
        byte = *data++;
        for (i = 0; i < 8; i++) {
            bit = crc & 0x80;
            crc = (crc << 1) | ((byte >> (7 - i)) & 0x01);
            if (bit) crc ^= 0x07;
        }
    }

    for (i = 0; i < 8; i++) {
        bit = crc & 0x80;
        crc <<= 1;
        if (bit) crc ^= 0x07;
    }

    return crc;
}

/*****************************************************************************/
