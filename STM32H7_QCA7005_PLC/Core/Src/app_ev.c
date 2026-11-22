/*
 * app_ev.c - vehicle (EV) side: Control Pilot + SLAC
 *
 *   CP B with 3..7 % PWM (debounced) : start SLAC (once per plug-in)
 *   SLAC matched                     : same AVLN, next: SDP / V2G client
 *   SLAC failed                      : retry after 5 s while 5 % is present
 *   CP A (unplugged) / E / F         : leave AVLN (private NMK), open S2
 *
 * S2 is not touched by SLAC. The V2G layer closes it (cp_ev_set_s2(1))
 * when the charge parameters are agreed (state B -> C).
 */
#include "app_conf.h"

#if PLC_ROLE == PLC_ROLE_EV

#include <stdio.h>
#include "main.h"
#include "app_plc.h"
#include "cp.h"
#include "slac_ev.h"

#define CP_PERIOD_MS     50u
#define HLC_DEBOUNCE     3u        /* 150 ms of 5 % before SLAC starts   */
#define UNPLUG_DEBOUNCE  2u
#define RETRY_PAUSE_MS   5000u
#define RETRY_MAX        3u

static slac_ev_t  s_slac;
static uint8_t    s_slac_ok;
static cp_meas_t  s_cp;
static uint8_t    s_hlc_count, s_unplug_count;
static uint8_t    s_started;       /* SLAC started for this plug-in */
static uint8_t    s_retries;
static uint8_t    s_failed;
static uint32_t   s_t_cp;
static uint32_t   s_t_retry;
static cp_state_t s_cp_shown = CP_STATE_INVALID;

static void on_slac_event(void *user, slac_event_t e, int arg)
{
  (void)user;
  switch (e)
  {
  case SLAC_EVT_MATCHED:
    printf("[EV] *** SLAC matched with EVSE " PLC_MAC_FMT " (%u dB) -> start SDP / V2G client ***\r\n",
           PLC_MAC_ARG(s_slac.evse_mac), s_slac.evse_atten);
    s_failed = 0;
    s_retries = 0;
    break;
  case SLAC_EVT_FAILED:
    printf("[EV] SLAC: %s\r\n", slac_fail_name((slac_fail_t)arg));
    if (arg != SLAC_FAIL_SET_KEY)
    {
      s_failed = 1;
      s_t_retry = HAL_GetTick() + RETRY_PAUSE_MS;
    }
    break;
  default:
    break;
  }
}

static int hlc_offered(const cp_meas_t *m)
{
  return (m->state == CP_STATE_B || m->state == CP_STATE_C || m->state == CP_STATE_D) &&
         m->pwm && m->duty_permille >= 30u && m->duty_permille <= 70u;
}

void app_role_init(uint32_t now)
{
  slac_ev_init(&s_slac, &g_plc_io, on_slac_event, NULL, now);
  s_slac_ok = 1;
  if (s_started)
    slac_ev_start(&s_slac, now);          /* modem rebooted during a session */
}

void app_role_on_frame(const uint8_t *f, uint16_t len, uint32_t now)
{
  if (s_slac_ok)
    slac_ev_on_frame(&s_slac, f, len, now);
}

void app_role_process(uint32_t now)
{
  if (s_slac_ok)
    slac_ev_process(&s_slac, now);

  if ((int32_t)(now - s_t_cp) < 0)
    return;
  s_t_cp = now + CP_PERIOD_MS;

  cp_measure(&s_cp);
  if (s_cp.state != s_cp_shown)
  {
    printf("[EV] CP state %s (%ld mV, duty %u.%u %%)\r\n", cp_state_name(s_cp.state),
           (long)s_cp.high_mv, s_cp.duty_permille / 10u, s_cp.duty_permille % 10u);
    s_cp_shown = s_cp.state;
  }

  /* unplugged / EVSE error */
  if (s_cp.state == CP_STATE_A || s_cp.state == CP_STATE_E || s_cp.state == CP_STATE_F)
  {
    if (++s_unplug_count >= UNPLUG_DEBOUNCE && s_started)
    {
      printf("[EV] unplugged -> leave AVLN\r\n");
      s_started = 0;
      s_retries = 0;
      s_failed = 0;
      cp_ev_set_s2(0);
      if (s_slac_ok)
        slac_ev_leave(&s_slac, now);
    }
    s_hlc_count = 0;
    return;
  }
  s_unplug_count = 0;

  if (!hlc_offered(&s_cp))
  {
    s_hlc_count = 0;
    return;
  }
  if (s_hlc_count < 255)
    s_hlc_count++;
  if (s_hlc_count < HLC_DEBOUNCE || !s_slac_ok)
    return;

  if (!s_started)
  {
    printf("[EV] 5 %% PWM detected -> start SLAC\r\n");
    s_started = 1;
    slac_ev_start(&s_slac, now);
  }
  else if (s_failed && s_retries < RETRY_MAX && (int32_t)(now - s_t_retry) >= 0)
  {
    s_retries++;
    s_failed = 0;
    printf("[EV] retry SLAC (%u/%u)\r\n", s_retries, RETRY_MAX);
    slac_ev_start(&s_slac, now);
  }
}

void app_role_status(void)
{
  printf("CP %s %ld mV %u.%u %% | SLAC %s",
         cp_state_name(s_cp.state), (long)s_cp.high_mv,
         s_cp.duty_permille / 10u, s_cp.duty_permille % 10u,
         s_slac_ok ? slac_ev_state_name(slac_ev_state(&s_slac)) : "-");
}

const char *app_role_name(void) { return "EV (vehicle)"; }
int app_role_matched(void) { return s_slac_ok && slac_ev_state(&s_slac) == SLAC_EV_MATCHED; }
int app_role_failed(void)  { return s_failed; }

#endif /* PLC_ROLE == PLC_ROLE_EV */
