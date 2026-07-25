/*
 * can_tool.c - CAN / CAN-FD 송수신 CLI 도구 (Linux SocketCAN)
 *
 * PEAK PCAN-USB(FD) 어댑터를 꽂은 PC와 mcp251xfd 기반 CAN FD HAT을 쓰는 보드
 * 양쪽에서 동일하게 동작한다. PEAK의 Linux 드라이버(peak_usb)도 SocketCAN
 * 인터페이스(can0, can1, ...)로 노출되므로 별도 벤더 SDK 없이 이 프로그램
 * 그대로 빌드해서 쓰면 된다. (Windows에서 PCAN-Basic DLL을 쓰는 경우는
 * 이 코드로 커버되지 않는다.)
 *
 * 빌드:
 *   gcc -O2 -Wall -o can_tool can_tool.c
 *
 * 사용:
 *   ./can_tool recv -c can0
 *   ./can_tool send -c can0 123 DEADBEEF
 *   ./can_tool send -c can0 123 DEADBEEF --count 0 --interval 0.5
 *   ./can_tool send -c can0 123 DEADBEEF --no-fd
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

static int open_can_socket(const char *ifname, int fd_mode)
{
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket");
        exit(1);
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "인터페이스 %s 를 찾을 수 없습니다: %s\n", ifname, strerror(errno));
        exit(1);
    }

    if (fd_mode) {
        int enable = 1;
        if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable, sizeof(enable)) < 0) {
            perror("setsockopt(CAN_RAW_FD_FRAMES)");
            exit(1);
        }
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    return s;
}

static int parse_hex_id(const char *s, canid_t *out)
{
    char *end;
    unsigned long v = strtoul(s, &end, 16);
    if (*end != '\0')
        return -1;
    *out = (canid_t)v;
    return 0;
}

static int parse_hex_data(const char *s, uint8_t *buf, size_t bufsize)
{
    size_t len = 0;
    while (*s) {
        while (*s == ' ')
            s++;
        if (!*s)
            break;
        if (len >= bufsize)
            return -1;
        unsigned int byte;
        if (sscanf(s, "%2x", &byte) != 1)
            return -1;
        buf[len++] = (uint8_t)byte;
        s += 2;
    }
    return (int)len;
}

static void print_frame(const char *ifname, int is_fd, canid_t id, const uint8_t *data, uint8_t len)
{
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char tbuf[16];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tmv);

    printf("%s  %s  %s  ID=0x%X  DLC=%u  DATA=[", tbuf, ifname, is_fd ? "FD" : "CC",
           id & CAN_EFF_MASK, len);
    for (uint8_t i = 0; i < len; i++)
        printf("%02X%s", data[i], (i + 1 < len) ? " " : "");
    printf("]\n");
}

static void cmd_recv(const char *ifname, int fd_mode)
{
    int s = open_can_socket(ifname, fd_mode);
    printf("[%s] 수신 대기 중... (Ctrl+C 종료)\n", ifname);

    struct canfd_frame frame;
    while (1) {
        ssize_t nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("read");
            break;
        }
        int is_fd = (nbytes == CANFD_MTU);
        print_frame(ifname, is_fd, frame.can_id, frame.data, frame.len);
    }
    close(s);
}

static void cmd_send(const char *ifname, int fd_mode, int extended, canid_t id,
                      const uint8_t *data, uint8_t len, int count, double interval)
{
    if (!fd_mode && len > 8) {
        fprintf(stderr, "클래식 CAN 프레임은 데이터가 최대 8바이트입니다 (--fd 없이 %u바이트 지정됨)\n", len);
        exit(1);
    }

    int s = open_can_socket(ifname, fd_mode);

    struct canfd_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = id | (extended ? CAN_EFF_FLAG : 0);
    frame.len = len;
    memcpy(frame.data, data, len);
    if (fd_mode)
        frame.flags = CANFD_BRS;

    size_t frame_size = fd_mode ? CANFD_MTU : CAN_MTU;

    int sent = 0;
    while (count == 0 || sent < count) {
        ssize_t n = write(s, &frame, frame_size);
        if (n != (ssize_t)frame_size) {
            perror("write");
            break;
        }

        printf("전송: %s  ID=0x%X  DATA=[", ifname, frame.can_id & CAN_EFF_MASK);
        for (uint8_t i = 0; i < len; i++)
            printf("%02X%s", data[i], (i + 1 < len) ? " " : "");
        printf("]\n");

        sent++;
        if (count == 0 || sent < count) {
            struct timespec ts;
            ts.tv_sec = (time_t)interval;
            ts.tv_nsec = (long)((interval - (double)ts.tv_sec) * 1e9);
            nanosleep(&ts, NULL);
        }
    }
    close(s);
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "사용법:\n"
            "  %s recv -c <iface> [--no-fd]\n"
            "  %s send -c <iface> <id_hex> <data_hex> [--no-fd] [--extended] [--count N] [--interval SEC]\n"
            "\n예:\n"
            "  %s recv -c can0\n"
            "  %s send -c can0 123 DEADBEEF\n"
            "  %s send -c can0 123 DEADBEEF --count 0 --interval 0.5\n",
            prog, prog, prog, prog, prog);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc < 2)
        usage(argv[0]);

    const char *mode = argv[1];
    const char *ifname = "can0";
    int fd_mode = 1;
    int extended = 0;
    int count = 1;
    double interval = 1.0;

    char *pos_args[2] = {NULL, NULL};
    int pos_count = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--channel") == 0) {
            if (++i >= argc)
                usage(argv[0]);
            ifname = argv[i];
        } else if (strcmp(argv[i], "--no-fd") == 0) {
            fd_mode = 0;
        } else if (strcmp(argv[i], "--extended") == 0) {
            extended = 1;
        } else if (strcmp(argv[i], "--count") == 0) {
            if (++i >= argc)
                usage(argv[0]);
            count = atoi(argv[i]);
        } else if (strcmp(argv[i], "--interval") == 0) {
            if (++i >= argc)
                usage(argv[0]);
            interval = atof(argv[i]);
        } else if (argv[i][0] != '-') {
            if (pos_count < 2)
                pos_args[pos_count++] = argv[i];
        } else {
            fprintf(stderr, "알 수 없는 옵션: %s\n", argv[i]);
            usage(argv[0]);
        }
    }

    if (strcmp(mode, "recv") == 0) {
        cmd_recv(ifname, fd_mode);
    } else if (strcmp(mode, "send") == 0) {
        if (pos_count < 2)
            usage(argv[0]);
        canid_t id;
        if (parse_hex_id(pos_args[0], &id) < 0) {
            fprintf(stderr, "잘못된 ID: %s\n", pos_args[0]);
            return 1;
        }
        uint8_t data[64];
        int len = parse_hex_data(pos_args[1], data, sizeof(data));
        if (len < 0) {
            fprintf(stderr, "잘못된 데이터: %s\n", pos_args[1]);
            return 1;
        }
        cmd_send(ifname, fd_mode, extended, id, data, (uint8_t)len, count, interval);
    } else {
        usage(argv[0]);
    }

    return 0;
}
