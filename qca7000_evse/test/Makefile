# Host tests (PC): HAL/FreeRTOS are stubbed. Requires gcc and libcrypto (OpenSSL) for SHA-256.
CFLAGS = -std=c11 -Wall -Wextra -Istubs -I../src

all: test_slac test_qca
	./test_slac
	./test_qca

test_slac: test_slac.c ../src/slac_evse.c
	$(CC) $(CFLAGS) -Wno-deprecated-declarations $< -o $@ -lcrypto

test_qca: test_qca.c ../src/qca7000.c
	$(CC) $(CFLAGS) -DQCA7000_DMA_SECTION= $< -o $@

clean:
	rm -f test_slac test_qca
