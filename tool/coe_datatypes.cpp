/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include "coe_datatypes.h"

/****************************************************************************/

static const CoEDataType dataTypes[] = {
    {"int8",   0x0002, 1},
    {"int16",  0x0003, 2},
    {"int32",  0x0004, 4},
    {"uint8",  0x0005, 1},
    {"uint16", 0x0006, 2},
    {"uint32", 0x0007, 4},
    {"string", 0x0009, 0},
    {"raw",    0xffff, 0},
    {}
};

/****************************************************************************/

const CoEDataType *findDataType(const string &str)
{
    const CoEDataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (str == d->name)
            return d;

    return NULL;
}

/****************************************************************************/

const CoEDataType *findDataType(uint16_t code)
{
    const CoEDataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (code == d->coeCode)
            return d;

    return NULL;
}

/*****************************************************************************/
