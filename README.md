# Seeed Odyssey STM32MP157C + CAN FD HAT 활성화 가이드

Seeed Studio **ODYSSEY – STM32MP157C** 보드에 Seeed **2-Channel CAN BUS FD Shield (CAN FD HAT, MCP2517FD/MCP2518FD 기반)** 를 장착하고 CAN‑FD를 활성화하는 절차를 정리한 문서입니다.

## 1. 하드웨어

- Seeed Studio ODYSSEY – STM32MP157C (Raspberry Pi 40핀 호환 헤더 탑재)
- Seeed 2-Channel CAN BUS FD Shield / CAN FD HAT
  - MCP2517FD (RTC 없음) 또는 MCP2518FD (RTC 있음, 배터리 소켓으로 구분) 기반
  - SPI 통신, 보드 내 120Ω 종단저항을 스위치로 on/off 가능
  - CAN0 ↔ CAN1 채널 간 H/L 라인이 보드 상에서 교차 연결되어 있어, 케이블 없이도 두 채널끼리 루프백 테스트 가능

### 장착

1. 보드 전원을 끈 상태에서 CAN FD HAT를 ODYSSEY 40핀 헤더에 맞춰 장착합니다.
2. 필요 시 종단저항(120Ω) 스위치를 버스 양 끝단 노드에서만 ON 으로 설정합니다.
3. 전원을 연결하고 부팅합니다.

## 2. 소프트웨어 준비

ODYSSEY STM32MP157C 는 Debian 기반 이미지(U-Boot 오버레이 방식)로 동작합니다. 기본 계정은 `debian` / `temppwd` 입니다.

```bash
sudo apt update
sudo apt install -y git build-essential can-utils device-tree-compiler
```

## 3. 디바이스 트리 오버레이 빌드 및 설치

CAN FD HAT 드라이버(MCP2517FD/MCP2518FD)는 Seeed의 `seeed-linux-dtoverlays` 저장소에서 STM32MP1용 오버레이(`stm32mp1-MCP2517FD-can0-overlay.dts`)로 제공됩니다. 보드(ODYSSEY) 위에서 직접 빌드합니다.

```bash
git clone https://github.com/Seeed-Studio/seeed-linux-dtoverlays
cd seeed-linux-dtoverlays
make all_stm32mp1
sudo make install_stm32mp1
```

빌드/설치가 끝나면 `/lib/firmware/stm32mp1-MCP2517FD-can0.dtbo` 가 생성됩니다.

> 2채널 HAT에서 두 번째 채널(can1)까지 쓰려면 저장소의 stm32mp1 오버레이 목록에 can1용 오버레이가 있는지 확인하고 동일한 방식으로 함께 설치하세요. 없다면 `stm32mp1-MCP2517FD-can0-overlay.dts` 를 참고해 SPI CS/인터럽트 GPIO만 바꾼 두 번째 오버레이를 추가로 작성해 빌드해야 합니다.

## 4. U-Boot 오버레이 등록 (`/boot/uEnv.txt`)

ODYSSEY는 U-Boot 오버레이를 `uboot_overlay_addrN` 변수로 로드합니다. 사용하지 않은 인덱스 번호로 아래처럼 추가합니다.

```bash
sudo sh -c "echo uboot_overlay_addr1=/lib/firmware/stm32mp1-MCP2517FD-can0.dtbo >> /boot/uEnv.txt"
```

기존에 다른 오버레이(`addr0` 등, LCD/AP6236 등)를 이미 쓰고 있다면 번호가 겹치지 않도록 확인 후 재부팅합니다.

```bash
sudo reboot
```

## 5. 정상 인식 확인

```bash
dmesg | grep -i mcp25
ip link show | grep can
```

`can0` (2채널 HAT라면 `can1` 도) 인터페이스가 보이면 드라이버가 정상적으로 로드된 것입니다.

CRC read error 등이 뜨면 아래를 점검합니다.
- HAT가 40핀 헤더에 완전히 밀착됐는지
- 오버레이가 참조하는 SPI 클록(`clock-frequency`, 오실레이터 40MHz)과 실제 HAT 오실레이터 값이 일치하는지
- SPI 배선 접촉 불량 여부

## 6. CAN-FD 활성화

`ip link` 로 CAN 클래식 비트레이트(`bitrate`)와 FD 데이터 비트레이트(`dbitrate`)를 함께 지정하고 `fd on` 을 붙입니다.

```bash
sudo ip link set can0 up type can bitrate 500000 dbitrate 8000000 restart-ms 1000 berr-reporting on fd on
sudo ifconfig can0 txqueuelen 65536
```

2채널 HAT에서 두 채널을 함께 쓰는 경우:

```bash
sudo ip link set can0 up type can bitrate 500000 dbitrate 8000000 restart-ms 1000 berr-reporting on fd on
sudo ip link set can1 up type can bitrate 500000 dbitrate 8000000 restart-ms 1000 berr-reporting on fd on
sudo ifconfig can0 txqueuelen 65536
sudo ifconfig can1 txqueuelen 65536
```

## 7. 동작 테스트

`can-utils` 로 송수신을 확인합니다. (2채널 HAT는 보드 내부에서 can0/can1이 H/L로 교차 연결되어 있어 케이블 없이 상호 테스트 가능)

```bash
# 수신 대기
candump can0

# 다른 터미널에서 CAN-FD 프레임 전송 (-b: CAN-FD, -e: extended ID 옵션 예시)
cansend can0 123##1AABBCCDD

# 또는 랜덤 프레임 생성
cangen can0 -mv
```

## 8. 재부팅 후에도 유지되도록 자동 설정 (선택)

매 부팅 시 자동으로 인터페이스를 올리려면 systemd-networkd 설정을 추가합니다.

`/etc/systemd/network/80-can0.network`:

```ini
[Match]
Name=can0

[CAN]
BitRate=500000
DataBitRate=8000000
FDMode=true
RestartSec=1
```

동일하게 `can1`용 파일도 만든 뒤 활성화합니다.

```bash
sudo systemctl enable --now systemd-networkd
```

## 참고 자료

- [ODYSSEY – STM32MP157C | Seeed Studio Wiki](https://wiki.seeedstudio.com/ODYSSEY-STM32MP157C/)
- [2 Channel CAN BUS FD Shield for Raspberry Pi | Seeed Studio Wiki](https://wiki.seeedstudio.com/2-Channel-CAN-BUS-FD-Shield-for-Raspberry-Pi/)
- [Seeed-Studio/seeed-linux-dtoverlays](https://github.com/Seeed-Studio/seeed-linux-dtoverlays)
