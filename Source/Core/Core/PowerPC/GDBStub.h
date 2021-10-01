// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/CoreTiming.h"

#ifndef MSG_WAITALL
#define MSG_WAITALL (8)
#endif

typedef enum
{
  GDB_SIGTRAP = 5,
  GDB_SIGTERM = 15,
} gdb_signals;

typedef enum
{
  GDB_BP_TYPE_NONE = 0,
  GDB_BP_TYPE_X,
  GDB_BP_TYPE_R,
  GDB_BP_TYPE_W,
  GDB_BP_TYPE_A
} gdb_bp_type;

const s64 GDB_UPDATE_CYCLES = 100000;

void GDBStubUpdateCallback(u64 userdata, s64 cycles_late);

void gdb_init(u32 port);
void gdb_init_local(const char* socket);
void gdb_deinit();
bool gdb_active();
bool gdb_hasControl();
void gdb_takeControl();
void gdb_break();

void gdb_handle_exception(bool loopUntilContinue);
int gdb_signal(u32 signal);

int gdb_bp_x(u32 addr);
int gdb_bp_r(u32 addr);
int gdb_bp_w(u32 addr);
int gdb_bp_a(u32 addr);

bool gdb_add_bp(u32 type, u32 addr, u32 len);
void gdb_handle_exception(bool loop_until_continue);
void SendSignal(u32 signal);
}  // namespace GDBStub
