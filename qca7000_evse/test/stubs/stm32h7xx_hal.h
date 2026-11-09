#pragma once
#include <stdint.h>
typedef enum { HAL_OK=0, HAL_ERROR } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET=0, GPIO_PIN_SET } GPIO_PinState;
typedef struct { int x; } GPIO_TypeDef;
typedef struct { uint32_t Mode,Direction,DataSize,CLKPolarity,CLKPhase,NSS,BaudRatePrescaler,FirstBit,TIMode,CRCCalculation,NSSPMode,FifoThreshold,MasterKeepIOState; } SPI_InitTypeDef;
typedef struct { void *Instance; SPI_InitTypeDef Init; } SPI_HandleTypeDef;
typedef struct { int x; } RNG_HandleTypeDef;
typedef struct { uint8_t Enable,Number; uint32_t BaseAddress; uint8_t Size,SubRegionDisable,TypeExtField,AccessPermission,DisableExec,IsShareable,IsCacheable,IsBufferable; } MPU_Region_InitTypeDef;
extern GPIO_TypeDef gA, gB;
#define GPIOA (&gA)
#define GPIOB (&gB)
#define SPI1 ((void*)0)
#define GPIO_PIN_0 1u
#define GPIO_PIN_1 2u
#define GPIO_PIN_4 16u
enum { SPI_MODE_MASTER, SPI_DIRECTION_2LINES, SPI_DATASIZE_8BIT, SPI_POLARITY_HIGH, SPI_PHASE_2EDGE, SPI_NSS_SOFT, SPI_BAUDRATEPRESCALER_16, SPI_FIRSTBIT_MSB, SPI_TIMODE_DISABLE, SPI_CRCCALCULATION_DISABLE, SPI_NSS_PULSE_DISABLE, SPI_FIFO_THRESHOLD_01DATA, SPI_MASTER_KEEP_IO_STATE_ENABLE };
enum { MPU_REGION_ENABLE=1, MPU_REGION_NUMBER1=1, MPU_REGION_SIZE_32KB=14, MPU_TEX_LEVEL1=1, MPU_REGION_FULL_ACCESS=3, MPU_INSTRUCTION_ACCESS_DISABLE=1, MPU_ACCESS_NOT_SHAREABLE=0, MPU_ACCESS_NOT_CACHEABLE=0, MPU_ACCESS_NOT_BUFFERABLE=0, MPU_PRIVILEGED_DEFAULT=4 };
void HAL_GPIO_WritePin(GPIO_TypeDef*, uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*, uint16_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*, uint8_t*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*, uint8_t*, uint16_t, uint32_t);
HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef*, uint8_t*, uint8_t*, uint16_t);
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef*, uint8_t*, uint16_t);
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef*);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef*);
HAL_StatusTypeDef HAL_RNG_GenerateRandomNumber(RNG_HandleTypeDef*, uint32_t*);
void HAL_MPU_Disable(void); void HAL_MPU_Enable(uint32_t); void HAL_MPU_ConfigRegion(MPU_Region_InitTypeDef*);
