// Combinational multi in/out bus example
module combo_bus(
  input wire [3:0] a,
  input wire [3:0] b,
  input wire [3:0] c,
  input wire sel,
  output wire [3:0] sum,
  output reg [3:0] mix,
  output wire [3:0] aand,
  output wire eq_ab,
  output wire gt
);
  assign sum = a + b + c;
  assign aand = a & b;
  assign eq_ab = (a == b);
  assign gt = (a > b);

  always @* begin
    if (sel)
      mix = a | b;
    else
      mix = a & c;
  end
endmodule
