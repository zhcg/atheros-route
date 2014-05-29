#include <stdlib.h>
#include <stdio.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const unsigned char ss1[4][16] = 
{ 
	{14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7}, 
	{0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8}, 
	{4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0}, 
	{15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13} 
}; 

/* Table - ss2 */ 
const unsigned char ss2[4][16] = 
{ 
	{15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10}, 
	{3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5}, 
	{0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15}, 
	{13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9} 
}; 

/* Table - ss3 */ 
const unsigned char ss3[4][16] = 
{ 
	{10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8}, 
	{13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1}, 
	{13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7}, 
	{1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12 }
}; 

/* Table - ss4 */ 
const unsigned char ss4[4][16] = 
{ 
	{7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15}, 
	{13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9}, 
	{10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4}, 
	{3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14} 
}; 

/* Table - ss5 */ 
const unsigned char ss5[4][16] = 
{ 
	{2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9}, 
	{14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6}, 
	{4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14}, 
	{11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3 }
}; 

/* Table - ss6 */ 
const unsigned char ss6[4][16] = 
{ 
	{12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11}, 
	{10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8}, 
	{9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6}, 
	{4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13 }
}; 

/* Table - ss7 */ 
const unsigned char ss7[4][16] = 
{ 
	{4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1}, 
	{13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6}, 
	{1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2}, 
	{6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12 }
}; 

/* Table - ss8 */ 
const unsigned char ss8[4][16] = 
{ 
	{13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7}, 
	{1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2}, 
	{7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8}, 
	{2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11 }
}; 


/* Table - Shift */ 
const unsigned char shift[16] = 
{1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1 }; 


/* Table - Binary */ 
const unsigned char binary[64] = 
{ 
	0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 
	0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 
	1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 
	1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1 
}; 


/*----------------------------------------------------------------------------------------
des 加密和解密算法.每次调用此函数只固定处理8个字节的数据
参数1:原始数据指针
参数2:输出数据缓冲指针
参数3:密钥
参数4:算法类型(1--加密;0--解密)
-----------------------------------------------------------------------------------------*/
void Des128Bits(unsigned char *source, unsigned char *dest, unsigned char *inkey, unsigned char flg) 
{ 
	unsigned char bufout[64];
	unsigned char kwork[56];
	unsigned char worka[48];
	unsigned char kn[48];
	unsigned char buffer[64];
	unsigned char t_key[64]; 	
	
	unsigned char nbrofshift, temp1, temp2; 
	int valindex; 
	int i, j, k, iter; 

	/* MAIN PROCESS */ 
	/* Convert from 64-bit key into 64-byte key */ 
	for (i = 0; i < 8; i++) 
	{ 
		j = *(inkey + i);
		t_key[8 * i] = (j / 128) % 2; 
		t_key[8 * i + 1] = (j / 64) % 2; 
		t_key[8 * i + 2] = (j / 32) % 2; 
		t_key[8 * i + 3] = (j / 16) % 2; 
		t_key[8 * i + 4] = (j / 8) % 2; 
		t_key[8 * i + 5] = (j / 4) % 2; 
		t_key[8 * i + 6] = (j / 2) % 2; 
		t_key[8 * i + 7] = j % 2; 
	} 

	/* Convert from 64-bit data into 64-byte data */ 
	for (i = 0; i < 8; i++) 
	{ 
		j = *(source  + i);	
		buffer[8 * i] = (j / 128) % 2; 
		buffer[8 * i + 1] = (j / 64) % 2; 
		buffer[8 * i + 2] = (j / 32) % 2; 
		buffer[8 * i + 3] = (j / 16) % 2; 
		buffer[8 * i + 4] = (j / 8) % 2; 
		buffer[8 * i + 5] = (j / 4) % 2; 
		buffer[8 * i + 6] = (j / 2) % 2; 
		buffer[8 * i + 7] = j % 2; 
	} 

	/* Initial Permutation of Data */ 
	bufout[ 0] = buffer[57]; 
	bufout[ 1] = buffer[49]; 
	bufout[ 2] = buffer[41]; 
	bufout[ 3] = buffer[33]; 
	bufout[ 4] = buffer[25]; 
	bufout[ 5] = buffer[17]; 
	bufout[ 6] = buffer[ 9]; 
	bufout[ 7] = buffer[ 1]; 
	bufout[ 8] = buffer[59]; 
	bufout[ 9] = buffer[51]; 
	bufout[10] = buffer[43]; 
	bufout[11] = buffer[35]; 
	bufout[12] = buffer[27]; 
	bufout[13] = buffer[19]; 
	bufout[14] = buffer[11]; 
	bufout[15] = buffer[ 3]; 
	bufout[16] = buffer[61]; 
	bufout[17] = buffer[53]; 
	bufout[18] = buffer[45]; 
	bufout[19] = buffer[37]; 
	bufout[20] = buffer[29]; 
	bufout[21] = buffer[21]; 
	bufout[22] = buffer[13]; 
	bufout[23] = buffer[ 5]; 
	bufout[24] = buffer[63]; 
	bufout[25] = buffer[55]; 
	bufout[26] = buffer[47]; 
	bufout[27] = buffer[39]; 
	bufout[28] = buffer[31]; 
	bufout[29] = buffer[23]; 
	bufout[30] = buffer[15]; 
	bufout[31] = buffer[ 7]; 
	bufout[32] = buffer[56]; 
	bufout[33] = buffer[48]; 
	bufout[34] = buffer[40]; 
	bufout[35] = buffer[32]; 
	bufout[36] = buffer[24]; 
	bufout[37] = buffer[16]; 
	bufout[38] = buffer[ 8]; 
	bufout[39] = buffer[ 0]; 
	bufout[40] = buffer[58]; 
	bufout[41] = buffer[50]; 
	bufout[42] = buffer[42]; 
	bufout[43] = buffer[34]; 
	bufout[44] = buffer[26]; 
	bufout[45] = buffer[18]; 
	bufout[46] = buffer[10]; 
	bufout[47] = buffer[ 2]; 
	bufout[48] = buffer[60]; 
	bufout[49] = buffer[52]; 
	bufout[50] = buffer[44]; 
	bufout[51] = buffer[36]; 
	bufout[52] = buffer[28]; 
	bufout[53] = buffer[20]; 
	bufout[54] = buffer[12]; 
	bufout[55] = buffer[ 4]; 
	bufout[56] = buffer[62]; 
	bufout[57] = buffer[54]; 
	bufout[58] = buffer[46]; 
	bufout[59] = buffer[38]; 
	bufout[60] = buffer[30]; 
	bufout[61] = buffer[22]; 
	bufout[62] = buffer[14]; 
	bufout[63] = buffer[ 6]; 

	/* Initial Permutation of Key */ 
	kwork[ 0] = t_key[56]; 
	kwork[ 1] = t_key[48]; 
	kwork[ 2] = t_key[40]; 
	kwork[ 3] = t_key[32]; 
	kwork[ 4] = t_key[24]; 
	kwork[ 5] = t_key[16]; 
	kwork[ 6] = t_key[ 8]; 
	kwork[ 7] = t_key[ 0]; 
	kwork[ 8] = t_key[57]; 
	kwork[ 9] = t_key[49]; 
	kwork[10] = t_key[41]; 
	kwork[11] = t_key[33]; 
	kwork[12] = t_key[25]; 
	kwork[13] = t_key[17]; 
	kwork[14] = t_key[ 9]; 
	kwork[15] = t_key[ 1]; 
	kwork[16] = t_key[58]; 
	kwork[17] = t_key[50]; 
	kwork[18] = t_key[42]; 
	kwork[19] = t_key[34]; 
	kwork[20] = t_key[26]; 
	kwork[21] = t_key[18]; 
	kwork[22] = t_key[10]; 
	kwork[23] = t_key[ 2]; 
	kwork[24] = t_key[59]; 
	kwork[25] = t_key[51]; 
	kwork[26] = t_key[43]; 
	kwork[27] = t_key[35]; 
	kwork[28] = t_key[62]; 
	kwork[29] = t_key[54]; 
	kwork[30] = t_key[46]; 
	kwork[31] = t_key[38]; 
	kwork[32] = t_key[30]; 
	kwork[33] = t_key[22]; 
	kwork[34] = t_key[14]; 
	kwork[35] = t_key[ 6]; 
	kwork[36] = t_key[61]; 
	kwork[37] = t_key[53]; 
	kwork[38] = t_key[45]; 
	kwork[39] = t_key[37]; 
	kwork[40] = t_key[29]; 
	kwork[41] = t_key[21]; 
	kwork[42] = t_key[13]; 
	kwork[43] = t_key[ 5]; 
	kwork[44] = t_key[60]; 
	kwork[45] = t_key[52]; 
	kwork[46] = t_key[44]; 
	kwork[47] = t_key[36]; 
	kwork[48] = t_key[28]; 
	kwork[49] = t_key[20]; 
	kwork[50] = t_key[12]; 
	kwork[51] = t_key[ 4]; 
	kwork[52] = t_key[27]; 
	kwork[53] = t_key[19]; 
	kwork[54] = t_key[11]; 
	kwork[55] = t_key[ 3]; 

	/* 16 Iterations */ 
	for (iter = 1; iter < 17; iter++) 
	{ 
		for (i = 0; i < 32; i++) 
			buffer[i] = bufout[32 + i]; 

		/* Calculation of F(R, K) */ 
		/* Permute - E */ 
		worka[ 0] = buffer[31]; 
		worka[ 1] = buffer[ 0]; 
		worka[ 2] = buffer[ 1]; 
		worka[ 3] = buffer[ 2]; 
		worka[ 4] = buffer[ 3]; 
		worka[ 5] = buffer[ 4]; 
		worka[ 6] = buffer[ 3]; 
		worka[ 7] = buffer[ 4]; 
		worka[ 8] = buffer[ 5]; 
		worka[ 9] = buffer[ 6]; 
		worka[10] = buffer[ 7]; 
		worka[11] = buffer[ 8]; 
		worka[12] = buffer[ 7]; 
		worka[13] = buffer[ 8]; 
		worka[14] = buffer[ 9]; 
		worka[15] = buffer[10]; 
		worka[16] = buffer[11]; 
		worka[17] = buffer[12]; 
		worka[18] = buffer[11]; 
		worka[19] = buffer[12]; 
		worka[20] = buffer[13]; 
		worka[21] = buffer[14]; 
		worka[22] = buffer[15]; 
		worka[23] = buffer[16]; 
		worka[24] = buffer[15]; 
		worka[25] = buffer[16]; 
		worka[26] = buffer[17]; 
		worka[27] = buffer[18]; 
		worka[28] = buffer[19]; 
		worka[29] = buffer[20]; 
		worka[30] = buffer[19]; 
		worka[31] = buffer[20]; 
		worka[32] = buffer[21]; 
		worka[33] = buffer[22]; 
		worka[34] = buffer[23]; 
		worka[35] = buffer[24]; 
		worka[36] = buffer[23]; 
		worka[37] = buffer[24]; 
		worka[38] = buffer[25]; 
		worka[39] = buffer[26]; 
		worka[40] = buffer[27]; 
		worka[41] = buffer[28]; 
		worka[42] = buffer[27]; 
		worka[43] = buffer[28]; 
		worka[44] = buffer[29]; 
		worka[45] = buffer[30]; 
		worka[46] = buffer[31]; 
		worka[47] = buffer[ 0]; 

		/* KS Function Begin */// 加密
		if (!flg) 
		{ 
			nbrofshift = shift[iter - 1]; 
			for (i = 0; i < (int) nbrofshift; i++) 
			{ 
				temp1 = kwork[0]; 
				temp2 = kwork[28]; 
				for (j = 0; j < 27; j++) 
				{ 
					kwork[j] = kwork[j + 1]; 
					kwork[j+28] = kwork[j + 29]; 
				} 
				kwork[27] = temp1; 
				kwork[55] = temp2; 
			} 
		} 
		else if (iter > 1) 
		{ 
			nbrofshift = shift[17 - iter]; 
			for (i = 0; i < (int) nbrofshift; i++) 
			{ 
				temp1 = kwork[27]; 
				temp2 = kwork[55]; 
				for (j = 27; j > 0; j--) 
				{ 
					kwork[j] = kwork[j-1]; 
					kwork[j+28] = kwork[j+27]; 
				} 
				kwork[0] = temp1; 
				kwork[28] = temp2; 
			} 
		} 

		/* Permute kwork - PC2 */ 
		kn[ 0] = kwork[13]; 
		kn[ 1] = kwork[16]; 
		kn[ 2] = kwork[10]; 
		kn[ 3] = kwork[23]; 
		kn[ 4] = kwork[ 0]; 
		kn[ 5] = kwork[ 4]; 
		kn[ 6] = kwork[ 2]; 
		kn[ 7] = kwork[27]; 
		kn[ 8] = kwork[14]; 
		kn[ 9] = kwork[ 5]; 
		kn[10] = kwork[20]; 
		kn[11] = kwork[ 9]; 
		kn[12] = kwork[22]; 
		kn[13] = kwork[18]; 
		kn[14] = kwork[11]; 
		kn[15] = kwork[ 3]; 
		kn[16] = kwork[25]; 
		kn[17] = kwork[ 7]; 
		kn[18] = kwork[15]; 
		kn[19] = kwork[ 6]; 
		kn[20] = kwork[26]; 
		kn[21] = kwork[19]; 
		kn[22] = kwork[12]; 
		kn[23] = kwork[ 1]; 
		kn[24] = kwork[40]; 
		kn[25] = kwork[51]; 
		kn[26] = kwork[30]; 
		kn[27] = kwork[36]; 
		kn[28] = kwork[46]; 
		kn[29] = kwork[54]; 
		kn[30] = kwork[29]; 
		kn[31] = kwork[39]; 
		kn[32] = kwork[50]; 
		kn[33] = kwork[44]; 
		kn[34] = kwork[32]; 
		kn[35] = kwork[47]; 
		kn[36] = kwork[43]; 
		kn[37] = kwork[48]; 
		kn[38] = kwork[38]; 
		kn[39] = kwork[55]; 
		kn[40] = kwork[33]; 
		kn[41] = kwork[52]; 
		kn[42] = kwork[45]; 
		kn[43] = kwork[41]; 
		kn[44] = kwork[49]; 
		kn[45] = kwork[35]; 
		kn[46] = kwork[28]; 
		kn[47] = kwork[31]; 
		/* KS Function End */ 

		/* worka XOR kn */ 
		for (i = 0; i < 48; i++) 
			worka[i] = worka[i] ^ kn[i]; 

		/* 8 s-functions */ 
		valindex = ss1[2 * worka[0] + worka[5]] [2 * (2 * (2 * worka[1] + worka[2]) + worka[3]) + worka[4]]; 
		valindex *= 4; 
		kn[ 0] = binary[0 + valindex]; 
		kn[ 1] = binary[1 + valindex]; 
		kn[ 2] = binary[2 + valindex]; 
		kn[ 3] = binary[3 + valindex]; 
		valindex = ss2[2*worka[6] + worka[11]] 
		[2 * (2 * (2 * worka[7] + worka[8]) + worka[9]) + worka[10]]; 
		valindex *= 4; 
		kn[ 4] = binary[0 + valindex]; 
		kn[ 5] = binary[1 + valindex]; 
		kn[ 6] = binary[2 + valindex]; 
		kn[ 7] = binary[3 + valindex]; 
		valindex = ss3[2 * worka[12] + worka[17]] 
		[2 * (2 * (2 * worka[13] + worka[14]) +	worka[15]) + worka[16]]; 
		valindex *= 4; 
		kn[ 8] = binary[0+valindex]; 
		kn[ 9] = binary[1+valindex]; 
		kn[10] = binary[2+valindex]; 
		kn[11] = binary[3+valindex]; 
		valindex = ss4[2 * worka[18] + worka[23]] 
		[2 * (2 * (2 * worka[19] + worka[20]) + worka[21]) + worka[22]]; 
		valindex *= 4; 
		kn[12] = binary[0 + valindex]; 
		kn[13] = binary[1 + valindex]; 
		kn[14] = binary[2 + valindex]; 
		kn[15] = binary[3 + valindex]; 
		valindex = ss5[2 * worka[24] + worka[29]] 
		[2 * (2 * (2 * worka[25] + worka[26]) +	worka[27]) + worka[28]]; 
		valindex *= 4; 
		kn[16] = binary[0 + valindex]; 
		kn[17] = binary[1 + valindex]; 
		kn[18] = binary[2 + valindex]; 
		kn[19] = binary[3 + valindex]; 
		valindex = ss6[2 * worka[30] + worka[35]] 
		[2 * (2 * (2 * worka[31] + worka[32]) + worka[33]) + worka[34]]; 
		valindex *= 4; 
		kn[20] = binary[0 + valindex]; 
		kn[21] = binary[1 + valindex]; 
		kn[22] = binary[2 + valindex]; 
		kn[23] = binary[3 + valindex]; 
		valindex = ss7[2 * worka[36] + worka[41]] 
		[2 * (2 * (2 * worka[37] + worka[38]) + worka[39]) + worka[40]]; 
		valindex *= 4;
		kn[24] = binary[0 + valindex]; 
		kn[25] = binary[1 + valindex]; 
		kn[26] = binary[2 + valindex]; 
		kn[27] = binary[3 + valindex]; 
		valindex = ss8[2 * worka[42] + worka[47]] 
		[2 * (2 * (2 * worka[43] + worka[44]) +	worka[45]) + worka[46]]; 
		valindex *= 4;
		kn[28] = binary[0 + valindex]; 
		kn[29] = binary[1 + valindex]; 
		kn[30] = binary[2 + valindex]; 
		kn[31] = binary[3 + valindex]; 

		/* Permute - P */ 
		worka[ 0] = kn[15]; 
		worka[ 1] = kn[ 6]; 
		worka[ 2] = kn[19]; 
		worka[ 3] = kn[20]; 
		worka[ 4] = kn[28]; 
		worka[ 5] = kn[11]; 
		worka[ 6] = kn[27]; 
		worka[ 7] = kn[16]; 
		worka[ 8] = kn[ 0]; 
		worka[ 9] = kn[14]; 
		worka[10] = kn[22]; 
		worka[11] = kn[25]; 
		worka[12] = kn[ 4]; 
		worka[13] = kn[17]; 
		worka[14] = kn[30]; 
		worka[15] = kn[ 9]; 
		worka[16] = kn[ 1]; 
		worka[17] = kn[ 7]; 
		worka[18] = kn[23]; 
		worka[19] = kn[13]; 
		worka[20] = kn[31]; 
		worka[21] = kn[26]; 
		worka[22] = kn[ 2]; 
		worka[23] = kn[ 8]; 
		worka[24] = kn[18]; 
		worka[25] = kn[12]; 
		worka[26] = kn[29]; 
		worka[27] = kn[ 5]; 
		worka[28] = kn[21]; 
		worka[29] = kn[10]; 
		worka[30] = kn[ 3]; 
		worka[31] = kn[24]; 

		/* bufout XOR worka */ 
		for (i = 0; i < 32; i++) 
		{ 
			bufout[i + 32] = bufout[i] ^ worka[i]; 
			bufout[i] = buffer[i]; 
		} 
	} /* End of Iter */ 

	/* Prepare Output */ 
	for (i = 0; i < 32; i++) 
	{ 
		j = bufout[i]; 
		bufout[i] = bufout[32 + i]; 
		bufout[32 + i] = j; 
	} 

	/* Inverse Initial Permutation */ 
	buffer[ 0] = bufout[39]; 
	buffer[ 1] = bufout[ 7]; 
	buffer[ 2] = bufout[47]; 
	buffer[ 3] = bufout[15]; 
	buffer[ 4] = bufout[55]; 
	buffer[ 5] = bufout[23]; 
	buffer[ 6] = bufout[63]; 
	buffer[ 7] = bufout[31]; 
	buffer[ 8] = bufout[38]; 
	buffer[ 9] = bufout[ 6]; 
	buffer[10] = bufout[46]; 
	buffer[11] = bufout[14]; 
	buffer[12] = bufout[54]; 
	buffer[13] = bufout[22]; 
	buffer[14] = bufout[62]; 
	buffer[15] = bufout[30]; 
	buffer[16] = bufout[37]; 
	buffer[17] = bufout[ 5]; 
	buffer[18] = bufout[45]; 
	buffer[19] = bufout[13]; 
	buffer[20] = bufout[53]; 
	buffer[21] = bufout[21]; 
	buffer[22] = bufout[61]; 
	buffer[23] = bufout[29]; 
	buffer[24] = bufout[36]; 
	buffer[25] = bufout[ 4]; 
	buffer[26] = bufout[44]; 
	buffer[27] = bufout[12]; 
	buffer[28] = bufout[52]; 
	buffer[29] = bufout[20]; 
	buffer[30] = bufout[60]; 
	buffer[31] = bufout[28]; 
	buffer[32] = bufout[35]; 
	buffer[33] = bufout[ 3]; 
	buffer[34] = bufout[43]; 
	buffer[35] = bufout[11]; 
	buffer[36] = bufout[51]; 
	buffer[37] = bufout[19]; 
	buffer[38] = bufout[59]; 
	buffer[39] = bufout[27]; 
	buffer[40] = bufout[34]; 
	buffer[41] = bufout[ 2]; 
	buffer[42] = bufout[42]; 
	buffer[43] = bufout[10]; 
	buffer[44] = bufout[50]; 
	buffer[45] = bufout[18]; 
	buffer[46] = bufout[58]; 
	buffer[47] = bufout[26]; 
	buffer[48] = bufout[33]; 
	buffer[49] = bufout[ 1]; 
	buffer[50] = bufout[41]; 
	buffer[51] = bufout[ 9]; 
	buffer[52] = bufout[49]; 
	buffer[53] = bufout[17]; 
	buffer[54] = bufout[57]; 
	buffer[55] = bufout[25]; 
	buffer[56] = bufout[32]; 
	buffer[57] = bufout[ 0]; 
	buffer[58] = bufout[40]; 
	buffer[59] = bufout[ 8]; 
	buffer[60] = bufout[48]; 
	buffer[61] = bufout[16]; 
	buffer[62] = bufout[56]; 
	buffer[63] = bufout[24]; 

	j = 0; 
	for (i = 0; i < 8; i++) 
	{ 
		*(dest + i) = 0x00; 
		for (k = 0; k < 7; k++) 
			*(dest + i) = ((*(dest + i)) + buffer[j + k]) * 2; 
		*(dest + i) = *(dest + i) + buffer[j + 7]; 
		j += 8; 
	} 
}
/*----------------------------------------------------------------------------------------
des cbc加密和解密算法
DES CBC算法:
以KEY作为DES算法的密钥，初始数据为8字节的"0X00"，对第一分组数据与初始数据异或后的数据进行
运算，DES算法的输出数据与第二分组数据异或后，继续以KEY为密钥进行DES运算，依次做下去直至
最后一个分组处理完毕

参数1:原始数据指针
参数2:原始数据长度(必须为8的整数倍)
参数3:输出数据缓冲指针
参数4:密钥
参数5:算法类型(0--加密;1--解密)
-----------------------------------------------------------------------------------------*/
void DesCbc(unsigned char *pin_data,int in_len,unsigned char *pout_data,unsigned char *p_inkey,int flag)
{
	int i = 0,bytes = 0;
	unsigned char tbuf1[8];
	unsigned char tbuf2[8];
//	unsigned char tbuf3[8];
	unsigned char *ppin,*ppout;
	ppin = pin_data;
	ppout = pout_data;
	
	if(flag == 0)//encode
	{
		memset((void *)(tbuf1),0,sizeof(tbuf1));
		bytes = 0;
		while(bytes < in_len)
		{
			for(i = 0;i < 8;i++)
				tbuf2[i] = *(ppin++) ^ tbuf1[i];	
			Des128Bits((unsigned char *)tbuf2,(unsigned char *)tbuf1,(unsigned char *)p_inkey,0);
			memcpy((void *)(ppout),(void *)(tbuf1),sizeof(tbuf1));
			ppout += 8;
			bytes += 8;
		}
	}
	else//decode
	{
		memset((void *)(tbuf1),0,sizeof(tbuf1));
		while(bytes < in_len)
		{
			Des128Bits((unsigned char *)ppin,(unsigned char *)tbuf2,(unsigned char *)p_inkey,1);
			for(i = 0;i < 8;i++)
				tbuf1[i] ^= tbuf2[i];			
			memcpy((void *)(ppout),(void *)(tbuf1),sizeof(tbuf1));
			memcpy((void *)(tbuf1),(void *)(ppin),sizeof(tbuf1));
			ppout += 8;
			ppin += 8;
			bytes += 8;			
		}
	}
}
/*----------------------------------------------------------------------------------------
des ecb加密和解密算法
DES ecb算法:
以KEY作为DES算法的密钥，初始数据为8字节的"0X00"，对第一分组数据与初始数据异或后的数据进行
运算，DES算法的输出数据与第二分组数据异或后，继续以KEY为密钥进行DES运算，依次做下去直至
最后一个分组处理完毕

参数1:原始数据指针
参数2:原始数据长度(必须为8的整数倍)
参数3:输出数据缓冲指针
参数4:密钥
参数5:算法类型(0--加密;1--解密)
-----------------------------------------------------------------------------------------*/
void DesEcb(unsigned char *pin_data,int in_len,unsigned char *pout_data,unsigned char *p_inkey,int flag)
{
	int i = 0,bytes = 0;

	unsigned char *ppin,*ppout;
	ppin = pin_data;
	ppout = pout_data;
	
	if(flag == 0)//encode
	{
		bytes = 0;
		while(bytes < in_len)
		{	
			Des128Bits((unsigned char *)(&pin_data[bytes]),(unsigned char *)(&pout_data[bytes]),(unsigned char *)p_inkey,0);
			bytes += 8;
		}
	}
	else//decode
	{
		bytes = 0;
		while(bytes < in_len)
		{	
			Des128Bits((unsigned char *)(&pin_data[bytes]),(unsigned char *)(&pout_data[bytes]),(unsigned char *)p_inkey,1);
			bytes += 8;
		}
	}
}
#if 0
int main(void)
{
	unsigned char plant_data[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};	
	unsigned char cyber_data[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};	
	unsigned char key_data[8] = {8,9,10,11,12,13,14,15};
	int i;
printf("before encode plant_data::");
for(i = 0;i < 16;i++)
printf("%02X",plant_data[i]);
printf("\r\n");		
	
	DesEcb(plant_data,16,cyber_data,key_data,0);
		
printf("after encode cyber_data::");
for(i = 0;i < 16;i++)
printf("%02X",cyber_data[i]);
printf("\r\n");	

printf("before decode encode::");
for(i = 0;i < 16;i++)
printf("%02X",cyber_data[i]);
printf("\r\n");		
	
	DesEcb(cyber_data,16,plant_data,key_data,1);
		
printf("after decode plant_data::");
for(i = 0;i < 16;i++)
printf("%02X",plant_data[i]);
printf("\r\n");	
	return 1;
}
#endif
