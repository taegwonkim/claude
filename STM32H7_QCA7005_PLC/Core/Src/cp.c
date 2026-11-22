/*
 * cp.c - IEC 61851-1 Control Pilot
 *
 * Measurement: ADC1 runs continuously on PA3; for 2 ms (two 1 kHz periods,
 * timed with the DWT cycle counter) every conversion is classified as
 * positive or negative. The share of positive samples is the duty cycle,
 * the mean of the positive samples gives the state (A..F).
 *
 * Front end scaling: see CP_ADC_ZERO_MV / CP_ADC_MV_PER_V in app_conf.h.
 */
#include "main.h"
#include "cp.h"

#define CP_WINDOW_MS   2u

static int32_t adc_to_cp_mv(uint32_t raw)
{
  int32_t adc_mv = (int32_t)((raw * 3300u) / 4095u);
  return (adc_mv - CP_ADC_ZERO_MV) * 1000 / CP_ADC_MV_PER_V;
}

static cp_state_t classify(const cp_meas_t *m)
{
  int32_t h = m->high_mv;

  if (m->duty_permille == 0)                /* never positive */
    return (m->low_mv < -10000) ? CP_STATE_F : CP_STATE_E;
  if (h >= 10500 && h <= 13500) return CP_STATE_A;
  if (h >= 7500)                return CP_STATE_B;
  if (h >= 4500)                return CP_STATE_C;
  if (h >= 1500)                return CP_STATE_D;
  if (h > -1500)                return CP_STATE_E;
  return CP_STATE_INVALID;
}

void cp_init(void)
{
  if (HAL_ADC_Start(&hadc1) != HAL_OK)        /* continuous conversions */
    Error_Handler();
#if PLC_ROLE == PLC_ROLE_EVSE
  cp_evse_set_duty(1000);                     /* +12 V DC: state A / B1 */
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
    Error_Handler();
#else
  cp_ev_set_s2(0);
#endif
}

void cp_measure(cp_meas_t *m)
{
  ADC_TypeDef *adc = hadc1.Instance;
  uint32_t window = (SystemCoreClock / 1000u) * CP_WINDOW_MS;
  uint32_t t0 = DWT->CYCCNT;
  int64_t sum_hi = 0, sum_lo = 0;
  uint32_t n_hi = 0, n_lo = 0;

  (void)adc->DR;                              /* drop a stale sample */
  while ((DWT->CYCCNT - t0) < window)
  {
    int32_t v;
    if (!(adc->ISR & ADC_ISR_EOC))
      continue;
    v = adc_to_cp_mv(adc->DR);                /* reading DR clears EOC */
    if (v > 0) { sum_hi += v; n_hi++; }
    else       { sum_lo += v; n_lo++; }
  }
  adc->ISR = ADC_ISR_OVR;                     /* clear a possible overrun flag */

  m->samples = (uint16_t)((n_hi + n_lo) > 0xFFFFu ? 0xFFFFu : (n_hi + n_lo));
  m->high_mv = n_hi ? (int32_t)(sum_hi / (int64_t)n_hi) : 0;
  m->low_mv  = n_lo ? (int32_t)(sum_lo / (int64_t)n_lo) : m->high_mv;
  m->duty_permille = (n_hi + n_lo) ? (uint16_t)((n_hi * 1000u) / (n_hi + n_lo)) : 0;
  m->pwm = (m->duty_permille > 10 && m->duty_permille < 990);
  m->state = classify(m);
}

const char *cp_state_name(cp_state_t s)
{
  static const char *const n[] = {"A", "B", "C", "D", "E", "F", "?"};
  return (unsigned)s <= CP_STATE_INVALID ? n[s] : "?";
}

void cp_evse_set_duty(uint16_t permille)
{
#if PLC_ROLE == PLC_ROLE_EVSE
  /* PWM1: CCR > ARR (999) gives a constant high level */
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, permille >= 1000 ? 1000u : permille);
#else
  (void)permille;
#endif
}

void cp_ev_set_s2(int closed)
{
  HAL_GPIO_WritePin(CP_S2_PORT, CP_S2_PIN, closed ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
