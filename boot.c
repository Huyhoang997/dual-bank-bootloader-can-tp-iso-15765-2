#include "boot.h"

uint32_t test[4];

static void JUMP_To_App_A(void)
{
    volatile uint32_t app_A_vector_table_addr = *(volatile uint32_t *)APP_A_START_ADDR;
    volatile uint32_t app_A_reset_handler_addr = *(volatile uint32_t *)APP_A_RESET_HANDLER_ADDR;

    __disable_irq();
    HAL_DeInit();
    __set_MSP(app_A_vector_table_addr);
    void (*Funcptr)() = (void *)app_A_reset_handler_addr;
    SCB->VTOR = APP_A_START_ADDR;
    __enable_irq();
    Funcptr();
}

static void JUMP_To_App_B(void)
{
    volatile uint32_t app_B_vector_table_addr = *(volatile uint32_t *)APP_B_START_ADDR;
    volatile uint32_t app_B_reset_handler_addr = *(volatile uint32_t *)APP_B_RESET_HANDLER_ADDR;

    __disable_irq();
    HAL_DeInit();
    __set_MSP(app_B_vector_table_addr);
    void (*Funcptr)() = (void *)app_B_reset_handler_addr;
    SCB->VTOR = APP_B_START_ADDR;
    __enable_irq();
    Funcptr();
}

BOOTLOADER_Status_Typedef BOOTLOADER_SetActiveBank(METADATA_ActiveApp_Typedef active_bank)
{
    FLASH_EraseInitTypeDef erase_sector;
    METADATA_ActiveBank_t *checkActive = (METADATA_ActiveBank_t *)METADATA_ACTIVE_BANK_ADDR;
    uint32_t recoverData[2];
    HAL_StatusTypeDef checkStatus;

    erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_sector.PageAddress = METADATA_ACTIVE_BANK_ADDR;
    erase_sector.NbPages = 1;
    uint32_t error_page = 0;

    recoverData[0] = active_bank;
    recoverData[1] = checkActive->pFlag;

    HAL_FLASH_Unlock();

    checkStatus = HAL_FLASHEx_Erase(&erase_sector, &error_page);
    if(checkStatus != HAL_OK)
    {
        return BOOTLOADER_ERASE_ERR;
    }

    for(uint8_t i = 0; i < 2; i++)
    {
        checkStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_ACTIVE_BANK_ADDR + 4*i), recoverData[i]);
        if(checkStatus != HAL_OK)
        {
            return BOOTLOADER_ERASE_ERR;
        }
    }

    HAL_FLASH_Lock();
    return BOOTLOADER_OK;
}


BOOTLOADER_Status_Typedef BOOTLOADER_SetPendingFlag(bool pFlag)
{
    FLASH_EraseInitTypeDef erase_sector;
    METADATA_ActiveBank_t *checkActive = (METADATA_ActiveBank_t *)METADATA_ACTIVE_BANK_ADDR;
    uint32_t recoverData[2];
    HAL_StatusTypeDef checkStatus;

    erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_sector.PageAddress = METADATA_ACTIVE_BANK_ADDR;
    erase_sector.NbPages = 1;
    uint32_t error_page = 0;

    recoverData[0] = checkActive->activeBank;
    recoverData[1] = pFlag;

    HAL_FLASH_Unlock();

    checkStatus = HAL_FLASHEx_Erase(&erase_sector, &error_page);
    if(checkStatus != HAL_OK)
    {
        return BOOTLOADER_PROGRAM_ERR;
    }

    for(uint8_t i = 0; i < 2; i++)
    {
        checkStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_ACTIVE_BANK_ADDR + 4*i), recoverData[i]);
        if(checkStatus != HAL_OK)
        {
            return BOOTLOADER_PROGRAM_ERR;
        }
    }

    HAL_FLASH_Lock();
    return BOOTLOADER_OK;
}


BOOTLOADER_Status_Typedef BOOTLOADER_SaveInfoFrame(const uint8_t data[])
{
    METADATA_SectorDef_t *metadata = (METADATA_SectorDef_t *)METADATA_SECTOR_ADDR;
    METADATA_ActiveBank_t *checkActive = (METADATA_ActiveBank_t *)METADATA_ACTIVE_BANK_ADDR;
    FLASH_EraseInitTypeDef erase_sector;
    uint32_t error_page = 0;

    uint32_t recoverData[7];
    uint8_t host_cmd = data[0];
    uint16_t host_size = ((uint16_t)data[1] << 8) | data[2];
    uint32_t host_crc = ((uint32_t)data[3] << 24) | ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 8 | data[6]);
    uint8_t version = data[7];
    test[0] = host_cmd;
    test[1] = host_size;
    test[2] = host_crc;
    test[3] = version;

    HAL_FLASH_Unlock();
    
    switch (checkActive->activeBank)
    {
        case ACTIVE_APP_A:
                erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;
                erase_sector.PageAddress = METADATA_SECTOR_ADDR;
                erase_sector.NbPages = 1;

        break;

        case ACTIVE_APP_B:
                erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;
                erase_sector.PageAddress = METADATA_SECTOR_ADDR;
                erase_sector.NbPages = 1;
        break;
    
    default:
        break;
    }

    HAL_FLASH_Lock();

    return BOOTLOADER_OK;
}

static void BOOTLOADER_ReceiveInfoFrame()
{
    CAN_RxHeaderTypeDef Rx_config;
    uint8_t rxdata[8] = {0};

    while(HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_FILTER_FIFO0 )  == 0);
    HAL_CAN_GetRxMessage(&hcan, CAN_FILTER_FIFO0 , &Rx_config, rxdata);
    switch (rxdata[0])
    {
        case BOOTLOADER_WRITE_CMD:

        BOOTLOADER_SaveInfoFrame(rxdata);
        mPrintf("Waiting for new firmware...\n");
        //BOOTLOADER_ReceiveFirmware()
        break;

        case BOOTLOADER_ERASE_CMD:
        mPrintf("Erasing current firmware...\n");
        break;
    
    default:
        mPrintf("Unknow Command!\n");
        break;
    }

    
}

static void JUMP_To_Boot(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);

    BOOTLOADER_ReceiveInfoFrame();
}


void BOOTLOADER_Init(void)
{
    METADATA_ActiveBank_t *checkActive = (METADATA_ActiveBank_t *)METADATA_ACTIVE_BANK_ADDR;
    METADATA_SectorDef_t *metadata = (METADATA_SectorDef_t *)METADATA_SECTOR_ADDR;
    uint32_t check_crc_App = 0, firmwareLength = 0;

    if(!(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)))
    {
        mPrintf("Enter Bootloader mode!\n");
        JUMP_To_Boot();
    }
    else
    {
        switch (checkActive->activeBank)
        {
            case  ACTIVE_APP_A:
                firmwareLength = metadata->Hostsize_AppB / 4;
                if(metadata->Hostsize_AppB % 4)
                {
                    firmwareLength++;
                }
                check_crc_App = HAL_CRC_Calculate(&hcrc, (uint32_t *)APP_B_START_ADDR, firmwareLength);


                if(checkActive->pFlag == 0)
                {
                    mPrintf("Current application firmware: %.X\n", checkActive->activeBank);
                    JUMP_To_App_A();
                }
                else if(checkActive->pFlag == 1 && metadata->Hostcrc_AppB == check_crc_App && metadata->Hostsize_AppB > 0)
                {
                    BOOTLOADER_SetActiveBank(ACTIVE_APP_B);
                    HAL_Delay(1);
                    BOOTLOADER_SetPendingFlag(false);

                    mPrintf("New firmware detected!\nJumping to new application..\n");
                    mPrintf("Current application firmware: %.X\n", checkActive->activeBank); 
                    JUMP_To_App_B();
                }
                else
                {
                    mPrintf("CRC check mismatch!\nRoll back to older firmware...\n");
                    mPrintf("Current application firmware: %.X\n", checkActive->activeBank); 
                    JUMP_To_App_A();
                }
            break;


            case  ACTIVE_APP_B:
                firmwareLength = metadata->Hostsize_AppA / 4;
                if(metadata->Hostsize_AppA % 4)
                {
                    firmwareLength++;
                }
                check_crc_App = HAL_CRC_Calculate(&hcrc, (uint32_t *)APP_A_START_ADDR, firmwareLength);

                if(checkActive->pFlag == 0)
                {
                    mPrintf("Current application firmware: %.X\n", checkActive->activeBank);
                    JUMP_To_App_B();
                }
                else if(checkActive->pFlag == 1 && metadata->Hostcrc_AppA == check_crc_App && metadata->Hostsize_AppA > 0)
                {
                    BOOTLOADER_SetActiveBank(ACTIVE_APP_A);
                    HAL_Delay(1);
                    BOOTLOADER_SetPendingFlag(false);

                    mPrintf("New firmware detected!\nJumping to new application..\n");
                    mPrintf("Current application firmware: %.X\n", checkActive->activeBank); 
                    JUMP_To_App_A();
                }
                else
                {
                    mPrintf("CRC check mismatch!\nRoll back to older firmware...\n");
                    mPrintf("Current application firmware: %.X\n", checkActive->activeBank); 
                    JUMP_To_App_B();
                }
            break;
            
            default:
                /* Check if there are no firmware available then roll back to Boot manager */
                mPrintf("No firmware detected!\n");
                JUMP_To_Boot();
                break;
        }

    }
}
