/*
 * v2g_conf.h - DIN SPEC 70121 / V2GTP / SDP configuration
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Timing values follow DIN SPEC 70121 / ISO 15118-2 as commonly implemented;
 * check them against the edition of the standard you certify against.
 */
#ifndef V2G_CONF_H
#define V2G_CONF_H

/* ---- SDP (UDP) ---------------------------------------------------------------- */
#define V2G_SDP_PORT                  15118u
#define V2G_SDP_RETRY_MS              250u     /* EV: repeat SECC Discovery Request   */
#define V2G_SDP_MAX_TRIES             50u
#define V2G_SDP_SECURITY_TLS          0x00u
#define V2G_SDP_SECURITY_NONE         0x10u
#define V2G_SDP_TRANSPORT_TCP         0x00u

/* ---- TCP --------------------------------------------------------------------- */
#define V2G_SECC_TCP_PORT             61851u   /* any port in 49152..65535 */
#define V2G_MSG_MAX                   2048u    /* largest V2GTP message accepted */

/* ---- EVCC (vehicle) timeouts --------------------------------------------------- */
#define V2G_EVCC_MSG_TIMEOUT_MS       2000u    /* response to a request             */
#define V2G_EVCC_CURRENT_DEMAND_TIMEOUT_MS 250u /* CurrentDemand response (performance) */
#define V2G_EVCC_ONGOING_PERIOD_MS    100u     /* repeat while EVSEProcessing=Ongoing */
#define V2G_EVCC_LOOP_PERIOD_MS       100u     /* CurrentDemand / PreCharge cycle    */
#define V2G_EVCC_CABLECHECK_TIMEOUT_MS 38000u
#define V2G_EVCC_PRECHARGE_TIMEOUT_MS 7000u
#define V2G_EVCC_PRECHARGE_DELTA_V    20.0f    /* |EVSE - battery| to finish PreCharge */
#define V2G_EVCC_PRECHARGE_CURRENT_A  2.0f     /* DIN: max 2 A during PreCharge        */
#define V2G_EVCC_WELDING_MAX          20u      /* WeldingDetection requests            */
#define V2G_EVCC_WELDING_SAFE_V       40.0f
#define V2G_EVCC_CONNECT_TIMEOUT_MS   2000u

/* ---- SECC (charger) timeouts --------------------------------------------------- */
#define V2G_SECC_SEQUENCE_TIMEOUT_MS  60000u   /* no request from the EV for this long */

/* ---- protocol negotiation -------------------------------------------------------- */
#define V2G_DIN_NAMESPACE             "urn:din:70121:2012:MsgDef"
#define V2G_DIN_VERSION_MAJOR         2u
#define V2G_DIN_VERSION_MINOR         0u
#define V2G_DIN_SCHEMA_ID             1u

#endif /* V2G_CONF_H */
