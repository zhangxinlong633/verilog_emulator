// Multi I/O sequential example: 4-bit ALU register
// op=0: y <= a + b;  op=1: y <= a ^ b;  zflag = (y == 0)
module alu4(
  input wire clk,
  input wire rst,
  input wire [3:0] a,
  input wire [3:0] b,
  input wire op,
  output reg [3:0] y,
  output wire zflag
);
  assign zflag = (y == 4'd0);

  always @(posedge clk) begin
    if (rst)
      y <= 4'd0;
    else if (op)
      y <= a ^ b;
    else
      y <= a + b;
  end
endmodule
