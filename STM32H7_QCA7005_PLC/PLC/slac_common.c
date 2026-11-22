/*
 * slac_common.c - helpers shared by the EVSE and EV SLAC state machines
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "slac.h"

const char *slac_fail_name(slac_fail_t f)
{
    switch (f) {
    case SLAC_FAIL_NONE:           return "none";
    case SLAC_FAIL_SET_KEY:        return "no CM_SET_KEY.CNF (modem)";
    case SLAC_FAIL_INIT_TIMEOUT:   return "TT_EVSE_SLAC_init expired";
    case SLAC_FAIL_NO_PARAM_CNF:   return "no CM_SLAC_PARAM.CNF";
    case SLAC_FAIL_NO_ATTEN_CHAR:  return "no CM_ATTEN_CHAR.IND";
    case SLAC_FAIL_NO_ATTEN_RSP:   return "no CM_ATTEN_CHAR.RSP";
    case SLAC_FAIL_NO_MATCH_REQ:   return "no CM_SLAC_MATCH.REQ";
    case SLAC_FAIL_NO_MATCH_CNF:   return "no CM_SLAC_MATCH.CNF";
    case SLAC_FAIL_ATTEN_TOO_HIGH: return "attenuation above limit";
    case SLAC_FAIL_SEND:           return "send error";
    default:                       return "?";
    }
}
