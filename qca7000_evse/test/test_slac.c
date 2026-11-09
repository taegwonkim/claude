/* Host test: fake FreeRTOS + include slac_evse.c directly */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "FreeRTOS.h"
static TickType_t g_tick;
TickType_t xTaskGetTickCount(void){ return g_tick; }
void vTaskDelay(TickType_t t){ g_tick += t; }
BaseType_t xTaskCreate(void(*f)(void*), const char*n, uint32_t s, void*a, UBaseType_t p, TaskHandle_t*h){(void)f;(void)n;(void)s;(void)a;(void)p; if(h)*h=(void*)1; return pdPASS;}
typedef struct { size_t isz, depth, head, cnt; unsigned char *buf; } fq_t;
QueueHandle_t xQueueCreate(UBaseType_t d, UBaseType_t s){ fq_t*q=calloc(1,sizeof*q); q->isz=s; q->depth=d; q->buf=malloc(d*s); return q; }
BaseType_t xQueueSend(QueueHandle_t h, const void*it, TickType_t t){ fq_t*q=h;(void)t; if(q->cnt==q->depth) return pdFALSE; memcpy(q->buf+((q->head+q->cnt)%q->depth)*q->isz,it,q->isz); q->cnt++; return pdTRUE; }
BaseType_t xQueueReceive(QueueHandle_t h, void*it, TickType_t t){ fq_t*q=h;(void)t; if(!q->cnt) return pdFALSE; memcpy(it,q->buf+q->head*q->isz,q->isz); q->head=(q->head+1)%q->depth; q->cnt--; return pdTRUE; }

/* capture TX */
static uint8_t txf[32][200]; static uint16_t txl[32]; static int ntx;
int qca7000_send(const uint8_t *f, uint16_t len, uint32_t t){ (void)t; memcpy(txf[ntx],f,len); txl[ntx]=len; ntx++; return 0; }
static const uint8_t NMK_HPAV[16]={0x50,0xD3,0xE4,0x93,0x3F,0x85,0x5B,0x70,0x40,0x78,0x4D,0xF8,0x15,0xAA,0x8D,0xB7};
void slac_port_random(uint8_t *o, uint32_t n){ memcpy(o,NMK_HPAV,n); }
void slac_port_sha256(const uint8_t *in, uint32_t len, uint8_t out[32]){ SHA256(in,len,out); }

#include "slac_evse.c"

static int fails;
#define CHECK(c) do{ if(!(c)){ printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c); fails++; } }while(0)
static int evs[8], nev; static void cb(slac_evse_event_t e,const uint8_t m[6],void*u){(void)m;(void)u; evs[nev++]=e;}

static void pump(void){ slac_item_t it; while (xQueueReceive(S.q,&it,0)==pdTRUE){ if(it.kind==ITEM_CMD){ if(it.cmd==CMD_START) do_start(); else enter(ST_IDLE,0);} else if(S.st!=ST_IDLE) handle_frame(it.data,it.len);} if(S.tmo_on && (int32_t)(xTaskGetTickCount()-S.deadline)>=0) handle_timeout(); }

static const uint8_t HOST[6]={2,0,0,0x12,0x34,0x56}, EV[6]={0xEE,1,2,3,4,5}, MODEM[6]={0,0xB0,0x52,0xAA,0xBB,0xCC};
static const uint8_t RUN[8]={1,2,3,4,5,6,7,8};
static void mkhdr(hpgp_hdr_t*h,const uint8_t*src,const uint8_t*dst,uint16_t mt){ memcpy(h->eth.oda,dst,6); memcpy(h->eth.osa,src,6); hpgp_wr_be16(h->eth.ethertype,0x88E1); h->mmv=1; hpgp_wr_le16(h->mmtype,mt); }
static void rx(void*f,uint16_t len){ uint8_t b[200]={0}; memcpy(b,f,len); if(len<60)len=60; slac_evse_on_frame(b,len); pump(); }

int main(void){
  slac_evse_cfg_t c; slac_evse_default_cfg(&c); memcpy(c.host_mac,HOST,6); memcpy(c.modem_mac,MODEM,6); c.on_event=cb;
  CHECK(slac_evse_init(&c,1)==0);

  /* NID test vector: NMK "HomePlugAV" -> NID B0F2E695666B03 */
  uint8_t nid[7]; nid_from_nmk(NMK_HPAV,nid);
  static const uint8_t NID_EXP[7]={0xB0,0xF2,0xE6,0x95,0x66,0x6B,0x03};
  printf("NID = "); for(int i=0;i<7;i++) printf("%02X",nid[i]); printf("\n");
  CHECK(memcmp(nid,NID_EXP,7)==0);

  slac_evse_start(); pump();
  CHECK(ntx==1); cm_set_key_req_t *sk=(void*)txf[0];
  CHECK(memcmp(sk->h.eth.oda,MODEM,6)==0 && hpgp_rd_le16(sk->h.mmtype)==0x6008 && sk->key_type==1 && sk->pid==4 && sk->new_eks==1 && memcmp(sk->new_key,NMK_HPAV,16)==0 && memcmp(sk->nid,NID_EXP,7)==0);
  CHECK(txf[0][12]==0x88 && txf[0][13]==0xE1 && txf[0][15]==0x08 && txf[0][16]==0x60);

  cm_set_key_cnf_t skc={0}; mkhdr(&skc.h,MODEM,HOST,0x6009); rx(&skc,sizeof skc);
  CHECK(S.st==ST_WAIT_PARM_REQ && nev==1 && evs[0]==SLAC_EVSE_EV_KEY_SET);

  cm_slac_parm_req_t pr={0}; mkhdr(&pr.h,EV,k_bcast,0x6064); memcpy(pr.run_id,RUN,8); rx(&pr,sizeof pr);
  CHECK(ntx==2); cm_slac_parm_cnf_t *pc=(void*)txf[1];
  CHECK(hpgp_rd_le16(pc->h.mmtype)==0x6065 && memcmp(pc->h.eth.oda,EV,6)==0 && pc->num_sounds==10 && pc->time_out==6 && pc->resp_type==1 && memcmp(pc->forwarding_sta,EV,6)==0 && memcmp(pc->run_id,RUN,8)==0 && pc->msound_target[0]==0xFF);
  CHECK(S.st==ST_WAIT_START_ATTEN);

  cm_start_atten_char_ind_t sa={0}; mkhdr(&sa.h,EV,k_bcast,0x606A); sa.num_sounds=10; sa.time_out=6; sa.resp_type=1; memcpy(sa.forwarding_sta,HOST,6); memcpy(sa.run_id,RUN,8);
  for(int i=0;i<3;i++) rx(&sa,sizeof sa);
  CHECK(S.st==ST_SOUNDING);

  /* 9 profiles then timeout (one sound lost) ; aag group k = 20+k + (i%2) */
  for(int i=0;i<9;i++){ cm_atten_profile_ind_t ap={0}; mkhdr(&ap.h,MODEM,HOST,0x6086); memcpy(ap.pev_mac,EV,6); ap.num_groups=58; for(int k=0;k<58;k++) ap.aag[k]=(uint8_t)(20+k+(i%2)); rx(&ap,sizeof ap); g_tick+=50; }
  CHECK(S.st==ST_SOUNDING && ntx==2);
  g_tick+=200; pump();   /* 600 ms window expires */
  CHECK(S.st==ST_WAIT_ATTEN_RSP && ntx==3);
  cm_atten_char_ind_t *ac=(void*)txf[2];
  CHECK(hpgp_rd_le16(ac->h.mmtype)==0x606E && ac->num_sounds==9 && ac->num_groups==58 && memcmp(ac->source_address,EV,6)==0);
  /* avg of 5x(20+k) and 4x(21+k) = 20+k+0.44 -> rounds to 20+k */
  CHECK(ac->aag[0]==20 && ac->aag[57]==77);
  CHECK(txl[2]==129);

  /* lose first RSP -> retry */
  g_tick+=200; pump(); CHECK(ntx==4 && hpgp_rd_le16(((hpgp_hdr_t*)txf[3])->mmtype)==0x606E);
  cm_atten_char_rsp_t ar={0}; mkhdr(&ar.h,EV,HOST,0x606F); memcpy(ar.source_address,EV,6); memcpy(ar.run_id,RUN,8); rx(&ar,sizeof ar);
  CHECK(S.st==ST_WAIT_MATCH_REQ);

  cm_slac_match_req_t mr={0}; mkhdr(&mr.h,EV,HOST,0x607C); hpgp_wr_le16(mr.mvf_length,0x3E); memcpy(mr.pev_mac,EV,6); memcpy(mr.run_id,RUN,8); rx(&mr,sizeof mr);
  CHECK(S.st==ST_MATCHED && ntx==5 && evs[nev-1]==SLAC_EVSE_EV_MATCHED);
  cm_slac_match_cnf_t *mc=(void*)txf[4];
  CHECK(hpgp_rd_le16(mc->h.mmtype)==0x607D && hpgp_rd_le16(mc->mvf_length)==0x56 && memcmp(mc->evse_mac,HOST,6)==0 && memcmp(mc->pev_mac,EV,6)==0 && memcmp(mc->nid,NID_EXP,7)==0 && memcmp(mc->nmk,NMK_HPAV,16)==0 && memcmp(mc->run_id,RUN,8)==0 && txl[4]==109);

  /* EV repeats MATCH.REQ -> CNF re-sent, no second event */
  int nev0=nev; rx(&mr,sizeof mr); CHECK(ntx==6 && nev==nev0);

  /* wrong RunID ignored */
  mr.run_id[0]=9; rx(&mr,sizeof mr); CHECK(ntx==6);

  /* Failure path: start again, no PARM.REQ within TT_EVSE_SLAC_init */
  slac_evse_start(); pump(); rx(&skc,sizeof skc); CHECK(S.st==ST_WAIT_PARM_REQ);
  g_tick+=50001; pump(); CHECK(S.st==ST_IDLE && evs[nev-1]==SLAC_EVSE_EV_FAILED);

  /* MNBC sound frames are filtered out before the queue */
  { uint8_t b[60]={0}; mkhdr((hpgp_hdr_t*)b,EV,k_bcast,0x6076); slac_evse_on_frame(b,60); CHECK(((fq_t*)S.q)->cnt==0); }

  printf(fails? "%d FAILURE(S)\n" : "ALL SLAC TESTS PASSED\n", fails); return fails!=0;
}
