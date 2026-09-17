// @vs view matrix Y rows=1 cols=1 cells=y
// @vs expr y = a
module inv_anno(input wire a, output wire y);
  assign y = ~a;
endmodule
