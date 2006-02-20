/******************************************************************************
 *
 *  E t h e r C A T _ s i . h
 *
 *  EtherCAT Slave-Interface.
 *
 *  $Id$
 *
 *****************************************************************************/

#define EC_PROC_DATA(SLAVE) ((unsigned char *) ((SLAVE)->process_data))

/*****************************************************************************/

#define EC_READ_EL10XX(SLAVE, CHANNEL) \
    ((EC_PROC_DATA(SLAVE)[0] >> (CHANNEL)) & 0x01)

/*****************************************************************************/

#define EC_WRITE_EL200X(SLAVE, CHANNEL, VALUE) \
    do { \
        if (VALUE) EC_PROC_DATA(SLAVE)[0] |=  (1 << (CHANNEL)); \
        else       EC_PROC_DATA(SLAVE)[0] &= ~(1 << (CHANNEL)); \
    } while (0)

/*****************************************************************************/

#define EC_READ_EL310X(SLAVE, CHANNEL) \
    ((signed short int) ((EC_PROC_DATA(SLAVE)[(CHANNEL) * 3 + 2] << 8) | \
                          EC_PROC_DATA(SLAVE)[(CHANNEL) * 3 + 1]))

#define EC_READ_EL316X(SLAVE, CHANNEL) \
    ((unsigned short int) ((EC_PROC_DATA(SLAVE)[(CHANNEL) * 3 + 2] << 8) | \
                            EC_PROC_DATA(SLAVE)[(CHANNEL) * 3 + 1]))

/*****************************************************************************/

#define EC_WRITE_EL410X(SLAVE, CHANNEL, VALUE) \
    do { \
        EC_PROC_DATA(SLAVE)[(CHANNEL) * 3 + 1] = ((VALUE) & 0xFF00) >> 8; \
        EC_PROC_DATA(SLAVE)[(CHANNEL) * 3 + 2] =  (VALUE) & 0xFF; \
    } while (0)

/*****************************************************************************/

#define EC_CONF_EL5001_BAUD (0x4067)

#define EC_READ_EL5001_STATE(SLAVE) \
    ((unsigned char) EC_PROC_DATA(SLAVE)[0])

#define EC_READ_EL5001_VALUE(SLAVE) \
    ((unsigned int) (EC_PROC_DATA(SLAVE)[1] | \
                     (EC_PROC_DATA(SLAVE)[2] << 8) | \
                     (EC_PROC_DATA(SLAVE)[3] << 16) | \
                     (EC_PROC_DATA(SLAVE)[4] << 24)))

/*****************************************************************************/

#define EC_READ_EL5101_STATE(SLAVE) \
    ((unsigned char) EC_PROC_DATA(SLAVE)[0])

#define EC_READ_EL5101_VALUE(SLAVE) \
    ((unsigned int) (EC_PROC_DATA(SLAVE)[1] | \
                     (EC_PROC_DATA(SLAVE)[2] << 8)))

#define EC_READ_EL5101_LATCH(SLAVE) \
    ((unsigned int) (EC_PROC_DATA(SLAVE)[3] | \
                     (EC_PROC_DATA(SLAVE)[4] << 8)))

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
