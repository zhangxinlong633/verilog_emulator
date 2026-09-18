// Auto-generated: GEMM X·Wq → Q
// @vs view matrix A array=a rows=4 cols=4
// @vs view matrix B array=b rows=4 cols=4
// @vs view matrix C array=c rows=4 cols=4
// @vs op matmul out=C left=A right=B
module demo_gemm;
  reg  [7:0]  a [0:3][0:3];
  reg  [7:0]  b [0:3][0:3];
  wire [31:0] c [0:3][0:3];
  initial begin
    a[0][0] = 8'd3;
    a[0][1] = 8'd2;
    a[0][2] = 8'd5;
    a[0][3] = 8'd1;
    a[1][0] = 8'd5;
    a[1][1] = 8'd4;
    a[1][2] = 8'd3;
    a[1][3] = 8'd3;
    a[2][0] = 8'd2;
    a[2][1] = 8'd3;
    a[2][2] = 8'd1;
    a[2][3] = 8'd5;
    a[3][0] = 8'd5;
    a[3][1] = 8'd3;
    a[3][2] = 8'd1;
    a[3][3] = 8'd1;
    b[0][0] = 8'd3;
    b[0][1] = 8'd3;
    b[0][2] = 8'd0;
    b[0][3] = 8'd5;
    b[1][0] = 8'd2;
    b[1][1] = 8'd4;
    b[1][2] = 8'd3;
    b[1][3] = 8'd7;
    b[2][0] = 8'd4;
    b[2][1] = 8'd3;
    b[2][2] = 8'd7;
    b[2][3] = 8'd1;
    b[3][0] = 8'd2;
    b[3][1] = 8'd3;
    b[3][2] = 8'd2;
    b[3][3] = 8'd3;
  end
  blas_gemm #(.N(4), .K(4), .M(4), .WA(8), .WB(8), .CW(32)) u (
    .a(a), .b(b), .c(c)
  );
endmodule
