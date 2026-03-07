# D-Cache & DMA 버퍼 정렬 주의사항

## 문제: STM32H5는 D-Cache 기본 활성화

STM32H5 Cortex-M33은 부트 시 D-Cache가 활성화됩니다.
GPDMA는 CPU 캐시를 거치지 않고 **직접 SRAM**에 접근하므로
캐시와 실제 메모리 내용이 불일치할 수 있습니다.

```
CPU ─[캐시]─ SRAM ←─── GPDMA
               ↑
           불일치 발생 지점
```

## 해결책

### 1. 쓰기 전: Cache Clean (Write-Back)

```c
// CPU 캐시의 수정된 내용을 SRAM에 반영
// → DMA가 SRAM에서 최신 데이터를 읽을 수 있음
SCB_CleanDCache_by_Addr((uint32_t *)writeBuf, size);
HAL_SD_WriteBlocks_DMA(&hsd1, writeBuf, block, count);
```

### 2. 읽기 후: Cache Invalidate

```c
HAL_SD_ReadBlocks_DMA(&hsd1, readBuf, block, count);
// DMA 완료 대기...
// CPU가 읽기 전 캐시 무효화 → SRAM의 최신 데이터를 읽도록
SCB_InvalidateDCache_by_Addr((uint32_t *)readBuf, size);
```

### 3. 버퍼 정렬 요구사항

```c
// 32바이트(캐시 라인) 정렬 필수
__attribute__((aligned(32))) uint8_t dma_buf[512];

// 또는 링커 섹션 활용
__attribute__((section(".dma_buffer"), aligned(32))) uint8_t dma_buf[512];
```

### 4. MPU로 DMA 버퍼 Non-Cacheable 설정 (대안)

캐시 flush/invalidate 없이 사용하려면 MPU로 해당 메모리 영역을
Non-Cacheable로 설정합니다.

```c
MPU_Region_InitTypeDef MPU_InitStruct = {0};

HAL_MPU_Disable();

MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
MPU_InitStruct.BaseAddress      = 0x20040000;  /* SRAM2 시작 주소 */
MPU_InitStruct.Size             = MPU_REGION_SIZE_256KB;
MPU_InitStruct.SubRegionDisable = 0x00;
MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;  /* 핵심 */
MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;

HAL_MPU_ConfigRegion(&MPU_InitStruct);
HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
```

그런 다음 DMA 버퍼를 SRAM2(`__attribute__((section(".sram2")))`)에 배치하면
캐시 동기화 없이 안전하게 DMA 사용 가능.

## 트러블슈팅 체크리스트

| 증상 | 원인 | 해결 |
|------|------|------|
| 읽은 데이터가 0 또는 이전 값 | D-Cache Invalidate 누락 | `SCB_InvalidateDCache_by_Addr` 추가 |
| 쓴 데이터와 다른 값 읽힘 | D-Cache Clean 누락 | `SCB_CleanDCache_by_Addr` 추가 |
| HardFault / DMA 에러 | 버퍼 정렬 불량 | `aligned(32)` 속성 확인 |
| SDMMC 초기화 실패 | PLL1Q 클럭 미설정 | RCC 주변장치 클럭 설정 확인 |
| DMA 전송 타임아웃 | DMA 인터럽트 미등록 | NVIC Enable / Priority 확인 |
| UART 출력 깨짐 | UART DMA 우선순위 역전 | GPDMA2 Priority < GPDMA1 Priority 확인 |
