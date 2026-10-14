// -----------------------------------------------------------------------------
// uart_top.v  -  Cyclone IV 예제 탑 : RX → RX FIFO → (사용자 로직) → TX FIFO → TX
//   예제에서는 받은 바이트를 그대로 되돌려 보낸다(에코).
//   사용자 로직은 rx_fifo 출력 / tx_fifo 입력 사이에 넣으면 된다.
// -----------------------------------------------------------------------------
module uart_top #(
    parameter integer CLK_HZ = 50_000_000,
    parameter integer BAUD   = 115_200,
    parameter integer FIFO_AW = 6               // 64 바이트
)(
    input  wire CLOCK_50,
    input  wire RESET_N,                        // 보드 KEY 등 (active low)
    input  wire UART_RXD,
    output wire UART_TXD,
    output wire [1:0] LED                       // [0]=RX overflow, [1]=framing err
);
    // ---- 리셋 동기화 ----
    reg [2:0] rst_sr = 3'b111;
    always @(posedge CLOCK_50 or negedge RESET_N)
        if (!RESET_N) rst_sr <= 3'b111;
        else          rst_sr <= {rst_sr[1:0], 1'b0};
    wire rst = rst_sr[2];

    // ---- RX ----
    wire [7:0] rx_data;
    wire       rx_valid, rx_ferr;
    uart_rx #(.CLK_HZ(CLK_HZ), .BAUD(BAUD)) u_rx (
        .clk(CLOCK_50), .rst(rst), .rxd(UART_RXD),
        .rx_data(rx_data), .rx_valid(rx_valid), .rx_ferr(rx_ferr));

    wire [7:0] rxf_dout;
    wire       rxf_empty, rxf_full, rxf_ovf, rxf_rd;
    uart_fifo #(.WIDTH(8), .AW(FIFO_AW)) u_rxf (
        .clk(CLOCK_50), .rst(rst),
        .din(rx_data), .wr_en(rx_valid), .full(rxf_full),
        .dout(rxf_dout), .rd_en(rxf_rd), .empty(rxf_empty), .overflow(rxf_ovf));

    // ---- 사용자 로직 자리 (예제: 에코) ----
    wire       txf_full;
    assign rxf_rd = !rxf_empty && !txf_full;

    // ---- TX ----
    wire [7:0] txf_dout;
    wire       txf_empty, tx_ready, tx_busy;
    uart_fifo #(.WIDTH(8), .AW(FIFO_AW)) u_txf (
        .clk(CLOCK_50), .rst(rst),
        .din(rxf_dout), .wr_en(rxf_rd), .full(txf_full),
        .dout(txf_dout), .rd_en(tx_ready & ~txf_empty), .empty(txf_empty), .overflow());

    uart_tx #(.CLK_HZ(CLK_HZ), .BAUD(BAUD)) u_tx (
        .clk(CLOCK_50), .rst(rst),
        .tx_data(txf_dout), .tx_valid(~txf_empty), .tx_ready(tx_ready),
        .txd(UART_TXD), .tx_busy(tx_busy));

    // ---- 상태 LED (sticky) ----
    reg ferr_seen;
    always @(posedge CLOCK_50)
        if (rst) ferr_seen <= 1'b0;
        else if (rx_ferr) ferr_seen <= 1'b1;
    assign LED = {ferr_seen, rxf_ovf};
endmodule
