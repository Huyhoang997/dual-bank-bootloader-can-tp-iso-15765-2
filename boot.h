#ifndef BOOT_H_
#define BOOT_H_

#include "main.h"
#include "metadata.h"
#include "print_log.h"
#include <stdbool.h>


typedef enum
{
    BOOTLOADER_OK,
    BOOTLOADER_ERR,
    BOOTLOADER_INVALID_CMD,
    BOOTLOADER_ERASE_ERR,
    BOOTLOADER_PROGRAM_ERR,
    BOOTLOADER_CAN_ERR,
    BOOTLOADER_CAN_TIMEOUT,
    BOOTLOADER_CHECK_CRC_FAILD
} BOOTLOADER_Status_Typedef;


#define APP_A_START_ADDR            (0x08008800UL)
#define APP_A_RESET_HANDLER_ADDR    (APP_A_START_ADDR + 4)

#define APP_B_START_ADDR            (0x0800C400UL)
#define APP_B_RESET_HANDLER_ADDR    (APP_B_START_ADDR + 4)

#define HOST_CAN_ID                 0x34U

#define BOOTLOADER_WRITE_CMD        0x01U
#define BOOTLOADER_ERASE_CMD        0x02U

void BOOTLOADER_Init(void);
BOOTLOADER_Status_Typedef BOOTLOADER_SetActiveBank(METADATA_ActiveApp_Typedef active_bank);
BOOTLOADER_Status_Typedef BOOTLOADER_SetPendingFlag(bool pFlag);

#endif
