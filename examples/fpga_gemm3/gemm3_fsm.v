// =============================================================================
// gemm3_fsm — LOAD / COMPUTE / STORE
// =============================================================================
//
// COMPUTE：1 cycle 锁存 c_comb（由顶层 gemm3_mac_array 算出）。
// a_mem/b_mem 输出到顶层，供 gemm{N}_mac_array 接线（vs 不支持嵌套 array 端口实例）。
// 当前 demo 用 gemm4_mac_array（N=4）。
// =============================================================================
module gemm3_fsm #(
  parameter N  = 4,
  parameter WA = 8,
  parameter WB = 8,
  parameter CW = 32
)(
  input  wire clk,
  input  wire rst,
  input  wire start,
  input  wire          host_we,
  input  wire          host_is_b,
  input  wire [7:0]    host_i,
  input  wire [7:0]    host_j,
  input  wire [WA-1:0] host_wdata,
  output reg           done,
  output reg  [2:0]    phase,
  output reg  [15:0]   load_cnt,
  // 供顶层乘法器集群：A/B 源、C 组合结果
  output reg  [WA-1:0] a_mem [0:3][0:3],
  output reg  [WB-1:0] b_mem [0:3][0:3],
  input  wire [CW-1:0] c_comb [0:3][0:3],
  output reg  [CW-1:0] c_mem [0:3][0:3]
);
  integer ii, jj;
  reg started;
  wire [15:0] load_total;

  assign load_total = 2 * N * N;

  always @(posedge clk) begin
    if (rst) begin
      done <= 1'b0;
      phase <= 3'd0;
      started <= 1'b0;
      load_cnt <= 16'd0;
      for (ii = 0; ii < N; ii = ii + 1)
        for (jj = 0; jj < N; jj = jj + 1) begin
          a_mem[ii][jj] <= 8'd0;
          b_mem[ii][jj] <= 8'd0;
          c_mem[ii][jj] <= 32'd0;
        end
    end else if (phase == 3'd0) begin
      if (start && !started) begin
        done <= 1'b0;
        started <= 1'b1;
        phase <= 3'd1;
        load_cnt <= 16'd0;
        for (ii = 0; ii < N; ii = ii + 1)
          for (jj = 0; jj < N; jj = jj + 1)
            c_mem[ii][jj] <= 32'd0;
      end
    end else if (phase == 3'd1) begin
      if (host_we) begin
        if (!host_is_b)
          a_mem[host_i][host_j] <= host_wdata;
        else
          b_mem[host_i][host_j] <= host_wdata;
      end
      if (load_cnt + 16'd1 >= load_total)
        phase <= 3'd2;
      else
        load_cnt <= load_cnt + 16'd1;
    end else if (phase == 3'd2) begin
      // 1 cycle：锁存固定乘法器集群的组合输出
      for (ii = 0; ii < N; ii = ii + 1)
        for (jj = 0; jj < N; jj = jj + 1)
          c_mem[ii][jj] <= c_comb[ii][jj];
      phase <= 3'd3;
    end else if (phase == 3'd3) begin
      done <= 1'b1;
      phase <= 3'd0;
      started <= 1'b0;
    end
  end
endmodule
