// docs/examples/counter.v
// P0: must parse. P2: simulation target.
module counter(input wire clk, input wire rst, output reg [3:0] q);
  always @(posedge clk) begin
    if (rst)
      q <= 4'd0;
    else
      q <= q + 4'd1;
  end
endmodule
