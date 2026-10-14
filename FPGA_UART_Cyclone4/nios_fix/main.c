/*
 * "Hello World" example.
 *
 * This example prints 'Hello from Nios II' to the STDOUT stream. It runs on
 * the Nios II 'standard', 'full_featured', 'fast', and 'low_cost' example
 * designs. It runs with or without the MicroC/OS-II RTOS and requires a STDOUT
 * device in your system's hardware.
 * The memory footprint of this hosted application is ~69 kbytes by default
 * using the standard reference design.
 *
 * For a reduced footprint version of this template, and an explanation of how
 * to reduce the memory footprint for a given application, see the
 * "small_hello_world" template.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "system.h"
#include "sys/alt_stdio.h"
#include "sys/alt_irq.h"
#include "sys/alt_dev.h"
#include "priv/alt_iic_isr_register.h"

#include "altera_avalon_timer_regs.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_spi_regs.h"
#include "altera_avalon_spi.h"

#include <unistd.h>

#include "main.h"
#include "pc_cmd.h"
#include "nios_io.h"
#include "adc_ltc2292.h"


/* Global variables ----------------------------------------------------------*/
volatile unsigned long  TickCnt=0, WaitCnt=0;

unsigned char UART1_RxOK_Flag=0, UART1_Error_Flag=0;
unsigned char UART1_RxBuf[RXBUFSIZE]={0};
unsigned int  UART1_RxCnt=0, UART1_RxLength=0;

unsigned char UART2_RxOK_Flag=0, UART2_Error_Flag=0;
unsigned char UART2_RxBuf[RXBUFSIZE]={0};
unsigned int  UART2_RxCnt=0, UART2_RxLength=0;

alt_8 Wifi_Command_Flag=0;


// UART 수신 ISR 은 nios_io.c 로 이동 (링버퍼 방식)


//------------------------------------------------------------------------------
//		void timer_1ms_isr(void)
//------------------------------------------------------------------------------
void timer_1ms_isr(void *context)
{
	static unsigned int  led_cnt = 0, wifi_led_cnt=0;
	static unsigned int  led_toggle = 1;

	IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_1MS_BASE, 0x0);

	TickCnt++;
	WaitCnt++;

	if (led_cnt++ > 300)
	{
		led_cnt = 0;

		if(led_toggle)
		{
			led_toggle = 0;
			IOWR_ALTERA_AVALON_PIO_SET_BITS(LED_OUT_BASE, LED_NIOS);
		}
		else
		{
			led_toggle = 1;
			IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(LED_OUT_BASE, LED_NIOS);
		}
	}
/*
	if(Wifi_LED_Flag)
	{
		wifi_led_cnt++;

		if(wifi_led_cnt > 100)
		{
			wifi_led_cnt = 0;
			Wifi_LED_Flag = 0;
			IOWR_ALTERA_AVALON_PIO_SET_BITS(LED_OUT_BASE, LED_WIFI);
		}

	}
	*/
}

int main(void)
{
	char buf[100]={0};


	IOWR_ALTERA_AVALON_PIO_SET_BITS(MCU_RX_BASE, 0x01);
	IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(UART2_DE_BASE, 0x01);

	// UART1/UART2 수신 인터럽트 + 링버퍼 초기화
	Uart_Init();

	IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_1MS_BASE, ALTERA_AVALON_TIMER_CONTROL_START_MSK|ALTERA_AVALON_TIMER_CONTROL_CONT_MSK|ALTERA_AVALON_TIMER_CONTROL_ITO_MSK);
	alt_iic_isr_register(TIMER_1MS_IRQ_INTERRUPT_CONTROLLER_ID, TIMER_1MS_IRQ, timer_1ms_isr, 0x0, 0x0);

	sprintf(buf, "SGDT_DC_6CH, FPGA NIOS II\r\n");
	uart2_string_write(buf, strlen(buf));
	sprintf(buf, "Build Date  : %s %s\r\n", __DATE__, __TIME__);
	uart2_string_write(buf, strlen(buf));

	Init_ADC_Average_Buffer();

	TickCnt = 0;

	while(1)
	{
		Uart_Rx_Poll();				// 링버퍼 → 패킷 조립 (UART1_RxOK_Flag 등 갱신)

		if(Mcu_Command_Check() == TRUE)
		{
			Mcu_Command_Proc();
		}

		if(PeakDetect_Flag)
		{
			if(TickCnt > ReadInterval)
			{
				TickCnt = 0;

				ADC_Data_Read_from_Dpram(DPRAM_ADC0_BASE, 0);
				IOWR_ALTERA_AVALON_PIO_SET_BITS(NIOS_TX_BASE, 0x01);

				ADC_Data_Read_from_Dpram(DPRAM_ADC1_BASE, 1);
				IOWR_ALTERA_AVALON_PIO_SET_BITS(NIOS_TX_BASE, 0x02);

				ADC_Data_Read_from_Dpram(DPRAM_ADC2_BASE, 2);
				IOWR_ALTERA_AVALON_PIO_SET_BITS(NIOS_TX_BASE, 0x04);

				ADC_Data_Read_from_Dpram(DPRAM_ADC3_BASE, 3);
				IOWR_ALTERA_AVALON_PIO_SET_BITS(NIOS_TX_BASE, 0x08);

				ADC_Data_Read_from_Dpram(DPRAM_ADC4_BASE, 4);
				IOWR_ALTERA_AVALON_PIO_SET_BITS(NIOS_TX_BASE, 0x10);

				ADC_Data_Read_from_Dpram(DPRAM_ADC5_BASE, 5);
				IOWR_ALTERA_AVALON_PIO_SET_BITS(NIOS_TX_BASE, 0x20);

				if(PeakDetect_Flag)
				{
					ADC_Data_to_MCU_All();
				}

				Trigger_Voltage_Set();

			}

			if((IORD_ALTERA_AVALON_PIO_DATA(NIOS_RX_BASE) & 0x01) == 0x01)
			{
				IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(NIOS_TX_BASE, 0x01);
			}

			if((IORD_ALTERA_AVALON_PIO_DATA(NIOS_RX_BASE) & 0x02) == 0x02)
			{
				IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(NIOS_TX_BASE, 0x02);
			}

			if((IORD_ALTERA_AVALON_PIO_DATA(NIOS_RX_BASE) & 0x04) == 0x04)
			{
				IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(NIOS_TX_BASE, 0x04);
			}

			if((IORD_ALTERA_AVALON_PIO_DATA(NIOS_RX_BASE) & 0x08) == 0x08)
			{
				IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(NIOS_TX_BASE, 0x08);
			}

			if((IORD_ALTERA_AVALON_PIO_DATA(NIOS_RX_BASE) & 0x10) == 0x10)
			{
				IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(NIOS_TX_BASE, 0x10);
			}

			if((IORD_ALTERA_AVALON_PIO_DATA(NIOS_RX_BASE) & 0x20) == 0x20)
			{
				IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(NIOS_TX_BASE, 0x020);
			}
		}

		ADC_Data_Average();
	}

	return 0;
}
