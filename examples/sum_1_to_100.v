// Accumulate 1 + 2 + ... + 100 => sum = 5050
module sum_1_to_100(
  input wire clk,
  input wire rst,
  output reg [15:0] sum,
  output reg [7:0] i,
  output reg done
);
  always @(posedge clk) begin
    if (rst) begin
      sum <= 16'd0;
      i <= 8'd1;
      done <= 1'b0;
    end else if (!done) begin
      if (i <= 8'd100) begin
        sum <= sum + i;
        i <= i + 8'd1;
      end else begin
        done <= 1'b1;
      end
    end
  end
endmodule
