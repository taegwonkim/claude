#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
GPIO_TypeDef gA, gB;
static TickType_t g_tick;
TickType_t xTaskGetTickCount(void){ return g_tick; }
void vTaskDelay(TickType_t t){ g_tick+=t; }
BaseType_t xTaskCreate(void(*f)(void*), const char*n, uint32_t s, void*a, UBaseType_t p, TaskHandle_t*h){(void)f;(void)n;(void)s;(void)a;(void)p; *h=(void*)1; return pdPASS;}
BaseType_t xTaskNotifyWait(uint32_t a, uint32_t b, uint32_t*c, TickType_t d){(void)a;(void)b;(void)c;(void)d;return pdFALSE;}
BaseType_t xTaskNotify(TaskHandle_t t, uint32_t v, eNotifyAction a){(void)t;(void)v;(void)a;return pdPASS;}
BaseType_t xTaskNotifyFromISR(TaskHandle_t t, uint32_t v, eNotifyAction a, BaseType_t*w){(void)t;(void)v;(void)a;(void)w;return pdPASS;}
typedef struct { size_t isz, depth, head, cnt; unsigned char *buf; } fq_t;
QueueHandle_t xQueueCreate(UBaseType_t d, UBaseType_t s){ fq_t*q=calloc(1,sizeof*q); q->isz=s; q->depth=d; q->buf=malloc(d*s); return q; }
BaseType_t xQueueSend(QueueHandle_t h, const void*it, TickType_t t){ fq_t*q=h;(void)t; if(q->cnt==q->depth) return pdFALSE; memcpy(q->buf+((q->head+q->cnt)%q->depth)*q->isz,it,q->isz); q->cnt++; return pdTRUE; }
BaseType_t xQueueReceive(QueueHandle_t h, void*it, TickType_t t){ fq_t*q=h;(void)t; if(!q->cnt) return pdFALSE; memcpy(it,q->buf+q->head*q->isz,q->isz); q->head=(q->head+1)%q->depth; q->cnt--; return pdTRUE; }
static int sem;
SemaphoreHandle_t xSemaphoreCreateBinary(void){ return (void*)&sem; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t){(void)s;(void)t; if(sem){sem=0;return pdTRUE;} return pdFALSE;}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t*w){(void)s;(void)w; sem=1; return pdTRUE;}

/* ---------------- simulated QCA7000 SPI slave (burst mode) --------------- */
static int cs_low_now, int_pin;
static uint16_t r_sig=0xAA55, r_wrspace=0x0C5B, r_cause, r_enable, r_bfr, r_cfg;
static uint8_t rdbuf[4000]; static uint16_t rdlen;
static uint8_t wrbuf[4000]; static uint16_t wrlen;
static int cause_acked_val=-1, n_cfg_writes;
static uint16_t *reg(uint16_t a){ switch(a){case 0x1A00:return &r_sig;case 0x0200:return &r_wrspace;case 0x0C00:return &r_cause;case 0x0D00:return &r_enable;case 0x0100:return &r_bfr;case 0x0400:return &r_cfg;} static uint16_t z; z=0; return &z; }
static uint16_t rdbyte_ava(void){ return rdlen; }
static void xact(const uint8_t *tx, uint8_t *rx, uint16_t n){
  if (!cs_low_now){ printf("SPI without CS!\n"); exit(1);} 
  uint16_t cmd=(uint16_t)(tx[0]<<8|tx[1]); int rd=cmd&0x8000, in=cmd&0x4000; uint16_t a=cmd&0x3FFF;
  if (rx) memset(rx,0,n);
  if (in){ if(n!=4){printf("bad reg len %u\n",n);exit(1);} 
    if (rd){ uint16_t v = (a==0x0300)? rdbyte_ava() : *reg(a); rx[2]=v>>8; rx[3]=v; }
    else { uint16_t v=(uint16_t)(tx[2]<<8|tx[3]); if(a==0x0C00){cause_acked_val=v; r_cause&=~v;} else if(a==0x0400){r_cfg=v;n_cfg_writes++;} else *reg(a)=v; }
  } else {
    if ((uint16_t)(n-2)!=r_bfr){ printf("BFR_SIZE mismatch %u vs %u\n",n-2,r_bfr); exit(1);} 
    if (rd){ memcpy(rx+2,rdbuf,n-2); rdlen=0; }
    else { memcpy(wrbuf,tx+2,n-2); wrlen=n-2; }
  }
}
void HAL_GPIO_WritePin(GPIO_TypeDef*p, uint16_t pin, GPIO_PinState s){ if(p==GPIOA&&pin==GPIO_PIN_4) cs_low_now=(s==GPIO_PIN_RESET); }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*p, uint16_t pin){(void)p;(void)pin; return int_pin?GPIO_PIN_SET:GPIO_PIN_RESET;}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*h, uint8_t*t, uint8_t*r, uint16_t n, uint32_t to){(void)h;(void)to; xact(t,r,n); return HAL_OK;}
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef*h, uint8_t*t, uint16_t n, uint32_t to){(void)h;(void)to; xact(t,NULL,n); return HAL_OK;}
void qca7000_spi_dma_isr(void);
HAL_StatusTypeDef HAL_SPI_TransmitReceive_DMA(SPI_HandleTypeDef*h, uint8_t*t, uint8_t*r, uint16_t n){(void)h; xact(t,r,n); qca7000_spi_dma_isr(); return HAL_OK;}
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef*h, uint8_t*t, uint16_t n){(void)h; xact(t,NULL,n); qca7000_spi_dma_isr(); return HAL_OK;}
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef*h){(void)h;return HAL_OK;}

#include "qca7000.c"

static int fails;
#define CHECK(c) do{ if(!(c)){ printf("FAIL %d: %s\n",__LINE__,#c); fails++; } }while(0)
static uint8_t got[4][1600]; static uint16_t gotl[4]; static int ngot;
static void rxcb(const uint8_t*f,uint16_t l,void*a){(void)a; memcpy(got[ngot],f,l); gotl[ngot++]=l;}
static void add_rx_frame(const uint8_t *f, uint16_t l){ uint8_t *p=rdbuf+rdlen; p[0]=0;p[1]=0;p[2]=(l+10)>>8;p[3]=(uint8_t)(l+10); p+=4; memset(p,0xAA,4); p[4]=(uint8_t)l; p[5]=l>>8; p[6]=p[7]=0; memcpy(p+8,f,l); p[8+l]=0x55;p[9+l]=0x55; rdlen+=4+10+l; }

int main(void){
  SPI_HandleTypeDef hs; qca7000_cfg_t c={0}; c.hspi=&hs; c.cs_port=GPIOA; c.cs_pin=GPIO_PIN_4; c.int_port=GPIOB; c.int_pin=GPIO_PIN_0; c.rx_cb=rxcb;
  CHECK(qca7000_init(&c)==0);
  /* CPU_ON after boot */
  r_cause=0x40; handle_irq();
  CHECK(s_state==QCA7000_READY && r_enable==0x47 && cause_acked_val==0x40);

  /* TX short frame -> padded to 60 */
  uint8_t f[20]; for(int i=0;i<20;i++) f[i]=(uint8_t)i;
  CHECK(qca7000_send(f,20,0)==0); tx_drain();
  CHECK(wrlen==70 && r_bfr==70);
  CHECK(wrbuf[0]==0xAA&&wrbuf[3]==0xAA&&wrbuf[4]==60&&wrbuf[5]==0&&wrbuf[6]==0&&wrbuf[7]==0);
  CHECK(memcmp(wrbuf+8,f,20)==0 && wrbuf[8+20]==0 && wrbuf[8+59]==0 && wrbuf[68]==0x55&&wrbuf[69]==0x55);

  /* TX no space -> stays pending, then sent */
  uint8_t big[1514]; memset(big,0x5A,sizeof big); r_wrspace=100; wrlen=0;
  CHECK(qca7000_send(big,1514,0)==0); tx_drain(); CHECK(wrlen==0 && s_tx_cur!=NULL);
  r_wrspace=0x0C5B; tx_drain(); CHECK(wrlen==1524 && s_tx_cur==NULL && wrbuf[4]==(1514&0xFF) && wrbuf[5]==(1514>>8));

  /* RX two frames in one read */
  uint8_t a[60],b[300]; memset(a,0x11,60); memset(b,0x22,300); add_rx_frame(a,60); add_rx_frame(b,300);
  r_cause=0x01; handle_irq();
  CHECK(ngot==2 && gotl[0]==60 && gotl[1]==300 && got[0][0]==0x11 && got[1][299]==0x22 && s_stats.rx_errors==0 && r_enable==0x47);

  /* corrupted EOF -> error counted, no delivery */
  add_rx_frame(a,60); rdbuf[4+8+60]=0x00; r_cause=0x01; handle_irq(); CHECK(ngot==2 && s_stats.rx_errors==1);

  /* WRBUF_ERR -> UNKNOWN -> soft reset -> RESET -> CPU_ON -> READY */
  r_cause=0x04; handle_irq(); CHECK(s_state==QCA7000_UNKNOWN);
  sync_update(); CHECK(s_state==QCA7000_RESET && n_cfg_writes==1 && (r_cfg&0x40));
  r_cause=0x40; handle_irq(); CHECK(s_state==QCA7000_READY && s_stats.device_resets==2);

  /* bad signature while READY -> UNKNOWN */
  r_sig=0x0000; sync_update(); CHECK(s_state==QCA7000_UNKNOWN);
  sync_update(); CHECK(s_state==QCA7000_UNKNOWN);   /* stays, no soft reset possible */
  r_sig=0xAA55; sync_update(); CHECK(s_state==QCA7000_RESET);
  g_tick+=3000; sync_update(); CHECK(s_state==QCA7000_UNKNOWN);  /* reset timeout */

  /* TX refused when not READY, slot returned */
  CHECK(qca7000_send(f,20,0)!=0); CHECK(((fq_t*)s_freeq)->cnt==QCA7000_TX_POOL);

  printf(fails? "%d FAILURE(S)\n":"ALL DRIVER TESTS PASSED\n", fails); return fails!=0;
}
