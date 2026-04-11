#include "metadata.h"

METADATA_SectorDef_t metadata_sector __attribute__((section(".metadata"))) = {
    .Hostcrc_AppA = 5,
    .Hostsize_AppA = 6,
    .version_AppA = 7,


    .Hostcrc_AppB = 8,
    .Hostsize_AppB = 9,
    .version_AppB = 0,
};

METADATA_ActiveBank_t metadata_active_bank __attribute__((section(".active_bank_section"))) = 
{
    .activeBank = 0xB,
    .pFlag = 0
};
