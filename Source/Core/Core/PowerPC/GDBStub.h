// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#pragma once

#include "Common/CommonTypes.h"

#ifdef _WIN32
#define SIGTRAP 5
#define SIGTERM 15
#define MSG_WAITALL 8
#endif

typedef enum {
  GDB_BP_TYPE_NONE = 0,
  GDB_BP_TYPE_X,
  GDB_BP_TYPE_R,
  GDB_BP_TYPE_W,
  GDB_BP_TYPE_A
} gdb_bp_type;

void gdb_init(u32 port);
void gdb_init_local(const char* socket);
void gdb_deinit();
bool gdb_active();
void gdb_break();

void gdb_handle_exception();
int gdb_signal(u32 signal);

int gdb_bp_x(u32 addr);
int gdb_bp_r(u32 addr);
int gdb_bp_w(u32 addr);
int gdb_bp_a(u32 addr);

bool gdb_add_bp(u32 type, u32 addr, u32 len);
