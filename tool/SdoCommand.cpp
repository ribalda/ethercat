/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include "SdoCommand.h"

/*****************************************************************************/

SdoCommand::SdoCommand(const string &name, const string &briefDesc):
    Command(name, briefDesc)
{
}

/****************************************************************************/

const SdoCommand::DataType *SdoCommand::findDataType(const string &str)
{
    const DataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (str == d->name)
            return d;

    return NULL;
}

/****************************************************************************/

const SdoCommand::DataType *SdoCommand::findDataType(uint16_t code)
{
    const DataType *d;
    
    for (d = dataTypes; d->name; d++)
        if (code == d->coeCode)
            return d;

    return NULL;
}

/****************************************************************************/

const char *SdoCommand::abortText(uint32_t abortCode)
{
    const AbortMessage *abortMsg;

    for (abortMsg = abortMessages; abortMsg->code; abortMsg++) {
        if (abortMsg->code == abortCode) {
            return abortMsg->message;
        }
    }

    return "???";
}

/****************************************************************************/

const SdoCommand::DataType SdoCommand::dataTypes[] = {
    {"int8",         0x0002, 1},
    {"int16",        0x0003, 2},
    {"int32",        0x0004, 4},
    {"uint8",        0x0005, 1},
    {"uint16",       0x0006, 2},
    {"uint32",       0x0007, 4},
    {"string",       0x0009, 0},
    {"octet_string", 0x000a, 0},
    {"raw",          0xffff, 0},
    {}
};

/*****************************************************************************/

/** SDO abort messages.
 *
 * The "Abort SDO transfer request" supplies an abort code, which can be
 * translated to clear text. This table does the mapping of the codes and
 * messages.
 */
const SdoCommand::AbortMessage SdoCommand::abortMessages[] = {
    {0x05030000, "Toggle bit not changed"},
    {0x05040000, "SDO protocol timeout"},
    {0x05040001, "Client/Server command specifier not valid or unknown"},
    {0x05040005, "Out of memory"},
    {0x06010000, "Unsupported access to an object"},
    {0x06010001, "Attempt to read a write-only object"},
    {0x06010002, "Attempt to write a read-only object"},
    {0x06020000, "This object does not exist in the object directory"},
    {0x06040041, "The object cannot be mapped into the PDO"},
    {0x06040042, "The number and length of the objects to be mapped would"
     " exceed the PDO length"},
    {0x06040043, "General parameter incompatibility reason"},
    {0x06040047, "Gerneral internal incompatibility in device"},
    {0x06060000, "Access failure due to a hardware error"},
    {0x06070010, "Data type does not match, length of service parameter does"
     " not match"},
    {0x06070012, "Data type does not match, length of service parameter too"
     " high"},
    {0x06070013, "Data type does not match, length of service parameter too"
     " low"},
    {0x06090011, "Subindex does not exist"},
    {0x06090030, "Value range of parameter exceeded"},
    {0x06090031, "Value of parameter written too high"},
    {0x06090032, "Value of parameter written too low"},
    {0x06090036, "Maximum value is less than minimum value"},
    {0x08000000, "General error"},
    {0x08000020, "Data cannot be transferred or stored to the application"},
    {0x08000021, "Data cannot be transferred or stored to the application"
     " because of local control"},
    {0x08000022, "Data cannot be transferred or stored to the application"
     " because of the present device state"},
    {0x08000023, "Object dictionary dynamic generation fails or no object"
     " dictionary is present"},
    {}
};

/****************************************************************************/
