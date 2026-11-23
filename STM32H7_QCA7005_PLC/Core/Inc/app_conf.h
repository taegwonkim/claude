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

/* ---- pins ---------------------------------------------------------------------
 * The pins are defined in STM32CubeMX (STM32H7_QCA7005_PLC.ioc) with these
 * user labels; CubeMX writes <LABEL>_Pin / <LABEL>_GPIO_Port into main.h.
 * To move a pin, change it in CubeMX and keep the label.
 *
 *   label       pin   function
 *   QCA_SCK     PA5   SPI1_SCK   -> QCA7005 SPI_CLK   (mode 3, 5 MHz)
 *   QCA_MISO    PA6   SPI1_MISO  <- QCA7005 SPI_MISO
 *   QCA_MOSI    PB5   SPI1_MOSI  -> QCA7005 SPI_MOSI
 *   QCA_CS      PD14  GPIO out   -> QCA7005 SPI_CS_L  (software chip select)
 *   QCA_INT     PE13  EXTI13     <- QCA7005 INTR      (rising edge)
 *   QCA_RST     PE14  GPIO out   -> QCA7005 RESET_L   (low = reset)
 *   CP_PWM      PE9   TIM1_CH1   -> CP driver (EVSE): 1 kHz, high = +12 V
 *   CP_ADC      PA3   ADC1_INP15 <- CP divider / level shifter
 *   CP_S2       PE11  GPIO out   -> S2 switch (EV): 1 = state C
 *   LED_GREEN   PB0   LD1, LED_YELLOW PE1 LD2, LED_RED PB14 LD3
 */
#define QCA_CS_PORT      QCA_CS_GPIO_Port
#define QCA_CS_PIN       QCA_CS_Pin
#define QCA_INT_PORT     QCA_INT_GPIO_Port
#define QCA_INT_PIN      QCA_INT_Pin
#define QCA_INT_IRQn     QCA_INT_EXTI_IRQn
#define QCA_RST_PORT     QCA_RST_GPIO_Port
#define QCA_RST_PIN      QCA_RST_Pin
#define CP_S2_PORT       CP_S2_GPIO_Port
#define CP_S2_PIN        CP_S2_Pin
#define LED_GREEN_PORT   LED_GREEN_GPIO_Port   /* SLAC matched / AVLN up      */
#define LED_GREEN_PIN    LED_GREEN_Pin
#define LED_YELLOW_PORT  LED_YELLOW_GPIO_Port  /* modem in sync, SLAC running */
#define LED_YELLOW_PIN   LED_YELLOW_Pin
#define LED_RED_PORT     LED_RED_GPIO_Port     /* no modem / SLAC failed      */
#define LED_RED_PIN      LED_RED_Pin

/* ---- Control Pilot front end -----------------------------------------------------
 * The ADC sees  V_adc = CP_ADC_ZERO_MV + V_cp * CP_ADC_MV_PER_V / 1000
 * e.g. a front end mapping -12 V..+12 V to 0.33 V..2.97 V:
 *   ZERO = 1650 mV, 110 mV per CP volt.  Adjust to your circuit. */
#define CP_ADC_ZERO_MV   1650
#define CP_ADC_MV_PER_V  110

/* status line on USART3 (ST-LINK VCP, 115200-8-N-1) every n ms */
#define APP_STATUS_PERIOD_MS 2000u

#endif /* APP_CONF_H */
