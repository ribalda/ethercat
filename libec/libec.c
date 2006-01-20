/******************************************************************************
 *
 *  l i b e c . c
 *
 *  EtherCAT-Library fuer Echtzeitmodule
 *
 *  $Id$
 *
 *****************************************************************************/

#include "libec.h"

/*****************************************************************************/

int LEC_read_EL10XX(ec_slave_t *slave, unsigned int channel)
{
    unsigned char *data = slave->process_data;

    return (data[0] >> channel) & 0x01;
}

/*****************************************************************************/

int LEC_read_EL31XX(ec_slave_t *slave, unsigned int channel)
{
    unsigned char *data = slave->process_data;

    return (short int) ((data[channel * 3 + 2] << 8) | data[channel * 3 + 1]);
}

/*****************************************************************************/

void LEC_write_EL20XX(ec_slave_t *slave, unsigned int channel, int value)
{
    unsigned char *data = slave->process_data;

    if (value) data[0] |= (1 << channel);
    else data[0] &= ~(1 << channel);
}

/*****************************************************************************/

void LEC_write_EL41XX(ec_slave_t *slave, unsigned int channel, int value)
{
    unsigned char *data = slave->process_data;

    data[channel * 3 + 1] = (value & 0xFF00) >> 8;
    data[channel * 3 + 2] = value & 0xFF;
}

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
