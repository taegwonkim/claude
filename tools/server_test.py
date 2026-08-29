#!/usr/bin/env python3
"""
server_test.py

STM32(ESP32-C3)가 접속할 서버 PC 쪽 테스트용 TCP 서버.
main.c 예제의 ESP32_SetServer("192.168.0.10", 8000) 과 짝을 맞춰 사용한다.

사용법:
    python3 server_test.py            # 0.0.0.0:8000 에서 대기
    python3 server_test.py 9000       # 포트 지정

동작:
    - 클라이언트(ESP32) 접속을 받으면 1초마다 "ping <n>\n" 을 보내고,
    - 클라이언트가 보낸 데이터를 콘솔에 그대로 출력한다.
    - 연결이 끊기면 다시 accept 대기 상태로 돌아가
      STM32 쪽 재접속 테스트(Wi-Fi/서버 재접속)를 반복해서 확인할 수 있다.
"""
import socket
import sys
import threading
import time

HOST = "0.0.0.0"
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000


def handle_client(conn: socket.socket, addr) -> None:
    print(f"[+] 클라이언트 접속: {addr}")
    conn.settimeout(1.0)
    counter = 0
    last_ping = time.monotonic()

    try:
        while True:
            now = time.monotonic()
            if now - last_ping >= 1.0:
                msg = f"ping {counter}\n".encode()
                conn.sendall(msg)
                counter += 1
                last_ping = now

            try:
                data = conn.recv(1024)
            except socket.timeout:
                continue

            if not data:
                print(f"[-] 클라이언트 연결 종료: {addr}")
                break

            print(f"[{addr}] 수신: {data!r}")
    except (ConnectionResetError, BrokenPipeError):
        print(f"[-] 클라이언트 강제 종료: {addr}")
    finally:
        conn.close()


def main() -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((HOST, PORT))
        srv.listen(1)
        print(f"[*] 대기 중: {HOST}:{PORT}")

        while True:
            conn, addr = srv.accept()
            t = threading.Thread(target=handle_client, args=(conn, addr), daemon=True)
            t.start()


if __name__ == "__main__":
    main()
