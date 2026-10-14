/**
  ******************************************************************************
  * File Name          : pc_cmd.c
  ******************************************************************************
  *  2026.10 수정
  *   - McuCmdBuf 복사 시 UART1_RxBuf 범위 밖(RxBuf[100]) 읽기 수정, 이전 명령 잔여 데이터 제거
  *   - 파라미터 위치 파싱을 문자열 끝('\0')까지만 수행, McuParaPoint[20] 범위 검사
  *   - 디버그 에코(uart2)를 MCU_CMD_DEBUG_ECHO 로 on/off
  *   - ADC_Data_Read_Command : 배열 주소를 출력하던 것을 [0] 값으로 수정
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include "system.h"
#include "sys/alt_stdio.h"
#include "sys/alt_irq.h"
#include "sys/alt_dev.h"
#include "priv/alt_iic_isr_register.h"

#include "altera_avalon_timer_regs.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_spi_regs.h"
#include "altera_avalon_spi.h"

#include "unistd.h"

#include "main.h"
#include "nios_io.h"
#include "pc_cmd.h"
#include "pc_uart.h"
#include "adc_ltc2292.h"


// 1 : MCU 에서 받은 명령을 uart2(디버그 RS485)로 에코 (원본 동작)
#define MCU_CMD_DEBUG_ECHO		1

/* Global variables ----------------------------------------------------------*/
volatile unsigned char McuLed_Flag=0, McuCmdMode_Flag=0;
volatile unsigned int  McuLed_Cnt=0;

/* Global variables ----------------------------------------------------------*/
unsigned char McuCmdBuf[CMDBUFSIZE]={0};
unsigned int  McuParaPoint[20], McuParaNum=0, McuCmdSize, McuCmdDataLen=0;
unsigned int  McuId=0, PanId=0;

unsigned char PeakDetect_Flag=0;
unsigned char Wifi_LED_Flag=0;

unsigned char Debug_ADC_Flag=0, Debug_ADC_All_Flag=0;

/* Private function prototypes -----------------------------------------------*/


//------------------------------------------------------------------------------
//		Mcu_Command_Check
//------------------------------------------------------------------------------
int Mcu_Command_Check(void)
{
	unsigned int i=0;

	
	if(UART1_RxOK_Flag)
	{
		UART1_RxOK_Flag = 0;
		
		McuLed_Flag = 1;
		McuLed_Cnt  = 0;
		//IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(LED_OUT_BASE, LED_WIFI);
		
		// command,para1,---,paraN + '\r'
		memset(McuCmdBuf, 0, CMDBUFSIZE);			// 이전 명령 잔여 데이터 제거
		for(i=0; (i < CMDBUFSIZE-1) && (i+1 < RXBUFSIZE); i++)
		{
			McuCmdBuf[i] = UART1_RxBuf[i+1];
			if((McuCmdBuf[i] == '\n') || (McuCmdBuf[i] == '\0'))
			{
				McuCmdBuf[i] = '\0';
				break;
			}
		}

#if MCU_CMD_DEBUG_ECHO
		// 수신 명령을 RS485(uart2) 로 에코 - 송신이 끝날 때까지 블로킹되므로 양산 시 0 권장
		uart2_string_write((char*)UART1_RxBuf, strlen((char*)UART1_RxBuf));
#endif

		//Mcu_Receive_Ready();

		return(TRUE);
	}
	
	return(FALSE);
}

//------------------------------------------------------------------------------
//		PC_Command_Proc
//------------------------------------------------------------------------------
void Mcu_Command_Proc(void)
{
	unsigned int i=0, n=0;
	char*  cptr;

	
	/*----------------------------------------------------
	 * Command format : "Scccc,---------\r"            *
	 *      cccc : Command from PC	                     *
	 ----------------------------------------------------*/
	
	cptr = (char*)&McuCmdBuf[0];
	McuCmdSize = Mcu_Command_Length(cptr);
	
	// 파라미터 시작 위치 : 명령 뒤 ',' 다음 문자열 기준 오프셋
	McuParaPoint[0] = 0;
	i = 1;
	if(cptr[McuCmdSize] == ',')
	{
		char * para = cptr + McuCmdSize + 1;

		for(n = 0; (para[n] != '\0') && (McuCmdSize + 1 + n < CMDBUFSIZE); n++)
		{
			if((para[n] == ',') && (i < sizeof(McuParaPoint)/sizeof(McuParaPoint[0])))
			{
				McuParaPoint[i++] = n + 1;
			}
		}
	}
	McuParaNum = i;

	cptr = (char*)&McuCmdBuf[0];

	//if(McuCmdMode_Flag)
	//{
	//}
	//else
	//{
		if (! strncmp(cptr, "RESET", 5)) {	// Board Reset
			Mcu_RESET_Command();
		}
		else if (! strncmp(cptr, "START", 5)) {	// start ADC Peak hold & read
			PeakDetect_Flag = 1;
		}
		else if (! strncmp(cptr, "STOP", 4)) {	// stop ADC Peak hold & read
			PeakDetect_Flag = 0;
		}
		else if (! strncmp(cptr, "READ", 4)) {	// stop ADC Peak hold & read
			ADC_Data_Read_Command();
		}
		else if (! strncmp(cptr, "SW_ALL", 6)) {	// Write All Setting Value
			Mcu_W_ALL_Command();
		}
		else if (! strncmp(cptr, "SR_ALL", 6)) {	// Read All Setting Value
			Mcu_R_ALL_Command();
		}
		else if (! strncmp(cptr, "SW_REFV", 7)) {	// Write Reference Voltage
			Mcu_W_REFV_Command();
		}
		else if (! strncmp(cptr, "SR_REFV", 7)) {	// Read Reference Voltage
			Mcu_R_REFV_Command();
		}
		else if (! strncmp(cptr, "SW_OFFSV", 8)) {	// Write Offset Voltage
			Mcu_W_OFFSV_Command();
		}
		else if (! strncmp(cptr, "SR_OFFSV", 8)) {	// Read Offset Voltage
			Mcu_R_OFFSV_Command();
		}
		else if (! strncmp(cptr, "SW_RDTIME", 9)) {	// Write Offset Voltage
			Mcu_W_RDTIME_Command();
		}
		else if (! strncmp(cptr, "SR_RDTIME", 9)) {	// Write Offset Voltage
			Mcu_R_RDTIME_Command();
		}
		else if (! strncmp(cptr, "ADC_START", 9)) {	// ADC debugging start
			Debug_ADC_START_Command();
		}
		else if (! strncmp(cptr, "ADC_STOP", 8)) {	// ADC debugging stop
			Debug_ADC_STOP_Command();
		}
		else if (! strncmp(cptr, "OK", 2)) {	// Write Offset Voltage
			IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(LED_OUT_BASE, LED_WIFI);
			Wifi_LED_Flag = 1;
		}
		/*
		else
		{
			Mcu_Send_NG();
			ret = FALSE;
		}

		if(ret == TRUE)	{
			Mcu_Send_OK();
		}
		*/
	//}
		
}

//============================================================================
// Board Reset
//----------------------------------------------------------------------------
//	BD,RLYS,ch,onoff
//----------------------------------------------------------------------------
void Mcu_RESET_Command(void)
{
}


//============================================================================
// ADC_DAta_Read_Command
//----------------------------------------------------------------------------
//	BD,RLYS,ch,onoff
//----------------------------------------------------------------------------
void ADC_Data_Read_Command(void)
{
	char txbuf[200]={0};

	sprintf(txbuf, "CH1,%lu,%lu\r\n", AdcPeakVal[0], AdcPeakCnt[0]);	// 원본은 배열 주소를 출력
	uart2_string_write(txbuf, strlen(txbuf));
}


//============================================================================
//	Reference Voltage
//============================================================================
//----------------------------------------------------------------------------
// Write Reference Voltage
//----------------------------------------------------------------------------
//	W_REFV,volt
//----------------------------------------------------------------------------
void Mcu_W_ALL_Command(void)
{
	char txbuf[50]={0};
	unsigned int offset = McuCmdSize+1;
	unsigned char * cptr = &McuCmdBuf[0] + offset;


	RefVolt = atol((char * )(cptr + McuParaPoint[0])) * 1;
	Trigger_Voltage_Set();

	OffsetVolt = atol((char * )(cptr + McuParaPoint[1])) * 1;
	Trigger_Voltage_Set();

	ReadInterval = atol((char * )(cptr + McuParaPoint[2])) * 1000;

	sprintf(txbuf, "%c%ld,%ld,%ld\r\n", STX, RefVolt, OffsetVolt, ReadInterval);
	uart2_string_write(txbuf, strlen(txbuf));
}

//----------------------------------------------------------------------------
// Read Reference Voltage
//----------------------------------------------------------------------------
//	R_REFV
//----------------------------------------------------------------------------
void Mcu_R_ALL_Command(void)
{
	char txbuf[50]={0};

	sprintf(txbuf, "%c%ld,%ld,%ld\r\n", STX, RefVolt, OffsetVolt, ReadInterval);
	uart1_string_write(txbuf, strlen(txbuf));
}

//============================================================================
//	Reference Voltage
//============================================================================
//----------------------------------------------------------------------------
// Write Reference Voltage
//----------------------------------------------------------------------------
//	W_REFV,volt
//----------------------------------------------------------------------------
void Mcu_W_REFV_Command(void)
{
	unsigned int offset = McuCmdSize+1;
	unsigned char * cptr = &McuCmdBuf[0] + offset;
	

	RefVolt = atol((char * )(cptr + McuParaPoint[0])) * 1;
	Trigger_Voltage_Set();
}

//----------------------------------------------------------------------------
// Read Reference Voltage
//----------------------------------------------------------------------------
//	R_REFV
//----------------------------------------------------------------------------
void Mcu_R_REFV_Command(void)
{
	char txbuf[50]={0};

	sprintf(txbuf, "%c%ld\r\n", STX, RefVolt);
	uart1_string_write(txbuf, strlen(txbuf));
}

//============================================================================
//	Offset Voltage
//============================================================================
//----------------------------------------------------------------------------
// Write Offset Voltage
//----------------------------------------------------------------------------
//	W_OFFSV,volt
//----------------------------------------------------------------------------
void Mcu_W_OFFSV_Command(void)
{
	unsigned int offset = McuCmdSize+1;
	unsigned char * cptr = &McuCmdBuf[0] + offset;
	
	OffsetVolt = atol((char * )(cptr + McuParaPoint[0])) * 1;
	Trigger_Voltage_Set();
}

//----------------------------------------------------------------------------
// Read Offset Voltage
//----------------------------------------------------------------------------
//	R_OFFSV
//----------------------------------------------------------------------------
void Mcu_R_OFFSV_Command(void)
{
	char txbuf[50]={0};

	sprintf(txbuf, "%c%ld%c", STX, OffsetVolt, ETX);
	uart1_string_write(txbuf, strlen(txbuf));
}

//============================================================================
//	Offset Voltage
//============================================================================
//----------------------------------------------------------------------------
// Write Offset Voltage
//----------------------------------------------------------------------------
//	W_OFFSV,volt
//----------------------------------------------------------------------------
void Mcu_W_RDTIME_Command(void)
{
	unsigned int offset = McuCmdSize+1;
	unsigned char * cptr = &McuCmdBuf[0] + offset;

	ReadInterval = atol((char * )(cptr + McuParaPoint[0]));
}

void Mcu_R_RDTIME_Command(void)
{
	char txbuf[50]={0};

	sprintf(txbuf, "%c%ld%c", STX, ReadInterval, ETX);
	uart1_string_write(txbuf, strlen(txbuf));
}

//============================================================================
//----------------------------------------------------------------------------
// ADC Debug Start (DS1343)
//----------------------------------------------------------------------------
//	ADC_START
//----------------------------------------------------------------------------
void Debug_ADC_START_Command(void)
{
	unsigned int offset = McuCmdSize+1;
	unsigned char * cptr = &McuCmdBuf[0] + offset;


	AdcChannel = atoi((char * )(cptr + McuParaPoint[0]));
	AdcReadNum = atoi((char * )(cptr + McuParaPoint[1]));

	Debug_ADC_Flag = 1;
	Debug_ADC_All_Flag = 0;
}

void Debug_ADC_STOP_Command(void)
{
	Debug_ADC_Flag = 0;
	Debug_ADC_All_Flag = 0;
}

//==============================================================================
//------------------------------------------------------------------------------
// Command_Length
//------------------------------------------------------------------------------
unsigned int Mcu_Command_Length(char * ptr)
{
	unsigned int i=0;

	while(1) {
		if( (ptr[i] == ',') || (ptr[i] == '\0') || (ptr[i] == '\r') ) break;
		i++;
	}

	return(i);
}

//------------------------------------------------------------------------------
//		Send "OK"/"NG" to PC
//------------------------------------------------------------------------------
void Mcu_Send_OK(void)
{
	char txbuf[20]={0};
				
	sprintf(txbuf, "%cOK\r\n", STX);
	uart1_string_write(txbuf, strlen(txbuf));
}

void Mcu_Send_NG(void)
{
	char txbuf[20]={0};
				
	sprintf(txbuf, "%cNG\r\n", STX);
	uart1_string_write(txbuf, strlen(txbuf));
}



/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
