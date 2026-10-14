/*
 * adc_ltc2292.h
 *
 *  Created on: 2017. 11. 27.
 *      Author: Taegwon Kim
 */

#ifndef ADC_LTC2292_H_
#define ADC_LTC2292_H_


//#define	ADC_CONST	(double)24.703557	// 50V
#define	ADC_CONST	(double)39.5257	// 80V

/* Global Variables ----------------------------------------------------------*/
extern	unsigned long int RefVolt, OffsetVolt, CheckVolt, ReadInterval;

extern	unsigned long int AdcNormalVal[6], AdcPeakVal[6];
extern	unsigned long int AdcNormalCnt[6], AdcPeakCnt[6];

extern	unsigned long int AdcAvrgVal[6], AdcAvrgSum[6], AdcAvrgCnt[6];

extern	unsigned int  AdcChannel, AdcReadNum;


/* Prototypes of Global Functions --------------------------------------------*/
void Init_ADC_Average_Buffer(void);

void ADC_Data_to_MCU_All(void);
void ADC_Data_Read_from_Dpram(unsigned long memory_base, int ch);
void ADC_Data_Average(void);


#endif /* ADC_LTC2292_H_ */
