/*
 * lwipopts.h - lwIP 2.1.2 configuration for ISO 15118 / DIN 70121 over HomePlug GP
 *
 * IPv6 only (link-local, no router), UDP for SDP, TCP for V2GTP.
 * NO_SYS = 1: raw API from the main loop, no RTOS.
 */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0   /* everything runs in the main loop */
#define LWIP_NETCONN                    0
#define LWIP_SOCKET                     0
#define LWIP_SINGLE_NETIF               1   /* one PLC interface, no address zones */

/* ---- memory ------------------------------------------------------------------ */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16 * 1024)
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                4
#define MEMP_NUM_TCP_PCB                4
#define MEMP_NUM_TCP_PCB_LISTEN         2
#define MEMP_NUM_TCP_SEG                24
#define MEMP_NUM_SYS_TIMEOUT            8
#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               1536

/* ---- protocols --------------------------------------------------------------- */
#define LWIP_IPV4                       0
#define LWIP_IPV6                       1
#define LWIP_ARP                        0
#define LWIP_ETHERNET                   1
#define LWIP_ICMP6                      1
#define LWIP_IPV6_MLD                   1
#define LWIP_IPV6_AUTOCONFIG            0   /* link-local only, no router */
#define LWIP_IPV6_SEND_ROUTER_SOLICIT   0
#define LWIP_IPV6_DHCP6                 0
#define LWIP_IPV6_FRAG                  0
#define LWIP_IPV6_REASS                 0
#define LWIP_IPV6_DUP_DETECT_ATTEMPTS   1   /* DAD ~1 s before the address is usable */
#define LWIP_IPV6_NUM_ADDRESSES         2
#define LWIP_ND6_QUEUEING               1
#define LWIP_RAW                        0
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_DNS                        0
#define LWIP_IGMP                       0
#define LWIP_DHCP                       0
#define LWIP_AUTOIP                     0

/* ---- TCP --------------------------------------------------------------------- */
#define TCP_MSS                         1220            /* IPv6 minimum MTU 1280 - 60 */
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                ((4 * TCP_SND_BUF) / TCP_MSS)
#define LWIP_TCP_KEEPALIVE              0
#define TCP_QUEUE_OOSEQ                 0

/* ---- netif ------------------------------------------------------------------- */
#define LWIP_NETIF_STATUS_CALLBACK      0
#define LWIP_NETIF_LINK_CALLBACK        0
#define LWIP_NETIF_LOOPBACK             0
#define LWIP_HAVE_LOOPIF                0
#define ETH_PAD_SIZE                    0

/* ---- checksums in software ----------------------------------------------------- */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_GEN_ICMP6              1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_CHECK_ICMP6            1

/* ---- statistics / debug -------------------------------------------------------- */
#define LWIP_STATS                      0
#define LWIP_DEBUG                      0

#endif /* LWIPOPTS_H */
