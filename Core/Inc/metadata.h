#ifndef METADATA_H_
#define METADATA_H_

#include "stdint.h" 

typedef enum
{
    ACTIVE_APP_A = 0xA,
    ACTIVE_APP_B = 0xB
} METADATA_ActiveApp_Typedef;


typedef struct 
{
    uint32_t Hostsize_AppA;
    uint32_t Hostcrc_AppA;
    uint32_t version_AppA;

    uint32_t Hostsize_AppB;
    uint32_t Hostcrc_AppB;
    uint32_t version_AppB;

    uint32_t HistoryCmd;

} METADATA_SectorDef_t;

typedef struct 
{
    METADATA_ActiveApp_Typedef activeBank;
    uint32_t pFlag;
} METADATA_ActiveBank_t;

#define METADATA_SECTOR_ADDR            (0x8008000UL)
#define METADATA_ACTIVE_BANK_ADDR       (0x8008400UL)


#endif 