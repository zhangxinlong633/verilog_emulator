// @vs view matrix A rows=2 cols=2 cells=a00,a01;a10,a11
// @vs view matrix B rows=2 cols=2 cells=b00,b01;b10,b11
// @vs view matrix C rows=2 cols=2 cells=c00,c01;c10,c11
// @vs op matmul out=C left=A right=B
// @vs expr c00 = a00*b00+a01*b10
// @vs expr c01 = a00*b01+a01*b11
// @vs expr c10 = a10*b00+a11*b10
// @vs expr c11 = a10*b01+a11*b11
// @vs view matrix
module dummy;
endmodule
