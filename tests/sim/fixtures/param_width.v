module param_width #(
  parameter W = 8
)(
  input wire [W-1:0] x,
  output wire [W-1:0] y
);
  assign y = x;
endmodule
