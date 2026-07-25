# CAN 송수신 도구

CM5 IO Board(또는 can0/can1이 이미 활성화된 아무 SocketCAN 보드)에서 CAN/CAN-FD 프레임을
송수신하는 CLI 도구입니다. candump/cansend 대신 사용할 수 있습니다.

두 가지 버전이 있습니다.

- **`can_tool.py`** — 보드에 SSH로 접속해서 빠르게 테스트할 때
- **`can_tool.c`** — PEAK PCAN-USB(FD) 어댑터를 꽂은 PC 쪽처럼 파이썬 없이 네이티브
  바이너리 하나로 돌리고 싶을 때. PEAK의 Linux 드라이버(`peak_usb`)도 can0/can1 같은
  SocketCAN 인터페이스로 잡히기 때문에 벤더 SDK(PCAN-Basic) 없이 이 코드 그대로
  빌드해서 쓰면 됩니다. (Windows PC에서 PCAN-Basic DLL을 쓰는 경우는 별도 구현이
  필요합니다 — 필요하면 요청해주세요.)

## can_tool.py

### 준비

인터페이스는 이 스크립트 실행 전에 이미 up 상태여야 합니다(메인 README의 CAN-FD 활성화
절차 참고).

```bash
sudo ip link set can0 up type can bitrate 500000 dbitrate 8000000 restart-ms 1000 berr-reporting on fd on
sudo ip link set can1 up type can bitrate 500000 dbitrate 8000000 restart-ms 1000 berr-reporting on fd on
```

Python 의존성 설치:

```bash
pip install python-can
```

### 사용법

PC에서 보드로 SSH 접속 후, 터미널 두 개(또는 tmux/screen)를 열어 한쪽은 수신, 한쪽은 송신으로 사용합니다.

**수신 (can0에서 대기):**

```bash
python3 tools/can_tool.py recv -c can0
```

**송신 (can1에서 FD 프레임 1회 전송):**

```bash
python3 tools/can_tool.py send -c can1 123 DEADBEEF
```

**반복 전송 (0.5초 간격으로 무한 전송):**

```bash
python3 tools/can_tool.py send -c can1 123 DEADBEEF --count 0 --interval 0.5
```

**클래식 CAN(비 FD) 프레임으로 전송:**

```bash
python3 tools/can_tool.py send -c can1 123 DEADBEEF --no-fd
```

이 HAT은 보드 내부에서 can0의 H/L과 can1의 H/L이 교차 연결되어 있어, 외부 배선 없이
`recv -c can0` / `send -c can1` 조합으로 바로 송수신 테스트가 가능합니다.

## can_tool.c

의존성 없이(python-can 불필요) SocketCAN 시스템 콜만으로 동작하는 C 버전입니다.
PEAK PCAN-USB(FD)가 붙은 PC와 CAN FD HAT이 붙은 보드 양쪽에서 동일한 소스로 빌드해서
쓸 수 있습니다.

### 빌드

```bash
gcc -O2 -Wall -o can_tool tools/can_tool.c
```

### 사용법

인터페이스는 마찬가지로 미리 up 상태여야 합니다(위 "준비" 절 참고). PEAK 어댑터를 쓰는
PC라면 `sudo ip link set can0 up type can bitrate 500000 dbitrate 8000000 fd on` 처럼
동일한 명령으로 올리면 됩니다.

**수신 (can0에서 대기):**

```bash
./can_tool recv -c can0
```

**송신 (can1에서 FD 프레임 1회 전송):**

```bash
./can_tool send -c can1 123 DEADBEEF
```

**반복 전송 (0.5초 간격으로 무한 전송):**

```bash
./can_tool send -c can1 123 DEADBEEF --count 0 --interval 0.5
```

**클래식 CAN(비 FD) 프레임으로 전송:**

```bash
./can_tool send -c can1 123 DEADBEEF --no-fd
```

옵션은 `can_tool.py`와 1:1로 동일합니다 (`-c/--channel`, `--no-fd`, `--extended`,
`--count`, `--interval`).
