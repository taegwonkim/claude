# STM32L552RCT6 X-CUBE-EEPROM 설정 가이드

## 1. STM32CubeMX 설정

### Flash 옵션 바이트
- **Dual-bank mode** 활성화 (`nDBANK = 0`)
  - `FLASH_OPTR` 레지스터의 `DBANK` 비트 = 1
  - 미설정 시 단일 뱅크로 동작하며 페이지 크기가 달라짐

### Middleware 추가
1. `Middleware and Software Packs` → `X-CUBE-EEPROM` 추가
2. EEPROM Emulation Application 선택
3. 자동 생성 파일: `eeprom_emul.c`, `eeprom_emul.h`, `flash_interface.c`(일부 버전)

### 클록 설정
- SYSCLK: 110 MHz (MSI → PLL)
- Flash Latency: 5 WS (자동 설정됨)

---

## 2. Flash 메모리 맵

```
0x08000000 ┌─────────────────────────────┐
           │   Bank1 (128KB)             │  ← 코드, const
           │   pages 0 ~ 63             │
0x08020000 ├─────────────────────────────┤
           │   Bank2 코드 영역 (96KB)    │  ← 코드 계속
           │   pages 64 ~ 111           │
0x08038000 ├─────────────────────────────┤
           │   EEPROM 에뮬레이션 (32KB)  │  ← 4 pages × 2KB × 4
           │   pages 112 ~ 127          │
0x0803FFFF └─────────────────────────────┘
```

> 링커 스크립트에서 FLASH LENGTH = 224K 로 설정하여 겹침 방지

---

## 3. 프로젝트에 파일 추가

```
Project/
├── Core/
│   ├── Inc/
│   │   ├── eeprom_emul_conf.h   ← 이 저장소 파일
│   │   ├── eeprom_manager.h     ← 이 저장소 파일
│   │   └── flash_interface.h    ← 이 저장소 파일
│   └── Src/
│       ├── eeprom_manager.c     ← 이 저장소 파일
│       ├── flash_interface.c    ← 이 저장소 파일
│       └── main.c               ← 이 저장소 파일
└── Middlewares/
    └── ST/
        └── STM32_EEPROM_Emul/
            ├── eeprom_emul.c    ← CubeMX 생성
            └── eeprom_emul.h    ← CubeMX 생성
```

---

## 4. eeprom_emul.h 주요 함수 (X-CUBE-EEPROM)

| 함수 | 설명 |
|------|------|
| `EE_Init(EE_FORCED_ERASE)` | 초기화 (최초 부팅 또는 포맷 필요 시) |
| `EE_Init(EE_NO_ERASE)` | 초기화 (기존 데이터 유지) |
| `EE_WriteVariable32bits(vAddr, data)` | 32비트 쓰기 |
| `EE_ReadVariable32bits(vAddr, &data)` | 32비트 읽기 |
| `EE_CleanUp()` | 페이지 전송 완료 (EE_CLEAN_NEEDED 시 호출) |
| `EE_Format(EE_FORCED_ERASE)` | 전체 초기화 |

---

## 5. 주의사항

1. **Flash 잠금**: 쓰기 전 `HAL_FLASH_Unlock()`, 완료 후 `HAL_FLASH_Lock()` 필수
2. **인터럽트**: EEPROM 쓰기 중 플래시 인터럽트가 발생하지 않도록 주의
3. **EE_CLEAN_NEEDED**: `EE_WriteVariable` 반환값이 `EE_CLEAN_NEEDED`이면 즉시 `EE_CleanUp()` 호출
4. **옵션 바이트**: Dual-bank 미설정 시 페이지 크기 = 4KB (`FLASH_PAGE_SIZE`도 변경 필요)
5. **TrustZone**: STM32L552는 TrustZone 지원 — EEPROM 페이지를 Non-secure 영역에 배치할 것
