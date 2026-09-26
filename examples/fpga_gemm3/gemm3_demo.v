// =============================================================================
// gemm3_demo — N=4：A=I，B=1..16 → C=B
// =============================================================================
//
// 顶层接线（固定乘法器集群与 FSM 并列，避免嵌套 array 实例）：
//
//   host stream ──► gemm3_fsm ── a_mem/b_mem ──► gemm3_mac_array
//                       ▲                            │
//                       └──────── c_comb ◄───────────┘
//                       │
//                    c_mem（1-cycle latch）
//
// @vs view matrix C array=c rows=4 cols=4
// =============================================================================
module gemm3_demo (
  input  wire clk,
  input  wire rst,
  output wire done,
  output wire [2:0] phase,
  output wire [31:0] c [0:3][0:3]
);
  reg start;
  reg kicked;
  wire [15:0] load_cnt;

  wire [7:0]  a_mem [0:3][0:3];
  wire [7:0]  b_mem [0:3][0:3];
  wire [31:0] c_comb [0:3][0:3];
  wire [31:0] c_mem [0:3][0:3];

  reg [7:0] stream_data [0:31];
  reg [7:0] stream_i [0:31];
  reg [7:0] stream_j [0:31];
  reg       stream_is_b [0:31];

  wire        host_we;
  wire        host_is_b;
  wire [7:0]  host_i;
  wire [7:0]  host_j;
  wire [7:0]  host_wdata;
  assign host_we = (phase == 3'd1);
  assign host_is_b = stream_is_b[load_cnt];
  assign host_i = stream_i[load_cnt];
  assign host_j = stream_j[load_cnt];
  assign host_wdata = stream_data[load_cnt];

  // 固定乘法器集群：64 路 mul + 16 路求和（纯组合）
  gemm4_mac_array u_mac (
    .a(a_mem),
    .b(b_mem),
    .c(c_comb)
  );

  gemm3_fsm #(.N(4), .WA(8), .WB(8), .CW(32)) u (
    .clk(clk),
    .rst(rst),
    .start(start),
    .host_we(host_we),
    .host_is_b(host_is_b),
    .host_i(host_i),
    .host_j(host_j),
    .host_wdata(host_wdata),
    .done(done),
    .phase(phase),
    .load_cnt(load_cnt),
    .a_mem(a_mem),
    .b_mem(b_mem),
    .c_comb(c_comb),
    .c_mem(c_mem)
  );

  assign c[0][0] = c_mem[0][0];
  assign c[0][1] = c_mem[0][1];
  assign c[0][2] = c_mem[0][2];
  assign c[0][3] = c_mem[0][3];
  assign c[1][0] = c_mem[1][0];
  assign c[1][1] = c_mem[1][1];
  assign c[1][2] = c_mem[1][2];
  assign c[1][3] = c_mem[1][3];
  assign c[2][0] = c_mem[2][0];
  assign c[2][1] = c_mem[2][1];
  assign c[2][2] = c_mem[2][2];
  assign c[2][3] = c_mem[2][3];
  assign c[3][0] = c_mem[3][0];
  assign c[3][1] = c_mem[3][1];
  assign c[3][2] = c_mem[3][2];
  assign c[3][3] = c_mem[3][3];

  integer r, col, idx;
  initial begin
    idx = 0;
    for (r = 0; r < 4; r = r + 1)
      for (col = 0; col < 4; col = col + 1) begin
        stream_i[idx] = r;
        stream_j[idx] = col;
        stream_is_b[idx] = 1'b0;
        if (r == col)
          stream_data[idx] = 8'd1;
        else
          stream_data[idx] = 8'd0;
        idx = idx + 1;
      end
    for (r = 0; r < 4; r = r + 1)
      for (col = 0; col < 4; col = col + 1) begin
        stream_i[idx] = r;
        stream_j[idx] = col;
        stream_is_b[idx] = 1'b1;
        stream_data[idx] = r * 4 + col + 1;
        idx = idx + 1;
      end
  end

  always @(posedge clk) begin
    if (rst) begin
      start <= 1'b0;
      kicked <= 1'b0;
    end else begin
      start <= 1'b0;
      if (!kicked) begin
        start <= 1'b1;
        kicked <= 1'b1;
      end
    end
  end
endmodule
