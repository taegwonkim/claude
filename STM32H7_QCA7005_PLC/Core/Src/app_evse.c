/*
 * app_evse.c - charger (EVSE) side: Control Pilot + SLAC
 *
 *   CP A (+12 V)          : PWM off (+12 V DC), SLAC stopped, fresh NMK
 *   CP B/C/D (EV plugged) : 5 % PWM on  -> EV starts SLAC, EVSE answers
 *   SLAC matched          : same AVLN, next: SDP / V2G (DIN 70121, ISO 15118-2)
 *   TT_EVSE_SLAC_init exp.: PWM off for 4 s, then 5 % again (retry)
 *   CP E/F/invalid        : PWM off, SLAC stopped
 */
#include "app_conf.h"

#if PLC_ROLE == PLC_ROLE_EVSE

#include <stdio.h>
#include "main.h"
#include "app_plc.h"
#include "cp.h"
#include "slac_evse.h"

#define CP_PERIOD_MS     50u
#define CP_DEBOUNCE      2u        /* equal readings before acting */
#define RETRY_PAUSE_MS   4000u
#define DUTY_DIGITAL     50u       /* 5 %: high level communication */

static slac_evse_t s_slac;
static uint8_t     s_slac_ok;      /* modem in sync, state machine initialised */
static cp_meas_t   s_cp;
static cp_state_t  s_cp_stable = CP_STATE_INVALID;
static cp_state_t  s_cp_last = CP_STATE_INVALID;
static uint8_t     s_cp_count;
static uint8_t     s_pwm_on;
static uint8_t     s_failed;
static uint32_t    s_t_cp;
static uint32_t    s_t_retry;

static void pwm(int on)
{
  cp_evse_set_duty(on ? DUTY_DIGITAL : 1000u);
  if (on != s_pwm_on)
    printf("[EVSE] CP PWM %s\r\n", on ? "5 % on" : "off (+12 V DC)");
  s_pwm_on = (uint8_t)on;
}

static void on_slac_event(void *user, slac_event_t e, int arg)
{
  (void)user;
  switch (e)
  {
  case SLAC_EVT_MATCHED:
    printf("[EVSE] *** SLAC matched with EV " PLC_MAC_FMT " -> start SDP / V2G server ***\r\n",
           PLC_MAC_ARG(s_slac.pev_mac));
    s_failed = 0;
    break;
  case SLAC_EVT_FAILED:
    printf("[EVSE] SLAC: %s\r\n", slac_fail_name((slac_fail_t)arg));
    if (arg == SLAC_FAIL_INIT_TIMEOUT)
    {
      /* no EV answered: drop the 5 % for a while, then offer it again */
      s_failed = 1;
      pwm(0);
      slac_evse_stop(&s_slac, HAL_GetTick());
      s_t_retry = HAL_GetTick() + RETRY_PAUSE_MS;
    }
    break;
  default:
    break;
  }
}

static int plugged(cp_state_t st)
{
  return st == CP_STATE_B || st == CP_STATE_C || st == CP_STATE_D;
}

void app_role_init(uint32_t now)
{
  slac_evse_init(&s_slac, &g_plc_io, on_slac_event, NULL, now);
  s_slac_ok = 1;
  if (s_pwm_on && plugged(s_cp_stable))
    slac_evse_start(&s_slac, now);        /* modem rebooted during a session */
}

void app_role_on_frame(const uint8_t *f, uint16_t len, uint32_t now)
{
  if (s_slac_ok)
    slac_evse_on_frame(&s_slac, f, len, now);
}

void app_role_process(uint32_t now)
{
  if (s_slac_ok)
    slac_evse_process(&s_slac, now);

  if ((int32_t)(now - s_t_cp) < 0)
    return;
  s_t_cp = now + CP_PERIOD_MS;

  cp_measure(&s_cp);
  if (s_cp.state == s_cp_last)
  {
    if (s_cp_count < 255) s_cp_count++;
  }
  else
  {
    s_cp_last = s_cp.state;
    s_cp_count = 1;
  }
  if (s_cp_count < CP_DEBOUNCE)
    return;

  if (s_cp_last != s_cp_stable)
  {
    printf("[EVSE] CP state %s -> %s (%ld mV, duty %u.%u %%)\r\n",
           cp_state_name(s_cp_stable), cp_state_name(s_cp_last), (long)s_cp.high_mv,
           s_cp.duty_permille / 10u, s_cp.duty_permille % 10u);
    s_cp_stable = s_cp_last;
  }

  if (plugged(s_cp_stable))
  {
    if (!s_pwm_on && (int32_t)(now - s_t_retry) >= 0)
    {
      pwm(1);
      s_failed = 0;
      if (s_slac_ok)
        slac_evse_start(&s_slac, now);
    }
  }
  else if (s_pwm_on || (s_slac_ok && slac_evse_state(&s_slac) != SLAC_EVSE_IDLE &&
                        slac_evse_state(&s_slac) != SLAC_EVSE_INIT_KEY))
  {
    /* unplugged (A) or error (E/F): stop, the next session gets a new NMK */
    pwm(0);
    s_failed = 0;
    s_t_retry = now;
    if (s_slac_ok)
      slac_evse_stop(&s_slac, now);
  }
}

void app_role_status(void)
{
  printf("CP %s %ld mV %u.%u %% | SLAC %s",
         cp_state_name(s_cp_stable), (long)s_cp.high_mv,
         s_cp.duty_permille / 10u, s_cp.duty_permille % 10u,
         s_slac_ok ? slac_evse_state_name(slac_evse_state(&s_slac)) : "-");
}

const char *app_role_name(void) { return "EVSE (charger)"; }
int app_role_matched(void) { return s_slac_ok && slac_evse_state(&s_slac) == SLAC_EVSE_MATCHED; }
int app_role_failed(void)  { return s_failed; }

#endif /* PLC_ROLE == PLC_ROLE_EVSE */
