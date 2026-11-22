/*
 * plc_modem.c - local QCA7005 modem queries (firmware version, AVLN status)
 *
 * Message layouts as in open-plc-utils plc/VersionInfo1.c and plc/NetInfo1.c.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "plc_modem.h"

#define LOG(...) PLC_LOG("[MODEM] " __VA_ARGS__)

void plc_modem_init(plc_modem_t *m, const slac_io_t *io, plc_link_cb_t cb, void *user, uint32_t now)
{
    memset(m, 0, sizeof(*m));
    m->io = *io;
    m->cb = cb;
    m->user = user;
    m->stations = -1;
    m->t_next = now;
}

void plc_modem_process(plc_modem_t *m, uint32_t now)
{
    uint16_t len;

    if (!plc_time_after_eq(now, m->t_next))
        return;
    m->t_next = now + PLC_MODEM_POLL_MS;

    if (!m->have_version) {
        len = hpgp_build_sw_ver(m->frame, m->io.host_mac);
        m->io.send(m->io.ctx, m->frame, len);
        return;
    }

    if (m->unanswered < 255)
        m->unanswered++;
    if (m->unanswered == 5)
        LOG("no VS_NW_INFO.CNF (firmware without MMV0 NW_INFO?)\r\n");
    len = hpgp_build_nw_info(m->frame, m->io.host_mac);
    m->io.send(m->io.ctx, m->frame, len);
}

void plc_modem_on_frame(plc_modem_t *m, const uint8_t *f, uint16_t len, uint32_t now)
{
    uint8_t mmv;
    int32_t mmtype = hpgp_mmtype(f, len, &mmv);
    (void)now;

    if (mmtype < 0 || mmv != HPGP_MMV_VENDOR || len < HPGP_OFS_VS_PAYLOAD)
        return;
    if (f[HPGP_OFS_VS_OUI] != 0x00 || f[HPGP_OFS_VS_OUI + 1] != 0xB0 || f[HPGP_OFS_VS_OUI + 2] != 0x52)
        return;

    if (mmtype == (VS_SW_VER | MMTYPE_CNF) && len > SW_CNF_VERSION) {
        unsigned n = f[SW_CNF_VERLEN];
        if (n > sizeof(m->version) - 1)
            n = sizeof(m->version) - 1;
        if (n > (unsigned)(len - SW_CNF_VERSION))
            n = (unsigned)(len - SW_CNF_VERSION);
        memcpy(m->version, &f[SW_CNF_VERSION], n);
        m->version[n] = 0;
        m->device_id = f[SW_CNF_DEVICEID];
        memcpy(m->modem_mac, &f[HPGP_OFS_OSA], PLC_MAC_LEN);
        if (!m->have_version)
            LOG("modem " PLC_MAC_FMT " device 0x%02X firmware \"%s\"\r\n",
                PLC_MAC_ARG(m->modem_mac), m->device_id, m->version);
        m->have_version = 1;
        return;
    }

    if (mmtype == (VS_NW_INFO | MMTYPE_CNF)) {
        int st = hpgp_parse_nw_info(f, len, m->nid);
        int up;
        if (st < 0)
            return;
        m->unanswered = 0;
        up = st > 0;
        if (st != m->stations || up != m->link_up) {
            LOG("AVLN stations %d -> link %s\r\n", st, up ? "UP" : "down");
            m->stations = st;
            m->link_up = (uint8_t)up;
            if (m->cb)
                m->cb(m->user, up, st);
        }
    }
}
