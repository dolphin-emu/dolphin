/* $Id: codelength.h,v 1.4 2012/09/27 15:40:29 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas BERNARD
 * copyright (c) 2005-2011 Thomas Bernard
 * This software is subjet to the conditions detailed in the
 * provided LICENCE file. */
#ifndef CODELENGTH_H_INCLUDED
#define CODELENGTH_H_INCLUDED

/* Encode length by using 7bit per Byte :
 * Most significant bit of each byte specifies that the
 * following byte is part of the code */
#define DECODELENGTH(n, p) n = 0; \
                           do { n = (n << 7) | (*p & 0x7f); } \
                           while((*(p++)&0x80) && (n<(1<<25)));

#define DECODELENGTH_CHECKLIMIT(n, p, p_limit) \
	n = 0; \
	do { \
		if((p) >= (p_limit)) break; \
		n = (n << 7) | (*(p) & 0x7f); \
	} while((*((p)++)&0x80) && (n<(1<<25)));

#define CODELENGTH(n, p) if(n>=268435456) *(p++) = (n >> 28) | 0x80; \
                         if(n>=2097152) *(p++) = (n >> 21) | 0x80; \
                         if(n>=16384) *(p++) = (n >> 14) | 0x80; \
                         if(n>=128) *(p++) = (n >> 7) | 0x80; \
                         *(p++) = n & 0x7f;

#endif

