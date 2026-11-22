# Host unit tests for the PLC library (no hardware needed)
#   make        build and run
#   make v      run with the library log output
CC      ?= gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -O1 -g -I../PLC -I. \
          -fsanitize=address,undefined -fno-omit-frame-pointer \
          '-DPLC_LOG(...)=test_log(__VA_ARGS__)' -include test_log.h
SRCS    = test_main.c test_frame_crypto.c test_qca7k_spi.c test_slac_sim.c \
          ../PLC/qca7k.c ../PLC/qca7k_frame.c ../PLC/hpgp.c ../PLC/sha256.c \
          ../PLC/slac_common.c ../PLC/slac_evse.c ../PLC/slac_ev.c ../PLC/plc_modem.c

all: run

test_plc: $(SRCS) $(wildcard ../PLC/*.h) *.h
	$(CC) $(CFLAGS) $(SRCS) -o $@

run: test_plc
	./test_plc

v: test_plc
	./test_plc -v

clean:
	rm -f test_plc

.PHONY: all run v clean
