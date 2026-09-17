module top;
  wire [7:0] a, y;
  leaf #(.W(8)) u (.a(a), .y(y));
endmodule

module leaf #(
  parameter W = 8
)(
  input  wire [W-1:0] a,
  output wire [W-1:0] y
);
  assign y = a;
endmodule
