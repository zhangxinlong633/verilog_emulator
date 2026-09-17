// C[N×M] = A[N×K] · B[K×M]  (combinational)
module blas_gemm #(
  parameter N = 2,
  parameter K = 2,
  parameter M = 2,
  parameter WA = 8,
  parameter WB = 8,
  parameter CW = 32
)(
  input  wire [WA-1:0] a [0:N-1][0:K-1],
  input  wire [WB-1:0] b [0:K-1][0:M-1],
  output reg  [CW-1:0] c [0:N-1][0:M-1]
);
  integer i, j, kk;
  always @* begin
    for (i = 0; i < N; i = i + 1)
      for (j = 0; j < M; j = j + 1) begin
        c[i][j] = 0;
        for (kk = 0; kk < K; kk = kk + 1)
          c[i][j] = c[i][j] + a[i][kk] * b[kk][j];
      end
  end
endmodule
