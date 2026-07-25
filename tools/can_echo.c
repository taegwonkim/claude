/*
 * can_echo.c - PC에서 보낸 CAN/CAN-FD 프레임을 받아 그대로 되돌려 보내는 에코 서버
 *
 * CM5 IO Board(또는 CAN FD HAT을 쓰는 모든 SocketCAN 보드)에서 실행한다.
 * PC가 PEAK PCAN-USB(FD) 등으로 같은 CAN 버스에 연결되어 프레임을 보내면,
 * 이 프로그램이 받은 프레임을 동일한 인터페이스로 그대로 재전송해서 PC가
 * 자신이 보낸 데이터를 에코로 돌려받을 수 있게 한다.
 *
 * SocketCAN 원시 소켓은 기본적으로 자신이 보낸 프레임을 다시 수신하지 않으므로
 * (CAN_RAW_RECV_OWN_MSGS 미설정) 에코 프레임 때문에 무한 루프가 생기지 않는다.
 *
 * 빌드: gcc -O2 -Wall -o can_echo can_echo.c
 * 사용: ./can_echo -c can0
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
#include <signal.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

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

static void print_frame(const char *tag, const char *ifname, int is_fd, canid_t id,
                         const uint8_t *data, uint8_t len)
{
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char tbuf[16];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tmv);

    printf("%s  %-4s  %s  %s  ID=0x%X  DLC=%u  DATA=[", tbuf, tag, ifname, is_fd ? "FD" : "CC",
           id & CAN_EFF_MASK, len);
    for (uint8_t i = 0; i < len; i++)
        printf("%02X%s", data[i], (i + 1 < len) ? " " : "");
    printf("]\n");
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "사용법: %s -c <iface> [--no-fd]\n"
            "  -c, --channel <iface>  수신/에코에 쓸 인터페이스 (기본 can0)\n"
            "  --no-fd                클래식 CAN 프레임만 처리 (기본은 FD 지원)\n",
            prog);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *ifname = "can0";
    int fd_mode = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--channel") == 0) {
            if (++i >= argc)
                usage(argv[0]);
            ifname = argv[i];
        } else if (strcmp(argv[i], "--no-fd") == 0) {
            fd_mode = 0;
        } else {
            usage(argv[0]);
        }
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    int s = open_can_socket(ifname, fd_mode);
    printf("[%s] CAN 에코 서버 시작 (Ctrl+C 종료)\n", ifname);

    struct canfd_frame frame;
    while (!g_stop) {
        ssize_t nbytes = read(s, &frame, sizeof(frame));
        if (nbytes < 0) {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }

        int is_fd = (nbytes == CANFD_MTU);
        print_frame("RX", ifname, is_fd, frame.can_id, frame.data, frame.len);

        size_t frame_size = is_fd ? (size_t)CANFD_MTU : (size_t)CAN_MTU;
        ssize_t n = write(s, &frame, frame_size);
        if (n != (ssize_t)frame_size) {
            perror("write (echo)");
            continue;
        }
        print_frame("TX", ifname, is_fd, frame.can_id, frame.data, frame.len);
    }

    close(s);
    return 0;
}
