module for_fill #(
  parameter N = 4,
  parameter W = 8
);
  integer i;
  reg [W-1:0] mem [0:N-1];
  always @* begin
    for (i = 0; i < N; i = i + 1)
      mem[i] = i;
  end
endmodule
