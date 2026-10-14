# uart_top.sdc - Cyclone IV 타이밍 제약
create_clock -name CLOCK_50 -period 20.000 [get_ports {CLOCK_50}]
derive_clock_uncertainty

# UART 핀은 비동기 신호 → 2단 동기화 FF 로 처리하므로 타이밍 분석 제외
set_false_path -from [get_ports {UART_RXD}]
set_false_path -to   [get_ports {UART_TXD}]
set_false_path -from [get_ports {RESET_N}]
set_false_path -to   [get_ports {LED[*]}]
