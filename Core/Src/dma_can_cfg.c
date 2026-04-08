#include "dma_can_cfg.h"


volatile uint8_t dma_flag = 0;

void DMA1_Channel1_IRQHandler(void) {
	 if(DMA1->ISR & (1U << 1)) {
		DMA1->IFCR = (1U << 1);
		dma_flag = 1;

	}
}

void BOOTLOADER_DMA_Config(void)
{
  RCC->AHBENR |= (1U << 0);				// Enable_DMA_1_CLK
  DMA1_Channel1->CCR  |= (1U << 14);    // Enable MEM2MEM
  DMA1_Channel1->CCR |= (1U << 12);		//Set_Priority
  DMA1_Channel1->CCR &= ~(3U << 10);	//Mem_Size 8_bit
  DMA1_Channel1->CCR &= ~(3U << 8);		//Peri_Size 8_bit
  DMA1_Channel1->CCR |= (1U << 7);		//Enable_Mem_increment
  DMA1_Channel1->CCR |= (1U << 6);		//Enable_Peri_increment
  DMA1_Channel1->CCR &= ~(1U << 5);		//Disable_Circular mode
  DMA1_Channel1->CCR &= ~(1U << 4);		//Data_Read_from_Peri
  DMA1_Channel1->CCR |= (1U << 1); 		//Enable_Full_Transfer_IF
  			//Number_of_data


  NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  
}

BOOTLOADER_Status_Typedef BOOTLOADER_DMA_Transmit_IT(uint8_t *Src, uint8_t *Dst, uint16_t length)
{
    DMA1_Channel1->CNDTR = length;
    DMA1_Channel1->CPAR = (uint32_t)Src;
    DMA1_Channel1->CMAR = (uint32_t)Dst;
    /* Enable DMA */
    DMA1_Channel1->CCR |= (1U << 0);

    return BOOTLOADER_OK;

}

BOOTLOADER_Status_Typedef BOOTLOADER_CAN_Config(BOOTLOADER_CAN_Config_t *Instance)
{
    CAN_FilterTypeDef filterConfig;
    HAL_StatusTypeDef checkStatus;

    filterConfig.FilterBank = 0;
    filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

    filterConfig.FilterIdHigh = (Instance->hostID << 5);
    filterConfig.FilterIdLow  = 0x0000;

    filterConfig.FilterMaskIdHigh = (0x7FF << 5);  // match full ID
    filterConfig.FilterMaskIdLow  = 0x0000;

    filterConfig.FilterFIFOAssignment = Instance->SelectFIFO;
    filterConfig.FilterActivation = CAN_FILTER_ENABLE;

    checkStatus = HAL_CAN_ConfigFilter(Instance->hcan, &filterConfig);
    if(checkStatus != HAL_OK)
    {
        return BOOTLOADER_CAN_ERR;
    }

    checkStatus = HAL_CAN_Start(Instance->hcan);
    if(checkStatus != HAL_OK)
    {
        return BOOTLOADER_CAN_ERR;
    }

    return BOOTLOADER_OK;
}