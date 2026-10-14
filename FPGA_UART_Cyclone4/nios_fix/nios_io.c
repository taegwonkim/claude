/*
 * nios_io.c
 *
 *  Created on: 2017. 11. 27.
 *      Author: Taegwon Kim
 *
 *  2026.10 수정 내용
 *   - UART 수신 ISR 은 하드웨어에서 바이트를 꺼내 링버퍼에 넣기만 한다 (최소 실행시간)
 *   - 패킷 조립(STX ~ '\n', STX ~ ETX)은 메인루프의 Uart_Rx_Poll() 에서 수행
 *       → Mcu_Command_Proc() 실행 중에 ISR 이 UART1_RxBuf 를 덮어쓰는 문제 제거
 *       → RxBuf 오버플로(인덱스 100 쓰기) 제거
 *   - ISR 에서 overrun/framing 에러 확인 및 클리어
 *   - 송신 종료를 TRDY 가 아니라 TMT/TX_IDLE (마지막 비트까지 송신 완료)로 판단
 *       → RS485 DE 를 너무 일찍 내려 마지막 1~2 바이트가 잘리던 문제 수정
 *   - 빈 for 루프 지연 제거
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
#include "adc_ltc2292.h"
#include "nios_io.h"

#if USE_FIFO_UART
#include "fifo_uart_regs.h"
#else
#include "altera_avalon_uart_regs.h"
#endif


/* UART 하드웨어 추상화 ------------------------------------------------------*/
#if USE_FIFO_UART
  #define UART_TX_READY(base)		(!(IORD_FIFO_UART_STATUS(base) & FIFO_UART_STATUS_TX_FULL_MSK))
  #define UART_TX_WRITE(base, c)	IOWR_FIFO_UART_DATA(base, (unsigned char)(c))
  #define UART_TX_DONE(base)		(IORD_FIFO_UART_STATUS(base) & FIFO_UART_STATUS_TX_IDLE_MSK)
  #define UART_RX_IRQ_ENABLE(base)	IOWR_FIFO_UART_CONTROL(base, FIFO_UART_CONTROL_RXIE_MSK)
#else
  #define UART_TX_READY(base)		(IORD_ALTERA_AVALON_UART_STATUS(base) & ALTERA_AVALON_UART_STATUS_TRDY_MSK)
  #define UART_TX_WRITE(base, c)	IOWR_ALTERA_AVALON_UART_TXDATA(base, (unsigned char)(c))
  #define UART_TX_DONE(base)		(IORD_ALTERA_AVALON_UART_STATUS(base) & ALTERA_AVALON_UART_STATUS_TMT_MSK)
  #define UART_RX_IRQ_ENABLE(base)	IOWR_ALTERA_AVALON_UART_CONTROL(base, ALTERA_AVALON_UART_CONTROL_RRDY_MSK)
  #define UART_ERR_MSK				(ALTERA_AVALON_UART_STATUS_ROE_MSK | ALTERA_AVALON_UART_STATUS_FE_MSK | \
									 ALTERA_AVALON_UART_STATUS_PE_MSK  | ALTERA_AVALON_UART_STATUS_BRK_MSK)
#endif

#define UART_RING_MASK			(UART_RING_SIZE - 1)


/* Global variables ----------------------------------------------------------*/
HW_UART_OBJ UART1_OBJ, UART2_OBJ;
UART_RING   Uart1_Ring, Uart2_Ring;

static volatile unsigned char uart2_tx_active = 0;


//------------------------------------------------------------------------------
//		ms 단위 대기 (timer_1ms_isr 의 WaitCnt 사용)
//------------------------------------------------------------------------------
static void wait_ms(unsigned long ms)
{
	WaitCnt = 0;
	while(WaitCnt <= ms) {}
}

//------------------------------------------------------------------------------
//		링버퍼
//------------------------------------------------------------------------------
static void ring_put(UART_RING *r, unsigned char c)
{
	unsigned int next = (r->head + 1) & UART_RING_MASK;

	if(next == r->tail)
	{
		r->overflow++;
		return;
	}
	r->buf[r->head] = c;
	r->head = next;
}

static int ring_get(UART_RING *r, unsigned char *c)
{
	if(r->tail == r->head) return 0;

	*c = r->buf[r->tail];
	r->tail = (r->tail + 1) & UART_RING_MASK;
	return 1;
}

//------------------------------------------------------------------------------
//		UART 수신 ISR : 하드웨어 → 링버퍼 (그 외 처리 금지)
//------------------------------------------------------------------------------
static void uart_rx_to_ring(unsigned long base, UART_RING *r, int discard)
{
#if USE_FIFO_UART
	unsigned long d, status;

	// FIFO 에 쌓인 바이트를 모두 꺼낸다
	while((d = IORD_FIFO_UART_DATA(base)) & FIFO_UART_DATA_VALID_MSK)
	{
		if(!discard) ring_put(r, (unsigned char)(d & FIFO_UART_DATA_MSK));
	}

	status = IORD_FIFO_UART_STATUS(base);
	if(status & (FIFO_UART_STATUS_RX_OVF_MSK | FIFO_UART_STATUS_RX_FERR_MSK))
	{
		r->hw_error++;
		IOWR_FIFO_UART_STATUS(base, FIFO_UART_STATUS_RX_OVF_MSK | FIFO_UART_STATUS_RX_FERR_MSK);
	}
#else
	unsigned long status = IORD_ALTERA_AVALON_UART_STATUS(base);

	if(status & UART_ERR_MSK)
	{
		// ROE 가 켜져 있으면 ISR 이 1문자 시간 안에 못 돌아서 바이트를 잃은 것
		r->hw_error++;
		IOWR_ALTERA_AVALON_UART_STATUS(base, 0);	// 에러 비트 클리어
	}

	if(status & ALTERA_AVALON_UART_STATUS_RRDY_MSK)
	{
		unsigned char c = (unsigned char)IORD_ALTERA_AVALON_UART_RXDATA(base);	// RRDY(=IRQ) 클리어
		if(!discard) ring_put(r, c);
	}
#endif
}

static void uart1_rxd_isr(void *context)
{
	uart_rx_to_ring(UART1_BASE, &Uart1_Ring, 0);
}

static void uart2_rxd_isr(void *context)
{
	uart_rx_to_ring(UART2_BASE, &Uart2_Ring, UART2_DISCARD_ECHO && uart2_tx_active);
}

//------------------------------------------------------------------------------
//		void Uart_Init(void)
//------------------------------------------------------------------------------
void Uart_Init(void)
{
	memset((void *)&Uart1_Ring, 0, sizeof(Uart1_Ring));
	memset((void *)&Uart2_Ring, 0, sizeof(Uart2_Ring));

	alt_iic_isr_register(UART1_IRQ_INTERRUPT_CONTROLLER_ID, UART1_IRQ, uart1_rxd_isr, 0x0, 0x0);
	UART_RX_IRQ_ENABLE(UART1_BASE);

	alt_iic_isr_register(UART2_IRQ_INTERRUPT_CONTROLLER_ID, UART2_IRQ, uart2_rxd_isr, 0x0, 0x0);
	UART_RX_IRQ_ENABLE(UART2_BASE);
}

//------------------------------------------------------------------------------
//		void Uart_Rx_Poll(void) : 메인루프에서 호출, 패킷 조립
//		  - 패킷 1개가 완성되면 즉시 리턴 (나머지 바이트는 링버퍼에 남겨 다음 호출에서 처리)
//		  - 원본 ISR 과 동일한 프로토콜/플래그 동작
//------------------------------------------------------------------------------
void Uart_Rx_Poll(void)
{
	static unsigned char rx1_active = 0;
	unsigned char data;

	// ---- UART1 : NIOS <==> MCU(WiFi), STX ~ '\n' ----
	while(ring_get(&Uart1_Ring, &data))
	{
		if(data == STX)
		{
			UART1_RxCnt = 0;
			UART1_RxOK_Flag = 0;
			UART1_Error_Flag = 0;
			rx1_active = 1;
		}

		if(!rx1_active) continue;

		if(UART1_RxCnt >= RXBUFSIZE - 1)		// '\0' 자리 확보
		{
			UART1_Error_Flag = 1;
			rx1_active = 0;
			continue;
		}

		UART1_RxBuf[UART1_RxCnt++] = data;

		if(data == '\n')
		{
			UART1_RxBuf[UART1_RxCnt] = 0x00;
			UART1_RxLength = UART1_RxCnt;
			UART1_RxOK_Flag = 1;
			rx1_active = 0;
			break;								// 완성된 패킷은 처리 후 다음 패킷
		}
	}

	// ---- UART2 : Debug Port, STX ~ ETX ----
	while(ring_get(&Uart2_Ring, &data))
	{
		if(UART2_RxCnt == 0)
		{
			if(data != STX)
			{
				UART2_Error_Flag = 1;
			}
			else
			{
				UART2_RxBuf[0] = data;
				UART2_RxCnt = 1;
				UART2_Error_Flag = 0;
			}
		}
		else
		{
			if(UART2_RxCnt >= RXBUFSIZE)
			{
				UART2_Error_Flag = 1;
				UART2_RxCnt = 0;
				continue;
			}

			UART2_RxBuf[UART2_RxCnt++] = data;

			if(data == ETX)
			{
				UART2_RxLength = UART2_RxCnt;
				UART2_RxOK_Flag = 1;
				UART2_Error_Flag = 0;
				UART2_RxCnt = 0;
				break;
			}
		}
	}
}


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
}


//------------------------------------------------------------------------------
//		int uart1_string_write(char* ptr, int len)
//		  MCU_RX Low → PRE 대기 → 송신 → 마지막 비트까지 완료 → POST 대기 → MCU_RX High
//------------------------------------------------------------------------------
int uart1_string_write(char* ptr, int len)
{
	int i;

	IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(MCU_RX_BASE, 0x01);
	wait_ms(UART1_PRE_DELAY_MS);

	for(i = 0; i < len; i++)
	{
		while(!UART_TX_READY(UART1_BASE)) {}
		UART_TX_WRITE(UART1_BASE, ptr[i]);
	}

	while(!UART_TX_DONE(UART1_BASE)) {}		// 시프트 레지스터까지 비었는지 확인

#if UART1_POST_DELAY_MS > 0
	wait_ms(UART1_POST_DELAY_MS);
#endif
	IOWR_ALTERA_AVALON_PIO_SET_BITS(MCU_RX_BASE, 0x01);

	return len;
}


//------------------------------------------------------------------------------
//		int uart2_string_write(char* ptr, int len) : RS485
//------------------------------------------------------------------------------
int uart2_string_write(char* ptr, int len)
{
	int i;

	uart2_tx_active = 1;
	IOWR_ALTERA_AVALON_PIO_SET_BITS(UART2_DE_BASE, 0x01);

	for(i = 0; i < len; i++)
	{
		while(!UART_TX_READY(UART2_BASE)) {}
		UART_TX_WRITE(UART2_BASE, ptr[i]);
	}

	// ★ TRDY 는 "홀딩 레지스터가 비었다"는 뜻일 뿐 마지막 바이트는 아직 송신 중.
	//   TMT(송신 완료)까지 기다린 뒤 DE 를 내려야 마지막 바이트가 잘리지 않는다.
	while(!UART_TX_DONE(UART2_BASE)) {}

	IOWR_ALTERA_AVALON_PIO_CLEAR_BITS(UART2_DE_BASE, 0x01);
	uart2_tx_active = 0;

	return len;
}


//------------------------------------------------------------------------------
