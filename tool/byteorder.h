/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <sys/types.h>

/*****************************************************************************/

#define swap16(x) \
        ((uint16_t)( \
        (((uint16_t)(x) & 0x00ffU) << 8) | \
        (((uint16_t)(x) & 0xff00U) >> 8) ))
#define swap32(x) \
        ((uint32_t)( \
        (((uint32_t)(x) & 0x000000ffUL) << 24) | \
        (((uint32_t)(x) & 0x0000ff00UL) <<  8) | \
        (((uint32_t)(x) & 0x00ff0000UL) >>  8) | \
        (((uint32_t)(x) & 0xff000000UL) >> 24) ))

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define le16tocpu(x) x
#define le32tocpu(x) x

#define cputole16(x) x
#define cputole32(x) x

#elif __BYTE_ORDER == __BIG_ENDIAN

#define le16tocpu(x) swap16(x)
#define le32tocpu(x) swap32(x)

#define cputole16(x) swap16(x)
#define cputole32(x) swap32(x)

#endif

/****************************************************************************/
