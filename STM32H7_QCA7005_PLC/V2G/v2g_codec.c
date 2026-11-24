/*
 * v2g_codec.c - EXI encode/decode wrappers around libcbv2g
 *               (chargebyte / EVerest, Apache-2.0)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include <math.h>
#include "v2g_codec.h"
#include "v2gtp.h"
#include "cbv2g/common/exi_bitstream.h"
#include "cbv2g/din/din_msgDefEncoder.h"
#include "cbv2g/din/din_msgDefDecoder.h"
#include "cbv2g/app_handshake/appHand_Encoder.h"
#include "cbv2g/app_handshake/appHand_Decoder.h"

struct din_exiDocument     g_din_doc;
struct appHand_exiDocument g_app_doc;

void v2g_din_clear(void)
{
    memset(&g_din_doc, 0, sizeof(g_din_doc));
}

void v2g_app_clear(void)
{
    memset(&g_app_doc, 0, sizeof(g_app_doc));
}

static int finish(exi_bitstream_t *s, uint8_t *buf, uint32_t *len)
{
    uint32_t exi_len = (uint32_t)exi_bitstream_get_length(s);
    v2gtp_write_header(buf, V2GTP_TYPE_EXI, exi_len);
    *len = V2GTP_HDR_LEN + exi_len;
    return 0;
}

int v2g_encode_din(uint8_t *buf, uint32_t cap, uint32_t *len)
{
    exi_bitstream_t s;
    if (cap <= V2GTP_HDR_LEN)
        return -1;
    memset(buf, 0, cap);
    exi_bitstream_init(&s, buf, cap, V2GTP_HDR_LEN, NULL);
    if (encode_din_exiDocument(&s, &g_din_doc) != 0)
        return -1;
    return finish(&s, buf, len);
}

int v2g_encode_app(uint8_t *buf, uint32_t cap, uint32_t *len)
{
    exi_bitstream_t s;
    if (cap <= V2GTP_HDR_LEN)
        return -1;
    memset(buf, 0, cap);
    exi_bitstream_init(&s, buf, cap, V2GTP_HDR_LEN, NULL);
    if (encode_appHand_exiDocument(&s, &g_app_doc) != 0)
        return -1;
    return finish(&s, buf, len);
}

int v2g_decode_din(const uint8_t *exi, uint32_t len)
{
    exi_bitstream_t s;
    v2g_din_clear();
    exi_bitstream_init(&s, (uint8_t *)exi, len, 0, NULL);
    return decode_din_exiDocument(&s, &g_din_doc) == 0 ? 0 : -1;
}

int v2g_decode_app(const uint8_t *exi, uint32_t len)
{
    exi_bitstream_t s;
    v2g_app_clear();
    exi_bitstream_init(&s, (uint8_t *)exi, len, 0, NULL);
    return decode_appHand_exiDocument(&s, &g_app_doc) == 0 ? 0 : -1;
}

void v2g_pv_set(struct din_PhysicalValueType *pv, float value, din_unitSymbolType unit)
{
    int8_t m = 0;
    float x = value;

    /* scale down until it fits, then keep one decimal where possible */
    while (fabsf(x) > 32767.0f && m < 3) {
        x /= 10.0f;
        m++;
    }
    if (m == 0 && fabsf(x * 10.0f) <= 32767.0f && fabsf(x - roundf(x)) > 0.001f) {
        x *= 10.0f;
        m = -1;
    }
    pv->Value = (int16_t)lroundf(x);
    pv->Multiplier = m;
    pv->Unit = unit;
    pv->Unit_isUsed = 1;
}

float v2g_pv_get(const struct din_PhysicalValueType *pv)
{
    float v = (float)pv->Value;
    int8_t m = pv->Multiplier;
    while (m > 0) { v *= 10.0f; m--; }
    while (m < 0) { v /= 10.0f; m++; }
    return v;
}

const char *v2g_din_body_name(const struct din_BodyType *b)
{
    if (b->SessionSetupReq_isUsed) return "SessionSetupReq";
    if (b->SessionSetupRes_isUsed) return "SessionSetupRes";
    if (b->ServiceDiscoveryReq_isUsed) return "ServiceDiscoveryReq";
    if (b->ServiceDiscoveryRes_isUsed) return "ServiceDiscoveryRes";
    if (b->ServicePaymentSelectionReq_isUsed) return "ServicePaymentSelectionReq";
    if (b->ServicePaymentSelectionRes_isUsed) return "ServicePaymentSelectionRes";
    if (b->ContractAuthenticationReq_isUsed) return "ContractAuthenticationReq";
    if (b->ContractAuthenticationRes_isUsed) return "ContractAuthenticationRes";
    if (b->ChargeParameterDiscoveryReq_isUsed) return "ChargeParameterDiscoveryReq";
    if (b->ChargeParameterDiscoveryRes_isUsed) return "ChargeParameterDiscoveryRes";
    if (b->CableCheckReq_isUsed) return "CableCheckReq";
    if (b->CableCheckRes_isUsed) return "CableCheckRes";
    if (b->PreChargeReq_isUsed) return "PreChargeReq";
    if (b->PreChargeRes_isUsed) return "PreChargeRes";
    if (b->PowerDeliveryReq_isUsed) return "PowerDeliveryReq";
    if (b->PowerDeliveryRes_isUsed) return "PowerDeliveryRes";
    if (b->CurrentDemandReq_isUsed) return "CurrentDemandReq";
    if (b->CurrentDemandRes_isUsed) return "CurrentDemandRes";
    if (b->WeldingDetectionReq_isUsed) return "WeldingDetectionReq";
    if (b->WeldingDetectionRes_isUsed) return "WeldingDetectionRes";
    if (b->SessionStopReq_isUsed) return "SessionStopReq";
    if (b->SessionStopRes_isUsed) return "SessionStopRes";
    return "(other)";
}
