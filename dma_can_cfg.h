#ifndef DMA_CAN_CFG_H_
#define DMA_CAN_CFG_H_

#include "boot.h"


extern volatile uint8_t dma_flag;

typedef enum 
{
    CAN_ACK_OK,
    CAN_ACK_ERR,
    CAN_ACK_BUSY,
    CAN_START,
    CAN_DONE
} CAN_AckCode_t;

/* Define CAN protocol configurate structure (standard ID only)*/
typedef struct 
{
    CAN_HandleTypeDef *hcan;

    uint32_t hostID;
    uint32_t SelectFIFO;
    uint32_t StdId;
} BOOTLOADER_CAN_Config_t;


void BOOTLOADER_DMA_Config(void);

BOOTLOADER_Status_Typedef BOOTLOADER_DMA_Transmit_IT(uint8_t *Src, uint8_t *Dst, uint16_t length);

BOOTLOADER_Status_Typedef BOOTLOADER_CAN_RxConfig(BOOTLOADER_CAN_Config_t *Instance);

#endif
