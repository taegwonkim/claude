// tb_fifo_uart_avalon.v - Avalon FIFO UART 루프백 테스트
//   CPU 가 64바이트를 한 번에 TX FIFO 에 넣고, 수신 바이트를 "늦게"(전송이 다 끝난 뒤) 읽어도
//   하나도 잃지 않는지 확인 (altera_avalon_uart 는 1바이트 버퍼라 여기서 63바이트 손실)
`timescale 1ns/1ps
module tb_fifo_uart_avalon;
    reg clk = 0; always #10 clk = ~clk;
    reg reset = 1;
    reg  [1:0]  address = 0;
    reg         read = 0, write = 0;
    reg  [31:0] writedata = 0;
    wire [31:0] readdata;
    wire        irq, line;
    integer i, errors = 0;
    reg [31:0] r;

    fifo_uart_avalon #(.CLK_HZ(50_000_000), .BAUD(921_600), .FIFO_AW(8)) dut (
        .clk(clk), .reset(reset), .address(address), .read(read), .write(write),
        .writedata(writedata), .readdata(readdata), .irq(irq), .rxd(line), .txd(line));

    task av_write(input [1:0] a, input [31:0] d);
        begin @(posedge clk); address <= a; writedata <= d; write <= 1;
              @(posedge clk); write <= 0; end
    endtask
    task av_read(input [1:0] a, output [31:0] d);
        begin @(posedge clk); address <= a; read <= 1;
              @(posedge clk); read <= 0;
              @(negedge clk); d = readdata; end      // readLatency 1
    endtask

    initial begin
        repeat (5) @(posedge clk); reset = 0;
        av_write(2, 1);                                  // RXIE
        for (i = 0; i < 64; i = i + 1) av_write(0, i * 7 + 3);
        av_read(3, r); $display("TX FIFO count after 64 writes = %0d", r);
        wait (irq);
        // 전송이 모두 끝날 때까지 읽지 않는다 (ISR 지연이 매우 큰 상황)
        av_read(1, r);
        while (!r[2]) av_read(1, r);
        #20000;
        av_read(1, r); $display("STATUS: rx_count=%0d ovf=%b ferr=%b tx_idle=%b", r[31:16], r[3], r[4], r[2]);
        if (r[31:16] != 64 || r[3] || r[4]) errors = errors + 1;
        for (i = 0; i < 64; i = i + 1) begin
            av_read(0, r);
            if (!r[8] || r[7:0] != ((i * 7 + 3) & 8'hFF)) begin
                errors = errors + 1;
                $display("byte %0d: got %h", i, r);
            end
        end
        av_read(0, r); if (r[8]) errors = errors + 1;    // 비었으면 VALID=0
        if (irq) errors = errors + 1;
        if (errors == 0) $display("RESULT: PASS"); else $display("RESULT: FAIL (%0d)", errors);
        $finish;
    end
endmodule
