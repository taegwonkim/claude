/*
 * nios_io.h
 *
 *  Created on: 2017. 11. 27.
 *      Author: Taegwon Kim
 */

#ifndef NIOS_IO_H_
#define NIOS_IO_H_


/* Defines -------------------------------------------------------------------*/
typedef struct
{
	unsigned long Port;
	unsigned long Baud;
	void (*ISR_FuncPtr)(char Ch);
	void *pBase;

} HW_UART_OBJ;


/* Global Variables ----------------------------------------------------------*/


/* Prototypes of Global Functions --------------------------------------------*/
void Control_Output_Init(void);

void Read_Reference_Voltage(void);
void Read_Offset_Voltage(void);
void Trigger_Voltage_Set(void);

void Read_Data_ReadCount(void);

int uart1_string_write(char* ptr, int len);
int uart2_string_write(char* ptr, int len);


#endif /* NIOS_IO_H_ */
