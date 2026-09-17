module array2d_write;
  integer i, j;
  reg [7:0] m [0:1][0:1];
  always @* begin
    for (i = 0; i < 2; i = i + 1)
      for (j = 0; j < 2; j = j + 1)
        m[i][j] = 0;
    m[1][0] = 5;
  end
endmodule
