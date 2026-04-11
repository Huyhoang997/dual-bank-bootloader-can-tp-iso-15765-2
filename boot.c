#include "boot.h"
#include "dma_can_cfg.h"


static BOOTLOADER_CAN_Config_t can_cfg = {
    .hostID = 0x34U,
    .SelectFIFO = CAN_FILTER_FIFO0,
    .hcan = &hcan,
    .StdId = 0x123
};


static BOOTLOADER_Status_Typedef BOOTLOADER_SendACK(CAN_AckCode_t AckCode)
{
    CAN_TxHeaderTypeDef Header;
	uint32_t TxMailbox;
	HAL_StatusTypeDef checkStatus;
    uint8_t data[8] = {0};
    data[0] = AckCode;

	Header.StdId = can_cfg.StdId;
	Header.IDE = CAN_ID_STD;
	Header.RTR = CAN_RTR_DATA;
	Header.DLC = 1;

    
	checkStatus = HAL_CAN_AddTxMessage(can_cfg.hcan, &Header, data, &TxMailbox);
	if(checkStatus != HAL_OK)
	{
		return BOOTLOADER_ERR;
	}

    while(HAL_CAN_IsTxMessagePending(can_cfg.hcan, TxMailbox));

    return BOOTLOADER_OK;
}


static void JUMP_To_App_A(void)
{
    volatile uint32_t app_A_vector_table_addr = *(volatile uint32_t *)APP_A_START_ADDR;
    volatile uint32_t app_A_reset_handler_addr = *(volatile uint32_t *)APP_A_RESET_HANDLER_ADDR;

    __disable_irq();
    HAL_DeInit();
    HAL_CAN_DeInit(can_cfg.hcan);
    HAL_UART_DeInit(&huart1);
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
    HAL_CAN_DeInit(can_cfg.hcan);
    HAL_UART_DeInit(&huart1);
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
    HAL_StatusTypeDef checkStatus;

    uint32_t recoverData[7];
    uint8_t host_cmd = data[0];
    uint16_t host_size = ((uint16_t)data[1] << 8) | data[2];
    uint32_t host_crc = ((uint32_t)data[3] << 24) | ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 8 | data[6]);
    uint8_t version = data[7];

    HAL_FLASH_Unlock();
    
    switch (checkActive->activeBank)
    {
        case ACTIVE_APP_A:
            erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;
            erase_sector.PageAddress = METADATA_SECTOR_ADDR;
            erase_sector.NbPages = 1;

            recoverData[0] = metadata->Hostsize_AppA;
            recoverData[1] = metadata->Hostcrc_AppA;
            recoverData[2] = metadata->version_AppA;

            recoverData[3] = host_size;
            recoverData[4] = host_crc;
            recoverData[5] = version;

            recoverData[6] = host_cmd;

            checkStatus = HAL_FLASHEx_Erase(&erase_sector, &error_page);
            if(checkStatus != HAL_OK)
            {
                return BOOTLOADER_PROGRAM_ERR;
            }

            for(uint8_t i = 0; i < 7; i++)
            {
                checkStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_SECTOR_ADDR + 4*i), recoverData[i]);
                if(checkStatus != HAL_OK)
                {
                    return BOOTLOADER_PROGRAM_ERR;
                }
            }
        break;

        case ACTIVE_APP_B:
            erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;
            erase_sector.PageAddress = METADATA_SECTOR_ADDR;
            erase_sector.NbPages = 1;

            recoverData[0] = host_size;
            recoverData[1] = host_crc;
            recoverData[2] = version;

            recoverData[3] = metadata->Hostsize_AppB;
            recoverData[4] = metadata->Hostcrc_AppB;
            recoverData[5] = metadata->version_AppB;

            recoverData[6] =  host_cmd;

            checkStatus = HAL_FLASHEx_Erase(&erase_sector, &error_page);
            if(checkStatus != HAL_OK)
            {
                return BOOTLOADER_PROGRAM_ERR;
            }

            for(uint8_t i = 0; i < 7; i++)
            {
            checkStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)(METADATA_SECTOR_ADDR + 4*i), recoverData[i]);
                if(checkStatus != HAL_OK)
                {
                    return BOOTLOADER_PROGRAM_ERR;
                }
            }
        break;
    
        default:
                return BOOTLOADER_ERR;
    }

    HAL_FLASH_Lock();

    return BOOTLOADER_OK;
}


BOOTLOADER_Status_Typedef BOOTLOADER_ReceiveFirmware(void)
{
    METADATA_SectorDef_t *metadata = (METADATA_SectorDef_t *)METADATA_SECTOR_ADDR;
    METADATA_ActiveBank_t *checkActive = (METADATA_ActiveBank_t *)METADATA_ACTIVE_BANK_ADDR;
    CAN_RxHeaderTypeDef Rx_config;

    uint8_t rxdata[8] = {0};
    uint8_t write_flash[1024];
    static uint16_t index = 0;
    uint16_t offet_addr = 0;
    uint16_t firmware_length = 0;
    
    
    FLASH_EraseInitTypeDef erase_sector;
    uint32_t error_page = 0;
    HAL_StatusTypeDef checkStatus;
    
    HAL_FLASH_Unlock();

    switch (checkActive->activeBank)
    {
        case ACTIVE_APP_A:
            erase_sector.PageAddress = APP_B_START_ADDR;
            erase_sector.NbPages = 15;
            erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;

            firmware_length = metadata->Hostsize_AppB / 8;
            if(metadata->Hostsize_AppB % 8)
            {
                firmware_length++;
            }

            checkStatus = HAL_FLASHEx_Erase(&erase_sector, &error_page);
            if(checkStatus != HAL_OK)
            {
                return BOOTLOADER_ERASE_ERR;
            }

            BOOTLOADER_SendACK(CAN_START);

            for(uint32_t i = 0; i < firmware_length; i++)
            {
                while(HAL_CAN_GetRxFifoFillLevel(can_cfg.hcan, can_cfg.SelectFIFO)  == 0);
                HAL_CAN_GetRxMessage(can_cfg.hcan, can_cfg.SelectFIFO , &Rx_config, rxdata);

                for(uint8_t k = 0; k < Rx_config.DLC; k++)
                {
                    write_flash[index++] = rxdata[k];
                }
                
                if(index >= 1024)
                {
                    BOOTLOADER_SendACK(CAN_ACK_BUSY);
                    for(uint16_t j = 0; j < 1024; j += 4)
                    {
                        uint32_t word = write_flash[j] | (write_flash[j+1] << 8) | (write_flash[j+2] << 16) | (write_flash[j+3] << 24);

                        checkStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)APP_B_START_ADDR + offet_addr, word);
                        if(checkStatus != HAL_OK)
                        {
                            return BOOTLOADER_PROGRAM_ERR;
                        }

                        offet_addr += 4;
                    }
                    index = 0;
                    BOOTLOADER_SendACK(CAN_ACK_OK);
                }
            }

            if(index > 0)
                {
                    // Padding mảng bằng 0xFF cho đủ Word trước khi ghi nốt
                    while(index % 4 != 0) write_flash[index++] = 0xFF;

                    for(uint16_t j = 0; j < index; j += 4)
                    {
                        uint32_t word = write_flash[j] | (write_flash[j+1] << 8) | (write_flash[j+2] << 16) | (write_flash[j+3] << 24);
                        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_B_START_ADDR + offet_addr, word);
                        offet_addr += 4;
                    }
                }

        break;
    
        case ACTIVE_APP_B:
            erase_sector.PageAddress = APP_A_START_ADDR;
            erase_sector.NbPages = 15;
            erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;

            firmware_length = metadata->Hostsize_AppA / 8;
            if(metadata->Hostsize_AppA % 8)
            {
                firmware_length++;
            }

            checkStatus = HAL_FLASHEx_Erase(&erase_sector, &error_page);
            if(checkStatus != HAL_OK)
            {
                return BOOTLOADER_PROGRAM_ERR;
            }

            BOOTLOADER_SendACK(CAN_START);

            for(uint32_t i = 0; i < firmware_length; i++)
            {
                while(HAL_CAN_GetRxFifoFillLevel(can_cfg.hcan, can_cfg.SelectFIFO)  == 0);
                HAL_CAN_GetRxMessage(can_cfg.hcan, can_cfg.SelectFIFO , &Rx_config, rxdata);

                for(uint8_t k = 0; k < Rx_config.DLC; k++)
                {
                    write_flash[index++] = rxdata[k];
                }
                
                if(index >= 1024)
                {
                    BOOTLOADER_SendACK(CAN_ACK_BUSY);
                    for(uint16_t j = 0; j < 1024; j += 4)
                    {
                        uint32_t word = write_flash[j] | (write_flash[j+1] << 8) | (write_flash[j+2] << 16) | (write_flash[j+3] << 24);

                        checkStatus = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)APP_A_START_ADDR + offet_addr, word);
                        if(checkStatus != HAL_OK)
                        {
                            return BOOTLOADER_PROGRAM_ERR;
                        }

                        offet_addr += 4;
                    }
                    index = 0;
                    BOOTLOADER_SendACK(CAN_ACK_OK);
                }
            }

            if(index > 0)
                {
                    // Padding mảng bằng 0xFF cho đủ Word trước khi ghi nốt
                    while(index % 4 != 0) write_flash[index++] = 0xFF;

                    for(uint16_t j = 0; j < index; j += 4)
                    {
                        uint32_t word = write_flash[j] | (write_flash[j+1] << 8) | (write_flash[j+2] << 16) | (write_flash[j+3] << 24);
                        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_A_START_ADDR + offet_addr, word);
                        offet_addr += 4;
                    }
                }

        break;

        default:
            mPrintf("Error while receiving new firmware!\n");
            return BOOTLOADER_PROGRAM_ERR;
        break;
    }

    HAL_FLASH_Lock();
    BOOTLOADER_SendACK(CAN_DONE);

    return BOOTLOADER_OK;
}


BOOTLOADER_Status_Typedef BOOTLOADER_CheckFirmwareCRC(void)
{
    METADATA_SectorDef_t *metadata = (METADATA_SectorDef_t *)METADATA_SECTOR_ADDR;
    METADATA_ActiveBank_t *checkActive = (METADATA_ActiveBank_t *)METADATA_ACTIVE_BANK_ADDR;
    BOOTLOADER_Status_Typedef checkStatus;
    uint16_t firmware_length = 0;
    uint32_t checkCRC;

    switch (checkActive->activeBank)
    {
        case ACTIVE_APP_A:
            firmware_length = metadata->Hostsize_AppB / 4;
            if(metadata->Hostsize_AppB % 4)
            {
                firmware_length++;
            }

            checkCRC = HAL_CRC_Calculate(&hcrc, (uint32_t *)APP_B_START_ADDR, firmware_length);

            mPrintf("crc_host: %.08lX\n", metadata->Hostcrc_AppB);
            mPrintf("real crc: %.08lX\n", checkCRC);
            if(checkCRC == metadata->Hostcrc_AppB)
            {
                checkStatus = BOOTLOADER_SetPendingFlag(true);
                if(checkStatus != BOOTLOADER_OK)
                {
                    return checkStatus;
                }
            }
            else
            {
                return BOOTLOADER_CHECK_CRC_FAILD;
            }
        break;
    
        case ACTIVE_APP_B:
            firmware_length = metadata->Hostsize_AppA / 4;
            if(metadata->Hostsize_AppA % 4)
            {
                firmware_length++;
            }

            checkCRC = HAL_CRC_Calculate(&hcrc, (uint32_t *)APP_A_START_ADDR, firmware_length);

            mPrintf("crc_host: %.08lX\n", metadata->Hostcrc_AppA);
            mPrintf("real crc: %.08lX\n", checkCRC);

            if(checkCRC == metadata->Hostcrc_AppA)
            {
                checkStatus = BOOTLOADER_SetPendingFlag(true);
                if(checkStatus != BOOTLOADER_OK)
                {
                    return checkStatus;
                }
            }
            else
            {
                return BOOTLOADER_CHECK_CRC_FAILD;
            }
        break;

        default:
            return BOOTLOADER_CHECK_CRC_FAILD;
        break;
    }
    return BOOTLOADER_OK;
}


BOOTLOADER_Status_Typedef BOOTLOADER_EraseCurrentFirmware(void)
{
    METADATA_ActiveBank_t *checkActive = (METADATA_ActiveBank_t *)METADATA_ACTIVE_BANK_ADDR;
    FLASH_EraseInitTypeDef erase_sector;
    uint32_t error_page = 0;
    HAL_StatusTypeDef checkStatus;

    HAL_FLASH_Unlock();
    switch (checkActive->activeBank)
    {
        case ACTIVE_APP_A:
            erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;
            erase_sector.PageAddress = APP_A_START_ADDR;
            erase_sector.NbPages = 15;

            checkStatus = HAL_FLASHEx_Erase(&erase_sector, &error_page);
            if(checkStatus != HAL_OK)
            {
                return BOOTLOADER_PROGRAM_ERR;
            }
            
            BOOTLOADER_SetActiveBank(ACTIVE_APP_B);
        break;
    
        case ACTIVE_APP_B:
            erase_sector.TypeErase = FLASH_TYPEERASE_PAGES;
            erase_sector.PageAddress = APP_B_START_ADDR;
            erase_sector.NbPages = 15;

            checkStatus = HAL_FLASHEx_Erase(&erase_sector, &error_page);
            if(checkStatus != HAL_OK)
            {
                return BOOTLOADER_PROGRAM_ERR;
            }

            BOOTLOADER_SetActiveBank(ACTIVE_APP_A);
        break;

        default: 
            HAL_FLASH_Lock();
            return BOOTLOADER_ERR;
    }

    HAL_FLASH_Lock();
    return BOOTLOADER_OK;
}


static void BOOTLOADER_ReceiveCmd(void)
{
    BOOTLOADER_Status_Typedef checkStatus;
    CAN_RxHeaderTypeDef Rx_config;
    uint8_t rxdata[8] = {0};

    while(HAL_CAN_GetRxFifoFillLevel(can_cfg.hcan, can_cfg.SelectFIFO)  == 0);
    HAL_CAN_GetRxMessage(can_cfg.hcan, can_cfg.SelectFIFO , &Rx_config, rxdata);
    switch (rxdata[0])
    {
        case BOOTLOADER_WRITE_CMD:
            checkStatus = BOOTLOADER_SaveInfoFrame(rxdata);
            if(checkStatus != BOOTLOADER_OK)
            {
                mPrintf("Save infomation frame failed!\n");
            }
            mPrintf("Waiting for new firmware...\n");


            checkStatus = BOOTLOADER_ReceiveFirmware();
            if(checkStatus != BOOTLOADER_OK)
            {
                mPrintf("Receive firmware failed!\n");
                mPrintf("Error ID: %d\n", checkStatus);
            }

            mPrintf("Received new firmware!\nChecking CRC....\n");
            checkStatus = BOOTLOADER_CheckFirmwareCRC();
            if(checkStatus != BOOTLOADER_OK)
            {
                mPrintf("CRC mismatch, Please retry!\n");
                
                mPrintf("Error ID: %d\n", checkStatus);
            }

        break;

        case BOOTLOADER_ERASE_CMD:
            mPrintf("Erasing current firmware...\n");
            checkStatus = BOOTLOADER_EraseCurrentFirmware();
            if(checkStatus != BOOTLOADER_OK)
            {
                mPrintf("Erase failed!\n");
            }
            mPrintf("Erase firmware finished!\n");
        break;
    
        default:
        mPrintf("Unknow Command!\n");
        break;
    } 
}


static void JUMP_To_Boot(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);

    BOOTLOADER_ReceiveCmd();
}


void BOOTLOADER_Init(void)
{
    METADATA_ActiveBank_t *checkActive = (METADATA_ActiveBank_t *)METADATA_ACTIVE_BANK_ADDR;
    METADATA_SectorDef_t *metadata = (METADATA_SectorDef_t *)METADATA_SECTOR_ADDR;
    uint32_t check_crc_App = 0, firmwareLength = 0;

    BOOTLOADER_Status_Typedef checkStatus;

    checkStatus = BOOTLOADER_CAN_RxConfig(&can_cfg);
    if(checkStatus != BOOTLOADER_OK)
    {
        mPrintf("Something is wrong with CAN!\n");
    }

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

                if(checkActive->pFlag == 1 && metadata->Hostcrc_AppB == check_crc_App && metadata->Hostsize_AppB > 0)
                {
                    BOOTLOADER_SetActiveBank(ACTIVE_APP_B);
                    HAL_Delay(1);
                    BOOTLOADER_SetPendingFlag(false);

                    mPrintf("New firmware detected!\nJumping to new application..\n");
                    mPrintf("Current application firmware: %.X\n", checkActive->activeBank); 
                    JUMP_To_App_B();
                }
                else if(checkActive->pFlag == 1 && metadata->Hostcrc_AppB == check_crc_App)
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

                if(checkActive->pFlag == 1 && metadata->Hostcrc_AppA == check_crc_App && metadata->Hostsize_AppA > 0)
                {
                    BOOTLOADER_SetActiveBank(ACTIVE_APP_A);
                    HAL_Delay(1);
                    BOOTLOADER_SetPendingFlag(false);

                    mPrintf("New firmware detected!\nJumping to new application..\n");
                    mPrintf("Current application firmware: %.X\n", checkActive->activeBank); 
                    JUMP_To_App_A();
                }
                else if(checkActive->pFlag == 1 && metadata->Hostcrc_AppA == check_crc_App)
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
