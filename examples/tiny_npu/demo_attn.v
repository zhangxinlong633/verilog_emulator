// Auto-generated: hard attention O = onehot(argmax(Q·Kᵀ)) · V
// @vs view matrix Q array=q rows=4 cols=4
// @vs view matrix K array=k rows=4 cols=4
// @vs view matrix V array=v rows=4 cols=4
// @vs view matrix S array=s rows=4 cols=4
// @vs view matrix A array=a rows=4 cols=4
// @vs view matrix O array=o rows=4 cols=4
module demo_attn;
  reg  [31:0] q [0:3][0:3];
  reg  [31:0] k [0:3][0:3];
  reg  [31:0] v [0:3][0:3];
  wire [31:0] s [0:3][0:3];
  wire [31:0] a [0:3][0:3];
  wire [31:0] o [0:3][0:3];
  initial begin
    q[0][0] = 32'd35;
    q[0][1] = 32'd35;
    q[0][2] = 32'd43;
    q[0][3] = 32'd37;
    q[1][0] = 32'd41;
    q[1][1] = 32'd49;
    q[1][2] = 32'd39;
    q[1][3] = 32'd65;
    q[2][0] = 32'd26;
    q[2][1] = 32'd36;
    q[2][2] = 32'd26;
    q[2][3] = 32'd47;
    q[3][0] = 32'd27;
    q[3][1] = 32'd33;
    q[3][2] = 32'd18;
    q[3][3] = 32'd50;
    k[0][0] = 32'd35;
    k[0][1] = 32'd21;
    k[0][2] = 32'd40;
    k[0][3] = 32'd20;
    k[1][0] = 32'd49;
    k[1][1] = 32'd27;
    k[1][2] = 32'd48;
    k[1][3] = 32'd36;
    k[2][0] = 32'd35;
    k[2][1] = 32'd16;
    k[2][2] = 32'd26;
    k[2][3] = 32'd20;
    k[3][0] = 32'd33;
    k[3][1] = 32'd19;
    k[3][2] = 32'd36;
    k[3][3] = 32'd32;
    v[0][0] = 32'd52;
    v[0][1] = 32'd30;
    v[0][2] = 32'd15;
    v[0][3] = 32'd41;
    v[1][0] = 32'd60;
    v[1][1] = 32'd44;
    v[1][2] = 32'd23;
    v[1][3] = 32'd43;
    v[2][0] = 32'd50;
    v[2][1] = 32'd45;
    v[2][2] = 32'd16;
    v[2][3] = 32'd33;
    v[3][0] = 32'd30;
    v[3][1] = 32'd23;
    v[3][2] = 32'd16;
    v[3][3] = 32'd21;
  end
  blas_gemm_bt #(.N(4), .K(4), .M(4), .WA(32), .WB(32), .CW(32)) u_s (
    .a(q), .b(k), .c(s)
  );
  blas_row_argmax #(.T(4), .W(32)) u_a (
    .s(s), .a(a)
  );
  blas_gemm #(.N(4), .K(4), .M(4), .WA(32), .WB(32), .CW(32)) u_o (
    .a(a), .b(v), .c(o)
  );
endmodule
