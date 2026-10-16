# PC 에서 파서 단위 시험 :  make -C tests
CC     ?= gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -O1 -fsanitize=address,undefined -I../Core/Inc -I../Drivers/BSP/Components/lan8770
SRCS    = test_parsers.c ../Core/Src/veh_frame.c ../Core/Src/someip.c ../Core/Src/veh_ecu.c ../Core/Src/vcan_proto.c

test: test_parsers
	./test_parsers > test_output.txt; status=$$?; tail -1 test_output.txt; grep FAIL test_output.txt; exit $$status
	python3 -m unittest -q test_vcan_decode

test_parsers: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f test_parsers test_output.txt vcan_vectors.txt
	rm -rf __pycache__

.PHONY: test clean
