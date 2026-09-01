# STM32L562 — RTC 기반 주기적 Software Reset 예제

STM32CubeMX / STM32CubeIDE 기반, **STM32L562** 에서 RTC 로 주기적인
소프트웨어 리셋(`HAL_NVIC_SystemReset()`)을 거는 두 가지 방식을
**각각 독립 프로젝트**로 정리했습니다.

| | ① [**STM32L562_RTC_WakeUp_Reset**](STM32L562_RTC_WakeUp_Reset) | ② [**STM32L562_RTC_AlarmA_Reset**](STM32L562_RTC_AlarmA_Reset) |
|---|---|---|
| 트리거 | RTC **Wakeup Timer** | RTC **Alarm A** |
| 기준 | **부팅 시점**부터 N초 | **벽시계 시각** |
| 기본 설정 | 24시간(86400초)마다 | 매일 03:00:00 |
| 달력 시각 | 불필요 (아무 값이어도 동작) | **실제 시각과 맞아야 함** |
| 주기 변경 | `RESET_PERIOD_SEC` 한 줄 | `ALARM_RESET_HOUR/MINUTE/SECOND` |
| 최대 주기 | 131072초 (약 36.4시간) | 24시간 (날짜 마스킹) |
| 반복 방식 | 하드웨어 자동 재장전 | 날짜 마스크로 매일 자동 |
| 클럭 정확도 | LSI 도 사용 가능 | **LSE 강력 권장** |
| 이럴 때 | "가동 후 N시간마다 리프레시" | "트래픽 없는 새벽에만 리셋" |

## 어느 쪽을 고를까

- **장비를 켠 시점 기준으로 일정 시간마다** 리프레시하면 되고, 시각 동기화 수단이
  없다면 → **①  Wakeup Timer**. 설정이 한 줄이고 달력과 무관해서 가장 단순합니다.
- **매일 정해진 시각**(새벽 3시 등)에 리셋해야 한다면 → **② Alarm A**.
  단, RTC 달력을 실제 시각으로 맞춰야 하고 LSE 크리스탈이 사실상 필수입니다.

## 두 프로젝트의 공통 사항

- 대상 : **STM32L562** (TrustZone Disabled 기준), STM32CubeIDE
- 인터럽트 콜백에서는 플래그만 세우고 `main()` 루프에서 리셋 → UART 로그 보존
- 부팅 시 `RCC->CSR` 로 **리셋 원인**(`SFTRSTF` 등) 판별 후 출력
- **TAMP 백업 레지스터**에 소프트 리셋 누적 횟수 저장 (리셋해도 보존)
- 디버그 UART(115200) / 상태 LED 는 매크로로 on/off, 핀도 매크로로 변경
- HAL 드라이버·링커 스크립트·startup 파일은 미포함 →
  각 프로젝트 README 3장대로 CubeMX 로 생성 후 소스 4개를 덮어쓰면 됩니다

각 프로젝트의 상세 설정 절차와 STM32L5 주의사항은 해당 폴더의 `README.md` 를 보세요.
