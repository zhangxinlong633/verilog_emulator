// Toy NPU: single-layer Transformer with hard attention + FFN/ReLU.
// FSM top; math leaves live in examples/blas/*.v (pass those files after this one).
// Defaults T=D=HF=2, W=8.
//
// Golden (Wq=Wk=Wv=W1=W2 = I):
//   X=[[1,2],[3,4]]
//   S=Q·Kᵀ=[[5,11],[11,25]] → A=[[0,1],[0,1]]
//   O=A·V=[[3,4],[3,4]] → Y=[[3,4],[3,4]]
//
// @vs view matrix X array=x rows=T cols=D
// @vs view matrix Q array=q rows=T cols=D
// @vs view matrix K array=k rows=T cols=D
// @vs view matrix V array=v rows=T cols=D
// @vs view matrix S array=s rows=T cols=T
// @vs view matrix A array=a rows=T cols=T
// @vs view matrix O array=o rows=T cols=D
// @vs view matrix Y array=y rows=T cols=D
module npu_transformer #(
  parameter T = 2,
  parameter D = 2,
  parameter HF = 2,
  parameter W = 8
)(
  input wire clk,
  input wire rst,
  input wire [W-1:0] x [0:T-1][0:D-1],
  input wire [W-1:0] wq [0:D-1][0:D-1],
  input wire [W-1:0] wk [0:D-1][0:D-1],
  input wire [W-1:0] wv [0:D-1][0:D-1],
  input wire [W-1:0] w1 [0:D-1][0:HF-1],
  input wire [W-1:0] w2 [0:HF-1][0:D-1],
  output reg done,
  output reg [3:0] phase,
  output reg [31:0] q [0:T-1][0:D-1],
  output reg [31:0] k [0:T-1][0:D-1],
  output reg [31:0] v [0:T-1][0:D-1],
  output reg [31:0] s [0:T-1][0:T-1],
  output reg [31:0] a [0:T-1][0:T-1],
  output reg [31:0] o [0:T-1][0:D-1],
  output reg [31:0] h [0:T-1][0:HF-1],
  output reg [31:0] y [0:T-1][0:D-1]
);
  integer i, j;
  reg started;

  wire [31:0] q_w [0:T-1][0:D-1];
  wire [31:0] k_w [0:T-1][0:D-1];
  wire [31:0] v_w [0:T-1][0:D-1];
  wire [31:0] s_w [0:T-1][0:T-1];
  wire [31:0] a_w [0:T-1][0:T-1];
  wire [31:0] o_w [0:T-1][0:D-1];
  wire [31:0] h_pre_w [0:T-1][0:HF-1];
  wire [31:0] h_w [0:T-1][0:HF-1];
  wire [31:0] y_w [0:T-1][0:D-1];

  blas_gemm #(.N(T), .K(D), .M(D), .WA(W), .WB(W), .CW(32)) u_q (
    .a(x), .b(wq), .c(q_w)
  );
  blas_gemm #(.N(T), .K(D), .M(D), .WA(W), .WB(W), .CW(32)) u_k (
    .a(x), .b(wk), .c(k_w)
  );
  blas_gemm #(.N(T), .K(D), .M(D), .WA(W), .WB(W), .CW(32)) u_v (
    .a(x), .b(wv), .c(v_w)
  );
  blas_gemm_bt #(.N(T), .K(D), .M(T), .WA(32), .WB(32), .CW(32)) u_s (
    .a(q), .b(k), .c(s_w)
  );
  blas_row_argmax #(.T(T), .W(32)) u_a (
    .s(s), .a(a_w)
  );
  blas_gemm #(.N(T), .K(T), .M(D), .WA(32), .WB(32), .CW(32)) u_o (
    .a(a), .b(v), .c(o_w)
  );
  blas_gemm #(.N(T), .K(D), .M(HF), .WA(32), .WB(W), .CW(32)) u_h (
    .a(o), .b(w1), .c(h_pre_w)
  );
  blas_relu #(.N(T), .M(HF), .W(32)) u_relu (
    .x(h), .y(h_w)
  );
  blas_gemm #(.N(T), .K(HF), .M(D), .WA(32), .WB(W), .CW(32)) u_y (
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
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < D; j = j + 1) begin
          q[i][j] = q_w[i][j];
          k[i][j] = k_w[i][j];
          v[i][j] = v_w[i][j];
        end
      phase <= 4'd2;
    end else if (phase == 4'd2) begin
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < T; j = j + 1)
          s[i][j] = s_w[i][j];
      phase <= 4'd3;
    end else if (phase == 4'd3) begin
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < T; j = j + 1)
          a[i][j] = a_w[i][j];
      phase <= 4'd4;
    end else if (phase == 4'd4) begin
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < D; j = j + 1)
          o[i][j] = o_w[i][j];
      phase <= 4'd5;
    end else if (phase == 4'd5) begin
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < HF; j = j + 1)
          h[i][j] = h_pre_w[i][j];
      phase <= 4'd6;
    end else if (phase == 4'd6) begin
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < HF; j = j + 1)
          h[i][j] = h_w[i][j];
      phase <= 4'd7;
    end else if (phase == 4'd7) begin
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < D; j = j + 1)
          y[i][j] = y_w[i][j];
      done <= 1'b1;
      phase <= 4'd8;
    end
  end
endmodule
