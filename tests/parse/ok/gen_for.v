module gen_for #(
  parameter N = 2
);
  genvar i;
  wire [7:0] y [0:1];
  wire [7:0] a [0:1];
  generate
    for (i = 0; i < N; i = i + 1) begin : g
      assign y[i] = a[i];
    end
    if (N > 1) begin : wide
      assign y[0] = a[0];
    end else begin : narrow
      assign y[0] = a[1];
    end
  endgenerate
endmodule
