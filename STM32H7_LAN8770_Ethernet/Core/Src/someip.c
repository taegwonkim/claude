/**
  ******************************************************************************
  * @file    someip.c
  * @brief   SOME/IP / SOME/IP-SD 메시지 해석·생성
  ******************************************************************************
  */

#include "someip.h"
#include <stddef.h>
#include <string.h>

#define SD_ENTRY_LEN   16U

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t rd24(const uint8_t *p) { return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2]; }
static uint32_t rd32(const uint8_t *p)
{
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void wr24(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 16); p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)v; }
static void wr32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

/* ---------------------------------------------------------------------------
 *  SOME/IP
 * ------------------------------------------------------------------------ */
int32_t someip_parse(const uint8_t *buf, uint32_t len, someip_msg_t *msg)
{
  uint32_t length;

  if ((buf == NULL) || (msg == NULL) || (len < SOMEIP_HDR_LEN))
  {
    return SOMEIP_ERR_SHORT;
  }

  length = rd32(&buf[4]);
  if ((length < 8U) || (length > (len - 8U)))
  {
    return SOMEIP_ERR_LENGTH;
  }

  msg->service_id        = rd16(&buf[0]);
  msg->method_id         = rd16(&buf[2]);
  msg->length            = length;
  msg->client_id         = rd16(&buf[8]);
  msg->session_id        = rd16(&buf[10]);
  msg->protocol_version  = buf[12];
  msg->interface_version = buf[13];
  msg->message_type      = buf[14];
  msg->return_code       = buf[15];
  msg->payload           = &buf[SOMEIP_HDR_LEN];
  msg->payload_len       = length - 8U;

  return (int32_t)(length + 8U);
}

int someip_is_sd(const someip_msg_t *msg)
{
  return (msg->service_id == SOMEIP_SD_SERVICE_ID) && (msg->method_id == SOMEIP_SD_METHOD_ID);
}

int someip_is_event(const someip_msg_t *msg)
{
  return (msg->method_id & 0x8000U) != 0U;
}

/* ---------------------------------------------------------------------------
 *  SOME/IP-SD
 * ------------------------------------------------------------------------ */
int someip_sd_parse(const uint8_t *payload, uint32_t len, someip_sd_t *sd)
{
  uint32_t elen, olen;

  if ((payload == NULL) || (sd == NULL) || (len < 12U))
  {
    return -1;
  }
  elen = rd32(&payload[4]);
  if ((elen > (len - 12U)) || ((elen % SD_ENTRY_LEN) != 0U))
  {
    return -1;
  }
  olen = rd32(&payload[8U + elen]);
  if (olen > (len - 12U - elen))
  {
    return -1;
  }

  sd->flags       = payload[0];
  sd->entries     = &payload[8];
  sd->entries_len = elen;
  sd->options     = &payload[12U + elen];
  sd->options_len = olen;
  return 0;
}

uint32_t someip_sd_entry_count(const someip_sd_t *sd)
{
  return sd->entries_len / SD_ENTRY_LEN;
}

int someip_sd_get_entry(const someip_sd_t *sd, uint32_t index, someip_sd_entry_t *e)
{
  const uint8_t *p;

  if (index >= someip_sd_entry_count(sd))
  {
    return -1;
  }
  p = &sd->entries[index * SD_ENTRY_LEN];

  e->type          = p[0];
  e->index1        = p[1];
  e->index2        = p[2];
  e->num1          = (uint8_t)(p[3] >> 4);
  e->num2          = (uint8_t)(p[3] & 0x0FU);
  e->service_id    = rd16(&p[4]);
  e->instance_id   = rd16(&p[6]);
  e->major_version = p[8];
  e->ttl           = rd24(&p[9]);
  e->minor_version = 0U;
  e->counter       = 0U;
  e->eventgroup_id = 0U;

  if ((e->type == SOMEIP_SD_SUBSCRIBE) || (e->type == SOMEIP_SD_SUBSCRIBE_ACK))
  {
    e->counter       = (uint8_t)(p[13] & 0x0FU);
    e->eventgroup_id = rd16(&p[14]);
  }
  else
  {
    e->minor_version = rd32(&p[12]);
  }
  return 0;
}

/* index 번째 옵션의 시작 위치. 없으면 NULL */
static const uint8_t *option_at(const someip_sd_t *sd, uint32_t index, uint32_t *size)
{
  uint32_t off = 0U, i = 0U;

  while ((off + 3U) <= sd->options_len)
  {
    uint32_t sz = 3U + rd16(&sd->options[off]);

    if ((off + sz) > sd->options_len)
    {
      return NULL;
    }
    if (i == index)
    {
      *size = sz;
      return &sd->options[off];
    }
    off += sz;
    i++;
  }
  return NULL;
}

uint32_t someip_sd_option_count(const someip_sd_t *sd)
{
  uint32_t n = 0U, sz;

  while (option_at(sd, n, &sz) != NULL)
  {
    n++;
  }
  return n;
}

int someip_sd_get_option(const someip_sd_t *sd, uint32_t index, someip_sd_option_t *o)
{
  uint32_t sz = 0U;
  const uint8_t *p = option_at(sd, index, &sz);

  if (p == NULL)
  {
    return -1;
  }
  memset(o, 0, sizeof(*o));
  o->length      = rd16(&p[0]);
  o->type        = p[2];
  o->discardable = (uint8_t)((sz > 3U) ? ((p[3] >> 7) & 1U) : 0U);

  if (((o->type == SOMEIP_SD_OPT_IPV4_EP) || (o->type == SOMEIP_SD_OPT_IPV4_MCAST) ||
       (o->type == SOMEIP_SD_OPT_IPV4_SD_EP)) && (sz >= 12U))
  {
    memcpy(o->ipv4, &p[4], 4);
    o->l4_proto = p[9];
    o->port     = rd16(&p[10]);
  }
  return 0;
}

int someip_sd_entry_endpoint(const someip_sd_t *sd, const someip_sd_entry_t *e,
                             someip_sd_option_t *ep)
{
  uint32_t k;

  for (k = 0; k < e->num1; k++)
  {
    if ((someip_sd_get_option(sd, (uint32_t)e->index1 + k, ep) == 0) && (ep->type == SOMEIP_SD_OPT_IPV4_EP))
    {
      return 0;
    }
  }
  for (k = 0; k < e->num2; k++)
  {
    if ((someip_sd_get_option(sd, (uint32_t)e->index2 + k, ep) == 0) && (ep->type == SOMEIP_SD_OPT_IPV4_EP))
    {
      return 0;
    }
  }
  return -1;
}

uint32_t someip_sd_build_find(uint8_t *buf, uint32_t cap, uint16_t client_id,
                              uint16_t session_id, uint8_t flags,
                              uint16_t service_id, uint16_t instance_id,
                              uint8_t major, uint32_t minor, uint32_t ttl)
{
  /* header 16 + flags/reserved 4 + entries len 4 + entry 16 + options len 4 */
  const uint32_t total = SOMEIP_HDR_LEN + 4U + 4U + SD_ENTRY_LEN + 4U;
  uint8_t *e;

  if ((buf == NULL) || (cap < total))
  {
    return 0U;
  }
  memset(buf, 0, total);

  wr16(&buf[0], SOMEIP_SD_SERVICE_ID);
  wr16(&buf[2], SOMEIP_SD_METHOD_ID);
  wr32(&buf[4], total - 8U);
  wr16(&buf[8], client_id);
  wr16(&buf[10], session_id);
  buf[12] = SOMEIP_PROTOCOL_VERSION;
  buf[13] = 0x01U;                         /* SD interface version */
  buf[14] = SOMEIP_MT_NOTIFICATION;
  buf[15] = 0x00U;

  buf[16] = flags;                         /* + reserved 3 B (0) */
  wr32(&buf[20], SD_ENTRY_LEN);

  e = &buf[24];
  e[0] = SOMEIP_SD_FIND_SERVICE;           /* index/num options = 0 */
  wr16(&e[4], service_id);
  wr16(&e[6], instance_id);
  e[8] = major;
  wr24(&e[9], ttl);
  wr32(&e[12], minor);

  wr32(&buf[40], 0U);                      /* options length */
  return total;
}

/* ---------------------------------------------------------------------------
 *  이름
 * ------------------------------------------------------------------------ */
const char *someip_msg_type_name(uint8_t type)
{
  switch (type)
  {
    case 0x00: return "REQUEST";
    case 0x01: return "REQUEST_NO_RETURN";
    case 0x02: return "NOTIFICATION";
    case 0x80: return "RESPONSE";
    case 0x81: return "ERROR";
    case 0x20: return "TP_REQUEST";
    case 0x21: return "TP_REQUEST_NO_RETURN";
    case 0x22: return "TP_NOTIFICATION";
    case 0xA0: return "TP_RESPONSE";
    case 0xA1: return "TP_ERROR";
    default:   return "?";
  }
}

const char *someip_return_code_name(uint8_t rc)
{
  static const char *const names[] =
  {
    "E_OK", "E_NOT_OK", "E_UNKNOWN_SERVICE", "E_UNKNOWN_METHOD", "E_NOT_READY",
    "E_NOT_REACHABLE", "E_TIMEOUT", "E_WRONG_PROTOCOL_VERSION",
    "E_WRONG_INTERFACE_VERSION", "E_MALFORMED_MESSAGE", "E_WRONG_MESSAGE_TYPE",
    "E_E2E_REPEATED", "E_E2E_WRONG_SEQUENCE", "E_E2E", "E_E2E_NOT_AVAILABLE", "E_E2E_NO_NEW_DATA"
  };
  return (rc < (sizeof(names) / sizeof(names[0]))) ? names[rc] : "E_RESERVED";
}

const char *someip_sd_entry_name(const someip_sd_entry_t *e)
{
  switch (e->type)
  {
    case SOMEIP_SD_FIND_SERVICE:  return "FindService";
    case SOMEIP_SD_OFFER_SERVICE: return (e->ttl == 0U) ? "StopOfferService" : "OfferService";
    case SOMEIP_SD_SUBSCRIBE:     return (e->ttl == 0U) ? "StopSubscribeEventgroup" : "SubscribeEventgroup";
    case SOMEIP_SD_SUBSCRIBE_ACK: return (e->ttl == 0U) ? "SubscribeEventgroupNack" : "SubscribeEventgroupAck";
    default:                      return "UnknownEntry";
  }
}

const char *someip_sd_option_name(uint8_t type)
{
  switch (type)
  {
    case SOMEIP_SD_OPT_CONFIG:       return "Configuration";
    case SOMEIP_SD_OPT_LOAD_BALANCE: return "LoadBalancing";
    case SOMEIP_SD_OPT_IPV4_EP:      return "IPv4Endpoint";
    case SOMEIP_SD_OPT_IPV6_EP:      return "IPv6Endpoint";
    case SOMEIP_SD_OPT_IPV4_MCAST:   return "IPv4Multicast";
    case SOMEIP_SD_OPT_IPV6_MCAST:   return "IPv6Multicast";
    case SOMEIP_SD_OPT_IPV4_SD_EP:   return "IPv4SDEndpoint";
    case SOMEIP_SD_OPT_IPV6_SD_EP:   return "IPv6SDEndpoint";
    default:                         return "Unknown";
  }
}
