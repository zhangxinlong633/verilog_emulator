// Toy NPU: single-layer Transformer with hard attention + FFN/ReLU.
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
  integer i, j, kk, best_j;
  reg [31:0] best;
  reg [31:0] acc;
  reg started;

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
      // QKV: Q=X·Wq, K=X·Wk, V=X·Wv
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < D; j = j + 1) begin
          acc = 0;
          for (kk = 0; kk < D; kk = kk + 1)
            acc = acc + x[i][kk] * wq[kk][j];
          q[i][j] = acc;
          acc = 0;
          for (kk = 0; kk < D; kk = kk + 1)
            acc = acc + x[i][kk] * wk[kk][j];
          k[i][j] = acc;
          acc = 0;
          for (kk = 0; kk < D; kk = kk + 1)
            acc = acc + x[i][kk] * wv[kk][j];
          v[i][j] = acc;
        end
      phase <= 4'd2;
    end else if (phase == 4'd2) begin
      // S = Q · K^T  => s[i][j] = sum_kk q[i][kk]*k[j][kk]
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < T; j = j + 1) begin
          acc = 0;
          for (kk = 0; kk < D; kk = kk + 1)
            acc = acc + q[i][kk] * k[j][kk];
          s[i][j] = acc;
        end
      phase <= 4'd3;
    end else if (phase == 4'd3) begin
      // Hard attention: row argmax → one-hot A (lowest index on ties)
      for (i = 0; i < T; i = i + 1) begin
        best = s[i][0];
        best_j = 0;
        for (j = 1; j < T; j = j + 1) begin
          if (s[i][j] > best) begin
            best = s[i][j];
            best_j = j;
          end
        end
        for (j = 0; j < T; j = j + 1) begin
          if (j == best_j)
            a[i][j] = 1;
          else
            a[i][j] = 0;
        end
      end
      phase <= 4'd4;
    end else if (phase == 4'd4) begin
      // O = A · V
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < D; j = j + 1) begin
          acc = 0;
          for (kk = 0; kk < T; kk = kk + 1)
            acc = acc + a[i][kk] * v[kk][j];
          o[i][j] = acc;
        end
      phase <= 4'd5;
    end else if (phase == 4'd5) begin
      // H = O · W1
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < HF; j = j + 1) begin
          acc = 0;
          for (kk = 0; kk < D; kk = kk + 1)
            acc = acc + o[i][kk] * w1[kk][j];
          h[i][j] = acc;
        end
      phase <= 4'd6;
    end else if (phase == 4'd6) begin
      // ReLU (MSB of 32-bit two's complement)
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < HF; j = j + 1) begin
          acc = h[i][j];
          if (acc[31] == 1'b1)
            h[i][j] = 0;
        end
      phase <= 4'd7;
    end else if (phase == 4'd7) begin
      // Y = H · W2
      for (i = 0; i < T; i = i + 1)
        for (j = 0; j < D; j = j + 1) begin
          acc = 0;
          for (kk = 0; kk < HF; kk = kk + 1)
            acc = acc + h[i][kk] * w2[kk][j];
          y[i][j] = acc;
        end
      done <= 1'b1;
      phase <= 4'd8;
    end
  end
endmodule
