/**
  ******************************************************************************
  * @file    veh_frame.c
  * @brief   Ethernet / 802.1Q 헤더 해석
  ******************************************************************************
  */

#include "veh_frame.h"
#include <string.h>

static uint16_t rd16(const uint8_t *p)
{
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

int veh_frame_parse(const uint8_t *frame, uint32_t len, veh_frame_info_t *out)
{
  uint16_t type;

  if ((frame == NULL) || (out == NULL) || (len < 14U))
  {
    return -1;
  }

  memcpy(out->dst, &frame[0], 6);
  memcpy(out->src, &frame[6], 6);
  type = rd16(&frame[12]);

  out->tagged     = 0U;
  out->pcp        = 0U;
  out->dei        = 0U;
  out->vid        = VEH_VLAN_NONE;
  out->ethertype  = type;
  out->header_len = 14U;

  if ((type == VEH_ETHTYPE_VLAN) || (type == VEH_ETHTYPE_QINQ))
  {
    uint16_t tci;

    if (len < 18U)
    {
      return -1;
    }
    tci = rd16(&frame[14]);
    out->tagged     = 1U;
    out->pcp        = (uint8_t)(tci >> 13);
    out->dei        = (uint8_t)((tci >> 12) & 1U);
    out->vid        = (uint16_t)(tci & 0x0FFFU);
    out->ethertype  = rd16(&frame[16]);
    out->header_len = 18U;
  }
  return 0;
}

uint16_t veh_vlan_tci(uint8_t pcp, uint16_t vid)
{
  return (uint16_t)(((uint16_t)(pcp & 0x07U) << 13) | (vid & 0x0FFFU));
}

const char *veh_ethertype_name(uint16_t ethertype)
{
  switch (ethertype)
  {
    case VEH_ETHTYPE_IPV4:   return "IPv4";
    case VEH_ETHTYPE_ARP:    return "ARP";
    case VEH_ETHTYPE_VLAN:   return "802.1Q";
    case VEH_ETHTYPE_QINQ:   return "802.1ad";
    case VEH_ETHTYPE_IPV6:   return "IPv6";
    case VEH_ETHTYPE_AVTP:   return "AVTP(1722)";
    case VEH_ETHTYPE_GPTP:   return "gPTP";
    case VEH_ETHTYPE_LLDP:   return "LLDP";
    case VEH_ETHTYPE_MACSEC: return "MACsec";
    default:                 return "?";
  }
}
