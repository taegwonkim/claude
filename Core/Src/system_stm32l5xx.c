/**
 ******************************************************************************
 * @file    system_stm32l5xx.c
 * @brief   CMSIS Cortex-M33 시스템 소스 파일 (STM32L5xx)
 *
 * SystemInit()은 리셋 후 startup 코드에서 호출됩니다.
 * SystemCoreClock는 현재 코어 클럭 주파수를 저장합니다.
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx.h"

/* Global variables ----------------------------------------------------------*/

/* 시스템 코어 클럭 (기본값: MSI 4MHz, SystemClock_Config() 후 110MHz) */
uint32_t SystemCoreClock = 4000000UL;

/* MSI 주파수 테이블 (range 0~15) */
static const uint32_t s_msi_range_table[16] = {
    100000UL,   200000UL,   400000UL,   800000UL,
    1000000UL,  2000000UL,  4000000UL,  8000000UL,
    16000000UL, 24000000UL, 32000000UL, 48000000UL,
    0UL, 0UL, 0UL, 0UL
};

/* AHB Prescaler 테이블 */
static const uint8_t s_ahb_presc_table[16] = {
    0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
    1U, 2U, 3U, 4U, 6U, 7U, 8U, 9U
};

/* APB Prescaler 테이블 */
static const uint8_t s_apb_presc_table[8] = {
    0U, 0U, 0U, 0U, 1U, 2U, 3U, 4U
};

/* Functions -----------------------------------------------------------------*/

/**
 * @brief  시스템 초기화
 *
 * 리셋 후 C 런타임(data/bss 초기화) 전에 호출됩니다.
 * FPU 활성화 및 벡터 테이블 설정만 수행합니다.
 */
void SystemInit(void)
{
#if defined(__FPU_PRESENT) && (__FPU_PRESENT == 1U)
    /* FPU 활성화: CP10과 CP11에 Full Access 설정 */
    SCB->CPACR |= ((3UL << 20U) | (3UL << 22U));
#endif

    /* 벡터 테이블을 Flash 시작 주소로 설정 */
    SCB->VTOR = FLASH_BASE;
}

/**
 * @brief  SystemCoreClock 변수 업데이트
 *
 * 클럭 설정 변경 후 호출하여 SystemCoreClock을 실제 값으로 업데이트합니다.
 */
void SystemCoreClockUpdate(void)
{
    uint32_t msirange, pllvco, pllr, pllsource, pllm;
    uint32_t tmp;

    /* 클럭 소스 확인 */
    switch (RCC->CFGR & RCC_CFGR_SWS) {
    case 0x00:  /* MSI */
        if ((RCC->CR & RCC_CR_MSIRGSEL) == 0U) {
            /* CSR의 MSISRANGE 사용 */
            msirange = (RCC->CSR & RCC_CSR_MSISRANGE) >> 8U;
        } else {
            /* CR의 MSIRANGE 사용 */
            msirange = (RCC->CR & RCC_CR_MSIRANGE) >> 4U;
        }
        SystemCoreClock = s_msi_range_table[msirange];
        break;

    case 0x04:  /* HSI */
        SystemCoreClock = 16000000UL;
        break;

    case 0x08:  /* HSE */
        SystemCoreClock = HSE_VALUE;
        break;

    case 0x0C:  /* PLL */
        pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC);
        pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> 4U) + 1U;

        switch (pllsource) {
        case 0x01:  /* MSI as PLL source */
            if ((RCC->CR & RCC_CR_MSIRGSEL) == 0U) {
                msirange = (RCC->CSR & RCC_CSR_MSISRANGE) >> 8U;
            } else {
                msirange = (RCC->CR & RCC_CR_MSIRANGE) >> 4U;
            }
            pllvco = s_msi_range_table[msirange] / pllm;
            break;
        case 0x02:  /* HSI as PLL source */
            pllvco = 16000000UL / pllm;
            break;
        case 0x03:  /* HSE as PLL source */
            pllvco = HSE_VALUE / pllm;
            break;
        default:
            pllvco = s_msi_range_table[6] / pllm;  /* 4MHz default */
            break;
        }

        pllvco *= ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> 8U);
        pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> 25U) + 1U) * 2U;
        SystemCoreClock = pllvco / pllr;
        break;

    default:
        SystemCoreClock = s_msi_range_table[6];  /* 4MHz default */
        break;
    }

    /* AHB prescaler 적용 */
    tmp = (RCC->CFGR & RCC_CFGR_HPRE) >> 4U;
    SystemCoreClock >>= s_ahb_presc_table[tmp];
}
