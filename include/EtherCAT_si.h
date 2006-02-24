/******************************************************************************
 *
 *  E t h e r C A T _ s i . h
 *
 *  EtherCAT Slave-Interface.
 *
 *  $Id$
 *
 *****************************************************************************/

#include <asm/byteorder.h>

/*****************************************************************************/

// Bitwise read/write macros

#define EC_READ_BIT(PD, CH) (*((uint8_t *) (PD)) >> (CH)) & 0x01)

#define EC_WRITE_BIT(PD, CH, VAL) \
    do { \
        if (VAL) *((uint8_t *) (PD)) |=  (1 << (CH)); \
        else     *((uint8_t *) (PD)) &= ~(1 << (CH)); \
    } while (0)

/*****************************************************************************/

// Read macros

#define EC_READ_U8(PD) ((uint8_t) *((uint8_t *) (PD)))
#define EC_READ_S8(PD) ((int8_t)  *((uint8_t *) (PD)))

#define EC_READ_U16(PD) ((uint16_t) le16_to_cpup((void *) (PD)))
#define EC_READ_S16(PD) ((int16_t)  le16_to_cpup((void *) (PD)))

#define EC_READ_U32(PD) ((uint32_t) le32_to_cpup((void *) (PD)))
#define EC_READ_S32(PD) ((int32_t)  le32_to_cpup((void *) (PD)))

/*****************************************************************************/

// Write macros

#define EC_WRITE_U8(PD, VAL) \
    do { \
        *((uint8_t *)(PD)) = ((uint8_t) (VAL)); \
    } while (0)

#define EC_WRITE_S8(PD, VAL) EC_WRITE_U8(PD, VAL)

#define EC_WRITE_U16(PD, VAL) \
    do { \
        *((uint16_t *) (PD)) = (uint16_t) (VAL); \
        cpu_to_le16s(PD); \
    } while (0)

#define EC_WRITE_S16(PD, VAL) EC_WRITE_U16(PD, VAL)

#define EC_WRITE_U32(PD, VAL) \
    do { \
        *((uint32_t *) (PD)) = (uint32_t) (VAL); \
        cpu_to_le16s(PD); \
    } while (0)

#define EC_WRITE_S32(PD, VAL) EC_WRITE_U32(PD, VAL)

/*****************************************************************************/

/* Emacs-Konfiguration
;;; Local Variables: ***
;;; c-basic-offset:4 ***
;;; End: ***
*/
