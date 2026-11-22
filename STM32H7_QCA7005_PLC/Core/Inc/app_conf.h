/*
 * app_conf.h - build configuration: role (charger / vehicle), pins, CP front-end
 *
 * Pick the role here (STM32CubeIDE) or on the make command line:
 *   make ROLE=EVSE      -> charger side   (build/evse/STM32H7_QCA7005_PLC.elf)
 *   make ROLE=EV        -> vehicle side   (build/ev/STM32H7_QCA7005_PLC.elf)
 */
#ifndef APP_CONF_H
#define APP_CONF_H

#define PLC_ROLE_EVSE 1   /* charger (EVSE, SECC side)       */
#define PLC_ROLE_EV   2   /* vehicle (EV / PEV, EVCC side)   */

#ifndef PLC_ROLE
#define PLC_ROLE PLC_ROLE_EVSE      /* <<< change to PLC_ROLE_EV for the vehicle side */
#endif

#if PLC_ROLE != PLC_ROLE_EVSE && PLC_ROLE != PLC_ROLE_EV
#error "PLC_ROLE must be PLC_ROLE_EVSE or PLC_ROLE_EV"
#endif

/* ---- QCA7005 on SPI1 ----------------------------------------------------------
 *   PA5  SPI1_SCK   -> QCA7005 SPI_CLK
 *   PA6  SPI1_MISO  <- QCA7005 SPI_MISO
 *   PB5  SPI1_MOSI  -> QCA7005 SPI_MOSI
 *   PD14 GPIO out   -> QCA7005 SPI_CS_L   (software chip select)
 *   PE13 EXTI13     <- QCA7005 INTR       (rising edge, active high)
 *   PE14 GPIO out   -> QCA7005 RESET_L    (low = reset)
 * SPI1 kernel clock = PLL1Q 80 MHz, prescaler 16 -> 5 MHz, mode 3 */
#define QCA_CS_PORT      GPIOD
#define QCA_CS_PIN       GPIO_PIN_14
#define QCA_INT_PORT     GPIOE
#define QCA_INT_PIN      GPIO_PIN_13
#define QCA_INT_IRQn     EXTI15_10_IRQn
#define QCA_RST_PORT     GPIOE
#define QCA_RST_PIN      GPIO_PIN_14
#define QCA_SPI_PRESCALER SPI_BAUDRATEPRESCALER_16

/* ---- Control Pilot ------------------------------------------------------------
 *   PE9  TIM1_CH1   -> CP driver (EVSE only): 1 kHz, high = +12 V, low = -12 V
 *   PA3  ADC1_INP15 <- CP divider / level shifter (both roles)
 *   PE11 GPIO out   -> S2 switch (EV only): 1 = 1.3 kOhm in parallel (state C)
 *
 * The ADC sees  V_adc = CP_ADC_ZERO_MV + V_cp * CP_ADC_MV_PER_V / 1000
 * e.g. a front end mapping -12 V..+12 V to 0.33 V..2.97 V:
 *   ZERO = 1650 mV, 110 mV per CP volt.  Adjust to your circuit. */
#define CP_ADC_ZERO_MV   1650
#define CP_ADC_MV_PER_V  110
#define CP_S2_PORT       GPIOE
#define CP_S2_PIN        GPIO_PIN_11

/* ---- LEDs (NUCLEO-H743ZI2) ------------------------------------------------------ */
#define LED_GREEN_PORT   GPIOB      /* LD1: SLAC matched / AVLN up        */
#define LED_GREEN_PIN    GPIO_PIN_0
#define LED_YELLOW_PORT  GPIOE      /* LD2: modem in sync, SLAC running   */
#define LED_YELLOW_PIN   GPIO_PIN_1
#define LED_RED_PORT     GPIOB      /* LD3: no modem / SLAC failed        */
#define LED_RED_PIN      GPIO_PIN_14

/* status line on USART3 (ST-LINK VCP, 115200-8-N-1) every n ms */
#define APP_STATUS_PERIOD_MS 2000u

#endif /* APP_CONF_H */
