// tb_uart_fifo.v - FIFO 랜덤 읽기/쓰기 vs 참조 모델
`timescale 1ns/1ps
module tb_uart_fifo;
    reg clk = 0; always #5 clk = ~clk;
    reg rst = 1, wr = 0, rd = 0;
    reg [7:0] din = 0;
    wire [7:0] dout; wire full, empty, ovf; wire [4:0] count;
    uart_fifo #(.WIDTH(8), .AW(4)) dut (.clk(clk), .rst(rst), .din(din), .wr_en(wr), .full(full),
        .dout(dout), .rd_en(rd), .empty(empty), .count(count), .overflow(ovf));
    reg [7:0] model [0:1023]; integer mh = 0, mt = 0, i, errors = 0, maxc = 0;
    initial begin
        repeat (3) @(posedge clk); rst = 0;
        for (i = 0; i < 20000; i = i + 1) begin
            @(negedge clk);
            wr = ($random % 100) < ((i / 2000) % 2 ? 30 : 70) && !full;
            rd = ($random % 100) < ((i / 2000) % 2 ? 70 : 30) && !empty;
            din = $random;
            if (rd) begin
                if (mt == mh || dout !== model[mt % 1024]) errors = errors + 1;
                mt = mt + 1;
            end
            if (wr) begin model[mh % 1024] = din; mh = mh + 1; end
            @(posedge clk); #1;
            if (count != mh - mt) errors = errors + 1;
            if (count > maxc) maxc = count;
        end
        if (ovf) errors = errors + 1;
        $display("transfers=%0d max_count=%0d", mt, maxc);
        if (errors == 0) $display("RESULT: PASS"); else $display("RESULT: FAIL (%0d)", errors);
        $finish;
    end
endmodule
