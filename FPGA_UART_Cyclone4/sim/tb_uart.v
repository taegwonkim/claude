// -----------------------------------------------------------------------------
// tb_uart.v  -  검증 테스트벤치 (Icarus Verilog)
//   TEST 1 : uart_tx 연속 송신 시 프레임 간 간격 측정 (기대값 0 비트)
//   TEST 2 : 간격 0 으로 연속 수신 + 에코, 송신측 보레이트 오차 ±ERR_PPM
//   실행 : iverilog -g2005 -o sim.out -DERR_PPM=20000 sim/tb_uart.v rtl/*.v && vvp sim.out
// -----------------------------------------------------------------------------
`timescale 1ns/1ps
`ifndef ERR_PPM
 `define ERR_PPM 0
`endif
module tb_uart;
    localparam integer CLK_HZ = 50_000_000;
    localparam integer BAUD   = 115_200;
    localparam real    TB_NOM = 1.0e9 / BAUD;                       // ns
    localparam real    TB_SND = TB_NOM / (1.0 + `ERR_PPM / 1.0e6);  // 상대 송신기 비트시간
    localparam integer NBYTES = 200;

    reg clk = 0;  always #10 clk = ~clk;   // 50 MHz
    reg rst = 1;
    integer errors = 0;

    // =============== TEST 1 : uart_tx 단독 ===============
    reg  [7:0] t1_data = 8'h00;
    reg        t1_valid = 0;
    wire       t1_ready, t1_txd, t1_busy;
    uart_tx #(.CLK_HZ(CLK_HZ), .BAUD(BAUD)) dut_tx (
        .clk(clk), .rst(rst), .tx_data(t1_data), .tx_valid(t1_valid),
        .tx_ready(t1_ready), .txd(t1_txd), .tx_busy(t1_busy));
    always @(posedge clk) if (t1_valid && t1_ready) t1_data <= t1_data + 8'd1;

    // =============== TEST 2 : uart_top 에코 ===============
    reg  rxd = 1;
    wire txd;
    wire [1:0] led;
    uart_top #(.CLK_HZ(CLK_HZ), .BAUD(BAUD)) dut (
        .CLOCK_50(clk), .RESET_N(~rst), .UART_RXD(rxd), .UART_TXD(txd), .LED(led));

    // ---- 행동 모델 : 1프레임 수신 (공칭 보레이트) ----
    task automatic rx_frame(input reg line_sel, output [7:0] d, output reg ok, output realtime t_start);
        integer i;
        begin
            if (line_sel) @(negedge t1_txd); else @(negedge txd);
            t_start = $realtime;
            #(TB_NOM/2);
            ok = ((line_sel ? t1_txd : txd) == 1'b0);
            for (i = 0; i < 8; i = i + 1) begin
                #(TB_NOM);
                d[i] = line_sel ? t1_txd : txd;
            end
            #(TB_NOM);
            ok = ok & ((line_sel ? t1_txd : txd) == 1'b1);
        end
    endtask

    // ---- 행동 모델 : 1프레임 송신 (보레이트 오차 포함, 간격 0) ----
    task automatic tx_frame(input [7:0] d);
        integer i;
        begin
            rxd = 0; #(TB_SND);
            for (i = 0; i < 8; i = i + 1) begin rxd = d[i]; #(TB_SND); end
            rxd = 1; #(TB_SND);
        end
    endtask

    // ---------------- TEST 1 ----------------
    realtime t_prev, t_now, gap, gap_max;
    reg [7:0] d; reg ok;
    integer k;
    initial begin
        #200 rst = 0;
        #100 @(posedge clk) t1_valid = 1;
        gap_max = 0;
        rx_frame(1, d, ok, t_prev);
        for (k = 1; k < 30; k = k + 1) begin
            rx_frame(1, d, ok, t_now);
            if (!ok || d != k[7:0]) begin
                errors = errors + 1;
                $display("TEST1 FAIL frame %0d data=%02h ok=%0d", k, d, ok);
            end
            gap = (t_now - t_prev) - 10.0 * TB_NOM;   // 10비트 시간 초과분 = 바이트 간 지연
            if (gap > gap_max) gap_max = gap;
            t_prev = t_now;
        end
        t1_valid = 0;
        $display("TEST1 uart_tx back-to-back : max inter-byte gap = %0.1f ns (%0.3f bit), 1 clk = 20 ns",
                 gap_max, gap_max / TB_NOM);
        if (gap_max > 25.0) begin
            errors = errors + 1;
            $display("TEST1 FAIL : gap too large");
        end
    end

    // ---------------- TEST 2 ----------------
    reg [7:0] sent [0:NBYTES-1];
    integer n, m;
    initial begin
        for (n = 0; n < NBYTES; n = n + 1) sent[n] = $random;
        #2000;
        for (n = 0; n < NBYTES; n = n + 1) tx_frame(sent[n]);   // 간격 0, 연속 송신
    end
    realtime ts;
    initial begin
        #1000;
        for (m = 0; m < NBYTES; m = m + 1) begin
            rx_frame(0, d, ok, ts);
            if (!ok || d !== sent[m]) begin
                errors = errors + 1;
                if (errors < 10) $display("TEST2 FAIL byte %0d : got %02h exp %02h ok=%0d", m, d, sent[m], ok);
            end
        end
        $display("TEST2 echo %0d bytes, sender baud error %0d ppm : LED(ferr,ovf)=%b", NBYTES, `ERR_PPM, led);
        if (led != 2'b00) errors = errors + 1;
        if (errors == 0) $display("RESULT: PASS");
        else             $display("RESULT: FAIL (%0d errors)", errors);
        $finish;
    end

    initial begin #(NBYTES * 12 * TB_NOM * 1.2 + 5e6); $display("RESULT: TIMEOUT"); $finish; end
endmodule
