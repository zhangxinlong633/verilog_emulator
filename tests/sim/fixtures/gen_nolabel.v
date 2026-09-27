module gen_nolabel;
  genvar i;
  generate
    for (i = 0; i < 2; i = i + 1) begin
      wire w;
      assign w = 1;
    end
  endgenerate
endmodule
