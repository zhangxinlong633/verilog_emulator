// Array of instances via generate-for. Ports must be simple names, so each
// iteration copies the unpacked element through a local wire.
module gen_buf2;
  wire [7:0] a [0:1];
  wire [7:0] y [0:1];
  genvar i;
  generate
    for (i = 0; i < 2; i = i + 1) begin : g
      wire [7:0] ai;
      wire [7:0] yi;
      assign ai = a[i];
      buf8 u (.a(ai), .y(yi));
      assign y[i] = yi;
    end
  endgenerate
endmodule

module buf8 (
  input  wire [7:0] a,
  output wire [7:0] y
);
  assign y = a;
endmodule
