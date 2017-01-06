// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_set>

#include <gtest/gtest.h>  // NOLINT

#include "Common/Common.h"
#include "Common/CommonFuncs.h"
#include "Common/MathUtil.h"
#include "VideoCommon/TextureDecoder.h"

#define TEST_WIDTH 16
#define TEST_HEIGHT 16

// Texture formats explanation:
//  http://hitmen.c02.at/files/yagcd/yagcd/chap15.html
//  http://wiki.tockdom.com/wiki/Image_Formats#RGB565

void printTestImage(u32* img);

int testDecoder_DecodeC4(u32 *example, u32 *dst, u8 *src, int width, int height, int texformat, u16 *tlut, TlutFormat tlutfmt);

int testDecoder_DecodeC8(u32 *example, u32 *dst, u8 *src, int width, int height, int texformat, u16 *tlut, TlutFormat tlutfmt);

int testDecoder_DecodeC14X2(u32 *example, u32 *dst, u16 *src, int width, int height, int texformat, u16 *tlut, TlutFormat tlutfmt);

/* Print a test image in the console */
void printTestImage(u32* img) {
	int i, j;

	for (j=0; j<TEST_HEIGHT; j++) {
		for (i=0; i<TEST_WIDTH; i++) {
			printf("%08x ", img[j*TEST_WIDTH + i]);
		}
		printf("\n");
	}

}

/* ===============================================================================================*/
/* Test a C4 (4 bit color index, 8x8 tiles) texture decoder */
int testDecoder_DecodeC4(u32 *example, u32 *dst, u8 *src, int width, int height, int texformat, u16 *tlut, TlutFormat tlutfmt) {
	int comp = 0;
	int i, j, ti, tj, val, index;

	// Initialize texture (8x8 tiles)
	index = 0;
	for (j=0; j<TEST_HEIGHT; j+=8) {
		for (i=0; i<TEST_WIDTH/2; i+=4) {
			for (tj=0; tj < 8; tj++) {
				val = 0;
				if ((i/4)%2 == 1) {
					val = 8;
				}
				for (ti=0; ti < 4; ti++) {
					src[index++] = ((val << 4) | (val+1));
					val+=2;
				}
	
			}
		}
	}

	// Run test
	TexDecoder_Decode((u8*) dst, (u8*) src, width, height, texformat, (u8*) tlut, tlutfmt);

	// Compare the results
	comp = memcmp ( &example[0], &dst[0], TEST_WIDTH*TEST_HEIGHT*4);

	return comp;
}

/* ===============================================================================================*/
/* Test a C8 (8 bit color index, 8x4 tiles) texture decoder */
int testDecoder_DecodeC8(u32 *example, u32 *dst, u8 *src, int width, int height, int texformat, u16 *tlut, TlutFormat tlutfmt) {
	int comp = 0;
	int i, j, ti, tj, val, index;

	// Initialize texture (8x4 tiles)
	index = 0;
	for (j=0; j<TEST_HEIGHT; j+=4) {
		for (i=0; i<TEST_WIDTH; i+=8) {
			for (tj=0; tj < 4; tj++) {
				val = 0;
				if ((i/8)%2 == 1) {
					val = 8;
				}
				for (ti=0; ti < 8; ti++) {
					src[index++] = val++;
				}
			}
		}
	}

	// Run test
	TexDecoder_Decode((u8*) dst, (u8*) src, width, height, texformat, (u8*) tlut, tlutfmt);

	// Compare the results
	comp = memcmp ( &example[0], &dst[0], TEST_WIDTH*TEST_HEIGHT*4);

	return comp;
}


/* ===============================================================================================*/
/* Test a C14X2 (14 bit color index, 4x4 tiles) texture decoder */
int testDecoder_DecodeC14X2(u32 *example, u32 *dst, u16 *src, int width, int height, int texformat, u16 *tlut, TlutFormat tlutfmt) {
	int comp = 0;
	int i, j, ti, tj, val, index;

	// Initialize texture (4x4 tiles)
	index = 0;
	for (j=0; j<TEST_HEIGHT; j+=4) {
		for (i=0; i<TEST_WIDTH; i+=4) {
			for (tj=0; tj < 4; tj++) {
				val = 0;
				if ((i/4)%4 == 1) {
					val = 4;
				}
				if ((i/4)%4 == 2) {
					val = 8;
				}
				if ((i/4)%4 == 3) {
					val = 12;
				}
				for (ti=0; ti < 4; ti++) {
					src[index++] = Common::swap16(val++);
				}
			}
		}
	}

	// Run test
	TexDecoder_Decode((u8*) dst, (u8*) src, width, height, texformat, (u8*) tlut, tlutfmt);

	// Compare the results
	comp = memcmp ( &example[0], &dst[0], TEST_WIDTH*TEST_HEIGHT*4);

	return comp;
}



/* ===============================================================================================*/
/* CI4 (4 bit color index, 8x8 tiles) in IA8 format */
TEST (TextureDecoderTest, DecodeBytes_C4_IA8) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*(TEST_WIDTH/2)];
    int texformat = GX_TF_C4;

	// 4 bit color index = 16 colors
    u16 tlut[16] = {
		0x00FF, // Black
		0x10FF,
		0x20FF,
		0x30FF,

		0x40FF,
		0x50FF,
		0x60FF,
		0x70FF,

		0x80FF, // Gray
		0x90FF,
		0xA0FF,
		0xB0FF,

		0xC0FF, // Silver
		0xD0FF,
		0xE0FF,
		0xFFFF // White
	};
    TlutFormat tlutfmt = GX_TL_IA8;

	int i, j, val;
	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = (tlut[j] >> 8) & 0xFF;
			example[i+j] = (0xFF << 24) | (val << 16) | (val << 8) | val;
		}
	}

	comp = testDecoder_DecodeC4(example, dst, src, width, height, texformat, tlut, tlutfmt);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);
}

/* ===============================================================================================*/
/* CI4 (4 bit color index, 8x8 tiles) in RGB565 format */
TEST (TextureDecoderTest, DecodeBytes_C4_RGB565) { 
	int r,g,b;
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*(TEST_WIDTH/2)];
    int texformat = GX_TF_C4;

	// 4 bit color index = 16 colors
    u16 tlut[16] = {
		0x0000, // Black
		0xF800, // Red
		0x07E0, // Green
		0x001F, // Blue

		0x000F,
		0x00F0,
		0x0F00,
		0xF000,

		0x1111,
		0x2222,
		0x3333,
		0x4444,

		0x5555,
		0x6666,
		0x7777,
		0xFFFF // White
	};
    TlutFormat tlutfmt = GX_TL_RGB565;

	int i, j, val;

	// Swap the color bytes to make them BIG ENDIAN
	for (i=0; i<16; i++) {
		tlut[i] = Common::swap16(tlut[i]);
	}
	

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = Common::swap16(tlut[j]) & 0xFFFF;

			r = ((val >> 11) & 0x1f) << 3;
			g = ((val >> 5) & 0x3f) << 2;
			b = ((val)&0x1f) << 3;
			example[i+j] = (0xFF << 24) | (b << 16) | (g << 8) | r;
		}
	}

	comp = testDecoder_DecodeC4(example, dst, src, width, height, texformat, tlut, tlutfmt);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);

}


/* ===============================================================================================*/
/* CI4 (4 bit color index, 8x8 tiles) in RGB5A3 format */
TEST (TextureDecoderTest, DecodeBytes_C4_RGB5A3) { 
	int r,g,b, a;
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*(TEST_WIDTH/2)];
    int texformat = GX_TF_C4;

	// 4 bit color index = 16 colors
    u16 tlut[16] = {
		0x7000, // Black
		0x7F00, // Red
		0x70F0, // Green
		0x700F, // Blue

		0x0000, // Transparent
		0x5F00, 
		0x50F0, 
		0x500F,

		0xFC00, // Red
		0x83E0, // Green
		0x801F, // Blue
		0x4444,

		0x5555,
		0x6666,
		0x7777,
		0xFFFF // White
	};
    TlutFormat tlutfmt = GX_TL_RGB5A3;

	int i, j, val;

	// Swap the color bytes to make them BIG ENDIAN
	for (i=0; i<16; i++) {
		tlut[i] = Common::swap16(tlut[i]);
	}
	

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = Common::swap16(tlut[j]) & 0xFFFF;

		  if ((val & 0x8000))
		  {
			r = ((val >> 10) & 0x1f) << 3;
			g = ((val >> 5) & 0x1f) << 3;
			b = ((val)&0x1f) << 3;
			a = 0xFF;
		  }
		  else
		  {
			a = ((val >> 12) & 0x7) << 5;
			r = ((val >> 8) & 0xf) << 4;
			g = ((val >> 4) & 0xf) << 4;
			b = ((val)&0xf) << 4;
		  }
		  example[i+j] = ( r | (g << 8) | (b << 16) | (a << 24) );
		}
	}

	comp = testDecoder_DecodeC4(example, dst, src, width, height, texformat, tlut, tlutfmt);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);

}


/* ===============================================================================================*/
/* I4 (4 bit gray scale color, 8x8 tiles) */
TEST (TextureDecoderTest, DecodeBytes_I4) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*(TEST_WIDTH/2)];
    int texformat = GX_TF_I4;

    u16* tlut = NULL;
    TlutFormat tlutfmt = (TlutFormat) 0;

	int i, j, val;

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
		  val = (j << 4) | j;
		  example[i+j] = ( val | (val << 8) | (val << 16) | (val << 24) );
		}
	}

	comp = testDecoder_DecodeC4(example, dst, src, width, height, texformat, tlut, tlutfmt);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);

}


/* ===============================================================================================*/
/* C8 (8 bit color index, 8x4 tiles) in IA8 format */
TEST (TextureDecoderTest, DecodeBytes_C8_IA8) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*(TEST_WIDTH)];
    int texformat = GX_TF_C8;

	// 8 bit color index = 256 colors
    u16 tlut[256];
    TlutFormat tlutfmt = GX_TL_IA8;

	int i, j, val;

	// Initialize the color index
	for (i = 0; i < 256; i++) {
		tlut[i] = 0;
	}

    tlut[0] = 0x00FF; // Black
	tlut[1] = 0x10FF;
	tlut[2] = 0x20FF;
	tlut[3] = 0x30FF;
	tlut[4] = 0x40FF;
	tlut[5] = 0x50FF;
	tlut[6] = 0x60FF;
	tlut[7] = 0x70FF;
	tlut[8] = 0x80FF; // Gray
	tlut[9] = 0x90FF;
	tlut[10] = 0xA0FF;
	tlut[11] = 0xB0FF;
	tlut[12] = 0xC0FF; // Silver
	tlut[13] = 0xD0FF;
	tlut[14] = 0xE0FF;
    tlut[15] = 0xFFFF; // White

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = (tlut[j] >> 8) & 0xFF;
			example[i+j] = (0xFF << 24) | (val << 16) | (val << 8) | val;
		}
	}

	comp = testDecoder_DecodeC8(example, dst, src, width, height, texformat, tlut, tlutfmt);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);
}

/* ===============================================================================================*/
/* C8 (8 bit color index, 8x4 tiles) in RGB565 format */
TEST (TextureDecoderTest, DecodeBytes_C8_RGB565) { 
	int r,g,b;
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*TEST_WIDTH];
    int texformat = GX_TF_C8;

	// 8 bit color index = 256 colors
    u16 tlut[256];
    TlutFormat tlutfmt = GX_TL_RGB565;

	int i, j, val;

	// Initialize the color index
	for (i = 0; i < 256; i++) {
		tlut[i] = 0;
	}

    tlut[0] = 0x0000; // Black
	tlut[1] = 0xF800; // Red
	tlut[2] = 0x07E0; // Green
	tlut[3] = 0x001F; // Blue

	tlut[4] = 0x000F;
	tlut[5] = 0x00F0;
	tlut[6] = 0x0F00;
	tlut[7] = 0xF000;

	tlut[8] = 0x1111; // Gray
	tlut[9] = 0x2222;
	tlut[10] = 0x3333;
	tlut[11] = 0x4444;

	tlut[12] = 0x5555; 
	tlut[13] = 0x6666;
	tlut[14] = 0x7777;
    tlut[15] = 0xFFFF; // White


	// Swap the color bytes to make them BIG ENDIAN
	for (i=0; i<16; i++) {
		tlut[i] = Common::swap16(tlut[i]);
	}
	

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = Common::swap16(tlut[j]) & 0xFFFF;

			r = ((val >> 11) & 0x1f) << 3;
			g = ((val >> 5) & 0x3f) << 2;
			b = ((val)&0x1f) << 3;
			example[i+j] = (0xFF << 24) | (b << 16) | (g << 8) | r;
		}
	}

	comp = testDecoder_DecodeC8(example, dst, src, width, height, texformat, tlut, tlutfmt);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);

}


/* ===============================================================================================*/
/* CI8 (8 bit color index, 8x4 tiles) in RGB5A3 format */
TEST (TextureDecoderTest, DecodeBytes_C8_RGB5A3) { 
	int r,g,b, a;
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*TEST_WIDTH];
    int texformat = GX_TF_C8;

	// 8 bit color index = 256 colors
    u16 tlut[256];

    TlutFormat tlutfmt = GX_TL_RGB5A3;

	int i, j, val;

	// Initialize the color index
	for (i = 0; i < 256; i++) {
		tlut[i] = 0;
	}

    tlut[0] = 0x7000; // Black
	tlut[1] = 0x7F00; // Red
	tlut[2] = 0x70F0; // Green
	tlut[3] = 0x700F; // Blue

	tlut[4] = 0x0000; // Transparent
	tlut[5] = 0x5F00;
	tlut[6] = 0x50F0;
	tlut[7] = 0x500F;

	tlut[8] = 0xFC00; // Red
	tlut[9] = 0x83E0; // Green
	tlut[10] = 0x801F; // Blue
	tlut[11] = 0x4444;

	tlut[12] = 0x5555; 
	tlut[13] = 0x6666;
	tlut[14] = 0x7777;
    tlut[15] = 0xFFFF; // White


	// Swap the color bytes to make them BIG ENDIAN
	for (i=0; i<16; i++) {
		tlut[i] = Common::swap16(tlut[i]);
	}
	

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = Common::swap16(tlut[j]) & 0xFFFF;

		  if ((val & 0x8000))
		  {
			r = ((val >> 10) & 0x1f) << 3;
			g = ((val >> 5) & 0x1f) << 3;
			b = ((val)&0x1f) << 3;
			a = 0xFF;
		  }
		  else
		  {
			a = ((val >> 12) & 0x7) << 5;
			r = ((val >> 8) & 0xf) << 4;
			g = ((val >> 4) & 0xf) << 4;
			b = ((val)&0xf) << 4;
		  }
		  example[i+j] = ( r | (g << 8) | (b << 16) | (a << 24) );
		}
	}

	comp = testDecoder_DecodeC8(example, dst, src, width, height, texformat, tlut, tlutfmt);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);

}

/* ===============================================================================================*/
/* I8 (8 bit gray scale color, 8x4 tiles) */
TEST (TextureDecoderTest, DecodeBytes_I8) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*TEST_WIDTH];
    int texformat = GX_TF_I8;

    u16* tlut = NULL;
    TlutFormat tlutfmt = (TlutFormat) 0;

	int i, j, val;

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
		  val = j;
		  example[i+j] = ( val | (val << 8) | (val << 16) | (val << 24) );
		}
	}

	comp = testDecoder_DecodeC8(example, dst, src, width, height, texformat, tlut, tlutfmt);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);

}


/* ===============================================================================================*/
/* IA4 (4 bit gray scale color + 4 bits alpha, 8x4 tiles) */
TEST (TextureDecoderTest, DecodeBytes_IA4) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*TEST_WIDTH];
    int texformat = GX_TF_IA4;

    u16* tlut = NULL;
    TlutFormat tlutfmt = (TlutFormat) 0;

	int i, j, val, alpha;

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
		  val = (j<<4 | j);
		  alpha = ((15-j) << 4) | (15-j);
		  example[i+j] = ( val | (val << 8) | (val << 16) | (alpha << 24) );
		}
	}


	int ti, tj, index;

	// Initialize texture (8x8 tiles)
	index = 0;
	for (j=0; j<TEST_HEIGHT; j+=4) {
		for (i=0; i<TEST_WIDTH; i+=8) {
			for (tj=0; tj < 4; tj++) {
				val = 0;
				if ((i/8)%2 == 1) {
					val = 8;
				}
				for (ti=0; ti < 8; ti++) {
					alpha = (15-val) & 0xf;
					src[index++] = (val | ((alpha) << 4));
					val++;
				}
			}
		}
	}

	// Run test
	TexDecoder_Decode((u8*) dst, (u8*) src, width, height, texformat, (u8*) tlut, tlutfmt);

	// Compare the results
	comp = memcmp ( &example[0], &dst[0], TEST_WIDTH*TEST_HEIGHT*4);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);

}


/* ===============================================================================================*/
/* IA8 (8 bit gray scale color + 8 bits alpha, 4x4 tiles) */
TEST (TextureDecoderTest, DecodeBytes_IA8) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u8 src[TEST_HEIGHT*TEST_WIDTH*2];
    int texformat = GX_TF_IA8;

    u16* tlut = NULL;
    TlutFormat tlutfmt = (TlutFormat) 0;

	int i, j, val, alpha;

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
		  val = (j<<4 | j);
		  alpha = ((15-j) << 4) | (15-j);
		  example[i+j] = ( val | (val << 8) | (val << 16) | (alpha << 24) );
		}
	}


	int ti, tj, index;

	// Initialize texture (4x4 tiles)
	index = 0;
	for (j=0; j<TEST_HEIGHT; j+=4) {
		for (i=0; i<TEST_WIDTH; i+=4) {
			for (tj=0; tj < 4; tj++) {
				val = 0;
				if ((i/4)%4 == 1) {
					val = 4;
				}
				if ((i/4)%4 == 2) {
					val = 8;
				}
				if ((i/4)%4 == 3) {
					val = 12;
				}
				for (ti=0; ti < 4; ti++) {
					alpha = (15-val) & 0xf;
					src[index++] = (alpha | ((alpha) << 4));
					src[index++] = (val | ((val) << 4));
					val++;
				}
			}
		}
	}

	// Run test
	TexDecoder_Decode((u8*) dst, (u8*) src, width, height, texformat, (u8*) tlut, tlutfmt);

	// Compare the results
	comp = memcmp ( &example[0], &dst[0], TEST_WIDTH*TEST_HEIGHT*4);

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);

}



/* ===============================================================================================*/
/* C14X2 (14 bit color index, 4x4 tiles) in IA8 format */
TEST (TextureDecoderTest, DecodeBytes_C14X2_IA8) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u16 src[TEST_HEIGHT*TEST_WIDTH];
    int texformat = GX_TF_C14X2;

	// 14 bit color index = 16384 colors
    u16 *tlut;
    TlutFormat tlutfmt = GX_TL_IA8;

	int i, j, val;

	// Initialize the color index
	tlut = (u16 *)malloc(sizeof(u16)*16384);
	memset(tlut, 0, sizeof(u16)*16384);

    tlut[0] = 0x00FF; // Black
	tlut[1] = 0x10FF;
	tlut[2] = 0x20FF;
	tlut[3] = 0x30FF;
	tlut[4] = 0x40FF;
	tlut[5] = 0x50FF;
	tlut[6] = 0x60FF;
	tlut[7] = 0x70FF;
	tlut[8] = 0x80FF; // Gray
	tlut[9] = 0x90FF;
	tlut[10] = 0xA0FF;
	tlut[11] = 0xB0FF;
	tlut[12] = 0xC0FF; // Silver
	tlut[13] = 0xD0FF;
	tlut[14] = 0xE0FF;
    tlut[15] = 0xFFFF; // White

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = (tlut[j] >> 8) & 0xFF;
			example[i+j] = (0xFF << 24) | (val << 16) | (val << 8) | val;
		}
	}

	comp = testDecoder_DecodeC14X2(example, dst, src, width, height, texformat, tlut, tlutfmt);
	free(tlut);	

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);
}


/* ===============================================================================================*/
/* C14X2 (14 bit color index, 4x4 tiles) in RGB565 format */
TEST (TextureDecoderTest, DecodeBytes_C14X2_RGB565) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u16 src[TEST_HEIGHT*TEST_WIDTH];
    int texformat = GX_TF_C14X2;

	// 14 bit color index = 16384 colors
    u16 *tlut;
    TlutFormat tlutfmt = GX_TL_RGB565;

	int i, j, val;
	int r, g, b;

	// Initialize the color index
	tlut = (u16 *)malloc(sizeof(u16)*16384);
	memset(tlut, 0, sizeof(u16)*16384);

    tlut[0] = 0x0000; // Black
	tlut[1] = 0xF800; // Red
	tlut[2] = 0x07E0; // Green
	tlut[3] = 0x001F; // Blue

	tlut[4] = 0x000F;
	tlut[5] = 0x00F0;
	tlut[6] = 0x0F00;
	tlut[7] = 0xF000;

	tlut[8] = 0x1111; // Gray
	tlut[9] = 0x2222;
	tlut[10] = 0x3333;
	tlut[11] = 0x4444;

	tlut[12] = 0x5555; 
	tlut[13] = 0x6666;
	tlut[14] = 0x7777;
    tlut[15] = 0xFFFF; // White

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = Common::swap16(tlut[j]) & 0xFFFF;

			r = ((val >> 11) & 0x1f) << 3;
			g = ((val >> 5) & 0x3f) << 2;
			b = ((val)&0x1f) << 3;
			example[i+j] = (0xFF << 24) | (b << 16) | (g << 8) | r;
		}
	}

	comp = testDecoder_DecodeC14X2(example, dst, src, width, height, texformat, tlut, tlutfmt);
	free(tlut);	

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);
}

/* ===============================================================================================*/
/* C14X2 (14 bit color index, 4x4 tiles) in RGB5A3 format */
TEST (TextureDecoderTest, DecodeBytes_C14X2_RGB5A3) { 
	int comp = 0;
	int width = TEST_WIDTH;
    int height = TEST_HEIGHT;
	u32 dst[TEST_WIDTH*TEST_HEIGHT];
	u32 example[TEST_WIDTH*TEST_HEIGHT];
	u16 src[TEST_HEIGHT*TEST_WIDTH];
    int texformat = GX_TF_C14X2;

	// 14 bit color index = 16384 colors
    u16 *tlut;
    TlutFormat tlutfmt = GX_TL_RGB5A3;

	int i, j, val;
	int r, g, b, a;


	// Initialize the color index
	tlut = (u16 *)malloc(sizeof(u16)*16384);
	memset(tlut, 0, sizeof(u16)*16384);

    tlut[0] = 0x7000; // Black
	tlut[1] = 0x7F00; // Red
	tlut[2] = 0x70F0; // Green
	tlut[3] = 0x700F; // Blue

	tlut[4] = 0x0000; // Transparent
	tlut[5] = 0x5F00;
	tlut[6] = 0x50F0;
	tlut[7] = 0x500F;

	tlut[8] = 0xFC00; // Red
	tlut[9] = 0x83E0; // Green
	tlut[10] = 0x801F; // Blue
	tlut[11] = 0x4444;

	tlut[12] = 0x5555; 
	tlut[13] = 0x6666;
	tlut[14] = 0x7777;
    tlut[15] = 0xFFFF; // White

	// Initizatize the example
	for (i=0; i < TEST_WIDTH*TEST_HEIGHT; i+=16) {
		for (j=0; j<16; j++) {
			val = Common::swap16(tlut[j]) & 0xFFFF;

		  if ((val & 0x8000))
		  {
			r = ((val >> 10) & 0x1f) << 3;
			g = ((val >> 5) & 0x1f) << 3;
			b = ((val)&0x1f) << 3;
			a = 0xFF;
		  }
		  else
		  {
			a = ((val >> 12) & 0x7) << 5;
			r = ((val >> 8) & 0xf) << 4;
			g = ((val >> 4) & 0xf) << 4;
			b = ((val)&0xf) << 4;
		  }
		  example[i+j] = ( r | (g << 8) | (b << 16) | (a << 24) );
		}
	}

	comp = testDecoder_DecodeC14X2(example, dst, src, width, height, texformat, tlut, tlutfmt);
	free(tlut);	

	if (comp != 0) {
		printf("Decoding error=\n");
		printf("EXAMPLE=\n");
		printTestImage(example);

		printf("DECODED=\n");
		printTestImage(dst);
	}


    EXPECT_EQ (0, comp);
}


