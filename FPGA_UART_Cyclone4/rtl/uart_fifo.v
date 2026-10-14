// -----------------------------------------------------------------------------
// uart_fifo.v  -  동기 FIFO (show-ahead / first-word-fall-through)
//   * dout 는 empty=0 일 때 항상 맨 앞 데이터. rd_en 으로 소비.
//   * 메모리는 "동기 읽기"로 작성 → Cyclone IV M9K 블록 RAM 으로 합성
//     (비동기 읽기로 쓰면 256x8 FIFO 하나가 LE 2000개 이상을 먹는다. EP4CE10 = 10K LE)
//   * 저장 용량 = 2^AW (RAM) + 1 (출력 레지스터)
// -----------------------------------------------------------------------------
module uart_fifo #(
    parameter integer WIDTH = 8,
    parameter integer AW    = 4          // RAM 깊이 = 2^AW
)(
    input  wire             clk,
    input  wire             rst,
    input  wire [WIDTH-1:0] din,
    input  wire             wr_en,
    output wire             full,
    output reg  [WIDTH-1:0] dout,        // = M9K 출력 레지스터
    input  wire             rd_en,
    output wire             empty,
    output wire [AW:0]      count,       // 저장된 데이터 수 (출력 레지스터 포함)
    output reg              overflow     // full 상태에서 쓰기 시도 (sticky)
);
    reg [WIDTH-1:0] mem [0:(1<<AW)-1];
    reg [AW:0] wptr, rptr;
    reg        dout_valid;

    wire ram_empty = (wptr == rptr);
    assign full  = (wptr[AW] != rptr[AW]) && (wptr[AW-1:0] == rptr[AW-1:0]);
    assign empty = !dout_valid;
    assign count = (wptr - rptr) + dout_valid;

    // 출력 레지스터가 비었거나 이번 클럭에 소비되면 RAM 에서 다음 데이터를 가져온다
    wire fetch = !ram_empty && (!dout_valid || rd_en);

    // RAM 쓰기
    always @(posedge clk)
        if (wr_en && !full) mem[wptr[AW-1:0]] <= din;

    // RAM 동기 읽기 (M9K)
    always @(posedge clk)
        if (fetch) dout <= mem[rptr[AW-1:0]];

    always @(posedge clk) begin
        if (rst) begin
            wptr       <= 0;
            rptr       <= 0;
            dout_valid <= 1'b0;
            overflow   <= 1'b0;
        end else begin
            if (wr_en && !full) wptr <= wptr + 1'b1;
            if (wr_en && full)  overflow <= 1'b1;
            if (fetch) begin
                rptr       <= rptr + 1'b1;
                dout_valid <= 1'b1;
            end else if (rd_en) begin
                dout_valid <= 1'b0;
            end
        end
    end
endmodule
