/*
 * cc.h - lwIP compiler/platform abstraction for this project
 * (bare metal STM32H7 with arm-none-eabi-gcc, and the host unit tests)
 */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define LWIP_PROVIDE_ERRNO

#define LWIP_TIMEVAL_PRIVATE 0
#include <sys/time.h>

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__ ((__packed__))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#define LWIP_PLATFORM_DIAG(x)   do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { printf("lwIP assert \"%s\" %s:%d\r\n", x, __FILE__, __LINE__); } while (0)

/* random numbers for TCP ISN / ports: hardware RNG on the target */
uint32_t v2g_rand32(void);
#define LWIP_RAND() ((u32_t)v2g_rand32())

typedef int sys_prot_t;

#endif /* LWIP_ARCH_CC_H */
