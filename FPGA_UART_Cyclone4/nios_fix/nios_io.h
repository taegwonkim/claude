/*
 * nios_io.h
 *
 *  Created on: 2017. 11. 27.
 *      Author: Taegwon Kim
 *
 *  2026.10 : UART 수신을 링버퍼 + 메인루프 파싱 구조로 변경, RS485 DE 타이밍 수정
 */

#ifndef NIOS_IO_H_
#define NIOS_IO_H_


/* UART 설정 -----------------------------------------------------------------*/
// 0 : Platform Designer 의 altera_avalon_uart (RX 버퍼 1바이트) 사용 - 기존 하드웨어 그대로
// 1 : rtl/fifo_uart_avalon.v ("FIFO UART", RX/TX FIFO 256바이트) 로 교체한 경우
#define USE_FIFO_UART			0

// 소프트웨어 수신 링버퍼 크기 (2의 거듭제곱)
#define UART_RING_SIZE			512

// UART1(MCU) 송신 전/후 MCU_RX 핸드셰이크 대기 시간 [ms]
//   - 원본 코드 값(20ms / 3ms)을 그대로 두었다. 메시지마다 이만큼 공백이 생기므로
//     MCU 쪽이 허용하면 1~2ms 로 줄일 것.
//   - POST 대기는 마지막 바이트가 핀에서 완전히 나간 뒤(TX 완료)부터 센다.
#define UART1_PRE_DELAY_MS		20
#define UART1_POST_DELAY_MS		3

// RS485(UART2) 송신 중 자기 에코 수신 버림 (트랜시버 /RE 가 항상 Low 인 경우 1)
#define UART2_DISCARD_ECHO		1


/* Defines -------------------------------------------------------------------*/
typedef struct
{
	unsigned long Port;
	unsigned long Baud;
	void (*ISR_FuncPtr)(char Ch);
	void *pBase;

} HW_UART_OBJ;

typedef struct
{
	volatile unsigned char  buf[UART_RING_SIZE];
	volatile unsigned int   head;		// ISR 가 씀
	volatile unsigned int   tail;		// 메인루프가 씀
	volatile unsigned long  overflow;	// 링버퍼 가득 참으로 버린 바이트 수
	volatile unsigned long  hw_error;	// HW overrun / framing / parity 에러 횟수
} UART_RING;


/* Global Variables ----------------------------------------------------------*/
extern UART_RING Uart1_Ring, Uart2_Ring;


/* Prototypes of Global Functions --------------------------------------------*/
void Control_Output_Init(void);

void Read_Reference_Voltage(void);
void Read_Offset_Voltage(void);
void Trigger_Voltage_Set(void);

void Read_Data_ReadCount(void);

void Uart_Init(void);
void Uart_Rx_Poll(void);

int uart1_string_write(char* ptr, int len);
int uart2_string_write(char* ptr, int len);


#endif /* NIOS_IO_H_ */
