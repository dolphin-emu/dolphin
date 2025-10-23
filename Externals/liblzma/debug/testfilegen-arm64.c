// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       testfilegen-arm64.c
/// \brief      Generates uncompressed test file for the ARM64 filter
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>


static uint32_t pc4 = 0;


static void
put32le(uint32_t v)
{
	putchar((v >> 0) & 0xFF);
	putchar((v >> 8) & 0xFF);
	putchar((v >> 16) & 0xFF);
	putchar((v >> 24) & 0xFF);
	++pc4;
}


static void
putbl(uint32_t imm)
{
	imm &= (1U << 26) - 1;
	imm |= 0x25U << 26;
	put32le(imm);
}


static void
putadrp32(uint32_t imm)
{
	imm &= 0x1FFFFFU;

	// fprintf(stderr, "ADRP 0x%08X\n", imm);

	uint32_t instr = 0x90000000;
	instr |= (pc4 * 5 + 11) & 0x1F;
	instr |= (imm & 3) << 29;
	instr |= (imm >> 2) << 5;

	put32le(instr);
}


extern int
main(void)
{
	putbl(0);
	putbl(0x03FFFFFF);
	putbl(0x03FFFFFE);
	putbl(0x03FFFFFD);

	putbl(3);
	putbl(2);
	putbl(1);
	putbl(0);


	putbl(0x02000001);
	putbl(0x02000000);
	putbl(0x01FFFFFF);
	putbl(0x01FFFFFE);

	putbl(0x01111117);
	putbl(0x01111116);
	putbl(0x01111115);
	putbl(0x01111114);


	putbl(0x02222227);
	putbl(0x02222226);
	putbl(0x02222225);
	putbl(0x02222224);

	putbl(0U - pc4);
	putbl(0U - pc4);
	putbl(0U - pc4);
	putbl(0U - pc4);

	putadrp32(0x00);
	putadrp32(0x05);
	putadrp32(0x15);
	putadrp32(0x25);

	for (unsigned rep = 0; rep < 2; ++rep) {
		while ((pc4 << 2) & 4095)
			put32le(0x55555555U);

		for (unsigned i = 10; i <= 21; ++i) {
			const uint32_t neg = (0x1FFF00 >> (21 - i)) & ~255U;
			const uint32_t plus = 1U << (i - 1);
			putadrp32(0x000000 | plus);
			putadrp32(0x000005 | plus);
			putadrp32(0x0000FE | plus);
			putadrp32(0x0000FF | plus);

			putadrp32(0x000000 | neg);
			putadrp32(0x000005 | neg);
			putadrp32(0x0000FE | neg);
			putadrp32(0x0000FF | neg);
		}
	}

	return 0;
}
