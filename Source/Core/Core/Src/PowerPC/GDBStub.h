// Copyright (C) 2010 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#ifndef GDB_H__
#define GDB_H__

#include <signal.h>
#include "Common.h"
#include "Thread.h"
#include "PowerPC.h"
#include "../HW/CPU.h"
#include "../HW/Memmap.h"

#ifdef _WIN32
#define SIGTRAP 5
#define	SIGTERM		15
#define MSG_WAITALL  8
#endif

typedef enum {
	GDB_BP_TYPE_NONE = 0,
	GDB_BP_TYPE_X,
	GDB_BP_TYPE_R,
	GDB_BP_TYPE_W,
	GDB_BP_TYPE_A
} gdb_bp_type;

void gdb_init(u32 port);
void gdb_deinit();
bool gdb_active();
void gdb_break();

void gdb_handle_exception();
int  gdb_signal(u32 signal);

int  gdb_bp_x(u32 addr);
int  gdb_bp_r(u32 addr);
int  gdb_bp_w(u32 addr);
int  gdb_bp_a(u32 addr);

bool gdb_add_bp(u32 type, u32 addr, u32 len);

#endif
