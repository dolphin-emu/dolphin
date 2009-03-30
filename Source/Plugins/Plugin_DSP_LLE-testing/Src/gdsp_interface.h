/*====================================================================

   filename:     gdsp_interface.h
   project:      GCemu
   created:      2004-6-18
   mail:		  duddie@walla.com

   Copyright (c) 2005 Duddie & Tratax

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   ====================================================================*/
#ifndef _GDSP_INTERFACE_H
#define _GDSP_INTERFACE_H

#include "Globals.h"

#define GDSP_MBOX_CPU   0
#define GDSP_MBOX_DSP   1

#define DSP_DSMAH   0xce
#define DSP_DSMAL   0xcf
#define DSP_DSCR    0xc9
#define DSP_DSPA    0xcd
#define DSP_DSBL    0xcb
#define DSP_ACSAH   0xd4
#define DSP_ACSAL   0xd5
#define DSP_ACEAH   0xd6
#define DSP_ACEAL   0xd7
#define DSP_ACCAH   0xd8
#define DSP_ACCAL   0xd9

#define DSP_COEF_A1_0   0xa0
#define DSP_FORMAT      0xd1
#define DSP_PRED_SCALE  0xda
#define DSP_YN1         0xdb
#define DSP_YN2         0xdc
#define DSP_ARAM        0xdd
#define DSP_GAIN        0xde

extern u16 gdsp_ifx_regs[256];

u32  gdsp_mbox_peek(u8 mbx);
void    gdsp_mbox_write_h(u8 mbx, u16 val);
void    gdsp_mbox_write_l(u8 mbx, u16 val);
u16  gdsp_mbox_read_h(u8 mbx);
u16  gdsp_mbox_read_l(u8 mbx);

void    gdsp_ifx_init();

void    gdsp_idma_in(u16 dsp_addr, u32 addr, u32 size);


#endif

