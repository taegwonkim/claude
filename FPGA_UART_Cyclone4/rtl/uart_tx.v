// -----------------------------------------------------------------------------
// uart_tx.v  -  8-N-1 UART 송신기 (바이트 사이 지연 없는 연속 송신)
//
//  * 보레이트는 분수 누산기(NCO)로 생성 → 정수 분주 반올림 오차 없음
//  * valid/ready 핸드셰이크
//  * tx_ready 는 "STOP 비트의 마지막 클럭"에도 1 이 된다.
//    → 그 클럭에 다음 바이트가 들어오면 STOP 직후 곧바로 START 비트가 나가므로
//      바이트 사이 IDLE 간격 = 0 클럭
// -----------------------------------------------------------------------------
module uart_tx #(
    parameter integer CLK_HZ = 50_000_000,
    parameter integer BAUD   = 115_200
)(
    input  wire       clk,
    input  wire       rst,        // 동기 리셋, active high
    input  wire [7:0] tx_data,
    input  wire       tx_valid,
    output wire       tx_ready,
    output reg        txd,        // 레지스터 출력 (글리치 없음)
    output wire       tx_busy
);
    // 비트 경계 생성용 누산기: 매 클럭 BAUD 를 더하고 CLK_HZ 를 넘으면 1비트 경과
    reg  [31:0] acc;
    wire [32:0] acc_next = {1'b0, acc} + BAUD;
    reg         busy;
    reg  [8:0]  shreg;            // {STOP, D7..D0} - START 는 로드 시 바로 출력
    reg  [3:0]  bit_cnt;          // 남은 비트 경계 수

    wire bit_end = busy && (acc_next >= CLK_HZ);
    // STOP 비트가 끝나는 바로 그 클럭에도 다음 바이트를 받을 수 있다
    assign tx_ready = !busy || (bit_end && bit_cnt == 4'd0);
    assign tx_busy  = busy;

    always @(posedge clk) begin
        if (rst) begin
            busy    <= 1'b0;
            txd     <= 1'b1;
            acc     <= 32'd0;
            bit_cnt <= 4'd0;
            shreg   <= 9'h1FF;
        end else if (tx_valid && tx_ready) begin
            // 새 프레임 시작 : START 비트(0) 출력
            shreg   <= {1'b1, tx_data};
            txd     <= 1'b0;
            bit_cnt <= 4'd9;          // D0..D7 + STOP = 9 개의 경계 후 종료
            busy    <= 1'b1;
            // 연속 송신이면 위상 유지(누적오차 0), IDLE 에서 시작이면 0 부터
            acc     <= busy ? (acc_next - CLK_HZ) : 32'd0;
        end else if (busy) begin
            if (bit_end) begin
                acc <= acc_next - CLK_HZ;
                if (bit_cnt == 4'd0) begin
                    busy <= 1'b0;     // STOP 끝, 대기 바이트 없음
                    txd  <= 1'b1;
                end else begin
                    txd     <= shreg[0];
                    shreg   <= {1'b1, shreg[8:1]};
                    bit_cnt <= bit_cnt - 4'd1;
                end
            end else begin
                acc <= acc_next[31:0];
            end
        end
    end
endmodule
