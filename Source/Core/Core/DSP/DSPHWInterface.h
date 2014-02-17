/*====================================================================

   filename:     gdsp_interface.h
   project:      GCemu
   created:      2004-6-18
   mail:         duddie@walla.com

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   ====================================================================*/

#pragma once

#include "Common/Common.h"

#define GDSP_MBOX_CPU   0
#define GDSP_MBOX_DSP   1

u32  gdsp_mbox_peek(u8 mbx);
void gdsp_mbox_write_h(u8 mbx, u16 val);
void gdsp_mbox_write_l(u8 mbx, u16 val);
u16  gdsp_mbox_read_h(u8 mbx);
u16  gdsp_mbox_read_l(u8 mbx);

void gdsp_ifx_init();

void gdsp_ifx_write(u32 addr, u32 val);
u16  gdsp_ifx_read(u16 addr);
