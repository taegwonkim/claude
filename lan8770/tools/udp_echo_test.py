#!/usr/bin/env python3
"""
STM32H7 + LAN8770 UDP 에코 테스트

PC ── (100BASE-TX) ── 미디어 컨버터 ── (100BASE-T1) ── LAN8770 EVB ── STM32H7

사용법:
    python udp_echo_test.py                      # 기본 192.168.10.10:7
    python udp_echo_test.py 192.168.10.10 -n 1000 -s 1024
"""
import argparse
import os
import socket
import time


def main() -> None:
    ap = argparse.ArgumentParser(description="UDP echo test for STM32H7 + LAN8770")
    ap.add_argument("host", nargs="?", default="192.168.10.10")
    ap.add_argument("-p", "--port", type=int, default=7)
    ap.add_argument("-n", "--count", type=int, default=100, help="보낼 패킷 수")
    ap.add_argument("-s", "--size", type=int, default=512, help="페이로드 크기 (최대 1472)")
    ap.add_argument("-t", "--timeout", type=float, default=0.5, help="응답 대기 [s]")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(args.timeout)

    ok = lost = bad = 0
    rtts = []
    for i in range(args.count):
        payload = i.to_bytes(4, "big") + os.urandom(max(0, args.size - 4))
        t0 = time.perf_counter()
        sock.sendto(payload, (args.host, args.port))
        try:
            data, _ = sock.recvfrom(2048)
        except socket.timeout:
            lost += 1
            continue
        rtt = (time.perf_counter() - t0) * 1000.0
        if data == payload:
            ok += 1
            rtts.append(rtt)
        else:
            bad += 1

    print(f"target {args.host}:{args.port}  size={args.size}B  count={args.count}")
    print(f"  ok={ok}  lost={lost}  mismatch={bad}")
    if rtts:
        print(f"  RTT min/avg/max = {min(rtts):.3f} / {sum(rtts)/len(rtts):.3f} / {max(rtts):.3f} ms")


if __name__ == "__main__":
    main()
