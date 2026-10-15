# PC 에서 파서 단위 시험 :  make -C tests
CC     ?= gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -I../Core/Inc
SRCS    = test_parsers.c ../Core/Src/veh_frame.c ../Core/Src/someip.c

test: test_parsers
	./test_parsers

test_parsers: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f test_parsers

.PHONY: test clean
