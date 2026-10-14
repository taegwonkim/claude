/*
 * nios_io.c
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
#include "sys/alt_dev.h"
#include "priv/alt_iic_isr_register.h"

#include "altera_avalon_timer_regs.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_uart_regs.h"
#include "altera_avalon_uart.h"
#include "altera_avalon_uart_fd.h"
#include "altera_avalon_spi_regs.h"
#include "altera_avalon_spi.h"

#include "unistd.h"

#include "main.h"
#include "adc_ltc2292.h"
#include "nios_io.h"


/* Global variables ----------------------------------------------------------*/
HW_UART_OBJ UART1_OBJ, UART2_OBJ;

extern	volatile unsigned long  WaitCnt;


//------------------------------------------------------------------------------
//		Zigbee_Id_Read
//------------------------------------------------------------------------------
void Control_Output_Init(void)
{
	// LED off
	IOWR_ALTERA_AVALON_PIO_SET_BITS(LED_OUT_BASE, LED_ALL);
}

//----------------------------------------------------------------------------
// Read Reference Voltage
//----------------------------------------------------------------------------
void Read_Reference_Voltage(void)
{
	//RefVolt = atol((char * )&EEP_RxBuf[1]) * 1;
}

//----------------------------------------------------------------------------
// Read Offset Voltage
//----------------------------------------------------------------------------
void Read_Offset_Voltage(void)
{
	//OffsetVolt = atol((char * )&EEP_RxBuf[1]) * 1;
}

//------------------------------------------------------------------------------
//		Pan_Id_Read
//------------------------------------------------------------------------------
void Trigger_Voltage_Set(void)
{
	double f;
	unsigned long data;

	f = (double)(RefVolt + OffsetVolt);
	data = (unsigned long)(f / (double)39.5257);//ADC_CONST) + 0;

	IOWR_ALTERA_AVALON_PIO_DATA(NIOS_TX_COMP_BASE, data);
/*
	{
		char buf[100]={0};

		sprintf(buf, "\nComp Value=%ld\r\n", data);
		uart2_string_write(buf, strlen(buf));
	}
	*/
}


//------------------------------------------------------------------------------
//		int uart1_string_write(char* ptr, int len)
//------------------------------------------------------------------------------
int uart1_string_write(char* ptr, int len)
{
  unsigned int status;
  int count;

  IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(MCU_RX_BASE, 0x01);
  WaitCnt = 0;
  while(1)  {
	  if(WaitCnt > 20) break;
  }

  count = len;

  do
  {
    status = IORD_ALTERA_AVALON_UART_STATUS(UART1_BASE);

    if (status & ALTERA_AVALON_UART_STATUS_TRDY_MSK)
    {
      IOWR_ALTERA_AVALON_UART_TXDATA(UART1_BASE, *ptr);
      ptr++;
      count--;
    }
  }
  while (count);

  WaitCnt = 0;
  while(1)  {
	  if(WaitCnt > 3) break;
  }
  IOWR_ALTERA_AVALON_PIO_SET_BITS(MCU_RX_BASE, 0x01);

  return (len - count);
}


//------------------------------------------------------------------------------
//		int uart2_string_write(char* ptr, int len)
//------------------------------------------------------------------------------
int uart2_string_write(char* ptr, int len)
{
  unsigned int status;
  int count, i;

  IOWR_ALTERA_AVALON_PIO_SET_BITS(UART2_DE_BASE, 0x01);
  for(i=0; i<3; i++) {}

  count = len;

  do
  {
    status = IORD_ALTERA_AVALON_UART_STATUS(UART2_BASE);

    if (status & ALTERA_AVALON_UART_STATUS_TRDY_MSK)
    {
      IOWR_ALTERA_AVALON_UART_TXDATA(UART2_BASE, *ptr++);
      count--;
      //for(i=0; i<3; i++) {}
    }
  }
  while (count);

  for(i=0; i<3; i++) {}
  IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(UART2_DE_BASE, 0x01);

  return (len - count);
}


//------------------------------------------------------------------------------



