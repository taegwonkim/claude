/*
 * app_plc.h - application: QCA7005 driver + SLAC for the selected role
 */
#ifndef APP_PLC_H
#define APP_PLC_H

#include <stdint.h>
#include "slac.h"
#include "plc_modem.h"

void APP_PLC_Init(void);
void APP_PLC_Process(void);

/* shared by app_evse.c / app_ev.c */
extern slac_io_t   g_plc_io;
extern plc_modem_t g_plc_modem;

/* role specific part (app_evse.c or app_ev.c, chosen by PLC_ROLE) */
void        app_role_init(uint32_t now);                 /* modem in sync (again) */
void        app_role_on_frame(const uint8_t *f, uint16_t len, uint32_t now);
void        app_role_process(uint32_t now);
void        app_role_status(void);
const char *app_role_name(void);
int         app_role_matched(void);
int         app_role_failed(void);

#endif
