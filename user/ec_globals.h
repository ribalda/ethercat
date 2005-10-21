//---------------------------------------------------------------
//
//  e c _ g l o b a l s . h
//
//  $LastChangedDate$
//  $Author$
//
//---------------------------------------------------------------

#define EC_CMD_APRD 0x01 // Auto-increment physical read
#define EC_CMD_APWR 0x02 // Auto-increment physical write
#define EC_CMD_NPRD 0x04 // Node-addressed physical read
#define EC_CMD_NPWR 0x05 // Node-addressed physical write
#define EC_CMD_BRD  0x07 // Broadcast read
#define EC_CMD_BWR  0x08 // Broadcast write
#define EC_CMD_LRW  0x0C // Logical read/write

#define EC_STATE_UNKNOWN 0x00
#define EC_STATE_INIT    0x01
#define EC_STATE_PREOP   0x02
#define EC_STATE_SAVEOP  0x04
#define EC_STATE_OP      0x08

#define EC_ACK_STATE 0x10

//---------------------------------------------------------------
