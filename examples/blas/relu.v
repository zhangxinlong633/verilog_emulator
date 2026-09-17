// Y = ReLU(X): clear element when MSB of X is set (two's-complement negative).
module blas_relu #(
  parameter N = 2,
  parameter M = 2,
  parameter W = 32
)(
  input  wire [W-1:0] x [0:N-1][0:M-1],
  output reg  [W-1:0] y [0:N-1][0:M-1]
);
  integer i, j;
  reg [W-1:0] acc;
  always @* begin
    for (i = 0; i < N; i = i + 1)
      for (j = 0; j < M; j = j + 1) begin
        acc = x[i][j];
        if (acc[W-1] == 1'b1)
          y[i][j] = 0;
        else
          y[i][j] = acc;
      end
  end
endmodule
