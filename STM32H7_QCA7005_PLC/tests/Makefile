# Host unit tests (no hardware needed)
#   make        build and run all tests
#   make v      run with the library log output
#
#   test_plc : QCA7005 SPI driver (simulated slave), framing, SLAC EVSE <-> EV
#   test_v2g : EXI codec, SDP, V2GTP, complete DIN 70121 session between two
#              processes, each with its own lwIP (IPv6/TCP) instance
CC      ?= gcc
SAN      = -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS   = -std=c11 -Wall -Wextra -Werror -O1 -g -I../PLC -I. $(SAN) \
           '-DPLC_LOG(...)=test_log(__VA_ARGS__)' -include test_log.h
SRCS     = test_main.c test_frame_crypto.c test_qca7k_spi.c test_slac_sim.c \
           ../PLC/qca7k.c ../PLC/qca7k_frame.c ../PLC/hpgp.c ../PLC/sha256.c \
           ../PLC/slac_common.c ../PLC/slac_evse.c ../PLC/slac_ev.c ../PLC/plc_modem.c

# ---- V2G stack ------------------------------------------------------------------
LWIP     = ../Middlewares/Third_Party/LwIP
CBV2G    = ../Middlewares/Third_Party/cbv2g
V2G_INC  = -I../V2G -I../PLC -I. -I$(LWIP)/src/include -I$(LWIP)/system -I$(CBV2G)/include
V2G_DEFS = '-DPLC_LOG(...)=test_log(__VA_ARGS__)' -include test_log.h
V2G_SRCS = $(wildcard ../V2G/*.c)
LIB_SRCS = $(wildcard $(LWIP)/src/core/*.c) $(wildcard $(LWIP)/src/core/ipv6/*.c) \
           $(LWIP)/src/netif/ethernet.c \
           $(wildcard $(CBV2G)/lib/common/*.c) $(wildcard $(CBV2G)/lib/din/*.c) \
           $(wildcard $(CBV2G)/lib/app_handshake/*.c) $(CBV2G)/lib/exi_v2gtp.c
LIB_OBJS = $(patsubst ../%.c,obj/%.o,$(LIB_SRCS))

all: run

test_plc: $(SRCS) $(wildcard ../PLC/*.h) *.h
	$(CC) $(CFLAGS) $(SRCS) -o $@

# third-party code: built once, without -Werror
obj/%.o: ../%.c
	@mkdir -p $(dir $@)
	$(CC) -std=gnu11 -O1 -g $(SAN) $(V2G_INC) $(V2G_DEFS) -w -c $< -o $@

test_v2g: test_v2g.c $(V2G_SRCS) $(wildcard ../V2G/*.h) $(LIB_OBJS)
	$(CC) -std=gnu11 -Wall -Wextra -Werror -Wno-missing-field-initializers -O1 -g $(SAN) \
	    $(V2G_INC) $(V2G_DEFS) test_v2g.c $(V2G_SRCS) $(LIB_OBJS) -lm -o $@

run: test_plc test_v2g
	./test_plc
	./test_v2g

v: test_plc test_v2g
	./test_plc -v
	./test_v2g -v

clean:
	rm -rf test_plc test_v2g obj

.PHONY: all run v clean
