/*
 * v2g_sim.c - simulated charger power stage and vehicle battery
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <string.h>
#include "v2g_sim.h"

/* ---- charger power stage --------------------------------------------------------- */

static void s_limits(void *ctx, secc_limits_t *l) { *l = ((v2g_sim_secc_t *)ctx)->lim; }
static int  s_authorized(void *ctx) { (void)ctx; return 1; }
static int  s_cp_c(void *ctx) { return ((v2g_sim_secc_t *)ctx)->force_cp_c; }

static void s_iso_start(void *ctx)
{
    v2g_sim_secc_t *s = ctx;
    if (!s->iso_running) {
        s->iso_running = 1;
        s->t_iso = s->t_last + s->isolation_ms;
    }
}

static secc_isolation_t s_iso_result(void *ctx)
{
    v2g_sim_secc_t *s = ctx;
    if (!s->iso_running)
        return ISO_ONGOING;
    return (int32_t)(s->t_last - s->t_iso) >= 0 ? ISO_VALID : ISO_ONGOING;
}

static void s_output(void *ctx, int on, float v, float i)
{
    v2g_sim_secc_t *s = ctx;
    s->on = on;
    s->target_v = v > s->lim.max_voltage ? s->lim.max_voltage : v;
    s->limit_i = i > s->lim.max_current ? s->lim.max_current : i;
    if (s->target_v > 0 && s->limit_i * s->target_v > s->lim.max_power)
        s->limit_i = s->lim.max_power / s->target_v;
}

static void s_measure(void *ctx, float *v, float *i)
{
    v2g_sim_secc_t *s = ctx;
    *v = s->v;
    *i = s->i;
}

static void s_tick(void *ctx, uint32_t now)
{
    v2g_sim_secc_t *s = ctx;
    float dt = (float)(now - s->t_last) / 1000.0f;
    float goal = s->on ? s->target_v : 0.0f;
    float step = s->slew_v_per_s * dt;

    if (dt <= 0.0f || dt > 1.0f) {
        s->t_last = now;
        return;
    }
    s->t_last = now;
    if (s->v < goal) s->v = (s->v + step > goal) ? goal : s->v + step;
    else             s->v = (s->v - step < goal) ? goal : s->v - step;
    /* the battery takes what the charger allows once the voltages meet */
    s->i = (s->on && s->v >= goal - 5.0f) ? s->limit_i : 0.0f;
}

void v2g_sim_secc_init(v2g_sim_secc_t *s, secc_hw_t *hw)
{
    memset(s, 0, sizeof(*s));
    s->lim.max_voltage = 500.0f;
    s->lim.max_current = 125.0f;
    s->lim.max_power = 50000.0f;
    s->lim.min_voltage = 150.0f;
    s->lim.min_current = 0.0f;
    s->lim.current_tolerance = 1.0f;
    s->lim.peak_ripple = 1.0f;
    s->slew_v_per_s = 400.0f;
    s->isolation_ms = 1000u;

    memset(hw, 0, sizeof(*hw));
    hw->ctx = s;
    hw->limits = s_limits;
    hw->authorized = s_authorized;
    hw->cp_state_c = s_cp_c;
    hw->isolation_start = s_iso_start;
    hw->isolation_result = s_iso_result;
    hw->output = s_output;
    hw->measure = s_measure;
    hw->tick = s_tick;
}

/* ---- vehicle battery ------------------------------------------------------------- */

static void   e_limits(void *ctx, evcc_limits_t *l) { *l = ((v2g_sim_evcc_t *)ctx)->lim; }

static float  e_batt_v(void *ctx)
{
    v2g_sim_evcc_t *s = ctx;
    return s->pack_v_empty + (s->pack_v_full - s->pack_v_empty) * s->soc / 100.0f;
}

static int8_t e_soc(void *ctx) { return (int8_t)((v2g_sim_evcc_t *)ctx)->soc; }

static float  e_target_i(void *ctx)
{
    v2g_sim_evcc_t *s = ctx;
    /* simple taper above 80 % */
    if (s->soc > 80.0f)
        return s->max_charge_current * 0.5f;
    return s->max_charge_current;
}

static float  e_target_v(void *ctx)
{
    v2g_sim_evcc_t *s = ctx;
    return s->pack_v_full;
}

static int    e_complete(void *ctx)
{
    v2g_sim_evcc_t *s = ctx;
    return s->soc >= s->target_soc;
}

static void   e_s2(void *ctx, int c) { ((v2g_sim_evcc_t *)ctx)->s2 = c; }
static void   e_contactor(void *ctx, int c) { ((v2g_sim_evcc_t *)ctx)->contactor = c; }

static void   e_present(void *ctx, float v, float i)
{
    v2g_sim_evcc_t *s = ctx;
    s->evse_v = v;
    s->evse_i = i;
}

static void   e_tick(void *ctx, uint32_t now)
{
    v2g_sim_evcc_t *s = ctx;
    float dt = (float)(now - s->t_last) / 1000.0f;
    s->t_last = now;
    if (dt <= 0.0f || dt > 1.0f)
        return;
    if (s->contactor && s->evse_i > 0.0f && s->lim.capacity_wh > 0.0f) {
        float wh = s->evse_i * e_batt_v(s) * dt / 3600.0f;
        s->soc += 100.0f * wh / s->lim.capacity_wh;
        if (s->soc > 100.0f)
            s->soc = 100.0f;
    }
}

void v2g_sim_evcc_init(v2g_sim_evcc_t *s, evcc_hw_t *hw)
{
    memset(s, 0, sizeof(*s));
    s->lim.max_voltage = 420.0f;
    s->lim.max_current = 100.0f;
    s->lim.max_power = 40000.0f;
    s->lim.capacity_wh = 300.0f;        /* small "battery" so a demo session takes seconds */
    s->lim.full_soc = 100;
    s->lim.bulk_soc = 80;
    s->pack_v_empty = 320.0f;
    s->pack_v_full = 400.0f;
    s->soc = 30.0f;
    s->target_soc = 40.0f;
    s->max_charge_current = 80.0f;

    memset(hw, 0, sizeof(*hw));
    hw->ctx = s;
    hw->limits = e_limits;
    hw->battery_voltage = e_batt_v;
    hw->soc = e_soc;
    hw->target_current = e_target_i;
    hw->target_voltage = e_target_v;
    hw->charge_complete = e_complete;
    hw->s2 = e_s2;
    hw->contactor = e_contactor;
    hw->evse_present = e_present;
    hw->tick = e_tick;
}
