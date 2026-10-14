// -----------------------------------------------------------------------------
// uart_fifo.v  -  동기 FIFO (show-ahead / first-word-fall-through)
//   dout 는 empty=0 일 때 항상 맨 앞 데이터. rd_en 으로 소비.
//   Quartus IP 를 쓰려면 SCFIFO(lpm_showahead="ON") 로 대체 가능.
// -----------------------------------------------------------------------------
module uart_fifo #(
    parameter integer WIDTH = 8,
    parameter integer AW    = 4          // 깊이 = 2^AW
)(
    input  wire             clk,
    input  wire             rst,
    input  wire [WIDTH-1:0] din,
    input  wire             wr_en,
    output wire             full,
    output wire [WIDTH-1:0] dout,
    input  wire             rd_en,
    output wire             empty,
    output reg              overflow     // full 상태에서 쓰기 시도 (sticky)
);
    reg [WIDTH-1:0] mem [0:(1<<AW)-1];
    reg [AW:0] wptr, rptr;

    assign empty = (wptr == rptr);
    assign full  = (wptr[AW] != rptr[AW]) && (wptr[AW-1:0] == rptr[AW-1:0]);
    assign dout  = mem[rptr[AW-1:0]];

    always @(posedge clk) begin
        if (rst) begin
            wptr     <= 0;
            rptr     <= 0;
            overflow <= 1'b0;
        end else begin
            if (wr_en && !full) begin
                mem[wptr[AW-1:0]] <= din;
                wptr <= wptr + 1'b1;
            end
            if (wr_en && full) overflow <= 1'b1;
            if (rd_en && !empty) rptr <= rptr + 1'b1;
        end
    end
endmodule
