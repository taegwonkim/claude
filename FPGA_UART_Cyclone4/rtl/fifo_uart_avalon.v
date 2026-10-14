// -----------------------------------------------------------------------------
// fifo_uart_avalon.v  -  Nios II 용 FIFO UART (Avalon-MM slave)
//
//   altera_avalon_uart 대체용. RX/TX 각각 2^FIFO_AW 바이트 FIFO 를 가지므로
//   ISR 이 늦게 돌아도(최대 FIFO 깊이 x 1문자 시간) 수신 바이트를 잃지 않는다.
//
//   레지스터 (32bit, 워드 주소)
//   0 DATA    R : [7:0] RX 데이터, [8] RX_VALID (1=유효, 읽으면 FIFO 에서 꺼냄)
//             W : [7:0] TX FIFO 에 넣기 (TX_FULL 이면 무시)
//   1 STATUS  R : [0] RX_AVAIL  [1] TX_FULL  [2] TX_IDLE(FIFO 비고 송신 완료)
//                 [3] RX_OVF(sticky) [4] RX_FERR(sticky)
//                 [15:8] 예약  [31:16] RX_COUNT
//             W : 1 을 쓴 비트 [3],[4] 클리어
//   2 CONTROL R/W : [0] RXIE (RX FIFO 에 데이터가 있으면 irq)
//   3 TXCOUNT R : [15:0] TX FIFO 에 남은 바이트 수
//
//   readLatency = 1 (Platform Designer 설정은 fifo_uart_hw.tcl 참고)
// -----------------------------------------------------------------------------
module fifo_uart_avalon #(
    parameter integer CLK_HZ  = 50_000_000,
    parameter integer BAUD    = 115_200,
    parameter integer FIFO_AW = 8                 // 256 바이트
)(
    input  wire        clk,
    input  wire        reset,
    // Avalon-MM slave
    input  wire [1:0]  address,
    input  wire        read,
    input  wire        write,
    input  wire [31:0] writedata,
    output reg  [31:0] readdata,
    output wire        irq,
    // UART 핀
    input  wire        rxd,
    output wire        txd
);
    // ---------------- RX ----------------
    wire [7:0] rx_data;
    wire       rx_valid, rx_ferr;
    uart_rx #(.CLK_HZ(CLK_HZ), .BAUD(BAUD)) u_rx (
        .clk(clk), .rst(reset), .rxd(rxd),
        .rx_data(rx_data), .rx_valid(rx_valid), .rx_ferr(rx_ferr));

    wire [7:0]       rxf_dout;
    wire             rxf_empty, rxf_full;
    wire [FIFO_AW:0] rxf_count;
    wire             rx_pop = read && (address == 2'd0) && !rxf_empty;
    uart_fifo #(.WIDTH(8), .AW(FIFO_AW)) u_rxf (
        .clk(clk), .rst(reset),
        .din(rx_data), .wr_en(rx_valid), .full(rxf_full),
        .dout(rxf_dout), .rd_en(rx_pop), .empty(rxf_empty),
        .count(rxf_count), .overflow());

    // ---------------- TX ----------------
    wire [7:0]       txf_dout;
    wire             txf_empty, txf_full, tx_ready, tx_busy;
    wire [FIFO_AW:0] txf_count;
    wire             tx_push = write && (address == 2'd0) && !txf_full;
    uart_fifo #(.WIDTH(8), .AW(FIFO_AW)) u_txf (
        .clk(clk), .rst(reset),
        .din(writedata[7:0]), .wr_en(tx_push), .full(txf_full),
        .dout(txf_dout), .rd_en(tx_ready & ~txf_empty), .empty(txf_empty),
        .count(txf_count), .overflow());

    uart_tx #(.CLK_HZ(CLK_HZ), .BAUD(BAUD)) u_tx (
        .clk(clk), .rst(reset),
        .tx_data(txf_dout), .tx_valid(~txf_empty), .tx_ready(tx_ready),
        .txd(txd), .tx_busy(tx_busy));

    // ---------------- 상태 / 제어 ----------------
    reg rx_ovf, rx_ferr_s, rxie;
    wire tx_idle = txf_empty && !tx_busy;

    always @(posedge clk) begin
        if (reset) begin
            rx_ovf    <= 1'b0;
            rx_ferr_s <= 1'b0;
            rxie      <= 1'b0;
        end else begin
            if (write && address == 2'd1) begin
                if (writedata[3]) rx_ovf    <= 1'b0;
                if (writedata[4]) rx_ferr_s <= 1'b0;
            end
            if (write && address == 2'd2) rxie <= writedata[0];
            if (rx_valid && rxf_full) rx_ovf    <= 1'b1;
            if (rx_ferr)              rx_ferr_s <= 1'b1;
        end
    end

    // readLatency = 1 : 읽기 사이클 다음 클럭에 readdata 유효
    always @(posedge clk) begin
        case (address)
        2'd0: readdata <= {23'd0, ~rxf_empty, rxf_dout};
        2'd1: readdata <= {{(16-FIFO_AW-1){1'b0}}, rxf_count, 8'd0,
                           3'd0, rx_ferr_s, rx_ovf, tx_idle, txf_full, ~rxf_empty};
        2'd2: readdata <= {31'd0, rxie};
        2'd3: readdata <= {{(32-FIFO_AW-1){1'b0}}, txf_count};
        endcase
    end

    assign irq = rxie && !rxf_empty;
endmodule
