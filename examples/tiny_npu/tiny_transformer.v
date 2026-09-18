// Tiny NPU: one Transformer block (hard attention + FFN/ReLU).
// T=4 tokens, D=4, HF=8. Weights from tiny_npu_weights (generated).
// Pass BLAS leaves after this file on `vs run`.
//
// @vs view matrix X array=x rows=4 cols=4
// @vs view matrix Q array=q rows=4 cols=4
// @vs view matrix K array=k rows=4 cols=4
// @vs view matrix V array=v rows=4 cols=4
// @vs view matrix S array=s rows=4 cols=4
// @vs view matrix A array=a rows=4 cols=4
// @vs view matrix O array=o rows=4 cols=4
// @vs view matrix H array=h rows=4 cols=8
// @vs view matrix Y array=y rows=4 cols=4
module tiny_transformer (
  input  wire clk,
  input  wire rst,
  input  wire [7:0] x [0:3][0:3],
  output reg  done,
  output reg  [3:0] phase,
  output reg  [31:0] q [0:3][0:3],
  output reg  [31:0] k [0:3][0:3],
  output reg  [31:0] v [0:3][0:3],
  output reg  [31:0] s [0:3][0:3],
  output reg  [31:0] a [0:3][0:3],
  output reg  [31:0] o [0:3][0:3],
  output reg  [31:0] h [0:3][0:7],
  output reg  [31:0] y [0:3][0:3]
);
  integer i, j;
  reg started;

  wire [7:0] wq [0:3][0:3];
  wire [7:0] wk [0:3][0:3];
  wire [7:0] wv [0:3][0:3];
  wire [7:0] w1 [0:3][0:7];
  wire [7:0] w2 [0:7][0:3];

  wire [31:0] q_w [0:3][0:3];
  wire [31:0] k_w [0:3][0:3];
  wire [31:0] v_w [0:3][0:3];
  wire [31:0] s_w [0:3][0:3];
  wire [31:0] a_w [0:3][0:3];
  wire [31:0] o_w [0:3][0:3];
  wire [31:0] h_pre_w [0:3][0:7];
  wire [31:0] h_w [0:3][0:7];
  wire [31:0] y_w [0:3][0:3];

  tiny_npu_weights u_w (
    .wq(wq), .wk(wk), .wv(wv), .w1(w1), .w2(w2)
  );

  blas_gemm #(.N(4), .K(4), .M(4), .WA(8), .WB(8), .CW(32)) u_q (
    .a(x), .b(wq), .c(q_w)
  );
  blas_gemm #(.N(4), .K(4), .M(4), .WA(8), .WB(8), .CW(32)) u_k (
    .a(x), .b(wk), .c(k_w)
  );
  blas_gemm #(.N(4), .K(4), .M(4), .WA(8), .WB(8), .CW(32)) u_v (
    .a(x), .b(wv), .c(v_w)
  );
  blas_gemm_bt #(.N(4), .K(4), .M(4), .WA(32), .WB(32), .CW(32)) u_s (
    .a(q), .b(k), .c(s_w)
  );
  blas_row_argmax #(.T(4), .W(32)) u_a (
    .s(s), .a(a_w)
  );
  blas_gemm #(.N(4), .K(4), .M(4), .WA(32), .WB(32), .CW(32)) u_o (
    .a(a), .b(v), .c(o_w)
  );
  blas_gemm #(.N(4), .K(4), .M(8), .WA(32), .WB(8), .CW(32)) u_h (
    .a(o), .b(w1), .c(h_pre_w)
  );
  blas_relu #(.N(4), .M(8), .W(32)) u_relu (
    .x(h), .y(h_w)
  );
  blas_gemm #(.N(4), .K(8), .M(4), .WA(32), .WB(8), .CW(32)) u_y (
    .a(h), .b(w2), .c(y_w)
  );

  always @(posedge clk) begin
    if (rst) begin
      done <= 1'b0;
      phase <= 4'd0;
      started <= 1'b0;
    end else if (!started) begin
      started <= 1'b1;
      phase <= 4'd1;
      done <= 1'b0;
    end else if (phase == 4'd1) begin
      for (i = 0; i < 4; i = i + 1)
        for (j = 0; j < 4; j = j + 1) begin
          q[i][j] = q_w[i][j];
          k[i][j] = k_w[i][j];
          v[i][j] = v_w[i][j];
        end
      phase <= 4'd2;
    end else if (phase == 4'd2) begin
      for (i = 0; i < 4; i = i + 1)
        for (j = 0; j < 4; j = j + 1)
          s[i][j] = s_w[i][j];
      phase <= 4'd3;
    end else if (phase == 4'd3) begin
      for (i = 0; i < 4; i = i + 1)
        for (j = 0; j < 4; j = j + 1)
          a[i][j] = a_w[i][j];
      phase <= 4'd4;
    end else if (phase == 4'd4) begin
      for (i = 0; i < 4; i = i + 1)
        for (j = 0; j < 4; j = j + 1)
          o[i][j] = o_w[i][j];
      phase <= 4'd5;
    end else if (phase == 4'd5) begin
      for (i = 0; i < 4; i = i + 1)
        for (j = 0; j < 8; j = j + 1)
          h[i][j] = h_pre_w[i][j];
      phase <= 4'd6;
    end else if (phase == 4'd6) begin
      for (i = 0; i < 4; i = i + 1)
        for (j = 0; j < 8; j = j + 1)
          h[i][j] = h_w[i][j];
      phase <= 4'd7;
    end else if (phase == 4'd7) begin
      for (i = 0; i < 4; i = i + 1)
        for (j = 0; j < 4; j = j + 1)
          y[i][j] = y_w[i][j];
      done <= 1'b1;
      phase <= 4'd8;
    end
  end
endmodule
