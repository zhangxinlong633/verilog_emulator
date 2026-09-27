// 2-wide dot product unrolled with generate-for (the leaf pattern of a MAC array).
// A=[3,4], B=[5,6] → products 15 and 24, y=39.
module gen_mac2 (
  input  wire [7:0]  a [0:1],
  input  wire [7:0]  b [0:1],
  output wire [31:0] y
);
  parameter N = 2;
  genvar k;
  wire [31:0] mul [0:1];
  generate
    for (k = 0; k < N; k = k + 1) begin : g
      wire [31:0] p;
      assign p = a[k] * b[k];
      assign mul[k] = p;
    end
    if (N > 1) begin : wide
      assign y = mul[0] + mul[1];
    end else begin : narrow
      assign y = mul[0];
    end
  endgenerate
endmodule
