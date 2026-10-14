/*
 * adc_ltc2292.c
 *
 *  Created on: 2017. 11. 27.
 *      Author: Taegwon Kim
 */


/* Includes ------------------------------------------------------------------*/
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include "system.h"
#include "sys/alt_stdio.h"
#include "sys/alt_irq.h"
#include "priv/alt_iic_isr_register.h"

#include "altera_avalon_timer_regs.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_uart_regs.h"
#include "altera_avalon_uart.h"
#include "unistd.h"

#include "main.h"
#include "nios_io.h"
#include "pc_cmd.h"
#include "adc_ltc2292.h"

/* Global variables ----------------------------------------------------------*/
unsigned long int RefVolt=5000, OffsetVolt=100, CheckVolt=0, ReadInterval=1000;

unsigned long int AdcNormalVal[6]={0}, AdcPeakVal[6]={0};
unsigned long int AdcNormalCnt[6]={0}, AdcPeakCnt[6]={0};

unsigned long int AdcAvrgVal[6]={0}, AdcAvrgSum[6]={0}, AdcAvrgCnt[6]={0};
double fAdcAvrgVal[6]={0.0}, fAdcAvrgSum[6]={0.0};

unsigned int  AdcChannel=0, AdcReadNum=1;


/* Private function prototypes -----------------------------------------------*/

//------------------------------------------------------------------------------
//		void ADC0_Data_Capture(void)
//------------------------------------------------------------------------------
void Init_ADC_Average_Buffer(void)
{
	int ch;

	for(ch=0; ch<6; ch++)
	{
		AdcAvrgCnt[ch] = 0;
		fAdcAvrgSum[ch] = 0;
	}
}

//------------------------------------------------------------------------------
//		void ADC_Data_to_MCU(void)
//------------------------------------------------------------------------------
void ADC_Data_to_MCU_All(void)
{
	char txbuf[200]={0xff};

	sprintf(txbuf, "%c\"%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\"\r\n",	STX,
		AdcPeakVal[0], AdcPeakCnt[0], AdcPeakVal[1], AdcPeakCnt[1], AdcPeakVal[2], AdcPeakCnt[2],
		AdcPeakVal[3], AdcPeakCnt[3], AdcPeakVal[4], AdcPeakCnt[4], AdcPeakVal[5], AdcPeakCnt[5]);
	uart1_string_write(txbuf, 100);

	sprintf(txbuf, "%c\"%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\"\r\n",	STX,
		fAdcAvrgSum[0]/(double)AdcAvrgCnt[0], fAdcAvrgSum[1]/(double)AdcAvrgCnt[1], fAdcAvrgSum[2]/(double)AdcAvrgCnt[2],
		fAdcAvrgSum[3]/(double)AdcAvrgCnt[3], fAdcAvrgSum[4]/(double)AdcAvrgCnt[4], fAdcAvrgSum[5]/(double)AdcAvrgCnt[5]);
	uart2_string_write(txbuf, strlen(txbuf));

	Init_ADC_Average_Buffer();

	if((AdcPeakCnt[0]+AdcPeakCnt[1]+AdcPeakCnt[2]+AdcPeakCnt[3]+AdcPeakCnt[4]+AdcPeakCnt[5]) == 0)
	{
		IOWR_ALTERA_AVALON_PIO_SET_BITS(LED_OUT_BASE, LED_ERROR);
		IOWR_ALTERA_AVALON_PIO_SET_BITS(TP_OUT_BASE, TPOINT_3);
	}
	else
	{
		IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(LED_OUT_BASE, LED_ERROR);
		IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(TP_OUT_BASE, TPOINT_3);
	}
}

//------------------------------------------------------------------------------
//		void ADC0_Capture(unsigned long memory_base)
//------------------------------------------------------------------------------
void ADC_Data_Read_from_Dpram(unsigned long memory_base, int ch)
{
	unsigned long Value;
	unsigned long *mem_base = (unsigned long *)memory_base;


	Value = *(mem_base + 0) - 0;
	AdcNormalVal[ch] = (unsigned long)((double)Value * (double)39.5257);//ADC_CONST);//25.3968);

	Value = *(mem_base + 1) - 0;
	AdcPeakVal[ch] = (unsigned long)((double)Value * (double)39.5257);//ADC_CONST);//25.3968);

	Value = *(mem_base + 2);
	AdcNormalCnt[ch] = Value;

	Value = *(mem_base + 3);
	AdcPeakCnt[ch] = Value;

}

//------------------------------------------------------------------------------
//		void ADC0_Data_Capture(void)
//------------------------------------------------------------------------------
void ADC_Data_Average(void)
{
	unsigned long dpram_data;
	unsigned long *mem_base;

	mem_base = (unsigned long *)DPRAM_ADC0_BASE;
	dpram_data = *(mem_base + 0);
	fAdcAvrgSum[0] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[0] += 1;

	mem_base = (unsigned long *)DPRAM_ADC1_BASE;
	dpram_data = *(mem_base + 0);
	fAdcAvrgSum[1] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[1] += 1;

	mem_base = (unsigned long *)DPRAM_ADC2_BASE;
	dpram_data = *(mem_base + 0);
	fAdcAvrgSum[2] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[2] += 1;

	mem_base = (unsigned long *)DPRAM_ADC3_BASE;
	dpram_data = *(mem_base + 0);
	fAdcAvrgSum[3] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[3] += 1;

	mem_base = (unsigned long *)DPRAM_ADC4_BASE;
	dpram_data = *(mem_base + 0);
	fAdcAvrgSum[4] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[4] += 1;

	mem_base = (unsigned long *)DPRAM_ADC5_BASE;
	dpram_data = *(mem_base + 0);
	fAdcAvrgSum[5] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[5] += 1;

}

//------------------------------------------------------------------------------


