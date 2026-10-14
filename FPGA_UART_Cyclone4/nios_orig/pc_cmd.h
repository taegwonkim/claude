/**
  ******************************************************************************
  * File Name          : pc_cmd.h
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef pc_cmd_H_
#define pc_cmd_H_


/* Includes ------------------------------------------------------------------*/

/* Defines -------------------------------------------------------------------*/
	 

/* Global variables ----------------------------------------------------------*/
extern	volatile unsigned char McuLed_Flag, McuCmdMode_Flag;
extern	volatile unsigned int  McuLed_Cnt;

/* Global variables ----------------------------------------------------------*/
extern	unsigned char McuCmdBuf[CMDBUFSIZE];
extern	unsigned int  McuParaPoint[20], McuParaNum, McuCmdSize, McuCmdDataLen;
extern	unsigned int  McuId, PanId;

extern	unsigned char PeakDetect_Flag;

extern	unsigned char Wifi_LED_Flag;

extern	unsigned char Debug_ADC_Flag, Debug_ADC_All_Flag;

	 
/* Prototypes of Global Functions --------------------------------------------*/
int Mcu_Command_Check(void);
void Mcu_Command_Proc(void);

void ADC_Data_Read_Command(void);

void PeakDetect_START_Command(void);

void Mcu_RESET_Command(void);

void Mcu_W_ALL_Command(void);
void Mcu_R_ALL_Command(void);
void Mcu_W_REFV_Command(void);
void Mcu_R_REFV_Command(void);
void Mcu_W_OFFSV_Command(void);
void Mcu_R_OFFSV_Command(void);
void Mcu_W_RDTIME_Command(void);
void Mcu_R_RDTIME_Command(void);

void Debug_ADC_START_Command(void);
void Debug_ADC_STOP_Command(void);

unsigned int Mcu_Command_Length(char * ptr);

void Mcu_Send_OK(void);
void Mcu_Send_NG(void);


#endif /*pc_cmd_H_ */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
