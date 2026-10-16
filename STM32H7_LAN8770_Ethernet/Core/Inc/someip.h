/**
  ******************************************************************************
  * @file    someip.h
  * @brief   SOME/IP / SOME/IP-SD 메시지 해석·생성 (하드웨어 독립, PC 시험 가능)
  ******************************************************************************
  * SOME/IP 헤더 (16 B, big endian)
  *   Service ID(16) Method/Event ID(16) | Length(32) |
  *   Client ID(16) Session ID(16) | ProtoVer(8) IfaceVer(8) MsgType(8) RetCode(8)
  *   Length = Request ID 부터 끝까지 = 8 + payload
  *
  * SOME/IP-SD : Service 0xFFFF, Method 0x8100, payload =
  *   Flags(8) Reserved(24) | EntriesLen(32) Entries[16B..] | OptionsLen(32) Options[..]
  ******************************************************************************
  */

#ifndef SOMEIP_H
#define SOMEIP_H

#include <stdint.h>

#define SOMEIP_HDR_LEN              16U
#define SOMEIP_PROTOCOL_VERSION     0x01U
#define SOMEIP_SD_SERVICE_ID        0xFFFFU
#define SOMEIP_SD_METHOD_ID         0x8100U

/* Message Type */
#define SOMEIP_MT_REQUEST           0x00U
#define SOMEIP_MT_REQUEST_NO_RETURN 0x01U
#define SOMEIP_MT_NOTIFICATION      0x02U
#define SOMEIP_MT_RESPONSE          0x80U
#define SOMEIP_MT_ERROR             0x81U
#define SOMEIP_MT_TP_FLAG           0x20U

/* SD Entry Type */
#define SOMEIP_SD_FIND_SERVICE      0x00U
#define SOMEIP_SD_OFFER_SERVICE     0x01U   /* TTL 0 = StopOffer     */
#define SOMEIP_SD_SUBSCRIBE         0x06U   /* TTL 0 = StopSubscribe */
#define SOMEIP_SD_SUBSCRIBE_ACK     0x07U   /* TTL 0 = Nack          */

/* SD Option Type */
#define SOMEIP_SD_OPT_CONFIG        0x01U
#define SOMEIP_SD_OPT_LOAD_BALANCE  0x02U
#define SOMEIP_SD_OPT_IPV4_EP       0x04U
#define SOMEIP_SD_OPT_IPV6_EP       0x06U
#define SOMEIP_SD_OPT_IPV4_MCAST    0x14U
#define SOMEIP_SD_OPT_IPV6_MCAST    0x16U
#define SOMEIP_SD_OPT_IPV4_SD_EP    0x24U
#define SOMEIP_SD_OPT_IPV6_SD_EP    0x26U

#define SOMEIP_SD_FLAG_REBOOT       0x80U
#define SOMEIP_SD_FLAG_UNICAST      0x40U
#define SOMEIP_SD_TTL_INFINITE      0xFFFFFFU

#define SOMEIP_ERR_SHORT            (-1)    /* 헤더보다 짧음         */
#define SOMEIP_ERR_LENGTH           (-2)    /* Length 필드가 이상함  */

typedef struct
{
  uint16_t       service_id;
  uint16_t       method_id;       /* bit15 = 1 이면 Event               */
  uint32_t       length;
  uint16_t       client_id;
  uint16_t       session_id;
  uint8_t        protocol_version;
  uint8_t        interface_version;
  uint8_t        message_type;
  uint8_t        return_code;
  const uint8_t *payload;
  uint32_t       payload_len;
} someip_msg_t;

typedef struct
{
  uint8_t        flags;
  const uint8_t *entries;
  uint32_t       entries_len;
  const uint8_t *options;
  uint32_t       options_len;
} someip_sd_t;

typedef struct
{
  uint8_t  type;
  uint8_t  index1, index2;        /* 옵션 run 시작 인덱스               */
  uint8_t  num1, num2;            /* 옵션 run 길이                       */
  uint16_t service_id;
  uint16_t instance_id;
  uint8_t  major_version;
  uint32_t ttl;                   /* 초, 0xFFFFFF = 무한                 */
  uint32_t minor_version;         /* Service entry (Find/Offer)          */
  uint8_t  counter;               /* Eventgroup entry                    */
  uint16_t eventgroup_id;         /* Eventgroup entry                    */
} someip_sd_entry_t;

typedef struct
{
  uint8_t  type;
  uint16_t length;                /* 옵션 Length 필드 값                 */
  uint8_t  discardable;
  uint8_t  ipv4[4];               /* IPv4 endpoint / multicast 일 때     */
  uint8_t  l4_proto;              /* 0x06 TCP, 0x11 UDP                  */
  uint16_t port;
} someip_sd_option_t;

/* 버퍼 앞의 SOME/IP 메시지 1 개 해석. 성공 시 소비한 바이트 수 (16+payload) */
int32_t someip_parse(const uint8_t *buf, uint32_t len, someip_msg_t *msg);

int someip_is_sd(const someip_msg_t *msg);
int someip_is_event(const someip_msg_t *msg);

/* SD payload 해석. 0 = 성공 */
int someip_sd_parse(const uint8_t *payload, uint32_t len, someip_sd_t *sd);
uint32_t someip_sd_entry_count(const someip_sd_t *sd);
int someip_sd_get_entry(const someip_sd_t *sd, uint32_t index, someip_sd_entry_t *e);
uint32_t someip_sd_option_count(const someip_sd_t *sd);
int someip_sd_get_option(const someip_sd_t *sd, uint32_t index, someip_sd_option_t *o);

/* entry 가 참조하는 옵션 중 첫 IPv4 endpoint 를 찾음. 0 = 찾음 */
int someip_sd_entry_endpoint(const someip_sd_t *sd, const someip_sd_entry_t *e,
                             someip_sd_option_t *ep);

/* FindService SD 메시지 생성. 반환 = 메시지 길이 (0 = 버퍼 부족) */
uint32_t someip_sd_build_find(uint8_t *buf, uint32_t cap, uint16_t client_id,
                              uint16_t session_id, uint8_t flags,
                              uint16_t service_id, uint16_t instance_id,
                              uint8_t major, uint32_t minor, uint32_t ttl);

const char *someip_msg_type_name(uint8_t type);
const char *someip_return_code_name(uint8_t rc);
const char *someip_sd_entry_name(const someip_sd_entry_t *e);
const char *someip_sd_option_name(uint8_t type);

#endif /* SOMEIP_H */
