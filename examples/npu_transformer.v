// Stub: replaced in Task 2 with full toy Transformer NPU.
module npu_transformer(
  input wire clk,
  input wire rst,
  output reg done,
  output reg [3:0] phase
);
  always @(posedge clk) begin
    if (rst) begin
      done <= 1'b0;
      phase <= 4'd0;
    end
  end
endmodule
