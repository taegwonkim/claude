/*
 * adc_ltc2292.c
 *
 *  Created on: 2017. 11. 27.
 *      Author: Taegwon Kim
 *
 *  2026.10 수정
 *   - ADC_Data_to_MCU_All() : uart1 로 항상 100 바이트를 보내던 것을 strlen() 으로 수정
 *       (문자열 뒤의 0x00 패딩 전송 / 100자 초과 시 '\n' 잘림 문제)
 *   - 평균 계산 시 AdcAvrgCnt == 0 이면 0 출력 (0 나누기 → "nan"/"inf" 방지)
 *   - DPRAM 읽기를 IORD_32DIRECT 로 변경 (데이터 캐시 우회 + 컴파일러 최적화 방지)
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
#include "unistd.h"
#include <io.h>

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
	char txbuf[200]={0};
	double avg[6];
	int ch, len;

	len = snprintf(txbuf, sizeof(txbuf), "%c\"%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\"\r\n",	STX,
		AdcPeakVal[0], AdcPeakCnt[0], AdcPeakVal[1], AdcPeakCnt[1], AdcPeakVal[2], AdcPeakCnt[2],
		AdcPeakVal[3], AdcPeakCnt[3], AdcPeakVal[4], AdcPeakCnt[4], AdcPeakVal[5], AdcPeakCnt[5]);
	if(len > (int)sizeof(txbuf) - 1) len = sizeof(txbuf) - 1;
	uart1_string_write(txbuf, len);		// ★ 원본: uart1_string_write(txbuf, 100);

	for(ch=0; ch<6; ch++)
	{
		avg[ch] = (AdcAvrgCnt[ch] != 0) ? (fAdcAvrgSum[ch] / (double)AdcAvrgCnt[ch]) : 0.0;
	}

	len = snprintf(txbuf, sizeof(txbuf), "%c\"%.0f,%.0f,%.0f,%.0f,%.0f,%.0f\"\r\n",	STX,
		avg[0], avg[1], avg[2], avg[3], avg[4], avg[5]);
	if(len > (int)sizeof(txbuf) - 1) len = sizeof(txbuf) - 1;
	uart2_string_write(txbuf, len);

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

	// IORD_32DIRECT : 캐시 우회, 매번 실제로 읽음 (FPGA 로직이 계속 갱신하는 메모리)
	Value = IORD_32DIRECT(memory_base, 0);
	AdcNormalVal[ch] = (unsigned long)((double)Value * (double)39.5257);//ADC_CONST);//25.3968);

	Value = IORD_32DIRECT(memory_base, 4);
	AdcPeakVal[ch] = (unsigned long)((double)Value * (double)39.5257);//ADC_CONST);//25.3968);

	Value = IORD_32DIRECT(memory_base, 8);
	AdcNormalCnt[ch] = Value;

	Value = IORD_32DIRECT(memory_base, 12);
	AdcPeakCnt[ch] = Value;

}

//------------------------------------------------------------------------------
//		void ADC0_Data_Capture(void)
//------------------------------------------------------------------------------
void ADC_Data_Average(void)
{
	unsigned long dpram_data;

	dpram_data = IORD_32DIRECT(DPRAM_ADC0_BASE, 0);
	fAdcAvrgSum[0] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[0] += 1;

	dpram_data = IORD_32DIRECT(DPRAM_ADC1_BASE, 0);
	fAdcAvrgSum[1] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[1] += 1;

	dpram_data = IORD_32DIRECT(DPRAM_ADC2_BASE, 0);
	fAdcAvrgSum[2] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[2] += 1;

	dpram_data = IORD_32DIRECT(DPRAM_ADC3_BASE, 0);
	fAdcAvrgSum[3] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[3] += 1;

	dpram_data = IORD_32DIRECT(DPRAM_ADC4_BASE, 0);
	fAdcAvrgSum[4] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[4] += 1;

	dpram_data = IORD_32DIRECT(DPRAM_ADC5_BASE, 0);
	fAdcAvrgSum[5] += (double)((double)dpram_data * (double)39.5257);
	AdcAvrgCnt[5] += 1;

}

//------------------------------------------------------------------------------


