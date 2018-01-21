// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#pragma once

#include <thread>

#ifdef _WIN32
#include <iphlpapi.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

#include "Common/CommonTypes.h"

#include "Core/HW/CPU.h"
#include "Core/PowerPC/BreakPoints.h"

// --------------------------------------------------------------------------------------
//  GDBThread
// --------------------------------------------------------------------------------------
// Threaded wrapper class for implementing debugging functionality through a gdb stub.
//
class GDBThread
{
public:
  GDBThread();
  ~GDBThread();

  bool Initialize();
  void Terminate();

private:
  enum class GDBBPType
  {
    None = 0,
    Execute,
    Read,
    Write,
    Access,
  };

  enum class SignalType
  {
    None = 0,
    Quit = 3,
    Trap = 5,
    Abort = 6,
    Kill = 9,
    Terminate = 15,
    Stop = 17,
    Continue = 19,

    Connected = 30,
  };

  void OnStart();
  void OnStop();
  void OnPause();
  void OnResume();
  void UpdateState(CPU::State current_state);

  void ThreadFunc();

  void GDBInit(u32 port);
#ifndef _WIN32
  void GDBInitLocal(const char* socket);
#endif
  void GDBDeinit();

  bool GDBActive() const;

  int GDBDataAvailable();

  void GDBHandleEvents();

  void GDBSignal(SignalType signal, u64 addr = UINT64_MAX,
                 MemCheckCondition cond = MemCheckCondition::Execute);

  void GDBInitGeneric(int domain, const sockaddr* server_addr, socklen_t server_addrlen,
                      sockaddr* client_addr, socklen_t* client_addrlen);

  void GDBReadCommand();
  void GDBParseCommand();

  u8 GDBReadByte();
  u8 GDBCalculateChecksum() const;

  void GDBReply(const char* reply);
  void GDBNak();
  void GDBAck();

  void GDBHandleSignal();

  void GDBContinue();

  void GDBDetach();

  void GDBReadRegisters();
  void GDBWriteRegisters();

  void GDBHandleSetThread();

  void GDBKill();

  void GDBReadMemory();
  void GDBWriteMemory();

  void GDBReadRegister();
  void GDBWriteRegister();

  void GDBHandleQuery();

  void GDBStep();

  void GDBAddBreakpoint();
  void GDBRemoveBreakpoint();

  void GDBPause();

  void AddBreakpoint(GDBBPType type, u32 addr, u32 len);
  void RemoveBreakpoint(GDBBPType type, u32 addr, u32 len);

  static constexpr int GDB_BFR_MAX = 10000;

  bool m_is_running = false;

  std::thread m_server_thread;

  CPU::State m_previous_state = CPU::State::PowerDown;

  bool m_connected = false;
  int m_tmpsock = -1;
  int m_sock = -1;

  u8 m_cmd_bfr[GDB_BFR_MAX + 1];
  u32 m_cmd_len = 0;

  SignalType m_last_signal = SignalType::None;
  u64 m_last_signal_address = UINT64_MAX;
  MemCheckCondition m_last_signal_condition = MemCheckCondition::Execute;
};
