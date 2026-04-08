#include "metadata.h"

METADATA_SectorDef_t metadata_sector __attribute__((section(".metadata"))) = {
    .Hostcrc_AppA = 0,
    .Hostsize_AppA = 0,
    .version_AppA = 0,


    .Hostcrc_AppB = 0,
    .Hostsize_AppB = 0,
    .version_AppB = 0,
};

METADATA_ActiveBank_t metadata_active_bank __attribute__((section(".active_bank_section"))) = 
{
    .activeBank = 0xA,
    .pFlag = 1
};