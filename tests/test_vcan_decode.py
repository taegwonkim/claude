"""C 인코더(vcan_proto.c)가 만든 벡터를 PC 도구(vnet_can_monitor.py)로 해석해서 값 비교"""
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import vnet_can_monitor as m  # noqa: E402


def load():
    out = {}
    with open(os.path.join(HERE, "vcan_vectors.txt")) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            name, off, hexdata = line.split()
            out[name] = m.decode(int(off, 16), bytes.fromhex(hexdata))
    return out


class DecodeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.v = load()

    def test_status(self):
        name, r = self.v["STATUS"]
        self.assertEqual(name, "STATUS")
        self.assertTrue(r["link_up"] and r["phy_ready"] and r["someip_on"] and not r["vlan_on"])
        self.assertEqual((r["role"], r["sqi"], r["uptime_ms"], r["ecu_count"], r["service_count"]),
                         ("MASTER", 6, 123456, 3, 2))
        self.assertEqual((r["rx_frames"], r["tx_frames"], r["rx_lost"], r["someip_messages"], r["ip"]),
                         (1000, 20, 7, 500, "192.168.10.10"))

    def test_eth_stats(self):
        _, r = self.v["ETH_STATS"]
        self.assertEqual([r[k] for k in m.ETH_FIELDS], list(range(100, 112)))

    def test_ecu(self):
        _, r = self.v["ECU"]
        self.assertEqual((r["ip"], r["mac"], r["vid"], r["packets"], r["bytes"]),
                         ("10.0.0.5", "02:1A:00:00:00:01", 2, 5, 300))
        self.assertEqual((r["last_age_ms"], r["n_flows"], r["n_events"], r["offers"]), (10, 1, 1, [(0x1001, 2)]))
        self.assertFalse(r["mac_changed"] or r["idle"])

    def test_flow(self):
        _, r = self.v["FLOW"]
        self.assertEqual((r["ecu_ip"], r["proto"], r["sport"], r["dport"], r["count"], r["avg_us"]),
                         ("10.0.0.5", 17, 30501, 30502, 5, 10000))

    def test_event(self):
        _, r = self.v["EVENT"]
        self.assertEqual((r["service"], r["method"], r["msg_type"], r["count"], r["last_len"]),
                         (0x1001, 0x8001, 0x02, 5, 10))
        self.assertEqual((r["avg_us"], r["min_us"], r["max_us"]), (10000, 10000, 10000))
        self.assertEqual(r["payload"], bytes([0xDE, 0xAD, 0xBE, 0xEF, 5, 6, 7, 8]))

    def test_service(self):
        _, r = self.v["SERVICE"]
        self.assertEqual((r["service"], r["instance"], r["major"], r["minor"], r["ttl"]), (0x1234, 1, 2, 7, 0xFFFFFF))
        self.assertEqual((r["ep_ip"], r["ep_port"], r["l4_proto"], r["from_ip"], r["offers"]),
                         ("10.0.0.5", 30501, 0x11, "10.0.0.5", 42))

    def test_dump_end_response(self):
        self.assertEqual(self.v["DUMP_END"][1], {"records": 9, "tx_dropped": 1})
        r = self.v["RESPONSE"][1]
        self.assertEqual((r["cmd"], r["result"], r["fw_version"], r["uptime_ms"], r["value"]), (1, 0, 0x0100, 5000, 77))

    def test_format_all(self):
        for name, rec in self.v.values():
            self.assertTrue(m.format_record(name, rec))

    def test_command_bytes(self):
        self.assertEqual(m.command_bytes(m.CMD_STATUS_MS, 500), bytes([4, 0xF4, 0x01, 0, 0, 0, 0, 0]))
        self.assertEqual(m.command_bytes(m.CMD_DUMP_MS, 20000)[:5], bytes([5, 0x20, 0x4E, 0, 0]))
        self.assertEqual(len(m.command_bytes(m.CMD_PING)), 8)


if __name__ == "__main__":
    unittest.main()
