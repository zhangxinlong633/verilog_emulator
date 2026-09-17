// A[T×T] = row-wise one-hot argmax(S); lowest index wins ties.
module blas_row_argmax #(
  parameter T = 2,
  parameter W = 32
)(
  input  wire [W-1:0] s [0:T-1][0:T-1],
  output reg  [W-1:0] a [0:T-1][0:T-1]
);
  integer i, j, best_j;
  reg [W-1:0] best;
  always @* begin
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
  end
endmodule
