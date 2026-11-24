/*
 * din_evcc.c - DIN SPEC 70121 vehicle side (EVCC), DC charging, EIM
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * SDP -> TCP -> supportedAppProtocol -> SessionSetup -> ServiceDiscovery ->
 * ServicePaymentSelection -> ContractAuthentication -> ChargeParameterDiscovery
 * -> (S2 closed) CableCheck -> PreCharge -> PowerDelivery(start) ->
 * CurrentDemand ... -> PowerDelivery(stop) -> WeldingDetection -> SessionStop
 *
 * Requests are sent from din_evcc_process(), responses are handled in the
 * lwIP receive callback. Hardware is reached only through evcc_hw_t.
 */
#include <string.h>
#include <math.h>
#include "din_evcc.h"
#include "v2g_conf.h"
#include "v2g_codec.h"
#include "v2g_netif.h"
#include "lwip/tcp.h"
#include "plc_util.h"

#define LOG(...) PLC_LOG("[EVCC] " __VA_ARGS__)

static void emit(din_evcc_t *e, v2g_event_t ev, int arg)
{
    if (e->cb)
        e->cb(e->user, ev, arg);
}

static void drop_conn(din_evcc_t *e, int abort_it)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *)e->conn;
    if (!pcb)
        return;
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_sent(pcb, NULL);
    if (abort_it || tcp_close(pcb) != ERR_OK) {
        tcp_abort(pcb);
        e->aborted = 1;
    }
    e->conn = NULL;
}

/* safe state: no power, contactor open, S2 open */
static void safe_state(din_evcc_t *e)
{
    if (e->hw->contactor) e->hw->contactor(e->hw->ctx, 0);
    if (e->hw->s2)        e->hw->s2(e->hw->ctx, 0);
}

static void fail(din_evcc_t *e, v2g_fail_t why)
{
    LOG("session failed in %s: %s\r\n", din_evcc_req_name(e->req), v2g_fail_name(why));
    safe_state(e);
    drop_conn(e, 1);
    v2g_sdp_client_stop(&e->sdp);
    e->phase = EVCC_FAILED;
    e->waiting = 0;
    emit(e, V2G_EVT_FAILED, why);
}

static void next(din_evcc_t *e, evcc_req_t r, uint32_t delay)
{
    e->req = r;
    e->waiting = 0;
    e->t_next = e->now + delay;
}

/* ---- request building ------------------------------------------------------------- */

static void ev_status(din_evcc_t *e, struct din_DC_EVStatusType *st, int ready)
{
    st->EVReady = ready;
    st->EVErrorCode = din_DC_EVErrorCodeType_NO_ERROR;
    st->EVRESSSOC = e->hw->soc ? e->hw->soc(e->hw->ctx) : 50;
}

static struct din_BodyType *begin_req(din_evcc_t *e)
{
    v2g_din_clear();
    if (e->session_id_len) {
        memcpy(g_din_doc.V2G_Message.Header.SessionID.bytes, e->session_id, e->session_id_len);
        g_din_doc.V2G_Message.Header.SessionID.bytesLen = e->session_id_len;
    } else {
        g_din_doc.V2G_Message.Header.SessionID.bytes[0] = 0x00;    /* "no session yet" */
        g_din_doc.V2G_Message.Header.SessionID.bytesLen = 1;
    }
    return &g_din_doc.V2G_Message.Body;
}

static int send_buf(din_evcc_t *e, uint32_t len)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *)e->conn;
    if (!pcb || tcp_write(pcb, e->tx, (u16_t)len, TCP_WRITE_FLAG_COPY) != ERR_OK)
        return -1;
    tcp_output(pcb);
    return 0;
}

static int send_request(din_evcc_t *e)
{
    struct din_BodyType *b;
    evcc_limits_t lim;
    uint32_t len;
    int quiet = 0;

    memset(&lim, 0, sizeof(lim));
    if (e->hw->limits)
        e->hw->limits(e->hw->ctx, &lim);

    if (e->req == EVCC_REQ_APPHAND) {
        struct appHand_AppProtocolType *a;
        v2g_app_clear();
        g_app_doc.supportedAppProtocolReq_isUsed = 1;
        g_app_doc.supportedAppProtocolReq.AppProtocol.arrayLen = 1;
        a = &g_app_doc.supportedAppProtocolReq.AppProtocol.array[0];
        memcpy(a->ProtocolNamespace.characters, V2G_DIN_NAMESPACE, sizeof(V2G_DIN_NAMESPACE) - 1);
        a->ProtocolNamespace.charactersLen = sizeof(V2G_DIN_NAMESPACE) - 1;
        a->VersionNumberMajor = V2G_DIN_VERSION_MAJOR;
        a->VersionNumberMinor = V2G_DIN_VERSION_MINOR;
        a->SchemaID = V2G_DIN_SCHEMA_ID;
        a->Priority = 1;
        if (v2g_encode_app(e->tx, sizeof(e->tx), &len) != 0)
            return -1;
        LOG("-> supportedAppProtocolReq (DIN 70121)\r\n");
        return send_buf(e, len);
    }

    b = begin_req(e);
    switch (e->req) {
    case EVCC_REQ_SESSION_SETUP:
        b->SessionSetupReq_isUsed = 1;
        memcpy(b->SessionSetupReq.EVCCID.bytes, e->mac, 6);
        b->SessionSetupReq.EVCCID.bytesLen = 6;
        break;
    case EVCC_REQ_SERVICE_DISCOVERY:
        b->ServiceDiscoveryReq_isUsed = 1;
        b->ServiceDiscoveryReq.ServiceCategory = din_serviceCategoryType_EVCharging;
        b->ServiceDiscoveryReq.ServiceCategory_isUsed = 1;
        break;
    case EVCC_REQ_PAYMENT_SELECTION:
        b->ServicePaymentSelectionReq_isUsed = 1;
        b->ServicePaymentSelectionReq.SelectedPaymentOption = din_paymentOptionType_ExternalPayment;
        b->ServicePaymentSelectionReq.SelectedServiceList.SelectedService.arrayLen = 1;
        b->ServicePaymentSelectionReq.SelectedServiceList.SelectedService.array[0].ServiceID = e->service_id;
        break;
    case EVCC_REQ_AUTHORIZATION:
        b->ContractAuthenticationReq_isUsed = 1;
        quiet = 1;
        break;
    case EVCC_REQ_CHARGE_PARAMETER: {
        struct din_DC_EVChargeParameterType *p = &b->ChargeParameterDiscoveryReq.DC_EVChargeParameter;
        b->ChargeParameterDiscoveryReq_isUsed = 1;
        b->ChargeParameterDiscoveryReq.EVRequestedEnergyTransferType = din_EVRequestedEnergyTransferType_DC_extended;
        b->ChargeParameterDiscoveryReq.DC_EVChargeParameter_isUsed = 1;
        ev_status(e, &p->DC_EVStatus, 1);
        v2g_pv_set(&p->EVMaximumCurrentLimit, lim.max_current, din_unitSymbolType_A);
        v2g_pv_set(&p->EVMaximumPowerLimit, lim.max_power, din_unitSymbolType_W);
        p->EVMaximumPowerLimit_isUsed = 1;
        v2g_pv_set(&p->EVMaximumVoltageLimit, lim.max_voltage, din_unitSymbolType_V);
        v2g_pv_set(&p->EVEnergyCapacity, lim.capacity_wh, din_unitSymbolType_Wh);
        p->EVEnergyCapacity_isUsed = 1;
        p->FullSOC = lim.full_soc;
        p->FullSOC_isUsed = 1;
        p->BulkSOC = lim.bulk_soc;
        p->BulkSOC_isUsed = 1;
        break;
    }
    case EVCC_REQ_CABLE_CHECK:
        b->CableCheckReq_isUsed = 1;
        ev_status(e, &b->CableCheckReq.DC_EVStatus, 1);
        quiet = 1;
        break;
    case EVCC_REQ_PRECHARGE:
        b->PreChargeReq_isUsed = 1;
        ev_status(e, &b->PreChargeReq.DC_EVStatus, 1);
        v2g_pv_set(&b->PreChargeReq.EVTargetVoltage,
                   e->hw->battery_voltage ? e->hw->battery_voltage(e->hw->ctx) : 0.0f, din_unitSymbolType_V);
        v2g_pv_set(&b->PreChargeReq.EVTargetCurrent, V2G_EVCC_PRECHARGE_CURRENT_A, din_unitSymbolType_A);
        quiet = 1;
        break;
    case EVCC_REQ_POWER_DELIVERY_ON:
    case EVCC_REQ_POWER_DELIVERY_OFF: {
        int on = e->req == EVCC_REQ_POWER_DELIVERY_ON;
        b->PowerDeliveryReq_isUsed = 1;
        b->PowerDeliveryReq.ReadyToChargeState = on;
        b->PowerDeliveryReq.DC_EVPowerDeliveryParameter_isUsed = 1;
        ev_status(e, &b->PowerDeliveryReq.DC_EVPowerDeliveryParameter.DC_EVStatus, 1);
        b->PowerDeliveryReq.DC_EVPowerDeliveryParameter.ChargingComplete = !on;
        LOG("-> PowerDeliveryReq ReadyToCharge=%d\r\n", on);
        quiet = 1;
        break;
    }
    case EVCC_REQ_CURRENT_DEMAND: {
        struct din_CurrentDemandReqType *q = &b->CurrentDemandReq;
        int complete = e->hw->charge_complete ? e->hw->charge_complete(e->hw->ctx) : 0;
        b->CurrentDemandReq_isUsed = 1;
        ev_status(e, &q->DC_EVStatus, 1);
        v2g_pv_set(&q->EVTargetCurrent, complete ? 0.0f : e->hw->target_current(e->hw->ctx), din_unitSymbolType_A);
        v2g_pv_set(&q->EVTargetVoltage, e->hw->target_voltage(e->hw->ctx), din_unitSymbolType_V);
        v2g_pv_set(&q->EVMaximumVoltageLimit, lim.max_voltage, din_unitSymbolType_V);
        q->EVMaximumVoltageLimit_isUsed = 1;
        v2g_pv_set(&q->EVMaximumCurrentLimit, lim.max_current, din_unitSymbolType_A);
        q->EVMaximumCurrentLimit_isUsed = 1;
        v2g_pv_set(&q->EVMaximumPowerLimit, lim.max_power, din_unitSymbolType_W);
        q->EVMaximumPowerLimit_isUsed = 1;
        q->ChargingComplete = complete;
        quiet = 1;
        break;
    }
    case EVCC_REQ_WELDING:
        b->WeldingDetectionReq_isUsed = 1;
        ev_status(e, &b->WeldingDetectionReq.DC_EVStatus, 0);
        quiet = 1;
        break;
    case EVCC_REQ_SESSION_STOP:
        b->SessionStopReq_isUsed = 1;
        break;
    default:
        return -1;
    }
    if (!quiet)
        LOG("-> %s\r\n", v2g_din_body_name(b));
    if (v2g_encode_din(e->tx, sizeof(e->tx), &len) != 0) {
        LOG("encode failed\r\n");
        return -1;
    }
    return send_buf(e, len);
}

static uint32_t timeout_of(evcc_req_t r)
{
    return r == EVCC_REQ_CURRENT_DEMAND ? V2G_EVCC_CURRENT_DEMAND_TIMEOUT_MS : V2G_EVCC_MSG_TIMEOUT_MS;
}

/* ---- response handling -------------------------------------------------------------- */

static void handle_apphand(din_evcc_t *e, const uint8_t *exi, uint32_t len)
{
    if (v2g_decode_app(exi, len) != 0 || !g_app_doc.supportedAppProtocolRes_isUsed) {
        fail(e, V2G_FAIL_DECODE);
        return;
    }
    if (g_app_doc.supportedAppProtocolRes.ResponseCode == appHand_responseCodeType_Failed_NoNegotiation ||
        (g_app_doc.supportedAppProtocolRes.SchemaID_isUsed &&
         g_app_doc.supportedAppProtocolRes.SchemaID != V2G_DIN_SCHEMA_ID)) {
        fail(e, V2G_FAIL_NO_PROTOCOL);
        return;
    }
    LOG("<- supportedAppProtocolRes: DIN 70121 accepted\r\n");
    emit(e, V2G_EVT_PROTOCOL, V2G_DIN_SCHEMA_ID);
    next(e, EVCC_REQ_SESSION_SETUP, 0);
}

static int rc_failed(din_evcc_t *e, din_responseCodeType rc)
{
    if (rc >= din_responseCodeType_FAILED) {
        LOG("<- response code %d\r\n", rc);
        fail(e, V2G_FAIL_RESPONSE_CODE);
        return 1;
    }
    return 0;
}

static void handle_din(din_evcc_t *e, const uint8_t *exi, uint32_t len)
{
    struct din_BodyType *b;
    float batt;

    if (v2g_decode_din(exi, len) != 0) {
        fail(e, V2G_FAIL_DECODE);
        return;
    }
    b = &g_din_doc.V2G_Message.Body;

    switch (e->req) {
    case EVCC_REQ_SESSION_SETUP:
        if (!b->SessionSetupRes_isUsed) break;
        if (rc_failed(e, b->SessionSetupRes.ResponseCode)) return;
        e->session_id_len = (uint8_t)g_din_doc.V2G_Message.Header.SessionID.bytesLen;
        if (e->session_id_len > sizeof(e->session_id))
            e->session_id_len = sizeof(e->session_id);
        memcpy(e->session_id, g_din_doc.V2G_Message.Header.SessionID.bytes, e->session_id_len);
        LOG("<- SessionSetupRes: session %02X%02X%02X%02X%02X%02X%02X%02X\r\n",
            e->session_id[0], e->session_id[1], e->session_id[2], e->session_id[3],
            e->session_id[4], e->session_id[5], e->session_id[6], e->session_id[7]);
        emit(e, V2G_EVT_SESSION, 0);
        next(e, EVCC_REQ_SERVICE_DISCOVERY, 0);
        return;

    case EVCC_REQ_SERVICE_DISCOVERY: {
        int i, ext = 0;
        if (!b->ServiceDiscoveryRes_isUsed) break;
        if (rc_failed(e, b->ServiceDiscoveryRes.ResponseCode)) return;
        for (i = 0; i < b->ServiceDiscoveryRes.PaymentOptions.PaymentOption.arrayLen; i++)
            if (b->ServiceDiscoveryRes.PaymentOptions.PaymentOption.array[i] == din_paymentOptionType_ExternalPayment)
                ext = 1;
        if (!ext) {
            LOG("EVSE offers no ExternalPayment\r\n");
            fail(e, V2G_FAIL_RESPONSE_CODE);
            return;
        }
        e->service_id = b->ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceID;
        next(e, EVCC_REQ_PAYMENT_SELECTION, 0);
        return;
    }

    case EVCC_REQ_PAYMENT_SELECTION:
        if (!b->ServicePaymentSelectionRes_isUsed) break;
        if (rc_failed(e, b->ServicePaymentSelectionRes.ResponseCode)) return;
        next(e, EVCC_REQ_AUTHORIZATION, 0);
        return;

    case EVCC_REQ_AUTHORIZATION:
        if (!b->ContractAuthenticationRes_isUsed) break;
        if (rc_failed(e, b->ContractAuthenticationRes.ResponseCode)) return;
        if (b->ContractAuthenticationRes.EVSEProcessing == din_EVSEProcessingType_Finished) {
            LOG("<- ContractAuthenticationRes: authorized\r\n");
            next(e, EVCC_REQ_CHARGE_PARAMETER, 0);
        } else {
            next(e, EVCC_REQ_AUTHORIZATION, V2G_EVCC_ONGOING_PERIOD_MS);
        }
        return;

    case EVCC_REQ_CHARGE_PARAMETER: {
        struct din_DC_EVSEChargeParameterType *p = &b->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter;
        if (!b->ChargeParameterDiscoveryRes_isUsed) break;
        if (rc_failed(e, b->ChargeParameterDiscoveryRes.ResponseCode)) return;
        if (b->ChargeParameterDiscoveryRes.EVSEProcessing != din_EVSEProcessingType_Finished) {
            next(e, EVCC_REQ_CHARGE_PARAMETER, V2G_EVCC_ONGOING_PERIOD_MS);
            return;
        }
        if (b->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed) {
            e->evse_max_v = v2g_pv_get(&p->EVSEMaximumVoltageLimit);
            e->evse_max_i = v2g_pv_get(&p->EVSEMaximumCurrentLimit);
            e->evse_max_p = p->EVSEMaximumPowerLimit_isUsed ? v2g_pv_get(&p->EVSEMaximumPowerLimit) : 0.0f;
        }
        LOG("<- ChargeParameterDiscoveryRes: EVSE max %.0f V %.0f A %.0f W -> close S2 (state C)\r\n",
            (double)e->evse_max_v, (double)e->evse_max_i, (double)e->evse_max_p);
        if (e->hw->s2)
            e->hw->s2(e->hw->ctx, 1);
        e->t_phase = e->now;
        next(e, EVCC_REQ_CABLE_CHECK, V2G_EVCC_ONGOING_PERIOD_MS);
        return;
    }

    case EVCC_REQ_CABLE_CHECK: {
        struct din_CableCheckResType *r = &b->CableCheckRes;
        if (!b->CableCheckRes_isUsed) break;
        if (rc_failed(e, r->ResponseCode)) return;
        if (r->EVSEProcessing == din_EVSEProcessingType_Finished &&
            r->DC_EVSEStatus.EVSEIsolationStatus_isUsed &&
            (r->DC_EVSEStatus.EVSEIsolationStatus == din_isolationLevelType_Valid ||
             r->DC_EVSEStatus.EVSEIsolationStatus == din_isolationLevelType_Warning)) {
            LOG("<- CableCheckRes: insulation OK after %lu ms -> PreCharge\r\n",
                (unsigned long)(e->now - e->t_phase));
            emit(e, V2G_EVT_CABLE_CHECK, 0);
            e->t_phase = e->now;
            next(e, EVCC_REQ_PRECHARGE, 0);
        } else if (r->DC_EVSEStatus.EVSEIsolationStatus_isUsed &&
                   r->DC_EVSEStatus.EVSEIsolationStatus == din_isolationLevelType_Fault) {
            fail(e, V2G_FAIL_ISOLATION);
        } else if (plc_time_after_eq(e->now, e->t_phase + V2G_EVCC_CABLECHECK_TIMEOUT_MS)) {
            fail(e, V2G_FAIL_TIMEOUT);
        } else {
            next(e, EVCC_REQ_CABLE_CHECK, V2G_EVCC_ONGOING_PERIOD_MS);
        }
        return;
    }

    case EVCC_REQ_PRECHARGE:
        if (!b->PreChargeRes_isUsed) break;
        if (rc_failed(e, b->PreChargeRes.ResponseCode)) return;
        e->evse_v = v2g_pv_get(&b->PreChargeRes.EVSEPresentVoltage);
        batt = e->hw->battery_voltage ? e->hw->battery_voltage(e->hw->ctx) : 0.0f;
        if (fabsf(e->evse_v - batt) <= V2G_EVCC_PRECHARGE_DELTA_V) {
            LOG("<- PreChargeRes: EVSE %.1f V, battery %.1f V -> PowerDelivery\r\n",
                (double)e->evse_v, (double)batt);
            emit(e, V2G_EVT_PRECHARGE_DONE, 0);
            next(e, EVCC_REQ_POWER_DELIVERY_ON, 0);
        } else if (plc_time_after_eq(e->now, e->t_phase + V2G_EVCC_PRECHARGE_TIMEOUT_MS)) {
            fail(e, V2G_FAIL_TIMEOUT);
        } else {
            next(e, EVCC_REQ_PRECHARGE, V2G_EVCC_LOOP_PERIOD_MS);
        }
        return;

    case EVCC_REQ_POWER_DELIVERY_ON:
        if (!b->PowerDeliveryRes_isUsed) break;
        if (rc_failed(e, b->PowerDeliveryRes.ResponseCode)) return;
        LOG("<- PowerDeliveryRes: close contactors, charging\r\n");
        if (e->hw->contactor)
            e->hw->contactor(e->hw->ctx, 1);
        e->cd_count = 0;
        e->t_log = e->now;
        emit(e, V2G_EVT_CHARGING, 0);
        next(e, EVCC_REQ_CURRENT_DEMAND, 0);
        return;

    case EVCC_REQ_CURRENT_DEMAND: {
        struct din_CurrentDemandResType *r = &b->CurrentDemandRes;
        int evse_stop;
        if (!b->CurrentDemandRes_isUsed) break;
        if (rc_failed(e, r->ResponseCode)) return;
        e->cd_count++;
        e->evse_v = v2g_pv_get(&r->EVSEPresentVoltage);
        e->evse_i = v2g_pv_get(&r->EVSEPresentCurrent);
        if (e->hw->evse_present)
            e->hw->evse_present(e->hw->ctx, e->evse_v, e->evse_i);
        evse_stop = r->DC_EVSEStatus.EVSENotification == din_EVSENotificationType_StopCharging ||
                    r->DC_EVSEStatus.EVSEStatusCode != din_DC_EVSEStatusCodeType_EVSE_Ready;
        if (plc_time_after_eq(e->now, e->t_log + 1000u)) {
            e->t_log = e->now;
            LOG("CurrentDemand #%lu: EVSE %.1f V %.1f A, SOC %d %%\r\n", (unsigned long)e->cd_count,
                (double)e->evse_v, (double)e->evse_i, e->hw->soc ? e->hw->soc(e->hw->ctx) : -1);
        }
        if (e->stop_req || evse_stop || (e->hw->charge_complete && e->hw->charge_complete(e->hw->ctx))) {
            LOG("stop charging (%s) after %lu CurrentDemand cycles, SOC %d %%\r\n",
                e->stop_req ? "EV request" : evse_stop ? "EVSE request" : "battery full",
                (unsigned long)e->cd_count, e->hw->soc ? e->hw->soc(e->hw->ctx) : -1);
            next(e, EVCC_REQ_POWER_DELIVERY_OFF, 0);
        } else {
            next(e, EVCC_REQ_CURRENT_DEMAND, V2G_EVCC_LOOP_PERIOD_MS);
        }
        return;
    }

    case EVCC_REQ_POWER_DELIVERY_OFF:
        if (!b->PowerDeliveryRes_isUsed) break;
        if (rc_failed(e, b->PowerDeliveryRes.ResponseCode)) return;
        LOG("<- PowerDeliveryRes: charging stopped, open contactors\r\n");
        if (e->hw->contactor)
            e->hw->contactor(e->hw->ctx, 0);
        if (e->hw->evse_present)
            e->hw->evse_present(e->hw->ctx, 0.0f, 0.0f);
        emit(e, V2G_EVT_CHARGING_STOPPED, 0);
        e->welding_count = 0;
        next(e, EVCC_REQ_WELDING, V2G_EVCC_LOOP_PERIOD_MS);
        return;

    case EVCC_REQ_WELDING:
        if (!b->WeldingDetectionRes_isUsed) break;
        if (rc_failed(e, b->WeldingDetectionRes.ResponseCode)) return;
        e->evse_v = v2g_pv_get(&b->WeldingDetectionRes.EVSEPresentVoltage);
        e->welding_count++;
        if (e->evse_v < V2G_EVCC_WELDING_SAFE_V || e->welding_count >= V2G_EVCC_WELDING_MAX) {
            LOG("<- WeldingDetectionRes: %.1f V -> SessionStop\r\n", (double)e->evse_v);
            next(e, EVCC_REQ_SESSION_STOP, 0);
        } else {
            next(e, EVCC_REQ_WELDING, V2G_EVCC_LOOP_PERIOD_MS);
        }
        return;

    case EVCC_REQ_SESSION_STOP:
        if (!b->SessionStopRes_isUsed) break;
        LOG("<- SessionStopRes: session done\r\n");
        if (e->hw->s2)
            e->hw->s2(e->hw->ctx, 0);
        drop_conn(e, 0);
        e->phase = EVCC_DONE;
        e->req = EVCC_REQ_NONE;
        e->waiting = 0;
        emit(e, V2G_EVT_DONE, 0);
        return;

    default:
        break;
    }
    LOG("unexpected %s while waiting for the answer to %s\r\n",
        v2g_din_body_name(b), din_evcc_req_name(e->req));
    fail(e, V2G_FAIL_SEQUENCE);
}

/* ---- lwIP callbacks ----------------------------------------------------------------- */

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    din_evcc_t *e = (din_evcc_t *)arg;
    uint16_t type;
    uint32_t plen;
    int r;
    (void)err;

    if (!e)
        return ERR_OK;
    e->aborted = 0;
    if (!p) {
        if (e->phase == EVCC_SESSION)
            fail(e, V2G_FAIL_CONNECTION_LOST);
        else
            drop_conn(e, 0);
        return e->aborted ? ERR_ABRT : ERR_OK;
    }
    if (p->tot_len <= sizeof(e->rx.buf) - e->rx.len) {
        pbuf_copy_partial(p, &e->rx.buf[e->rx.len], p->tot_len, 0);
        e->rx.len += p->tot_len;
    } else {
        v2gtp_rx_reset(&e->rx);
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    while (e->conn && e->waiting && (r = v2gtp_rx_peek(&e->rx, &type, &plen)) != 0) {
        if (r < 0 || type != V2GTP_TYPE_EXI) {
            fail(e, V2G_FAIL_DECODE);
            break;
        }
        e->waiting = 0;
        if (e->req == EVCC_REQ_APPHAND)
            handle_apphand(e, &e->rx.buf[V2GTP_HDR_LEN], plen);
        else
            handle_din(e, &e->rx.buf[V2GTP_HDR_LEN], plen);
        v2gtp_rx_pop(&e->rx);
    }
    return e->aborted ? ERR_ABRT : ERR_OK;
}

static void on_err(void *arg, err_t err)
{
    din_evcc_t *e = (din_evcc_t *)arg;
    if (!e)
        return;
    e->conn = NULL;
    LOG("TCP error %d\r\n", err);
    if (e->phase == EVCC_SESSION || e->phase == EVCC_CONNECTING)
        fail(e, e->phase == EVCC_CONNECTING ? V2G_FAIL_CONNECT : V2G_FAIL_CONNECTION_LOST);
}

static err_t on_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    din_evcc_t *e = (din_evcc_t *)arg;
    (void)pcb;
    if (err != ERR_OK) {
        fail(e, V2G_FAIL_CONNECT);
        return ERR_OK;
    }
    LOG("TCP connected to SECC\r\n");
    e->phase = EVCC_SESSION;
    emit(e, V2G_EVT_CONNECTED, 0);
    next(e, EVCC_REQ_APPHAND, 0);
    return ERR_OK;
}

static void connect_secc(din_evcc_t *e)
{
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_V6);
    ip_addr_t dst;

    if (!pcb) {
        fail(e, V2G_FAIL_CONNECT);
        return;
    }
    ip_addr_copy_from_ip6(dst, e->sdp.secc_addr);
    e->conn = pcb;
    v2gtp_rx_reset(&e->rx);
    tcp_arg(pcb, e);
    tcp_recv(pcb, on_recv);
    tcp_err(pcb, on_err);
    tcp_nagle_disable(pcb);
    e->phase = EVCC_CONNECTING;
    e->t_deadline = e->now + V2G_EVCC_CONNECT_TIMEOUT_MS;
    if (tcp_connect(pcb, &dst, e->sdp.secc_port, on_connected) != ERR_OK)
        fail(e, V2G_FAIL_CONNECT);
}

/* ---- public API -------------------------------------------------------------------- */

void din_evcc_start(din_evcc_t *e, const evcc_hw_t *hw, const uint8_t mac[6],
                    v2g_event_cb_t cb, void *user, uint32_t now)
{
    if (e->phase != EVCC_IDLE && e->hw)
        din_evcc_stop(e);
    memset(e, 0, sizeof(*e));
    e->hw = hw;
    e->cb = cb;
    e->user = user;
    e->now = now;
    memcpy(e->mac, mac, 6);
    e->phase = EVCC_SDP;
    e->req = EVCC_REQ_NONE;
    v2g_sdp_client_start(&e->sdp, now);
    LOG("start: SECC discovery\r\n");
}

void din_evcc_stop(din_evcc_t *e)
{
    if (e->hw)
        safe_state(e);
    drop_conn(e, 1);
    v2g_sdp_client_stop(&e->sdp);
    e->phase = EVCC_IDLE;
    e->waiting = 0;
}

void din_evcc_request_stop(din_evcc_t *e)
{
    e->stop_req = 1;
}

void din_evcc_process(din_evcc_t *e, uint32_t now)
{
    e->now = now;
    if (e->hw && e->hw->tick)
        e->hw->tick(e->hw->ctx, now);

    switch (e->phase) {
    case EVCC_SDP:
        v2g_sdp_client_process(&e->sdp, now);
        if (e->sdp.state == V2G_SDP_FOUND) {
            emit(e, V2G_EVT_SDP_DONE, 0);
            connect_secc(e);
        } else if (e->sdp.state == V2G_SDP_FAILED) {
            fail(e, V2G_FAIL_SDP);
        }
        break;

    case EVCC_CONNECTING:
        if (plc_time_after_eq(now, e->t_deadline))
            fail(e, V2G_FAIL_CONNECT);
        break;

    case EVCC_SESSION:
        if (e->waiting) {
            if (plc_time_after_eq(now, e->t_deadline))
                fail(e, V2G_FAIL_TIMEOUT);
        } else if (e->req != EVCC_REQ_NONE && plc_time_after_eq(now, e->t_next)) {
            if (send_request(e) != 0) {
                fail(e, V2G_FAIL_CONNECTION_LOST);
                break;
            }
            e->waiting = 1;
            e->t_deadline = now + timeout_of(e->req);
        }
        break;

    default:
        break;
    }
}

const char *din_evcc_phase_name(evcc_phase_t p)
{
    static const char *const n[] = {"IDLE", "SDP", "CONNECTING", "SESSION", "DONE", "FAILED"};
    return (unsigned)p < sizeof(n) / sizeof(n[0]) ? n[p] : "?";
}

const char *din_evcc_req_name(evcc_req_t r)
{
    static const char *const n[] = {
        "supportedAppProtocol", "SessionSetup", "ServiceDiscovery", "ServicePaymentSelection",
        "ContractAuthentication", "ChargeParameterDiscovery", "CableCheck", "PreCharge",
        "PowerDelivery(start)", "CurrentDemand", "PowerDelivery(stop)", "WeldingDetection",
        "SessionStop", "-"
    };
    return (unsigned)r < sizeof(n) / sizeof(n[0]) ? n[r] : "?";
}

const char *v2g_fail_name(v2g_fail_t f)
{
    switch (f) {
    case V2G_FAIL_NONE:            return "none";
    case V2G_FAIL_SDP:             return "no SECC Discovery Response";
    case V2G_FAIL_CONNECT:         return "TCP connect failed";
    case V2G_FAIL_NO_PROTOCOL:     return "no common application protocol";
    case V2G_FAIL_TIMEOUT:         return "timeout";
    case V2G_FAIL_RESPONSE_CODE:   return "FAILED response code";
    case V2G_FAIL_SEQUENCE:        return "sequence error";
    case V2G_FAIL_DECODE:          return "EXI decode error";
    case V2G_FAIL_ISOLATION:       return "insulation fault";
    case V2G_FAIL_CONNECTION_LOST: return "connection lost";
    case V2G_FAIL_EVSE_STOP:       return "EVSE stopped";
    default:                       return "?";
    }
}
