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
| `Core/Src/lan8770_example.c` | 초기화/링크 폴링 사용 예시 |

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
