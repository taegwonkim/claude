/*
 * cp.h - IEC 61851-1 Control Pilot: measurement (both roles), PWM (EVSE), S2 (EV)
 */
#ifndef CP_H
#define CP_H

#include <stdint.h>

typedef enum {
  CP_STATE_A = 0,   /* +12 V   not connected            */
  CP_STATE_B,       /* +9 V    connected, not ready      */
  CP_STATE_C,       /* +6 V    charging                  */
  CP_STATE_D,       /* +3 V    charging, ventilation     */
  CP_STATE_E,       /*  0 V    short / EVSE off           */
  CP_STATE_F,       /* -12 V   EVSE not available         */
  CP_STATE_INVALID
} cp_state_t;

typedef struct {
  int32_t    high_mv;        /* mean level while CP is positive          */
  int32_t    low_mv;         /* mean level while CP is negative          */
  uint16_t   duty_permille;  /* share of positive samples, 0..1000       */
  uint16_t   samples;
  uint8_t    pwm;            /* 1 if a PWM (not DC) was seen             */
  cp_state_t state;
} cp_meas_t;

void        cp_init(void);
/* samples the CP for 2 PWM periods (2 ms, blocking) */
void        cp_measure(cp_meas_t *m);
const char *cp_state_name(cp_state_t s);

/* EVSE: 0 = -12 V, 1000 = +12 V DC, 50 = 5 % (digital communication) */
void        cp_evse_set_duty(uint16_t permille);
/* EV: close / open S2 (state B <-> C) */
void        cp_ev_set_s2(int closed);

#endif
