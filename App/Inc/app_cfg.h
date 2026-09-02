/**
  ******************************************************************************
  * @file    app_cfg.h
  * @brief   SurgeDetector 컴파일 타임 설정
  ******************************************************************************
  */
#ifndef APP_CFG_H
#define APP_CFG_H

/* ---------------- 펌웨어 정보 ---------------- */
#define SD_FW_NAME                  "SurgeDetector"
#define SD_FW_VERSION               "1.0.0"

/* ---------------- 채널 / 프레임 ---------------- */
#define SD_ADC_CH_NUM               6u
#define SD_FPGA_FRAME_LEN           18u     /* 03_Protocol.md 1.3 참조 */
#define SD_FPGA_SOF0                0xA5u
#define SD_FPGA_SOF1                0x5Au
#define SD_FPGA_CHECK_CRC           1       /* 0 이면 CRC8 검사 생략 */
#define SD_FPGA_FRAME_TIMEOUT_MS    200u    /* 트리거 후 프레임 대기 시간 */

/* ---------------- 문자열 버퍼 ---------------- */
#define SD_SSID_MAX                 32u
#define SD_PASS_MAX                 64u
#define SD_IPSTR_MAX                16u     /* "255.255.255.255" + NUL */
#define SD_LINE_MAX                 96u     /* 데이터 출력 1라인 최대 */
#define SD_CLI_LINE_MAX             128u    /* PC 명령 1라인 최대 */
#define SD_AT_LINE_MAX              256u    /* ESP32 응답 1라인 최대 */

/* ---------------- UART DMA 링버퍼 ---------------- */
#define SD_U1_RX_BUF_SZ             512u    /* USART1 : ESP32   */
#define SD_U2_RX_BUF_SZ             256u    /* USART2 : FPGA    */
#define SD_U3_RX_BUF_SZ             512u    /* USART3 : RS485   */

/* ---------------- 큐 깊이 ---------------- */
#define SD_Q_SAMPLE_LEN             32u
#define SD_Q_WIFITX_LEN             32u
#define SD_Q_USBTX_LEN              16u
#define SD_Q_USBRX_LEN              512u

/* ---------------- 태스크 스택 (Words) ---------------- */
#define SD_STK_FPGA                 512u
#define SD_STK_ROUTER               640u
#define SD_STK_WIFI                 1024u
#define SD_STK_CLI_UART             768u
#define SD_STK_CLI_USB              768u
#define SD_STK_USBTX                384u

/* ---------------- 외부 플래시 (W25Q40CLS, 512KB) ---------------- */
#define SD_FLASH_SECTOR_SIZE        4096u
#define SD_FLASH_PAGE_SIZE          256u
#define SD_FLASH_TOTAL_SIZE         (512u * 1024u)
#define SD_CFG_SLOT_A_ADDR          0x000000u   /* sector 0 */
#define SD_CFG_SLOT_B_ADDR          0x001000u   /* sector 1 (미러) */
/* W25Q40CL JEDEC ID (9Fh) : EF 40 13 (SPI mode).
 * 제조사(0xEF)와 용량 코드(0x13 = 4Mbit)만 검증하고 메모리 타입 바이트는
 * 파트/모드에 따라 0x40 / 0x30 / 0x60 으로 다를 수 있어 참고용으로만 둔다. */
#define SD_W25Q40_MFG_ID            0xEFu
#define SD_W25Q40_CAP_ID            0x13u
#define SD_W25Q40_JEDEC_ID          0xEF4013u

/* ---------------- RS485 ---------------- */
/* 1 : PB1 을 일반 GPIO 로 두고 소프트웨어로 DE 제어
 * 0 : USART3 하드웨어 Driver Enable (권장)                              */
#define SD_RS485_SW_DE              0
/* 0 이면 주소 접두어(@nn:) 검사를 하지 않음 */
#define SD_RS485_ADDR               0

/* ---------------- 기타 ---------------- */
#define SD_ALLOW_PASS_READ          1       /* GET PASS 허용 여부 */
#define SD_USE_IWDG                 0       /* CubeMX 에서 IWDG 활성 시 1 */
#define SD_HEARTBEAT_MS             500u

/* ---------------- WiFi 백오프 ---------------- */
#define SD_WIFI_BACKOFF_MIN_MS      1000u
#define SD_WIFI_BACKOFF_MAX_MS      30000u
#define SD_WIFI_FAIL_BEFORE_HWRESET 5u

/* ---------------- 기본 설정값 ---------------- */
#define SD_DEF_SSID                 ""
#define SD_DEF_PASS                 ""
#define SD_DEF_SRV_IP               "192.168.0.10"
#define SD_DEF_SRV_PORT             50001u
#define SD_DEF_DHCP                 1u
#define SD_DEF_STA_IP               "192.168.0.50"
#define SD_DEF_GW                   "192.168.0.1"
#define SD_DEF_MASK                 "255.255.255.0"
#define SD_DEF_SAMPLE_MS            1000u

#endif /* APP_CFG_H */
