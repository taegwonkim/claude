/*
 * din_secc.c - DIN SPEC 70121 charger side (SECC), DC charging, EIM
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * One TCP connection, request -> response. Every request is decoded into
 * g_din_doc, checked against the expected step and answered at once from the
 * lwIP receive callback. Hardware is reached only through secc_hw_t.
 */
#include <string.h>
#include "din_secc.h"
#include "v2g_conf.h"
#include "v2g_codec.h"
#include "v2g_sdp.h"
#include "v2g_netif.h"
#include "lwip/tcp.h"
#include "plc_util.h"

#define LOG(...) PLC_LOG("[SECC] " __VA_ARGS__)

static void emit(din_secc_t *s, v2g_event_t e, int arg)
{
    if (s->cb)
        s->cb(s->user, e, arg);
}

/* ---- connection handling -------------------------------------------------------- */

static void drop_conn(din_secc_t *s, int abort_it)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *)s->conn;
    if (!pcb)
        return;
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    if (abort_it || tcp_close(pcb) != ERR_OK) {
        tcp_abort(pcb);
        s->aborted = 1;     /* a callback must then return ERR_ABRT */
    }
    s->conn = NULL;
}

static void fail(din_secc_t *s, v2g_fail_t why)
{
    LOG("session failed: %s\r\n", v2g_fail_name(why));
    if (s->hw->output)
        s->hw->output(s->hw->ctx, 0, 0.0f, 0.0f);
    s->phase = SECC_FAILED;
    emit(s, V2G_EVT_FAILED, why);
}

static int send_msg(din_secc_t *s, uint32_t len)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *)s->conn;
    if (!pcb)
        return -1;
    if (tcp_write(pcb, s->tx, (u16_t)len, TCP_WRITE_FLAG_COPY) != ERR_OK)
        return -1;
    tcp_output(pcb);
    return 0;
}

/* ---- response helpers -------------------------------------------------------------- */

static struct din_BodyType *begin_res(din_secc_t *s)
{
    v2g_din_clear();
    memcpy(g_din_doc.V2G_Message.Header.SessionID.bytes, s->session_id, sizeof(s->session_id));
    g_din_doc.V2G_Message.Header.SessionID.bytesLen = sizeof(s->session_id);
    return &g_din_doc.V2G_Message.Body;
}

static int send_din(din_secc_t *s)
{
    uint32_t len;
    if (v2g_encode_din(s->tx, sizeof(s->tx), &len) != 0) {
        LOG("encode failed\r\n");
        return -1;
    }
    return send_msg(s, len);
}

static void evse_status(din_secc_t *s, struct din_DC_EVSEStatusType *st)
{
    st->EVSEIsolationStatus_isUsed = s->iso_known;
    st->EVSEIsolationStatus = (din_isolationLevelType)s->iso_level;
    st->EVSEStatusCode = s->stop_req ? din_DC_EVSEStatusCodeType_EVSE_Shutdown
                                     : din_DC_EVSEStatusCodeType_EVSE_Ready;
    st->NotificationMaxDelay = 0;
    st->EVSENotification = s->stop_req ? din_EVSENotificationType_StopCharging
                                       : din_EVSENotificationType_None;
}

static void present(din_secc_t *s, float *v, float *i)
{
    *v = 0.0f;
    *i = 0.0f;
    if (s->hw->measure)
        s->hw->measure(s->hw->ctx, v, i);
}

/* ---- supportedAppProtocol --------------------------------------------------------- */

static void handle_apphand(din_secc_t *s, const uint8_t *exi, uint32_t len)
{
    struct appHand_supportedAppProtocolReq req;
    int i, chosen = -1;
    uint32_t out;
    uint8_t schema = 0, minor_dev = 0;

    if (v2g_decode_app(exi, len) != 0 || !g_app_doc.supportedAppProtocolReq_isUsed) {
        fail(s, V2G_FAIL_DECODE);
        drop_conn(s, 1);
        return;
    }
    req = g_app_doc.supportedAppProtocolReq;
    for (i = 0; i < req.AppProtocol.arrayLen; i++) {
        const struct appHand_AppProtocolType *a = &req.AppProtocol.array[i];
        LOG("<- supportedAppProtocolReq: %.*s %lu.%lu id %u prio %u\r\n",
            a->ProtocolNamespace.charactersLen, a->ProtocolNamespace.characters,
            (unsigned long)a->VersionNumberMajor, (unsigned long)a->VersionNumberMinor,
            a->SchemaID, a->Priority);
        if (a->ProtocolNamespace.charactersLen == sizeof(V2G_DIN_NAMESPACE) - 1 &&
            !memcmp(a->ProtocolNamespace.characters, V2G_DIN_NAMESPACE, sizeof(V2G_DIN_NAMESPACE) - 1) &&
            a->VersionNumberMajor == V2G_DIN_VERSION_MAJOR) {
            if (chosen < 0 || a->Priority < req.AppProtocol.array[chosen].Priority)
                chosen = i;
        }
    }

    v2g_app_clear();
    g_app_doc.supportedAppProtocolRes_isUsed = 1;
    if (chosen >= 0) {
        schema = req.AppProtocol.array[chosen].SchemaID;
        minor_dev = req.AppProtocol.array[chosen].VersionNumberMinor != V2G_DIN_VERSION_MINOR;
        g_app_doc.supportedAppProtocolRes.ResponseCode =
            minor_dev ? appHand_responseCodeType_OK_SuccessfulNegotiationWithMinorDeviation
                      : appHand_responseCodeType_OK_SuccessfulNegotiation;
        g_app_doc.supportedAppProtocolRes.SchemaID = schema;
        g_app_doc.supportedAppProtocolRes.SchemaID_isUsed = 1;
    } else {
        g_app_doc.supportedAppProtocolRes.ResponseCode = appHand_responseCodeType_Failed_NoNegotiation;
    }
    if (v2g_encode_app(s->tx, sizeof(s->tx), &out) == 0)
        send_msg(s, out);

    if (chosen < 0) {
        fail(s, V2G_FAIL_NO_PROTOCOL);
        drop_conn(s, 0);
        return;
    }
    LOG("-> supportedAppProtocolRes: DIN 70121 (schema %u)\r\n", schema);
    s->phase = SECC_SESSION;
    s->step = SECC_STEP_SESSION_SETUP;
    emit(s, V2G_EVT_PROTOCOL, schema);
}

/* ---- DIN 70121 requests --------------------------------------------------------------- */

static void handle_din(din_secc_t *s, const uint8_t *exi, uint32_t len)
{
    struct din_BodyType *req, *res;
    const struct din_MessageHeaderType *hdr;
    din_responseCodeType rc = din_responseCodeType_OK;
    secc_limits_t lim;
    float pv_v, pv_i;
    int close_after = 0;

    if (v2g_decode_din(exi, len) != 0) {
        fail(s, V2G_FAIL_DECODE);
        drop_conn(s, 1);
        return;
    }
    req = &g_din_doc.V2G_Message.Body;
    hdr = &g_din_doc.V2G_Message.Header;
    s->requests++;

    memset(&lim, 0, sizeof(lim));
    if (s->hw->limits)
        s->hw->limits(s->hw->ctx, &lim);

    /* session id: new in SessionSetup, then it must match */
    if (!req->SessionSetupReq_isUsed &&
        (hdr->SessionID.bytesLen != sizeof(s->session_id) ||
         memcmp(hdr->SessionID.bytes, s->session_id, sizeof(s->session_id))))
        rc = din_responseCodeType_FAILED_UnknownSession;

    /* ---------------------------------------------------------------- SessionSetup */
    if (req->SessionSetupReq_isUsed) {
        struct din_SessionSetupReqType q = req->SessionSetupReq;
        if (s->step != SECC_STEP_SESSION_SETUP)
            rc = din_responseCodeType_FAILED_SequenceError;
        else {
            uint32_t i;
            for (i = 0; i < sizeof(s->session_id); i++)
                s->session_id[i] = (uint8_t)LWIP_RAND();
            if (s->session_id[0] == 0)
                s->session_id[0] = 1;
        }
        LOG("<- SessionSetupReq EVCCID %02X:%02X:%02X:%02X:%02X:%02X\r\n",
            q.EVCCID.bytes[0], q.EVCCID.bytes[1], q.EVCCID.bytes[2],
            q.EVCCID.bytes[3], q.EVCCID.bytes[4], q.EVCCID.bytes[5]);
        res = begin_res(s);
        res->SessionSetupRes_isUsed = 1;
        res->SessionSetupRes.ResponseCode = rc == din_responseCodeType_OK
                                            ? din_responseCodeType_OK_NewSessionEstablished : rc;
        res->SessionSetupRes.EVSEID.bytes[0] = 0x00;
        res->SessionSetupRes.EVSEID.bytesLen = 1;
        if (rc == din_responseCodeType_OK) {
            s->step = SECC_STEP_SERVICE_DISCOVERY;
            emit(s, V2G_EVT_SESSION, 0);
        }
    }
    /* ------------------------------------------------------------ ServiceDiscovery */
    else if (req->ServiceDiscoveryReq_isUsed) {
        if (rc == din_responseCodeType_OK && s->step != SECC_STEP_SERVICE_DISCOVERY)
            rc = din_responseCodeType_FAILED_SequenceError;
        res = begin_res(s);
        res->ServiceDiscoveryRes_isUsed = 1;
        res->ServiceDiscoveryRes.ResponseCode = rc;
        res->ServiceDiscoveryRes.PaymentOptions.PaymentOption.array[0] = din_paymentOptionType_ExternalPayment;
        res->ServiceDiscoveryRes.PaymentOptions.PaymentOption.arrayLen = 1;
        res->ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceID = 1;
        res->ServiceDiscoveryRes.ChargeService.ServiceTag.ServiceCategory = din_serviceCategoryType_EVCharging;
        res->ServiceDiscoveryRes.ChargeService.FreeService = 0;
        res->ServiceDiscoveryRes.ChargeService.EnergyTransferType = din_EVSESupportedEnergyTransferType_DC_extended;
        if (rc == din_responseCodeType_OK)
            s->step = SECC_STEP_PAYMENT_SELECTION;
    }
    /* ------------------------------------------------------- ServicePaymentSelection */
    else if (req->ServicePaymentSelectionReq_isUsed) {
        if (rc == din_responseCodeType_OK && s->step != SECC_STEP_PAYMENT_SELECTION)
            rc = din_responseCodeType_FAILED_SequenceError;
        if (rc == din_responseCodeType_OK &&
            req->ServicePaymentSelectionReq.SelectedPaymentOption != din_paymentOptionType_ExternalPayment)
            rc = din_responseCodeType_FAILED_PaymentSelectionInvalid;
        res = begin_res(s);
        res->ServicePaymentSelectionRes_isUsed = 1;
        res->ServicePaymentSelectionRes.ResponseCode = rc;
        if (rc == din_responseCodeType_OK)
            s->step = SECC_STEP_AUTHORIZATION;
    }
    /* -------------------------------------------------------- ContractAuthentication */
    else if (req->ContractAuthenticationReq_isUsed) {
        int ok = 0;
        if (rc == din_responseCodeType_OK && s->step != SECC_STEP_AUTHORIZATION)
            rc = din_responseCodeType_FAILED_SequenceError;
        if (rc == din_responseCodeType_OK)
            ok = s->hw->authorized ? s->hw->authorized(s->hw->ctx) : 1;
        res = begin_res(s);
        res->ContractAuthenticationRes_isUsed = 1;
        res->ContractAuthenticationRes.ResponseCode = rc;
        res->ContractAuthenticationRes.EVSEProcessing = ok ? din_EVSEProcessingType_Finished
                                                           : din_EVSEProcessingType_Ongoing;
        if (rc == din_responseCodeType_OK && ok)
            s->step = SECC_STEP_CHARGE_PARAMETER;
    }
    /* ------------------------------------------------------ ChargeParameterDiscovery */
    else if (req->ChargeParameterDiscoveryReq_isUsed) {
        struct din_ChargeParameterDiscoveryReqType *q = &req->ChargeParameterDiscoveryReq;
        struct din_DC_EVSEChargeParameterType *cp;
        struct din_SAScheduleTupleType *t;
        float ev_max_v = 0, ev_max_i = 0;

        if (rc == din_responseCodeType_OK && s->step != SECC_STEP_CHARGE_PARAMETER)
            rc = din_responseCodeType_FAILED_SequenceError;
        if (rc == din_responseCodeType_OK && !q->DC_EVChargeParameter_isUsed)
            rc = din_responseCodeType_FAILED_WrongEnergyTransferType;
        if (q->DC_EVChargeParameter_isUsed) {
            ev_max_v = v2g_pv_get(&q->DC_EVChargeParameter.EVMaximumVoltageLimit);
            ev_max_i = v2g_pv_get(&q->DC_EVChargeParameter.EVMaximumCurrentLimit);
            LOG("<- ChargeParameterDiscoveryReq EV max %.0f V %.0f A SOC %d %%\r\n",
                (double)ev_max_v, (double)ev_max_i, q->DC_EVChargeParameter.DC_EVStatus.EVRESSSOC);
        }
        res = begin_res(s);
        res->ChargeParameterDiscoveryRes_isUsed = 1;
        res->ChargeParameterDiscoveryRes.ResponseCode = rc;
        res->ChargeParameterDiscoveryRes.EVSEProcessing = din_EVSEProcessingType_Finished;

        /* one schedule: maximum power for 24 h. DIN PMax is a plain short;
         * most implementations put the power in W clamped to 32767. */
        res->ChargeParameterDiscoveryRes.SAScheduleList_isUsed = 1;
        res->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.arrayLen = 1;
        t = &res->ChargeParameterDiscoveryRes.SAScheduleList.SAScheduleTuple.array[0];
        t->SAScheduleTupleID = 1;
        t->PMaxSchedule.PMaxScheduleID = 1;
        t->PMaxSchedule.PMaxScheduleEntry.arrayLen = 1;
        t->PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval_isUsed = 1;
        t->PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval.start = 0;
        t->PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval.duration = 86400;
        t->PMaxSchedule.PMaxScheduleEntry.array[0].RelativeTimeInterval.duration_isUsed = 1;
        t->PMaxSchedule.PMaxScheduleEntry.array[0].PMax =
            (int16_t)(lim.max_power > 32767.0f ? 32767 : (int)lim.max_power);

        res->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed = 1;
        cp = &res->ChargeParameterDiscoveryRes.DC_EVSEChargeParameter;
        evse_status(s, &cp->DC_EVSEStatus);
        v2g_pv_set(&cp->EVSEMaximumCurrentLimit, lim.max_current, din_unitSymbolType_A);
        v2g_pv_set(&cp->EVSEMaximumPowerLimit, lim.max_power, din_unitSymbolType_W);
        cp->EVSEMaximumPowerLimit_isUsed = 1;
        v2g_pv_set(&cp->EVSEMaximumVoltageLimit, lim.max_voltage, din_unitSymbolType_V);
        v2g_pv_set(&cp->EVSEMinimumCurrentLimit, lim.min_current, din_unitSymbolType_A);
        v2g_pv_set(&cp->EVSEMinimumVoltageLimit, lim.min_voltage, din_unitSymbolType_V);
        v2g_pv_set(&cp->EVSECurrentRegulationTolerance, lim.current_tolerance, din_unitSymbolType_A);
        cp->EVSECurrentRegulationTolerance_isUsed = 1;
        v2g_pv_set(&cp->EVSEPeakCurrentRipple, lim.peak_ripple, din_unitSymbolType_A);
        if (rc == din_responseCodeType_OK)
            s->step = SECC_STEP_CABLE_CHECK;
    }
    /* ---------------------------------------------------------------- CableCheck */
    else if (req->CableCheckReq_isUsed) {
        din_EVSEProcessingType proc = din_EVSEProcessingType_Ongoing;
        if (rc == din_responseCodeType_OK && s->step != SECC_STEP_CABLE_CHECK)
            rc = din_responseCodeType_FAILED_SequenceError;
        if (rc == din_responseCodeType_OK) {
            /* the test needs the vehicle in state C (S2 closed) */
            int c = s->hw->cp_state_c ? s->hw->cp_state_c(s->hw->ctx) : 1;
            if (c && !s->iso_started) {
                s->iso_started = 1;
                if (s->hw->isolation_start)
                    s->hw->isolation_start(s->hw->ctx);
                LOG("<- CableCheckReq: insulation test started\r\n");
            }
            if (s->iso_started) {
                secc_isolation_t r = s->hw->isolation_result ? s->hw->isolation_result(s->hw->ctx) : ISO_VALID;
                if (r == ISO_VALID || r == ISO_WARNING) {
                    s->iso_known = 1;
                    s->iso_level = r == ISO_VALID ? din_isolationLevelType_Valid : din_isolationLevelType_Warning;
                    proc = din_EVSEProcessingType_Finished;
                } else if (r == ISO_FAULT) {
                    s->iso_known = 1;
                    s->iso_level = din_isolationLevelType_Fault;
                    rc = din_responseCodeType_FAILED;
                    proc = din_EVSEProcessingType_Finished;
                }
            }
        }
        res = begin_res(s);
        res->CableCheckRes_isUsed = 1;
        res->CableCheckRes.ResponseCode = rc;
        evse_status(s, &res->CableCheckRes.DC_EVSEStatus);
        if (proc == din_EVSEProcessingType_Ongoing)
            res->CableCheckRes.DC_EVSEStatus.EVSEStatusCode = din_DC_EVSEStatusCodeType_EVSE_IsolationMonitoringActive;
        res->CableCheckRes.EVSEProcessing = proc;
        if (rc == din_responseCodeType_OK && proc == din_EVSEProcessingType_Finished) {
            LOG("-> CableCheckRes: insulation %s\r\n", s->iso_level == din_isolationLevelType_Valid ? "valid" : "warning");
            s->step = SECC_STEP_PRECHARGE;
            emit(s, V2G_EVT_CABLE_CHECK, 0);
        }
    }
    /* ------------------------------------------------------------------ PreCharge */
    else if (req->PreChargeReq_isUsed) {
        if (rc == din_responseCodeType_OK && s->step != SECC_STEP_PRECHARGE)
            rc = din_responseCodeType_FAILED_SequenceError;
        if (rc == din_responseCodeType_OK) {
            s->target_v = v2g_pv_get(&req->PreChargeReq.EVTargetVoltage);
            s->target_i = v2g_pv_get(&req->PreChargeReq.EVTargetCurrent);
            if (s->target_i > 2.0f)
                s->target_i = 2.0f;                 /* DIN: PreCharge current <= 2 A */
            if (s->hw->output)
                s->hw->output(s->hw->ctx, 1, s->target_v, s->target_i);
        }
        present(s, &pv_v, &pv_i);
        res = begin_res(s);
        res->PreChargeRes_isUsed = 1;
        res->PreChargeRes.ResponseCode = rc;
        evse_status(s, &res->PreChargeRes.DC_EVSEStatus);
        v2g_pv_set(&res->PreChargeRes.EVSEPresentVoltage, pv_v, din_unitSymbolType_V);
    }
    /* -------------------------------------------------------------- PowerDelivery */
    else if (req->PowerDeliveryReq_isUsed) {
        int start = req->PowerDeliveryReq.ReadyToChargeState;
        if (rc == din_responseCodeType_OK) {
            if (start && s->step != SECC_STEP_PRECHARGE)
                rc = din_responseCodeType_FAILED_SequenceError;
            if (!start && s->step != SECC_STEP_CHARGING && s->step != SECC_STEP_PRECHARGE)
                rc = din_responseCodeType_FAILED_SequenceError;
        }
        LOG("<- PowerDeliveryReq ReadyToCharge=%d\r\n", start);
        if (rc == din_responseCodeType_OK) {
            if (start) {
                s->step = SECC_STEP_CHARGING;
                emit(s, V2G_EVT_CHARGING, 0);
            } else {
                if (s->hw->output)
                    s->hw->output(s->hw->ctx, 0, 0.0f, 0.0f);
                s->step = SECC_STEP_WELDING;
                emit(s, V2G_EVT_CHARGING_STOPPED, 0);
            }
        }
        res = begin_res(s);
        res->PowerDeliveryRes_isUsed = 1;
        res->PowerDeliveryRes.ResponseCode = rc;
        res->PowerDeliveryRes.DC_EVSEStatus_isUsed = 1;
        evse_status(s, &res->PowerDeliveryRes.DC_EVSEStatus);
    }
    /* -------------------------------------------------------------- CurrentDemand */
    else if (req->CurrentDemandReq_isUsed) {
        struct din_CurrentDemandReqType *q = &req->CurrentDemandReq;
        int lim_i = 0, lim_v = 0, lim_p = 0;
        if (rc == din_responseCodeType_OK && s->step != SECC_STEP_CHARGING)
            rc = din_responseCodeType_FAILED_SequenceError;
        if (rc == din_responseCodeType_OK) {
            s->target_v = v2g_pv_get(&q->EVTargetVoltage);
            s->target_i = v2g_pv_get(&q->EVTargetCurrent);
            if (s->target_v > lim.max_voltage) { s->target_v = lim.max_voltage; lim_v = 1; }
            if (s->target_i > lim.max_current) { s->target_i = lim.max_current; lim_i = 1; }
            if (s->target_v > 0 && s->target_v * s->target_i > lim.max_power) {
                s->target_i = lim.max_power / s->target_v;
                lim_p = 1;
            }
            if (s->stop_req)
                s->target_i = 0.0f;
            if (s->hw->output)
                s->hw->output(s->hw->ctx, 1, s->target_v, s->target_i);
        }
        present(s, &pv_v, &pv_i);
        res = begin_res(s);
        res->CurrentDemandRes_isUsed = 1;
        res->CurrentDemandRes.ResponseCode = rc;
        evse_status(s, &res->CurrentDemandRes.DC_EVSEStatus);
        v2g_pv_set(&res->CurrentDemandRes.EVSEPresentVoltage, pv_v, din_unitSymbolType_V);
        v2g_pv_set(&res->CurrentDemandRes.EVSEPresentCurrent, pv_i, din_unitSymbolType_A);
        res->CurrentDemandRes.EVSECurrentLimitAchieved = lim_i;
        res->CurrentDemandRes.EVSEVoltageLimitAchieved = lim_v;
        res->CurrentDemandRes.EVSEPowerLimitAchieved = lim_p;
        v2g_pv_set(&res->CurrentDemandRes.EVSEMaximumVoltageLimit, lim.max_voltage, din_unitSymbolType_V);
        res->CurrentDemandRes.EVSEMaximumVoltageLimit_isUsed = 1;
        v2g_pv_set(&res->CurrentDemandRes.EVSEMaximumCurrentLimit, lim.max_current, din_unitSymbolType_A);
        res->CurrentDemandRes.EVSEMaximumCurrentLimit_isUsed = 1;
        v2g_pv_set(&res->CurrentDemandRes.EVSEMaximumPowerLimit, lim.max_power, din_unitSymbolType_W);
        res->CurrentDemandRes.EVSEMaximumPowerLimit_isUsed = 1;
    }
    /* ---------------------------------------------------------- WeldingDetection */
    else if (req->WeldingDetectionReq_isUsed) {
        if (rc == din_responseCodeType_OK && s->step != SECC_STEP_WELDING)
            rc = din_responseCodeType_FAILED_SequenceError;
        present(s, &pv_v, &pv_i);
        res = begin_res(s);
        res->WeldingDetectionRes_isUsed = 1;
        res->WeldingDetectionRes.ResponseCode = rc;
        evse_status(s, &res->WeldingDetectionRes.DC_EVSEStatus);
        v2g_pv_set(&res->WeldingDetectionRes.EVSEPresentVoltage, pv_v, din_unitSymbolType_V);
    }
    /* ---------------------------------------------------------------- SessionStop */
    else if (req->SessionStopReq_isUsed) {
        if (rc == din_responseCodeType_OK && s->step == SECC_STEP_SESSION_SETUP)
            rc = din_responseCodeType_FAILED_SequenceError;
        LOG("<- SessionStopReq\r\n");
        if (s->hw->output)
            s->hw->output(s->hw->ctx, 0, 0.0f, 0.0f);
        res = begin_res(s);
        res->SessionStopRes_isUsed = 1;
        res->SessionStopRes.ResponseCode = rc;
        s->step = SECC_STEP_STOPPED;
        close_after = 1;
    }
    else {
        LOG("unsupported request %s\r\n", v2g_din_body_name(req));
        fail(s, V2G_FAIL_SEQUENCE);
        drop_conn(s, 1);
        return;
    }

    if (send_din(s) != 0) {
        fail(s, V2G_FAIL_CONNECTION_LOST);
        drop_conn(s, 1);
        return;
    }

    if (rc >= din_responseCodeType_FAILED) {
        LOG("-> response code %d, closing\r\n", rc);
        fail(s, rc == din_responseCodeType_FAILED_SequenceError ? V2G_FAIL_SEQUENCE
                : (s->iso_known && s->iso_level == din_isolationLevelType_Fault) ? V2G_FAIL_ISOLATION
                : V2G_FAIL_RESPONSE_CODE);
        drop_conn(s, 0);
    } else if (close_after) {
        LOG("-> SessionStopRes, session done (%lu requests)\r\n", (unsigned long)s->requests);
        drop_conn(s, 0);
        s->phase = SECC_DONE;
        emit(s, V2G_EVT_DONE, 0);
    }
}

/* ---- lwIP callbacks ----------------------------------------------------------------- */

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    din_secc_t *s = (din_secc_t *)arg;
    uint16_t type;
    uint32_t plen;
    int r;
    (void)err;

    if (!s)
        return ERR_OK;
    if (!p) {                                   /* peer closed */
        if (s->phase == SECC_SESSION || s->phase == SECC_APPHAND)
            fail(s, V2G_FAIL_CONNECTION_LOST);
        drop_conn(s, 0);
        return ERR_OK;
    }
    s->aborted = 0;
    if (p->tot_len <= sizeof(s->rx.buf) - s->rx.len) {
        pbuf_copy_partial(p, &s->rx.buf[s->rx.len], p->tot_len, 0);
        s->rx.len += p->tot_len;
    } else {
        v2gtp_rx_reset(&s->rx);                 /* overflow: protocol error */
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    s->t_last_rx = s->now;

    while (s->conn && (r = v2gtp_rx_peek(&s->rx, &type, &plen)) != 0) {
        if (r < 0 || type != V2GTP_TYPE_EXI) {
            LOG("bad V2GTP header\r\n");
            fail(s, V2G_FAIL_DECODE);
            drop_conn(s, 1);
            break;
        }
        if (s->phase == SECC_APPHAND)
            handle_apphand(s, &s->rx.buf[V2GTP_HDR_LEN], plen);
        else if (s->phase == SECC_SESSION)
            handle_din(s, &s->rx.buf[V2GTP_HDR_LEN], plen);
        v2gtp_rx_pop(&s->rx);
    }
    return s->aborted ? ERR_ABRT : ERR_OK;
}

static void on_err(void *arg, err_t err)
{
    din_secc_t *s = (din_secc_t *)arg;
    (void)err;
    if (!s)
        return;
    s->conn = NULL;                             /* pcb already freed by lwIP */
    if (s->phase == SECC_SESSION || s->phase == SECC_APPHAND)
        fail(s, V2G_FAIL_CONNECTION_LOST);
}

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    din_secc_t *s = (din_secc_t *)arg;
    if (err != ERR_OK || !pcb)
        return ERR_VAL;
    if (s->conn || (s->phase != SECC_LISTEN && s->phase != SECC_DONE && s->phase != SECC_FAILED)) {
        tcp_abort(pcb);                         /* one vehicle at a time */
        return ERR_ABRT;
    }
    s->conn = pcb;
    s->phase = SECC_APPHAND;
    s->step = SECC_STEP_SESSION_SETUP;
    s->stop_req = 0;
    s->iso_started = s->iso_known = 0;
    s->requests = 0;
    s->t_last_rx = s->now;
    v2gtp_rx_reset(&s->rx);
    tcp_arg(pcb, s);
    tcp_recv(pcb, on_recv);
    tcp_err(pcb, on_err);
    tcp_nagle_disable(pcb);
    LOG("TCP connection from [%s]:%u\r\n", ipaddr_ntoa(&pcb->remote_ip), pcb->remote_port);
    emit(s, V2G_EVT_CONNECTED, 0);
    return ERR_OK;
}

/* ---- public API -------------------------------------------------------------------- */

int din_secc_start(din_secc_t *s, const secc_hw_t *hw, v2g_event_cb_t cb, void *user)
{
    struct tcp_pcb *pcb;

    if (s->phase != SECC_IDLE)
        din_secc_stop(s);
    memset(s, 0, sizeof(*s));
    s->hw = hw;
    s->cb = cb;
    s->user = user;

    pcb = tcp_new_ip_type(IPADDR_TYPE_V6);
    if (!pcb)
        return -1;
    if (tcp_bind(pcb, IP6_ADDR_ANY, V2G_SECC_TCP_PORT) != ERR_OK) {
        tcp_close(pcb);
        return -1;
    }
    s->listen = tcp_listen(pcb);
    if (!s->listen)
        return -1;
    tcp_arg((struct tcp_pcb *)s->listen, s);
    tcp_accept((struct tcp_pcb *)s->listen, on_accept);
    if (v2g_sdp_server_start(V2G_SECC_TCP_PORT) != 0)
        return -1;
    s->phase = SECC_LISTEN;
    LOG("waiting for the EV (TCP port %u)\r\n", V2G_SECC_TCP_PORT);
    return 0;
}

void din_secc_stop(din_secc_t *s)
{
    drop_conn(s, 1);
    if (s->listen) {
        tcp_close((struct tcp_pcb *)s->listen);
        s->listen = NULL;
    }
    v2g_sdp_server_stop();
    if (s->hw && s->hw->output)
        s->hw->output(s->hw->ctx, 0, 0.0f, 0.0f);
    s->phase = SECC_IDLE;
}

void din_secc_request_stop(din_secc_t *s)
{
    s->stop_req = 1;
}

void din_secc_process(din_secc_t *s, uint32_t now)
{
    s->now = now;
    if (s->hw && s->hw->tick)
        s->hw->tick(s->hw->ctx, now);
    if ((s->phase == SECC_APPHAND || s->phase == SECC_SESSION) && s->conn &&
        plc_time_after_eq(now, s->t_last_rx + V2G_SECC_SEQUENCE_TIMEOUT_MS)) {
        fail(s, V2G_FAIL_TIMEOUT);
        drop_conn(s, 1);
    }
}

const char *din_secc_phase_name(secc_phase_t p)
{
    static const char *const n[] = {"IDLE", "LISTEN", "APPHAND", "SESSION", "DONE", "FAILED"};
    return (unsigned)p < sizeof(n) / sizeof(n[0]) ? n[p] : "?";
}
