/**
  ******************************************************************************
  * @file    app_net.h
  * @brief   LAN8770 데모 애플리케이션 : UDP 에코 서버 + 링크/SQI 상태 로그
  ******************************************************************************
  */

#ifndef APP_NET_H
#define APP_NET_H

#ifdef __cplusplus
extern "C" {
#endif

/* MX_LWIP_Init() 다음에 한 번 호출 */
void APP_Net_Init(void);

/* while(1) 안에서 MX_LWIP_Process() 와 함께 계속 호출 */
void APP_Net_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_NET_H */
