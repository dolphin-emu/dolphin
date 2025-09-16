// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       vli_size.c
/// \brief      Calculates the encoded size of a variable-length integer
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


extern LZMA_API(uint32_t)
lzma_vli_size(lzma_vli vli)
{
	if (vli > LZMA_VLI_MAX)
		return 0;

	uint32_t i = 0;
	do {
		vli >>= 7;
		++i;
	} while (vli != 0);

	assert(i <= LZMA_VLI_BYTES_MAX);
	return i;
}
