/*
 * v2g_sim.h - simulated charger power stage and vehicle battery
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Lets two boards (or the host test) run a complete DIN 70121 DC session
 * without high voltage: the charger "output voltage" follows the requested
 * voltage with a slew rate, the vehicle "battery" gains SOC from the current
 * the charger reports. Replace with real hardware hooks for a product.
 */
#ifndef V2G_SIM_H
#define V2G_SIM_H

#include "v2g_hw.h"

typedef struct {
    secc_limits_t lim;
    float    slew_v_per_s;     /* output voltage slew rate */
    uint32_t isolation_ms;     /* duration of the insulation test */
    /* state */
    int      on;
    float    target_v, limit_i, v, i;
    uint32_t t_last, t_iso;
    int      iso_running;
    int      force_cp_c;       /* 1: report CP state C without measuring */
} v2g_sim_secc_t;

typedef struct {
    evcc_limits_t lim;
    float    pack_v_empty, pack_v_full; /* battery voltage at 0 % / 100 % */
    float    soc;                       /* % */
    float    target_soc;                /* stop at this SOC */
    float    max_charge_current;        /* A requested in CurrentDemand */
    /* state */
    int      s2, contactor;
    float    evse_v, evse_i;
    uint32_t t_last;
} v2g_sim_evcc_t;

/* defaults: 500 V / 125 A / 50 kW charger; 400 V class vehicle */
void v2g_sim_secc_init(v2g_sim_secc_t *s, secc_hw_t *hw);
void v2g_sim_evcc_init(v2g_sim_evcc_t *s, evcc_hw_t *hw);

#endif
