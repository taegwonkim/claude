#!/usr/bin/env python3
"""CM5 IO Board(can0/can1)에서 CAN/CAN-FD 프레임을 송수신하는 CLI 도구.

SSH로 접속한 PC 터미널에서 바로 실행해서 candump/cansend 대신 쓰는 용도.
인터페이스(can0/can1)는 사전에 `ip link set <ch> up type can bitrate ... dbitrate ... fd on`
으로 이미 올라와 있어야 한다. 이 스크립트는 bitrate 설정을 건드리지 않는다.

사용 예:
    python3 can_tool.py recv -c can0
    python3 can_tool.py send -c can1 123 DEADBEEF
    python3 can_tool.py send -c can1 123 DEADBEEF --count 0 --interval 0.5
"""
import argparse
import sys
import time

import can


def build_bus(channel: str, fd: bool) -> can.BusABC:
    return can.interface.Bus(channel=channel, interface="socketcan", fd=fd)


def cmd_recv(args: argparse.Namespace) -> None:
    bus = build_bus(args.channel, args.fd)
    print(f"[{args.channel}] 수신 대기 중... (Ctrl+C 종료)")
    try:
        for msg in bus:
            ts = time.strftime("%H:%M:%S", time.localtime(msg.timestamp))
            kind = "FD" if msg.is_fd else "CC"
            data_hex = msg.data.hex(" ")
            print(f"{ts}  {args.channel}  {kind}  ID=0x{msg.arbitration_id:X}  "
                  f"DLC={msg.dlc}  DATA=[{data_hex}]")
    except KeyboardInterrupt:
        pass
    finally:
        bus.shutdown()


def cmd_send(args: argparse.Namespace) -> None:
    bus = build_bus(args.channel, args.fd)
    try:
        arb_id = int(args.id, 16)
    except ValueError:
        sys.exit(f"잘못된 ID: {args.id} (16진수로 입력, 예: 123)")

    try:
        data = bytes.fromhex(args.data.replace(" ", ""))
    except ValueError:
        sys.exit(f"잘못된 데이터: {args.data} (16진수로 입력, 예: DEADBEEF)")

    msg = can.Message(
        arbitration_id=arb_id,
        data=data,
        is_extended_id=args.extended,
        is_fd=args.fd,
        bitrate_switch=args.fd,
    )

    try:
        sent = 0
        while args.count == 0 or sent < args.count:
            bus.send(msg)
            print(f"전송: {args.channel}  ID=0x{arb_id:X}  DATA=[{data.hex(' ')}]")
            sent += 1
            if args.count == 0 or sent < args.count:
                time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    finally:
        bus.shutdown()


def main() -> None:
    parser = argparse.ArgumentParser(description="CAN/CAN-FD 송수신 CLI 도구 (SocketCAN)")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_recv = sub.add_parser("recv", help="프레임 수신 대기")
    p_recv.add_argument("-c", "--channel", default="can0", help="인터페이스 이름 (기본 can0)")
    p_recv.add_argument("--no-fd", dest="fd", action="store_false", default=True,
                         help="클래식 CAN 프레임만 수신 (기본은 FD 지원)")
    p_recv.set_defaults(func=cmd_recv)

    p_send = sub.add_parser("send", help="프레임 송신")
    p_send.add_argument("-c", "--channel", default="can0", help="인터페이스 이름 (기본 can0)")
    p_send.add_argument("id", help="Arbitration ID (16진수, 예: 123)")
    p_send.add_argument("data", help="전송 데이터 (16진수, 예: DEADBEEF 또는 'DE AD BE EF')")
    p_send.add_argument("--extended", action="store_true", help="확장 ID(29bit) 사용")
    p_send.add_argument("--no-fd", dest="fd", action="store_false", default=True,
                         help="클래식 CAN 프레임으로 전송 (기본은 FD)")
    p_send.add_argument("--count", type=int, default=1, help="전송 횟수, 0이면 무한 반복 (기본 1)")
    p_send.add_argument("--interval", type=float, default=1.0, help="반복 전송 간격(초, 기본 1.0)")
    p_send.set_defaults(func=cmd_send)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
