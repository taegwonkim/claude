#!/usr/bin/env python3
"""
STM32H7 + LAN8770 PC 링크 모니터 (CAN FD)

장치가 FDCAN1 으로 보내는 STATUS / ETH_STATS / ECU / FLOW / EVENT / SERVICE 프레임을
해석해서 보여주고, 명령(PING, DUMP, RESET, 주기 변경)을 보냅니다.

필요 : pip install python-can   (+ 어댑터 드라이버)

예)
  # Linux SocketCAN (candleLight FD, PCAN 등) : 먼저 인터페이스 설정
  #   sudo ip link set can0 type can bitrate 500000 sample-point 0.8 \
  #        dbitrate 2000000 dsample-point 0.8 fd on && sudo ip link set can0 up
  python vnet_can_monitor.py -i socketcan -c can0 --dump

  # Windows PEAK PCAN-USB FD
  python vnet_can_monitor.py -i pcan -c PCAN_USBBUS1 --ping --dump

  # 어댑터 없이 해석기만 확인
  python vnet_can_monitor.py --selftest ../tests/vcan_vectors.txt
"""
import argparse
import struct
import sys
import time

# ----------------------------------------------------------------------------
#  프로토콜 정의 (Core/Inc/vcan_proto.h 와 같아야 함)
# ----------------------------------------------------------------------------
ID_STATUS, ID_ETH_STATS = 0x00, 0x01
ID_ECU, ID_EVENT, ID_FLOW, ID_SERVICE, ID_DUMP_END = 0x10, 0x11, 0x12, 0x13, 0x14
ID_COMMAND, ID_RESPONSE = 0x40, 0x41

CMD_PING, CMD_DUMP, CMD_RESET, CMD_STATUS_MS, CMD_DUMP_MS = 0x01, 0x02, 0x03, 0x04, 0x05
CMD_NAMES = {CMD_PING: "PING", CMD_DUMP: "DUMP", CMD_RESET: "RESET_STATS",
             CMD_STATUS_MS: "SET_STATUS_MS", CMD_DUMP_MS: "SET_DUMP_MS"}
RES_NAMES = {0: "OK", 1: "UNKNOWN_CMD", 2: "BAD_ARG"}

ETH_FIELDS = ["rx_frames", "tx_frames", "rx_dma_missed", "rx_fifo_overflow", "rx_fifo_missed",
              "rx_pool_empty", "rx_crc_errors", "rx_align_errors", "tx_errors",
              "mon_frames", "mon_tagged", "vlan_dropped"]

SOMEIP_MT = {0x00: "REQUEST", 0x01: "REQUEST_NO_RETURN", 0x02: "NOTIFICATION",
             0x80: "RESPONSE", 0x81: "ERROR"}
IP_PROTO = {1: "ICMP", 2: "IGMP", 6: "TCP", 17: "UDP"}


def ip_str(b):
    return "%d.%d.%d.%d" % (b[0], b[1], b[2], b[3])


def mac_str(b):
    return ":".join("%02X" % x for x in b)


def vid_str(v):
    return "untagged" if v == 0xFFFF else "VID %d" % v


def ms_str(us):
    return "%.1fms" % (us / 1000.0)


# ----------------------------------------------------------------------------
#  해석 (각 함수는 dict 반환)
# ----------------------------------------------------------------------------
def decode_status(d):
    (ver, flags, role, sqi, uptime, ecu_cnt, svc_cnt,
     rx, tx, lost, someip) = struct.unpack_from("<BBBBIHHIIII", d, 0)
    return dict(version=ver, link_up=bool(flags & 1), phy_ready=bool(flags & 2),
                vlan_on=bool(flags & 4), someip_on=bool(flags & 8), cnt_overflow=bool(flags & 0x10),
                role={0: "SLAVE", 1: "MASTER"}.get(role, "?"), sqi=None if sqi == 0xFF else sqi,
                uptime_ms=uptime, ecu_count=ecu_cnt, service_count=svc_cnt,
                rx_frames=rx, tx_frames=tx, rx_lost=lost, someip_messages=someip,
                ip=ip_str(d[28:32]))


def decode_eth_stats(d):
    return dict(zip(ETH_FIELDS, struct.unpack_from("<12I", d, 0)))


def decode_ecu(d):
    index, total = d[0], d[1]
    vid, flags, n_off = struct.unpack_from("<HBB", d, 12)
    packets, nbytes, age = struct.unpack_from("<III", d, 16)
    offers = []
    for i in range(min(n_off, 4)):
        svc, inst = struct.unpack_from("<HH", d, 32 + i * 4)
        offers.append((svc, inst))
    return dict(index=index, total=total, ip=ip_str(d[2:6]), mac=mac_str(d[6:12]), vid=vid,
                mac_changed=bool(flags & 1), idle=bool(flags & 2), num_offers=n_off,
                packets=packets, bytes=nbytes, last_age_ms=age, n_flows=d[28], n_events=d[29],
                flow_overflow=d[30], event_overflow=d[31], offers=offers)


def decode_event(d):
    svc, meth, mtype, n = struct.unpack_from("<HHBB", d, 4)
    count, avg, mn, mx, last_len = struct.unpack_from("<IIIII", d, 12)
    return dict(ecu_ip=ip_str(d[0:4]), service=svc, method=meth, msg_type=mtype,
                count=count, avg_us=avg, min_us=mn, max_us=mx, last_len=last_len,
                payload=bytes(d[32:32 + n]))


def decode_flow(d):
    proto = d[4]
    sport, dport = struct.unpack_from("<HH", d, 6)
    count, avg, mx = struct.unpack_from("<III", d, 12)
    return dict(ecu_ip=ip_str(d[0:4]), proto=proto, sport=sport, dport=dport,
                count=count, avg_us=avg, max_us=mx)


def decode_service(d):
    svc, inst, major, proto = struct.unpack_from("<HHBB", d, 0)
    minor, ttl = struct.unpack_from("<II", d, 8)
    port, = struct.unpack_from("<H", d, 20)
    offers, = struct.unpack_from("<I", d, 28)
    return dict(service=svc, instance=inst, major=major, l4_proto=proto, minor=minor, ttl=ttl,
                ep_ip=ip_str(d[16:20]), ep_port=port, from_ip=ip_str(d[24:28]), offers=offers)


def decode_dump_end(d):
    records, dropped = struct.unpack_from("<HH", d, 0)
    return dict(records=records, tx_dropped=dropped)


def decode_response(d):
    cmd, res, ver = d[0], d[1], d[2]
    fw, = struct.unpack_from("<H", d, 4)
    uptime, value = struct.unpack_from("<II", d, 8)
    return dict(cmd=cmd, result=res, proto_version=ver, fw_version=fw, uptime_ms=uptime, value=value)


DECODERS = {ID_STATUS: ("STATUS", decode_status), ID_ETH_STATS: ("ETH_STATS", decode_eth_stats),
            ID_ECU: ("ECU", decode_ecu), ID_EVENT: ("EVENT", decode_event),
            ID_FLOW: ("FLOW", decode_flow), ID_SERVICE: ("SERVICE", decode_service),
            ID_DUMP_END: ("DUMP_END", decode_dump_end), ID_RESPONSE: ("RESPONSE", decode_response)}


def decode(offset, data):
    """(이름, dict) 또는 None"""
    if offset not in DECODERS:
        return None
    name, fn = DECODERS[offset]
    return name, fn(bytes(data))


# ----------------------------------------------------------------------------
#  출력
# ----------------------------------------------------------------------------
def format_record(name, r):
    if name == "STATUS":
        return ("[STATUS] link %s  %s  SQI %s  IP %s  up %.1fs  ECU %d  SVC %d  rx %d tx %d lost %d  someip %d%s"
                % ("UP" if r["link_up"] else "DOWN", r["role"],
                   "-" if r["sqi"] is None else "%d/7" % r["sqi"], r["ip"], r["uptime_ms"] / 1000.0,
                   r["ecu_count"], r["service_count"], r["rx_frames"], r["tx_frames"], r["rx_lost"],
                   r["someip_messages"], "" if r["phy_ready"] else "  (PHY not found)"))
    if name == "ETH_STATS":
        return ("[ETH   ] rx %(rx_frames)d tx %(tx_frames)d | lost: no-desc %(rx_dma_missed)d "
                "fifo-ovf %(rx_fifo_overflow)d fifo-miss %(rx_fifo_missed)d pool-empty %(rx_pool_empty)d "
                "| crc %(rx_crc_errors)d align %(rx_align_errors)d tx-err %(tx_errors)d "
                "| mon %(mon_frames)d tagged %(mon_tagged)d vlan-drop %(vlan_dropped)d" % r)
    if name == "ECU":
        offers = " ".join("0x%04X/0x%04X" % o for o in r["offers"])
        return ("[ECU %d/%d] %s  %s%s  %s  pkts %d  bytes %d  last %.1fs%s  flows %d events %d%s"
                % (r["index"] + 1, r["total"], r["ip"], r["mac"], "(changed)" if r["mac_changed"] else "",
                   vid_str(r["vid"]), r["packets"], r["bytes"], r["last_age_ms"] / 1000.0,
                   " (idle)" if r["idle"] else "", r["n_flows"], r["n_events"],
                   ("  offers " + offers) if offers else ""))
    if name == "FLOW":
        return ("    flow  %-4s %5d->%-5d count %d  period %s  max %s"
                % (IP_PROTO.get(r["proto"], str(r["proto"])), r["sport"], r["dport"], r["count"],
                   ms_str(r["avg_us"]), ms_str(r["max_us"])))
    if name == "EVENT":
        kind = "evt" if r["method"] & 0x8000 else "mth"
        return ("    %s   0x%04X/0x%04X %-12s count %d  period %s (%s..%s)  len %d | %s"
                % (kind, r["service"], r["method"], SOMEIP_MT.get(r["msg_type"], "0x%02X" % r["msg_type"]),
                   r["count"], ms_str(r["avg_us"]), ms_str(r["min_us"]), ms_str(r["max_us"]),
                   r["last_len"], r["payload"].hex(" ").upper()))
    if name == "SERVICE":
        ep = "%s %s:%d" % (IP_PROTO.get(r["l4_proto"], "-"), r["ep_ip"], r["ep_port"]) if r["l4_proto"] else "-"
        ttl = "inf" if r["ttl"] == 0xFFFFFF else "%ds" % r["ttl"]
        return ("[SVC   ] 0x%04X/0x%04X v%d.%d  %s  from %s  ttl %s  offers %d"
                % (r["service"], r["instance"], r["major"], r["minor"], ep, r["from_ip"], ttl, r["offers"]))
    if name == "DUMP_END":
        return "[DUMP  ] end : %(records)d records, %(tx_dropped)d frames dropped on device" % r
    if name == "RESPONSE":
        return ("[RESP  ] %s -> %s  (proto v%d, fw %d.%d, uptime %.1fs, value %d)"
                % (CMD_NAMES.get(r["cmd"], "0x%02X" % r["cmd"]), RES_NAMES.get(r["result"], r["result"]),
                   r["proto_version"], r["fw_version"] >> 8, r["fw_version"] & 0xFF,
                   r["uptime_ms"] / 1000.0, r["value"]))
    return "%s %r" % (name, r)


# ----------------------------------------------------------------------------
#  명령
# ----------------------------------------------------------------------------
def command_bytes(cmd, value=None):
    data = bytes([cmd])
    if cmd == CMD_STATUS_MS:
        data += struct.pack("<H", value)
    elif cmd == CMD_DUMP_MS:
        data += struct.pack("<I", value)
    return data.ljust(8, b"\x00")          # CAN FD 유효 길이 8


def send_command(bus, base, cmd, value=None):
    import can
    msg = can.Message(arbitration_id=base + ID_COMMAND, data=command_bytes(cmd, value),
                      is_extended_id=False, is_fd=True, bitrate_switch=True)
    bus.send(msg)


def open_bus(args):
    import can
    if args.interface in ("socketcan", "virtual"):
        return can.Bus(interface=args.interface, channel=args.channel, fd=True)
    timing = can.BitTimingFd.from_sample_point(
        f_clock=args.f_clock, nom_bitrate=args.bitrate, nom_sample_point=80.0,
        data_bitrate=args.data_bitrate, data_sample_point=80.0)
    return can.Bus(interface=args.interface, channel=args.channel, fd=True, timing=timing)


def selftest(path):
    """C 단위 시험이 만든 벡터 파일 해석 (어댑터 없이)"""
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            name, off, hexdata = line.split()
            res = decode(int(off, 16), bytes.fromhex(hexdata))
            print(format_record(*res) if res else "%s: unknown" % name)


def main():
    ap = argparse.ArgumentParser(description="STM32H7 + LAN8770 CAN FD PC link monitor")
    ap.add_argument("-i", "--interface", default="socketcan",
                    help="python-can interface (socketcan, pcan, kvaser, vector, ...)")
    ap.add_argument("-c", "--channel", default="can0")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--data-bitrate", type=int, default=2000000)
    ap.add_argument("--f-clock", type=int, default=80000000, help="어댑터 CAN 클럭 (PCAN/Kvaser FD = 80 MHz)")
    ap.add_argument("--base", type=lambda x: int(x, 0), default=0x700)
    ap.add_argument("--ping", action="store_true")
    ap.add_argument("--dump", action="store_true", help="ECU/서비스 전체 즉시 요청")
    ap.add_argument("--reset", action="store_true", help="장치 통계 초기화")
    ap.add_argument("--status-ms", type=int, help="STATUS 주기 변경 (0 = 끔)")
    ap.add_argument("--dump-ms", type=int, help="자동 덤프 주기 변경 (0 = 끔)")
    ap.add_argument("--quiet-status", action="store_true", help="STATUS/ETH 주기 프레임은 숨김")
    ap.add_argument("--selftest", metavar="VECTORS", help="벡터 파일 해석만 하고 종료")
    args = ap.parse_args()

    if args.selftest:
        selftest(args.selftest)
        return

    bus = open_bus(args)
    try:
        if args.ping:
            send_command(bus, args.base, CMD_PING)
        if args.reset:
            send_command(bus, args.base, CMD_RESET)
        if args.status_ms is not None:
            send_command(bus, args.base, CMD_STATUS_MS, args.status_ms)
        if args.dump_ms is not None:
            send_command(bus, args.base, CMD_DUMP_MS, args.dump_ms)
        if args.dump:
            send_command(bus, args.base, CMD_DUMP)

        print("listening on %s/%s (base 0x%03X) ... Ctrl+C to stop" % (args.interface, args.channel, args.base))
        while True:
            msg = bus.recv(1.0)
            if msg is None or msg.is_extended_id:
                continue
            off = msg.arbitration_id - args.base
            res = decode(off, msg.data) if 0 <= off < 0x80 else None
            if res is None:
                continue
            if args.quiet_status and res[0] in ("STATUS", "ETH_STATS"):
                continue
            print("%s %s" % (time.strftime("%H:%M:%S"), format_record(*res)))
    except KeyboardInterrupt:
        pass
    finally:
        bus.shutdown()


if __name__ == "__main__":
    sys.exit(main())
