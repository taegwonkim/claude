/*
 * v2g_codec.h - EXI encode/decode of supportedAppProtocol and DIN 70121
 *               messages (libcbv2g) with the V2GTP header
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef V2G_CODEC_H
#define V2G_CODEC_H

#include <stdint.h>
#include "cbv2g/din/din_msgDefDatatypes.h"
#include "cbv2g/app_handshake/appHand_Datatypes.h"

/* One work document per kind (the DIN document is about 11 KB). Only one
 * role runs per firmware, so the charger or vehicle state machine owns them. */
extern struct din_exiDocument     g_din_doc;
extern struct appHand_exiDocument g_app_doc;

/* clear a work document before filling it. (libcbv2g's init_din_exiDocument()
 * does not reset the body flags, so always use these) */
void v2g_din_clear(void);
void v2g_app_clear(void);

/* encode g_xxx_doc into buf as V2GTP(0x8001) + EXI. 0 = ok */
int v2g_encode_din(uint8_t *buf, uint32_t cap, uint32_t *len);
int v2g_encode_app(uint8_t *buf, uint32_t cap, uint32_t *len);
/* decode an EXI payload (without V2GTP header) into g_xxx_doc. 0 = ok */
int v2g_decode_din(const uint8_t *exi, uint32_t len);
int v2g_decode_app(const uint8_t *exi, uint32_t len);

/* PhysicalValue = Value * 10^Multiplier [Unit] */
void  v2g_pv_set(struct din_PhysicalValueType *pv, float value, din_unitSymbolType unit);
float v2g_pv_get(const struct din_PhysicalValueType *pv);

/* name of the request/response in a DIN body, for logs */
const char *v2g_din_body_name(const struct din_BodyType *b);

#endif /* V2G_CODEC_H */
