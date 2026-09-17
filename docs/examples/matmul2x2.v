// 2x2 matrix multiply (combinational):
//   | a00 a01 |   | b00 b01 |   | c00 c01 |
//   | a10 a11 | x | b10 b11 | = | c10 c11 |
//
// Example: A=[[1,2],[3,4]] B=[[5,6],[7,8]]
//   c00=1*5+2*7=19  c01=1*6+2*8=22
//   c10=3*5+4*7=43  c11=3*6+4*8=50
module matmul2x2(
  input wire [7:0] a00,
  input wire [7:0] a01,
  input wire [7:0] a10,
  input wire [7:0] a11,
  input wire [7:0] b00,
  input wire [7:0] b01,
  input wire [7:0] b10,
  input wire [7:0] b11,
  output wire [15:0] c00,
  output wire [15:0] c01,
  output wire [15:0] c10,
  output wire [15:0] c11
);
  assign c00 = a00 * b00 + a01 * b10;
  assign c01 = a00 * b01 + a01 * b11;
  assign c10 = a10 * b00 + a11 * b10;
  assign c11 = a10 * b01 + a11 * b11;
endmodule
