# fifo_uart_hw.tcl - Platform Designer(Qsys) 컴포넌트 정의
# 사용: 이 파일과 rtl/*.v 를 프로젝트 폴더(또는 IP 검색 경로)에 두면
#       IP Catalog > Interface Protocols > Serial > "FIFO UART (8N1)" 로 나타난다.
package require -exact qsys 13.1

set_module_property NAME          fifo_uart
set_module_property VERSION       1.0
set_module_property DISPLAY_NAME  "FIFO UART (8N1)"
set_module_property GROUP         "Interface Protocols/Serial"
set_module_property DESCRIPTION   "8-N-1 UART with RX/TX FIFOs, zero inter-byte gap"
set_module_property EDITABLE      true
set_module_property INTERNAL      false

add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL fifo_uart_avalon
add_fileset_file fifo_uart_avalon.v VERILOG PATH fifo_uart_avalon.v TOP_LEVEL_FILE
add_fileset_file uart_rx.v          VERILOG PATH uart_rx.v
add_fileset_file uart_tx.v          VERILOG PATH uart_tx.v
add_fileset_file uart_fifo.v        VERILOG PATH uart_fifo.v

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG TOP_LEVEL fifo_uart_avalon
add_fileset_file fifo_uart_avalon.v VERILOG PATH fifo_uart_avalon.v
add_fileset_file uart_rx.v          VERILOG PATH uart_rx.v
add_fileset_file uart_tx.v          VERILOG PATH uart_tx.v
add_fileset_file uart_fifo.v        VERILOG PATH uart_fifo.v

# ---- 파라미터 ----
add_parameter CLK_HZ INTEGER 50000000
set_parameter_property CLK_HZ DISPLAY_NAME "Clock frequency (Hz)"
set_parameter_property CLK_HZ SYSTEM_INFO {CLOCK_RATE clock}
set_parameter_property CLK_HZ HDL_PARAMETER true
set_parameter_property CLK_HZ AFFECTS_GENERATION false

add_parameter BAUD INTEGER 115200
set_parameter_property BAUD DISPLAY_NAME "Baud rate"
set_parameter_property BAUD HDL_PARAMETER true

add_parameter FIFO_AW INTEGER 8
set_parameter_property FIFO_AW DISPLAY_NAME "FIFO depth (2^N bytes)"
set_parameter_property FIFO_AW ALLOWED_RANGES {4:12}
set_parameter_property FIFO_AW HDL_PARAMETER true

# ---- clock / reset ----
add_interface clock clock end
set_interface_property clock clockRate 0
add_interface_port clock clk clk Input 1

add_interface reset reset end
set_interface_property reset associatedClock clock
set_interface_property reset synchronousEdges BOTH
add_interface_port reset reset reset Input 1

# ---- Avalon-MM slave ----
add_interface s1 avalon end
set_interface_property s1 associatedClock      clock
set_interface_property s1 associatedReset      reset
set_interface_property s1 addressUnits         WORDS
set_interface_property s1 readLatency          1
set_interface_property s1 readWaitTime         0
set_interface_property s1 writeWaitTime        0
set_interface_property s1 maximumPendingReadTransactions 0
add_interface_port s1 address   address   Input  2
add_interface_port s1 read      read      Input  1
add_interface_port s1 write     write     Input  1
add_interface_port s1 writedata writedata Input  32
add_interface_port s1 readdata  readdata  Output 32

# ---- IRQ ----
add_interface irq interrupt end
set_interface_property irq associatedAddressablePoint s1
set_interface_property irq associatedClock clock
set_interface_property irq associatedReset reset
add_interface_port irq irq irq Output 1

# ---- 외부 핀 (기존 altera_avalon_uart 와 같은 이름 → top.v 수정 불필요) ----
add_interface external_connection conduit end
add_interface_port external_connection rxd rxd Input  1
add_interface_port external_connection txd txd Output 1
