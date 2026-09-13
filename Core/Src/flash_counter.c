/**
  ******************************************************************************
  * @file    flash_counter.c
  * @brief   내부 Flash 마지막 페이지를 이용한 비휘발성 카운터
  *
  *  저장 방식 (Flash 는 1->0 방향으로만 쓸 수 있고 지우는 단위가 페이지라서)
  *   - 마지막 페이지를 16바이트짜리 레코드로 쪼개서 "덧붙여" 기록한다.
  *   - 레코드 = [ 64bit 데이터 ][ 64bit 데이터의 보수 ]  (유효성 검사용)
  *   - 부팅 시 페이지를 훑어 마지막 유효 레코드를 읽는다.
  *   - 페이지가 꽉 차면(2048/16 = 128개) 페이지를 지우고 처음부터 다시 쓴다.
  *     => 128번 저장마다 1번 erase. Flash 지우기 수명 10,000회 기준
  *        1,280,000 번 저장 가능. (5분 주기 = 하루 288회 -> 약 12년)
  *
  *  주소는 런타임에 계산한다 (FLASHSIZE_BASE 의 실제 용량 정보 사용).
  *   - STM32L562RET6 (512KB) -> 0x0807F800
  *   - STM32L562RCT6 (256KB) -> 0x0803F800
  ******************************************************************************
  */
#include "flash_counter.h"

#if (USE_FLASH_COUNTER == 1U)

#include <stdint.h>
#include <string.h>

#define FC_REC_SIZE         16U          /* 레코드 1개 = 더블워드 2개 */

static uint32_t s_page_addr  = 0U;
static uint32_t s_page_size  = 0U;
static uint32_t s_rec_total  = 0U;
static uint32_t s_next_index = UINT32_MAX; /* 다음에 쓸 레코드 번호 (MAX = 미확인) */
static bool     s_ready      = false;

/* ---------------------------------------------------------------------------
 *  Flash 기하 정보 계산
 *    - DBANK=1 (출하 기본, 듀얼 뱅크) : 페이지 2KB, 페이지 번호는 뱅크 내 번호
 *    - DBANK=0 (싱글 뱅크)            : 페이지 4KB, 전체 통합 번호
 * ------------------------------------------------------------------------- */
static bool FC_IsDualBank(void)
{
#if defined(FLASH_OPTR_DBANK)
  return ((FLASH->OPTR & FLASH_OPTR_DBANK) != 0U);
#else
  return true;
#endif
}

static void FC_InitGeometry(void)
{
  uint32_t flash_bytes;

  if (s_ready)
  {
    return;
  }

  /* 칩에 새겨진 Flash 용량[KB] */
  flash_bytes = ((uint32_t)(*(volatile uint16_t *)FLASHSIZE_BASE)) * 1024U;

  s_page_size = FC_IsDualBank() ? 0x800U : 0x1000U;
  s_page_addr = FLASH_BASE + flash_bytes - s_page_size;   /* 마지막 페이지 */
  s_rec_total = s_page_size / FC_REC_SIZE;
  s_ready     = true;
}

static uint32_t FC_GetBank(uint32_t addr)
{
  uint32_t flash_bytes = ((uint32_t)(*(volatile uint16_t *)FLASHSIZE_BASE)) * 1024U;

  if (!FC_IsDualBank())
  {
    return FLASH_BANK_1;
  }
  return (addr < (FLASH_BASE + (flash_bytes / 2U))) ? FLASH_BANK_1 : FLASH_BANK_2;
}

static uint32_t FC_GetPageNumber(uint32_t addr)
{
  uint32_t flash_bytes = ((uint32_t)(*(volatile uint16_t *)FLASHSIZE_BASE)) * 1024U;
  uint32_t offset      = addr - FLASH_BASE;

  if (!FC_IsDualBank())
  {
    return offset / s_page_size;
  }
  return (offset % (flash_bytes / 2U)) / s_page_size;     /* 뱅크 내 페이지 번호 */
}

/* ---------------------------------------------------------------------------
 *  ICACHE : Flash 를 지우거나 쓰면 캐시 내용이 낡은 값이 되므로 잠시 끈다.
 * ------------------------------------------------------------------------- */
static void FC_CacheOff(void)
{
#if defined(HAL_ICACHE_MODULE_ENABLED)
  (void)HAL_ICACHE_Disable();
#endif
}

static void FC_CacheOn(void)
{
#if defined(HAL_ICACHE_MODULE_ENABLED)
  (void)HAL_ICACHE_Invalidate();
  (void)HAL_ICACHE_Enable();
#endif
}

/* ------------------------------------------------------------------------- */

uint32_t FlashCounter_GetPageAddr(void)
{
  FC_InitGeometry();
  return s_page_addr;
}

bool FlashCounter_Read(FlashCounter_t *out)
{
  bool     found = false;
  uint32_t i;

  FC_InitGeometry();

  out->total_reset = 0U;
  out->power_cycle = 0U;
  s_next_index     = s_rec_total;      /* 기본값 : 페이지가 꽉 찬 상태로 간주 */

  for (i = 0U; i < s_rec_total; i++)
  {
    const volatile uint64_t *rec =
        (const volatile uint64_t *)(s_page_addr + (i * FC_REC_SIZE));
    uint64_t data = rec[0];
    uint64_t inv  = rec[1];

    if ((data == UINT64_MAX) && (inv == UINT64_MAX))
    {
      s_next_index = i;                /* 여기부터가 빈 슬롯 */
      break;
    }

    if (inv == ~data)                  /* 유효한 레코드 */
    {
      out->total_reset = (uint32_t)(data & 0xFFFFFFFFU);
      out->power_cycle = (uint32_t)(data >> 32);
      found = true;
    }
    /* 유효하지 않으면(쓰다 만 레코드) 건너뛰고 계속 훑는다 */
  }

  return found;
}

bool FlashCounter_Erase(void)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t               error = 0U;
  HAL_StatusTypeDef      st;

  FC_InitGeometry();

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks     = FC_GetBank(s_page_addr);
  erase.Page      = FC_GetPageNumber(s_page_addr);
  erase.NbPages   = 1U;

  FC_CacheOff();
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    FC_CacheOn();
    return false;
  }
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
  st = HAL_FLASHEx_Erase(&erase, &error);
  (void)HAL_FLASH_Lock();
  FC_CacheOn();

  if ((st != HAL_OK) || (error != 0xFFFFFFFFU))
  {
    return false;
  }

  s_next_index = 0U;
  return true;
}

bool FlashCounter_Write(const FlashCounter_t *in)
{
  uint64_t data;
  uint64_t inv;
  uint32_t addr;
  bool     ok = true;

  FC_InitGeometry();

  /* 아직 인덱스를 모르면(부팅 후 Read 를 안 했으면) 먼저 훑는다 */
  if (s_next_index == UINT32_MAX)
  {
    FlashCounter_t dummy;
    (void)FlashCounter_Read(&dummy);
  }

  if (s_next_index >= s_rec_total)
  {
    if (!FlashCounter_Erase())
    {
      return false;
    }
  }

  data = ((uint64_t)in->power_cycle << 32) | (uint64_t)in->total_reset;
  inv  = ~data;
  addr = s_page_addr + (s_next_index * FC_REC_SIZE);

  FC_CacheOff();
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    FC_CacheOn();
    return false;
  }
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, data) != HAL_OK)
  {
    ok = false;
  }
  else if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + 8U, inv) != HAL_OK)
  {
    ok = false;
  }
  else
  {
    s_next_index++;
  }

  (void)HAL_FLASH_Lock();
  FC_CacheOn();

  return ok;
}

#endif /* USE_FLASH_COUNTER */
