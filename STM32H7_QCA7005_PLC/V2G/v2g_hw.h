/*
 * v2g_hw.h - hardware hooks of the DIN 70121 state machines
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * The protocol code never touches hardware directly. A charger fills a
 * secc_hw_t (power module, isolation monitor, CP), a vehicle an evcc_hw_t
 * (battery management, contactor, S2). v2g_sim.c has simulated versions for
 * bench tests without high voltage.
 */
#ifndef V2G_HW_H
#define V2G_HW_H

#include <stdint.h>

/* ---- charger (SECC / EVSE) ---------------------------------------------------- */
typedef struct {
    float max_voltage, max_current, max_power;   /* V, A, W */
    float min_voltage, min_current;
    float current_tolerance, peak_ripple;        /* A */
} secc_limits_t;

typedef enum { ISO_ONGOING = 0, ISO_VALID, ISO_WARNING, ISO_FAULT } secc_isolation_t;

typedef struct {
    void *ctx;
    void (*limits)(void *ctx, secc_limits_t *l);
    /* EIM: payment / RFID done (1) or still pending (0) */
    int  (*authorized)(void *ctx);
    /* vehicle closed S2 (CP state C or D) */
    int  (*cp_state_c)(void *ctx);
    /* insulation test of the DC cable (during CableCheck) */
    void (*isolation_start)(void *ctx);
    secc_isolation_t (*isolation_result)(void *ctx);
    /* power module: on/off, voltage target, current limit */
    void (*output)(void *ctx, int on, float voltage, float current);
    /* measured output (V, A) */
    void (*measure)(void *ctx, float *voltage, float *current);
    /* called every loop with the time in ms (models, watchdogs) */
    void (*tick)(void *ctx, uint32_t now);
} secc_hw_t;

/* ---- vehicle (EVCC / EV) ----------------------------------------------------- */
typedef struct {
    float max_voltage, max_current, max_power;   /* V, A, W */
    float capacity_wh;
    int8_t full_soc, bulk_soc;                   /* % */
} evcc_limits_t;

typedef struct {
    void *ctx;
    void   (*limits)(void *ctx, evcc_limits_t *l);
    float  (*battery_voltage)(void *ctx);         /* V, for PreCharge           */
    int8_t (*soc)(void *ctx);                     /* %                          */
    float  (*target_current)(void *ctx);          /* A, requested by the BMS    */
    float  (*target_voltage)(void *ctx);          /* V, charge voltage          */
    int    (*charge_complete)(void *ctx);         /* 1 = stop charging          */
    void   (*s2)(void *ctx, int closed);          /* CP state B <-> C           */
    void   (*contactor)(void *ctx, int closed);   /* DC contactors              */
    /* EVSE present values from every response (for the battery model) */
    void   (*evse_present)(void *ctx, float voltage, float current);
    void   (*tick)(void *ctx, uint32_t now);
} evcc_hw_t;

#endif /* V2G_HW_H */
