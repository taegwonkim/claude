#include "main.h"
#include "usb_device.h"
#include "usb_serial.h"
#include <string.h>

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

int main(void)
{
    uint8_t rxBuf[64];
    const char bootMsg[] = "STM32L562 USB CDC ready\r\n";

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USB_Device_Init();

    while (USB_Serial_IsConnected() == 0U) {
        HAL_Delay(10U);
    }

    USB_Serial_Transmit((const uint8_t *)bootMsg, (uint16_t)strlen(bootMsg));

    while (1) {
        uint16_t n = USB_Serial_Read(rxBuf, sizeof(rxBuf));

        if (n > 0U) {
            /* 수신한 데이터를 그대로 에코 전송 */
            USB_Serial_Transmit(rxBuf, n);
        }
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

/*
 * STM32L562는 크리스탈 없이 USB Full-Speed를 쓰는 경우
 * HSI48 + CRS(Clock Recovery System)로 48MHz를 보정해야 한다.
 * CubeMX의 Clock Configuration에서 USB 소스를 HSI48로 지정하고
 * RCC->CRS를 활성화하면 이 SystemClock_Config가 자동 생성된다.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        Error_Handler();
    }

    RCCEx_CRSInitTypeDef RCC_CRSInitStruct = {0};
    RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
    RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB1;
    RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
    RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000U, 1000U);
    RCC_CRSInitStruct.ErrorLimitValue = 34U;
    RCC_CRSInitStruct.HSI48CalibrationValue = 32U;
    HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
    }
}
