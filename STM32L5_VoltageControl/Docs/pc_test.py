#!/usr/bin/env python3
"""
STM32L5 전압 제어 시스템 - PC 테스트 스크립트
사용법: python3 pc_test.py [--port COM3] [--baud 115200]

명령어:
  SET <ch> <mv>     : 채널 목표 전압 설정 (ch:1~4, mv:0~3300)
  GET <ch>          : 채널 측정값 조회
  STATUS            : 전체 상태 조회
  ENABLE <ch> <0|1> : 채널 활성/비활성
  RESET <ch|ALL>    : 폴트 리셋
  SWEEP <ch>        : 전압 스윕 테스트 (0→3300mV)
  TEST              : 자동 테스트 실행
  quit / exit       : 종료
"""

import serial
import time
import sys
import argparse
import threading

# --- 설정 ---
DEFAULT_PORT = "COM3"   # Windows: COM3, Linux: /dev/ttyACM0
DEFAULT_BAUD = 115200
TIMEOUT_S    = 2.0

class VoltageController:
    def __init__(self, port: str, baud: int):
        self.port = port
        self.baud = baud
        self.ser  = None
        self._rx_thread = None
        self._running   = False

    def connect(self) -> bool:
        try:
            self.ser = serial.Serial(
                port     = self.port,
                baudrate = self.baud,
                bytesize = serial.EIGHTBITS,
                parity   = serial.PARITY_NONE,
                stopbits = serial.STOPBITS_ONE,
                timeout  = TIMEOUT_S
            )
            print(f"[연결됨] {self.port} @ {self.baud}bps")
            self._running = True
            self._rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
            self._rx_thread.start()
            return True
        except serial.SerialException as e:
            print(f"[오류] 연결 실패: {e}")
            return False

    def disconnect(self):
        self._running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        print("[연결 해제]")

    def _rx_loop(self):
        """수신 스레드: 비동기 이벤트(EVT) 및 응답 출력"""
        while self._running:
            try:
                if self.ser and self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='replace').strip()
                    if line:
                        self._handle_response(line)
            except Exception:
                pass
            time.sleep(0.001)

    def _handle_response(self, line: str):
        if line.startswith("EVT FAULT"):
            print(f"\n⚠️  [폴트 이벤트] {line}")
        elif line.startswith("EVT"):
            print(f"\nℹ️  [이벤트] {line}")
        else:
            print(f"← {line}")

    def send_cmd(self, cmd: str, wait_response: bool = True) -> list:
        if not self.ser or not self.ser.is_open:
            print("[오류] 연결되지 않음")
            return []

        self._running = False  # RX 스레드 일시 정지
        time.sleep(0.01)

        # 버퍼 비우기
        self.ser.reset_input_buffer()

        # 명령 전송
        cmd_bytes = (cmd.strip() + '\r\n').encode('utf-8')
        self.ser.write(cmd_bytes)
        print(f"→ {cmd.strip()}")

        # 응답 수집 (최대 TIMEOUT_S 동안)
        responses = []
        start = time.time()
        while time.time() - start < TIMEOUT_S:
            if self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    responses.append(line)
                    print(f"← {line}")
                    # 종료 조건
                    if line.startswith("ERR") or \
                       (line.startswith("OK") and "STATUS_END" not in line and
                        "STATUS_BEGIN" not in line):
                        if "STATUS_END" not in ' '.join(responses):
                            break
                    if "STATUS_END" in line:
                        break

        self._running = True
        return responses

    def set_voltage(self, ch: int, mv: int) -> bool:
        resp = self.send_cmd(f"SET {ch} {mv}")
        return any(r.startswith("OK") for r in resp)

    def get_channel(self, ch: int) -> dict:
        resp = self.send_cmd(f"GET {ch}")
        result = {}
        for line in resp:
            if "TGT=" in line:
                # 파싱: "OK CH1 TGT=1500mV MEAS=1498mV ERR=2.0mV CURR=50mA PWM=1864"
                parts = line.split()
                for part in parts:
                    if '=' in part:
                        key, val = part.split('=', 1)
                        result[key] = val.replace('mV', '').replace('mA', '')
        return result

    def get_status(self):
        self.send_cmd("STATUS")

    def enable_channel(self, ch: int, enable: bool) -> bool:
        resp = self.send_cmd(f"ENABLE {ch} {1 if enable else 0}")
        return any(r.startswith("OK") for r in resp)

    def reset_channel(self, ch_or_all) -> bool:
        resp = self.send_cmd(f"RESET {ch_or_all}")
        return any(r.startswith("OK") for r in resp)

    def voltage_sweep_test(self, ch: int):
        """전압 스윕 테스트 (0 → 3300mV → 0)"""
        print(f"\n=== CH{ch} 전압 스윕 테스트 ===")

        test_points = [0, 500, 1000, 1500, 1650, 2000, 2500, 3000, 3300,
                       3000, 2500, 2000, 1500, 1000, 500, 0]

        for target_mv in test_points:
            self.set_voltage(ch, target_mv)
            time.sleep(0.5)  # 안정화 대기

            info = self.get_channel(ch)
            meas = info.get('MEAS', 'N/A')
            err  = info.get('ERR', 'N/A')
            curr = info.get('CURR', 'N/A')

            print(f"  TGT={target_mv:4d}mV  MEAS={meas:>7}mV  "
                  f"ERR={err:>7}mV  CURR={curr:>5}mA")

        print("=== 스윕 완료 ===\n")

    def auto_test(self):
        """자동 테스트 시나리오"""
        print("\n=== 자동 테스트 시작 ===")

        # 1. 버전 확인
        print("\n[1] 버전 확인")
        self.send_cmd("VERSION")

        # 2. 전체 채널 활성화
        print("\n[2] 전체 채널 활성화")
        for i in range(1, 5):
            self.enable_channel(i, True)
            time.sleep(0.1)

        # 3. 각 채널 독립 전압 설정
        print("\n[3] 채널별 목표 전압 설정")
        targets = {1: 1000, 2: 1500, 3: 2000, 4: 2500}
        for ch, mv in targets.items():
            self.set_voltage(ch, mv)
            time.sleep(0.1)

        # 4. 안정화 대기 후 상태 확인
        print("\n[4] 2초 대기 후 상태 확인...")
        time.sleep(2.0)
        self.get_status()

        # 5. 오차 검증
        print("\n[5] 측정 오차 검증")
        for ch, target in targets.items():
            info = self.get_channel(ch)
            meas_str = info.get('MEAS', '0')
            try:
                meas = float(meas_str)
                err  = abs(meas - target)
                status = "✓ OK" if err < 50 else "✗ FAIL"
                print(f"  CH{ch}: 목표={target}mV, 측정={meas:.0f}mV, "
                      f"오차={err:.0f}mV {status}")
            except ValueError:
                print(f"  CH{ch}: 측정값 파싱 실패")

        # 6. CH1 스윕 테스트
        print("\n[6] CH1 전압 스윕")
        self.voltage_sweep_test(1)

        # 7. 전체 0V 설정
        print("\n[7] 전체 채널 0V 설정")
        for i in range(1, 5):
            self.set_voltage(i, 0)
        time.sleep(0.5)
        self.get_status()

        print("\n=== 자동 테스트 완료 ===")


def interactive_mode(ctrl: VoltageController):
    """대화형 모드"""
    print("\n명령어 입력 (HELP 참조, quit으로 종료)")
    print("-" * 50)

    while True:
        try:
            raw = input("CMD> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n종료")
            break

        if not raw:
            continue

        cmd_upper = raw.upper()

        if cmd_upper in ('QUIT', 'EXIT', 'Q'):
            break
        elif cmd_upper.startswith('SWEEP '):
            parts = raw.split()
            if len(parts) == 2:
                ctrl.voltage_sweep_test(int(parts[1]))
        elif cmd_upper == 'TEST':
            ctrl.auto_test()
        else:
            ctrl.send_cmd(raw)


def main():
    parser = argparse.ArgumentParser(description='STM32L5 전압 제어 테스트')
    parser.add_argument('--port',  default=DEFAULT_PORT, help=f'시리얼 포트 (기본: {DEFAULT_PORT})')
    parser.add_argument('--baud',  default=DEFAULT_BAUD, type=int, help='통신 속도')
    parser.add_argument('--test',  action='store_true', help='자동 테스트 실행 후 종료')
    args = parser.parse_args()

    ctrl = VoltageController(args.port, args.baud)

    if not ctrl.connect():
        sys.exit(1)

    try:
        if args.test:
            ctrl.auto_test()
        else:
            interactive_mode(ctrl)
    finally:
        ctrl.disconnect()


if __name__ == '__main__':
    main()
