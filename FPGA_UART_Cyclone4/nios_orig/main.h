/*
 * main.h
 *
 *  Created on: 2022. 8. 25.
 *      Author: taegw
 */

#ifndef MAIN_H_
#define MAIN_H_


/* Type Defines --------------------------------------------------------------*/
//typedef enum {FALSE=0, TRUE=1} Bool; //enables boolean functions
//typedef enum { FALSE  = 0, TRUE   = !FALSE}  BoolState;

typedef enum { FALSE  = 0, TRUE   = !FALSE}  BoolState;

/* Defines -------------------------------------------------------------------*/
#define STX					0x02     /* Start of text		 */
#define ETX					0x03     /* End of text			 */

#define TXBUFSIZE			100
#define RXBUFSIZE			100
#define CMDBUFSIZE			100

#define	ADC_AVERAGE_NUM		1000

/* Defines -------------------------------------------------------------------*/
#define LED_NIOS		(long)0x01
#define	LED_FPGA		(long)0x02
#define LED_WIFI		(long)0x04
#define LED_ERROR		(long)0x08
#define LED_ALL			(long)0x0f

#define TPOINT_1		(long)0x01
#define TPOINT_2		(long)0x02
#define TPOINT_3		(long)0x04
#define TPOINT_4		(long)0x08

#define msleep(ms)  usleep(1000*ms)

/* Global variables ----------------------------------------------------------*/
extern	unsigned long TickCnt;

extern	unsigned char UART1_RxOK_Flag, UART1_Error_Flag;
extern	unsigned char UART1_RxBuf[RXBUFSIZE];
extern	unsigned int  UART1_RxCnt, UART1_RxLength;

extern	unsigned char UART2_RxOK_Flag, UART2_Error_Flag;
extern	unsigned char UART2_RxBuf[RXBUFSIZE];
extern	unsigned int  UART2_RxCnt, UART2_RxLength;
/*
extern	volatile unsigned long TickCnt;

extern	volatile unsigned char UART1_RxOK_Flag, UART1_Error_Flag;
extern	volatile unsigned char UART1_RxBuf[RXBUFSIZE];
extern	volatile unsigned int  UART1_RxCnt, UART1_RxLength;

extern	volatile unsigned char UART2_RxOK_Flag, UART2_Error_Flag;
extern	volatile unsigned char UART2_RxBuf[RXBUFSIZE];
extern	volatile unsigned int  UART2_RxCnt, UART2_RxLength;
*/

#endif /* MAIN_H_ */
