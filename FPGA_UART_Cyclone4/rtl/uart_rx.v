// -----------------------------------------------------------------------------
// uart_rx.v  -  8-N-1 UART 수신기 (바이트 사이 간격 0 인 연속 수신 지원)
//
//  * 2단 동기화 FF → 메타스테빌리티 방지
//  * 16배 오버샘플링 (NCO 로 정확한 BAUD*16 틱 생성)
//  * 각 비트 중앙 3샘플 다수결 → 노이즈/글리치 내성
//  * ★ STOP 비트 "중앙"에서 바로 IDLE 로 복귀
//      → 송신측 클럭이 조금 빨라서 다음 START 하강엣지가 일찍 와도 놓치지 않음
//  * rx_valid 는 1클럭 펄스. 반드시 FIFO 로 받을 것 (uart_top 참고)
// -----------------------------------------------------------------------------
module uart_rx #(
    parameter integer CLK_HZ = 50_000_000,
    parameter integer BAUD   = 115_200
)(
    input  wire       clk,
    input  wire       rst,
    input  wire       rxd,        // 핀 입력 (비동기)
    output reg  [7:0] rx_data,
    output reg        rx_valid,   // 1클럭 펄스
    output reg        rx_ferr     // STOP 비트 오류(프레이밍 에러) 펄스
);
    localparam integer OS = 16;

    // ---- 1) 동기화 : 외부 비동기 신호를 반드시 2단 FF 로 ----
    (* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    reg [1:0] sync = 2'b11;
    always @(posedge clk) sync <= {sync[0], rxd};
    wire rx_s = sync[1];

    // ---- 2) 16x 오버샘플 틱 (분수 누산기) ----
    reg  [31:0] acc;
    wire [32:0] acc_next = {1'b0, acc} + BAUD * OS;
    wire        tick     = (acc_next >= CLK_HZ);
    always @(posedge clk) begin
        if (rst)       acc <= 32'd0;
        else if (tick) acc <= acc_next - CLK_HZ;
        else           acc <= acc_next[31:0];
    end

    // ---- 3) 최근 3틱 샘플 다수결 ----
    reg  [2:0] smp;
    always @(posedge clk) begin
        if (rst)       smp <= 3'b111;
        else if (tick) smp <= {smp[1:0], rx_s};
    end
    wire [2:0] smp_now = {smp[1:0], rx_s};    // 이번 틱 샘플 포함
    wire maj = (smp_now[0] & smp_now[1]) | (smp_now[1] & smp_now[2]) | (smp_now[0] & smp_now[2]);

    // ---- 4) 상태기계 ----
    localparam [1:0] S_IDLE = 2'd0, S_START = 2'd1, S_DATA = 2'd2, S_STOP = 2'd3;
    reg [1:0] state;
    reg [3:0] os_cnt;
    reg [2:0] bit_idx;
    reg [7:0] shreg;

    always @(posedge clk) begin
        rx_valid <= 1'b0;
        rx_ferr  <= 1'b0;
        if (rst) begin
            state   <= S_IDLE;
            os_cnt  <= 4'd0;
            bit_idx <= 3'd0;
        end else if (tick) begin
            case (state)
            S_IDLE: begin
                if (!rx_s) begin              // START 후보 (하강)
                    state  <= S_START;
                    os_cnt <= 4'd0;
                end
            end
            S_START: begin
                if (os_cnt == 4'd7) begin     // START 비트 중앙
                    if (!maj) begin
                        state   <= S_DATA;
                        os_cnt  <= 4'd0;
                        bit_idx <= 3'd0;
                    end else begin
                        state <= S_IDLE;      // 글리치였음
                    end
                end else os_cnt <= os_cnt + 4'd1;
            end
            S_DATA: begin
                if (os_cnt == OS-1) begin     // 데이터 비트 중앙
                    os_cnt <= 4'd0;
                    shreg  <= {maj, shreg[7:1]};   // LSB first
                    if (bit_idx == 3'd7) state <= S_STOP;
                    bit_idx <= bit_idx + 3'd1;
                end else os_cnt <= os_cnt + 4'd1;
            end
            S_STOP: begin
                if (os_cnt == OS-1) begin     // STOP 비트 중앙 → 즉시 IDLE
                    rx_data  <= shreg;
                    rx_valid <= maj;
                    rx_ferr  <= ~maj;
                    state    <= S_IDLE;
                    os_cnt   <= 4'd0;
                end else os_cnt <= os_cnt + 4'd1;
            end
            endcase
        end
    end
endmodule
