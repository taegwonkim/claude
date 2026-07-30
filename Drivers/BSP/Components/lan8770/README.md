# LAN8770 STM32H7 드라이버 스켈레톤

Microchip **LAN8770** (100BASE-T1 자동차용 이더넷 PHY)를 STM32H7 시리즈의
내장 Ethernet MAC(RMII)에 연결하기 위한 드라이버 스켈레톤입니다. ST가 자체
BSP PHY 컴포넌트(`lan8742` 등)에 쓰는 것과 동일한 "bus IO 함수 포인터"
패턴을 따르므로, STM32CubeH7 HAL / LwIP `ethernetif.c`에 바로 연결할 수
있습니다.

## 파일 구성

| 파일 | 역할 |
|---|---|
| `Drivers/BSP/Components/lan8770/lan8770.h/.c` | HAL 비의존 PHY 드라이버 본체 |
| `Core/Inc/Core/Src/lan8770_stm32_port.h/.c` | STM32H7 HAL(`HAL_ETH_ReadPHYRegister` 등) 연동 레이어 |
| `Core/Src/lan8770_example.c` | 초기화/링크 폴링 사용 예시 (베어메탈/폴링 루프) |
| `Core/Inc/Core/Src/lan8770_freertos_task.h/.c` | FreeRTOS(CMSIS-RTOS2) 태스크로 실행하는 버전 — 뮤텍스로 PHY 레지스터 접근 보호 |

## 전제 조건 (CubeMX)

1. **RMII** 인터페이스로 Ethernet 페리페럴을 활성화 (MII 아님).
2. RMII 핀 배치: `ETH_REF_CLK`, `ETH_MDIO`, `ETH_MDC`, `ETH_RXD0/1`,
   `ETH_TXD0/1`, `ETH_TX_EN`, `ETH_CRS_DV` — 사용 보드의 알터네이트 펑션에
   맞게 CubeMX에서 GPIO/클럭 설정.
3. `MX_ETH_Init()`이 생성하는 `ETH_HandleTypeDef heth`가 존재해야 하며, 이
   포트 레이어는 그 핸들을 그대로 재사용합니다(자체적으로 `HAL_ETH_Init()`을
   호출하지 않음).
4. LAN8770 쪽 50MHz RMII REF_CLK 소스(외부 오실레이터 또는 MCU
   `MCO`)를 데이터시트 레퍼런스 회로대로 설계.

## 사용법

```c
#include "lan8770_example.c" 대신, main.c 등에서:

LAN8770_Example_Init(LAN8770_MODE_MASTER); /* 또는 LAN8770_MODE_SLAVE */

while (1)
{
  if (LAN8770_Example_PollLink() == LAN8770_STATUS_LINK_UP_100M_FD)
  {
    /* netif_set_link_up() 등 */
  }
}
```

100BASE-T1은 지점 대 지점(point-to-point) 링크이므로 **케이블 양단 중 정확히
한쪽만 Master, 다른 쪽은 Slave**로 설정해야 링크가 올라옵니다.

## FreeRTOS 사용 시

`lan8770_freertos_task.h/.c`는 PHY 초기화 + 링크 폴링을 별도 태스크로 돌리고,
공유 자원인 MAC의 MDIO 컨트롤러 레지스터(MACMDIOAR) 접근을 뮤텍스로 보호합니다.
CMSIS-RTOS2 (`cmsis_os2.h`) API만 사용하므로 CubeMX가 생성하는 FreeRTOS
코드와 그대로 맞물립니다.

### CubeMX 설정

1. **Middleware and Software Packs → FREERTOS** 활성화, **Interface: CMSIS_V2**
   선택 (이 코드가 쓰는 `osThreadNew`/`osMutexNew`/`osDelay` 등은 CMSIS-RTOS2
   API이며, CMSIS_V1으로는 그대로 동작하지 않습니다).
2. **System Core → SYS → Timebase Source**를 `SysTick`에서 다른 여유 타이머
   (예: `TIM6`)로 변경. FreeRTOS가 SysTick을 스케줄러 틱으로 점유하므로,
   HAL_Delay()/HAL_GetTick()이 계속 동작하려면 필수입니다 (FreeRTOS를 켜면
   CubeMX가 자동으로 이 변경을 권고/강제합니다).
3. **NVIC**: FreeRTOS 활성화 시 CubeMX가 Priority Grouping을 4bit(선점 우선순위만
   사용)로 강제 전환합니다. `ETH_IRQn` 등 향후 `...FromISR()` 계열 API를 호출할
   인터럽트의 선점 우선순위 숫자는 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`
   (CubeMX FREERTOS Config 탭 기본값 5) **이상**(=우선순위는 더 낮게)으로 맞춰야
   합니다. 이 스켈레톤 자체는 폴링 방식이라 ISR을 쓰지 않지만, 추후 PHY의 `INT`
   핀을 EXTI로 연결해 인터럽트 기반 링크 감지를 추가할 경우 반드시 지켜야 합니다.
4. **FREERTOS Config parameters** 탭: `USE_MUTEXES = 1` (위 코드가
   `osMutexNew()`를 사용하므로 필수). LwIP를 함께 쓸 계획이면
   `TOTAL_HEAP_SIZE`를 여유 있게(예: 15~20KB 이상) 올려두는 것을 권장합니다.
5. Tasks and Queues 탭에 별도로 태스크를 등록할 필요는 없습니다 — 이 코드는
   `LAN8770_FreeRTOS_Start()` 호출 시 `osThreadNew()`로 태스크를 동적 생성합니다.

### 코드에서 호출하는 위치

`MX_ETH_Init()`이 끝난 뒤(즉 `heth`가 유효해진 뒤), CubeMX가 생성하는
`freertos.c`의 `MX_FREERTOS_Init()` 안, 또는 그 안에서 만드는 첫 애플리케이션
태스크에서 한 번 호출합니다.

```c
/* freertos.c, MX_FREERTOS_Init() 안 */
LAN8770_FreeRTOS_Start(LAN8770_MODE_MASTER); /* 보드 역할에 맞게 MASTER/SLAVE */
```

링크 상태 변화는 폴링 태스크가 감지해 `LAN8770_FreeRTOS_OnLinkChange()`를
호출합니다. 기본 구현은 no-op이므로, LwIP netif를 올리려면 애플리케이션
쪽에서 이 함수를 재정의(override)하세요:

```c
void LAN8770_FreeRTOS_OnLinkChange(int32_t LinkState)
{
  if (LinkState == LAN8770_STATUS_LINK_UP_100M_FD)
  {
    netif_set_link_up(&gnetif);
  }
  else
  {
    netif_set_link_down(&gnetif);
  }
}
```

다른 태스크(예: 커스텀 `ethernetif.c`)가 PHY 레지스터에 동시 접근해야 한다면
드라이버(`LAN8770_ReadReg`/`WriteReg`)를 직접 부르지 말고
`LAN8770_FreeRTOS_ReadReg()`/`LAN8770_FreeRTOS_WriteReg()`를 통해 접근하세요 —
동일한 뮤텍스로 보호됩니다.

## 아직 확인이 필요한 부분 (TODO)

웹에서 공개적으로 확인 가능한 자료만으로는 LAN8770의 **벤더 전용 레지스터**
(인터럽트 소스/마스크, T1 링크·SQI 상태, TC10 Sleep/Wake, 케이블 진단) 정확한
주소를 검증할 수 없었습니다. 이 스켈레톤에는 IEEE 802.3 표준 레지스터
(Clause 22 기본 레지스터, Clause 45 BASE-T1 Master/Slave 설정 레지스터
`0x0834`)만 확정값으로 채워져 있고, 나머지는 `lan8770.h`에 TODO 주석으로
표시해 두었습니다. 실기판에 올리기 전에 아래 문서로 반드시 대조하세요.

- LAN8770 데이터시트 `DS00002580` (Microchip)
- `EVB-LAN8770-RMII` 사용자 가이드 `DS50002978A` (Microchip)
- 보드 스키매틱의 MDIO 주소 스트랩 핀 설정

## 알려진 제한사항

- 인터럽트(`INT`) 핀 기반 링크 변화 감지는 미구현 (현재는 폴링 방식만 제공).
- TC10 Sleep/Wake 시퀀스 미구현.
- 케이블 진단 기능 미구현.
- FreeRTOS 태스크(`lan8770_freertos_task.c`)의 `LAN8770_Init()` 실패 처리는
  TODO로만 표시되어 있음 — 애플리케이션에 맞는 오류 보고(이벤트 플래그, 로그
  등)를 직접 추가해야 함.
