// Parameterized matrix multiply (combinational):
//   C[N×M] = A[N×K] * B[K×M]
// Default N=K=M=2, W=8:
//   A=[[1,2],[3,4]] B=[[5,6],[7,8]] → C=[[19,22],[43,50]]
//
// Element public names: a_i_j, b_i_j, c_i_j (row-major).
// @vs view matrix A array=a rows=N cols=K
// @vs view matrix B array=b rows=K cols=M
// @vs view matrix C array=c rows=N cols=M
// @vs op matmul out=C left=A right=B
module matmul #(
  parameter N = 2,
  parameter K = 2,
  parameter M = 2,
  parameter W = 8
)(
  input  wire [W-1:0] a [0:N-1][0:K-1],
  input  wire [W-1:0] b [0:K-1][0:M-1],
  output reg  [2*W-1:0] c [0:N-1][0:M-1]
);
  integer i, j, k;
  always @* begin
    for (i = 0; i < N; i = i + 1)
      for (j = 0; j < M; j = j + 1) begin
        c[i][j] = 0;
        for (k = 0; k < K; k = k + 1)
          c[i][j] = c[i][j] + a[i][k] * b[k][j];
      end
  end
endmodule
