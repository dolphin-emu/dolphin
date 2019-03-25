// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#include "Core/PowerPC/GDBThread.h"

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <iphlpapi.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#define SD_BOTH SHUT_RDWR
#endif
#include <stdarg.h>

#define fail(msg)                                                                                  \
  {                                                                                                \
    ERROR_LOG(GDB_THREAD, msg);                                                                    \
    GDBDeinit();                                                                                   \
    return;                                                                                        \
  }

#define failr(msg)                                                                                 \
  {                                                                                                \
    ERROR_LOG(GDB_THREAD, msg);                                                                    \
    GDBDeinit();                                                                                   \
    return 0;                                                                                      \
  }

#define GDB_MAX_BP 100

#define GDB_STUB_START '$'
#define GDB_STUB_END '#'
#define GDB_STUB_ACK '+'
#define GDB_STUB_NAK '-'

#ifdef _WIN32
#ifndef MSG_WAITALL
#define MSG_WAITALL 8
#endif
#endif

#define REGISTER_ID(category, index) ((category << 8) | index)

// --------------------------------------------------------------------------------------
//  GDBThread Implementations
// --------------------------------------------------------------------------------------
GDBThread::GDBThread() = default;

GDBThread::~GDBThread()
{
  Terminate();
}

bool GDBThread::Initialize()
{
  NOTICE_LOG(GDB_THREAD, "GDB thread: Initialize");

  const SConfig& config = SConfig::GetInstance();

  if (config.iGDBPort <= 0)
    return false;

  m_server_thread = std::thread(&GDBThread::ThreadFunc, this);

  return true;
}

void GDBThread::Terminate()
{
  NOTICE_LOG(GDB_THREAD, "GDB thread: Terminate");

  OnStop();

  if (m_server_thread.joinable())
    m_server_thread.join();
}

void GDBThread::OnStart()
{
  NOTICE_LOG(GDB_THREAD, "GDB thread: On Start");

  Common::SetCurrentThreadName("GDBThread");
}

void GDBThread::OnStop()
{
  NOTICE_LOG(GDB_THREAD, "GDB thread: On Stop");

  m_is_running = false;

  GDBSignal(SignalType::Terminate);
  GDBDeinit();
}

void GDBThread::OnPause()
{
  NOTICE_LOG(GDB_THREAD, "GDB thread: On Pause");

  BreakPoints::TTriggeredBreakPoint triggered_breakpoint =
      PowerPC::breakpoints.GetBreakpointTriggered();

  if (CPU::State::PowerDown == CPU::GetState())
  {
    GDBSignal(SignalType::Terminate);
    GDBDeinit();
  }
  else if (std::get<0>(triggered_breakpoint))
  {
    u64 addr = std::get<1>(triggered_breakpoint).value_or(UINT64_MAX);
    MemCheckCondition cond = std::get<2>(triggered_breakpoint).value_or(MemCheckCondition::Execute);

    GDBSignal(SignalType::Trap, addr, cond);
  }
  else
  {
    GDBSignal(SignalType::Stop);
  }
}
void GDBThread::OnResume()
{
  NOTICE_LOG(GDB_THREAD, "GDB thread: On Resume");

  if (CPU::State::PowerDown == CPU::GetState())
  {
    GDBSignal(SignalType::Terminate);
    GDBDeinit();
  }
  else
  {
    GDBSignal(SignalType::Continue);
  }
}

void GDBThread::UpdateState(CPU::State current_state)
{
  if (current_state != m_previous_state)
  {
    NOTICE_LOG(GDB_THREAD, "Previous state: %d - Current state: %d",
               static_cast<int>(m_previous_state), static_cast<int>(current_state));

    switch (current_state)
    {
    case CPU::State::Running:
      OnResume();
      break;
    case CPU::State::Stepping:
      OnPause();
      break;
    case CPU::State::PowerDown:
      OnStop();
      break;
    default:
      break;
    }
  }

  m_previous_state = current_state;
}

void GDBThread::ThreadFunc()
{
  OnStart();

  NOTICE_LOG(GDB_THREAD, "Starting GDB stub thread.");

  m_is_running = true;

  const SConfig& config = SConfig::GetInstance();

  while (m_is_running)
  {
    if (CPU::State::PowerDown != CPU::GetState())
    {
#ifndef _WIN32
      if (!config.gdb_socket.empty())
      {
        GDBInitLocal(config.gdb_socket.data());
      }
      else
#endif
          if (config.iGDBPort > 0)
      {
        GDBInit(static_cast<u32>(config.iGDBPort));
        // break at next instruction (the first instruction)
      }

      CPU::EnableStepping(true);

      // abuse abort signal for attach
      GDBSignal(SignalType::Connected);

      m_previous_state = CPU::State::Stepping;

      while (m_is_running && (0 <= GDBDataAvailable()))
      {
        GDBHandleEvents();

        UpdateState(CPU::GetState());

        Common::YieldCPU();
      }

      GDBDeinit();
    }

    Common::YieldCPU();
  }

  NOTICE_LOG(GDB_THREAD, "Terminating GDB stub thread.");
}

// private helpers
static u8 HexToChar(u8 hex)
{
  if (hex >= '0' && hex <= '9')
    return hex - '0';
  else if (hex >= 'a' && hex <= 'f')
    return hex - 'a' + 0xa;
  else if (hex >= 'A' && hex <= 'F')
    return hex - 'A' + 0xa;

  DEBUG_LOG(GDB_THREAD, "Invalid nibble: %c (%02x)\n", hex, hex);
  return 0;
}

static u8 NibbleToHex(u8 n)
{
  n &= 0xf;
  if (n < 0xa)
    return '0' + n;
  else
    return 'A' + n - 0xa;
}

static void MemToHex(u8* dst, u32 src, u32 len)
{
  while (len-- > 0)
  {
    u8 tmp = PowerPC::HostRead_U8(src++);

    *dst++ = NibbleToHex(tmp >> 4);
    *dst++ = NibbleToHex(tmp);
  }
}

static void HexToMem(u32 dst, u8* src, u32 len)
{
  while (len-- > 0)
  {
    u8 tmp = HexToChar(*src++) << 4;
    tmp |= HexToChar(*src++);

    PowerPC::HostWrite_U8(tmp, dst++);
  }
}

static void WriteHexBE32(u8* p, u32 v)
{
  for (u32 i = 0; i < 8; i++)
    p[i] = NibbleToHex(u8(v >> (28 - 4 * i)));
}

static void WriteHexBE64(u8* p, u64 v)
{
  for (u32 i = 0; i < 16; i++)
    p[i] = NibbleToHex(u8(v >> (60 - 4 * i)));
}

static u32 ReadHexBE32(const u8* p)
{
  u32 res = 0;

  for (u32 i = 0; i < 8; i++)
    res = (res << 4) | HexToChar(p[i]);

  return res;
}

static u64 ReadHexBE64(const u8* p)
{
  u64 res = 0;

  for (u32 i = 0; i < 16; i++)
    res = (res << 4) | HexToChar(p[i]);

  return res;
}

// GDB stub interface
#ifndef _WIN32
void GDBThread::GDBInitLocal(const char* socket)
{
  unlink(socket);

  sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, socket);

  GDBInitGeneric(PF_LOCAL, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr), nullptr,
                 nullptr);
}
#endif

void GDBThread::GDBInit(u32 port)
{
  GDBDeinit();

  sockaddr_in saddr_server = {};
  sockaddr_in saddr_client;

  saddr_server.sin_family = AF_INET;
  saddr_server.sin_port = htons(port);
  saddr_server.sin_addr.s_addr = INADDR_ANY;

  socklen_t client_addrlen = sizeof(saddr_client);

  GDBInitGeneric(PF_INET, reinterpret_cast<const sockaddr*>(&saddr_server), sizeof(saddr_server),
                 reinterpret_cast<sockaddr*>(&saddr_client), &client_addrlen);

  saddr_client.sin_addr.s_addr = ntohl(saddr_client.sin_addr.s_addr);
}

void GDBThread::GDBInitGeneric(int domain, const sockaddr* server_addr, socklen_t server_addrlen,
                               sockaddr* client_addr, socklen_t* client_addrlen)
{
#ifdef _WIN32
  WSADATA init_data;
  WSAStartup(MAKEWORD(2, 2), &init_data);
#endif

  m_tmpsock = socket(domain, SOCK_STREAM, 0);
  if (m_tmpsock == -1)
    ERROR_LOG(GDB_THREAD, "Failed to create gdb socket");

  int on = 1;
  if (setsockopt(m_tmpsock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on),
                 sizeof on) < 0)
    ERROR_LOG(GDB_THREAD, "Failed to setsockopt");

  if (bind(m_tmpsock, server_addr, server_addrlen) < 0)
    ERROR_LOG(GDB_THREAD, "Failed to bind gdb socket");

  if (listen(m_tmpsock, 1) < 0)
    ERROR_LOG(GDB_THREAD, "Failed to listen to gdb socket");

  NOTICE_LOG(GDB_THREAD, "Waiting for gdb to connect...\n");

  m_sock = accept(m_tmpsock, client_addr, client_addrlen);
  if (m_sock < 0)
    ERROR_LOG(GDB_THREAD, "Failed to accept gdb client");
  NOTICE_LOG(GDB_THREAD, "Client connected.\n");

#ifdef _WIN32
  closesocket(m_tmpsock);
#else
  close(m_tmpsock);
#endif
  m_tmpsock = -1;

  m_connected = (m_sock >= 0);
  if (!m_connected)
    GDBDeinit();
}

void GDBThread::GDBDeinit()
{
  if (m_tmpsock != -1)
  {
    shutdown(m_tmpsock, SD_BOTH);
#if _WIN32
    closesocket(m_tmpsock);
#else
    close(m_tmpsock);
#endif
    m_tmpsock = -1;
  }
  if (m_sock != -1)
  {
    shutdown(m_sock, SD_BOTH);
#if _WIN32
    closesocket(m_sock);
#else
    close(m_sock);
#endif
    m_sock = -1;
  }

  m_connected = false;

#ifdef _WIN32
  WSACleanup();
#endif
}

bool GDBThread::GDBActive() const
{
  return m_connected;
}

int GDBThread::GDBDataAvailable()
{
  timeval t;
  fd_set fds_;
  fd_set* fds = &fds_;

  FD_ZERO(fds);
  FD_SET(m_sock, fds);

  t.tv_sec = 0;
  t.tv_usec = 20;

  if (select(m_sock + 1, fds, nullptr, nullptr, &t) < 0)
    return -1;

  if (FD_ISSET(m_sock, fds))
    return 1;
  return 0;
}

void GDBThread::GDBHandleEvents()
{
  if (!m_connected)
    return;

  while (0 < GDBDataAvailable())
  {
    GDBReadCommand();
    GDBParseCommand();
  }
}

void GDBThread::GDBReadCommand()
{
  m_cmd_len = 0;
  memset(m_cmd_bfr, 0, sizeof m_cmd_bfr);

  u8 c = GDBReadByte();
  if (c != GDB_STUB_START)
  {
    ERROR_LOG(GDB_THREAD, "gdb: read invalid byte %02x\n", c);
    return;
  }

  while ((c = GDBReadByte()) != GDB_STUB_END)
  {
    m_cmd_bfr[m_cmd_len++] = c;
    if (m_cmd_len == sizeof m_cmd_bfr)
      fail("gdb: cmd_bfr overflow\n");
  }

  u8 chk_read = HexToChar(GDBReadByte()) << 4;
  chk_read |= HexToChar(GDBReadByte());

  u8 chk_calc = GDBCalculateChecksum();

  if (chk_calc != chk_read)
  {
    ERROR_LOG(GDB_THREAD,
              "gdb: invalid checksum: calculated %02x and read %02x for $%s# (length: %d)\n",
              chk_calc, chk_read, m_cmd_bfr, m_cmd_len);
    m_cmd_len = 0;

    GDBNak();
  }

  NOTICE_LOG(GDB_THREAD, "gdb: read command %c with a length of %d: %s\n", m_cmd_bfr[0], m_cmd_len,
             m_cmd_bfr);
}

void GDBThread::GDBParseCommand()
{
  if (m_cmd_len == 0)
    return;

  switch (m_cmd_bfr[0])
  {
  case 'q':
    GDBHandleQuery();
    break;
  case 'H':
    GDBHandleSetThread();
    break;
  case '?':
    GDBHandleSignal();
    break;
  case 'D':
    GDBDetach();
    break;
  case 'k':
    GDBKill();
    break;
  case 'g':
    GDBReadRegisters();
    break;
  case 'G':
    GDBWriteRegisters();
    break;
  case 'p':
    GDBReadRegister();
    break;
  case 'P':
    GDBWriteRegister();
    break;
  case 'm':
    GDBReadMemory();
    break;
  case 'M':
    GDBWriteMemory();
    break;
  case 'c':
    GDBContinue();
    break;
  case 's':
    GDBStep();
    break;
  case ' ':
    GDBPause();
    break;
  case 'z':
    GDBRemoveBreakpoint();
    break;
  case 'Z':
    GDBAddBreakpoint();
    break;
  default:
    GDBAck();
    GDBReply("");
    break;
  }
}

u8 GDBThread::GDBReadByte()
{
  if (!m_connected)
    return 0;

  u8 c;

  size_t res = recv(m_sock, reinterpret_cast<char*>(&c), 1, MSG_WAITALL);
  if (res != 1)
    failr("recv failed");

  return c;
}

u8 GDBThread::GDBCalculateChecksum() const
{
  u32 len = m_cmd_len;
  const u8* ptr = m_cmd_bfr;
  u8 c = 0;

  while (len-- > 0)
    c += *ptr++;

  return c;
}

void GDBThread::GDBReply(const char* reply)
{
  if (!m_connected)
    return;

  memset(m_cmd_bfr, 0, sizeof m_cmd_bfr);

  m_cmd_len = static_cast<u32>(strlen(reply));
  if (m_cmd_len + 4 > sizeof m_cmd_bfr)
    fail("GDBReply: cmd_bfr overflow");

  memcpy(m_cmd_bfr + 1, reply, m_cmd_len);

  m_cmd_len++;
  u8 chk = GDBCalculateChecksum();
  m_cmd_len--;
  m_cmd_bfr[0] = GDB_STUB_START;
  m_cmd_bfr[m_cmd_len + 1] = GDB_STUB_END;
  m_cmd_bfr[m_cmd_len + 2] = NibbleToHex(chk >> 4);
  m_cmd_bfr[m_cmd_len + 3] = NibbleToHex(chk);

  const u8* ptr = m_cmd_bfr;
  u32 left = m_cmd_len + 4;
  while (left > 0)
  {
    int n = send(m_sock, reinterpret_cast<const char*>(ptr), left, 0);
    if (n < 0)
      fail("gdb: send failed");
    left -= n;
    ptr += n;
  }
}

void GDBThread::GDBNak()
{
  if (!m_connected)
    return;

  const char nak = GDB_STUB_NAK;

  size_t res = send(m_sock, &nak, 1, 0);
  if (res != 1)
    fail("send failed");
}

void GDBThread::GDBAck()
{
  if (!m_connected)
    return;

  const char ack = GDB_STUB_ACK;

  size_t res = send(m_sock, &ack, 1, 0);
  if (res != 1)
    fail("send failed");
}

void GDBThread::GDBHandleSignal()
{
  if (!m_connected)
    return;

  char bfr[128] = {};

  GDBAck();

  switch (m_last_signal_condition)
  {
  case MemCheckCondition::Execute:
    sprintf(bfr, "T%02X%08X:%08X", static_cast<int>(m_last_signal), 64, PC);
    break;
  case MemCheckCondition::Read:
    sprintf(bfr, "T%02X%08X:%08X;rwatch:%08llX", static_cast<int>(m_last_signal), 64, PC,
            m_last_signal_address);
    break;
  case MemCheckCondition::Write:
    sprintf(bfr, "T%02X%08X:%08X;watch:%08llX", static_cast<int>(m_last_signal), 64, PC,
            m_last_signal_address);
    break;
  case MemCheckCondition::ReadWrite:
    sprintf(bfr, "T%02X%08X:%08X;awatch:%08llX", static_cast<int>(m_last_signal), 64, PC,
            m_last_signal_address);
    break;
  default:
    return;
    break;
  }

  GDBReply(bfr);
}

void GDBThread::GDBContinue()
{
  if (!m_connected)
    return;

  GDBAck();

  if (CPU::State::Stepping == CPU::GetState())
  {
    CPU::EnableStepping(false);

    UpdateState(CPU::GetState());
  }
}

void GDBThread::GDBDetach()
{
  if (!m_connected)
    return;

  GDBAck();
  GDBReply("OK");
  GDBDeinit();
}

void GDBThread::GDBReadRegisters()
{
  if (!m_connected)
    return;

  u8 bfr[GDB_BFR_MAX - 4] = {};

  GDBAck();

  u8* bufptr = bfr;

  for (u32 id = 0; id <= 71; ++id)
  {
    if (0 <= id && id <= 31)
    {
      WriteHexBE32(bufptr, GPR(id));
      bufptr += 8;
    }
    else if (32 <= id && id <= 63)
    {
      WriteHexBE64(bufptr, rPS(id - 32).PS1AsU64());
      bufptr += 16;
      WriteHexBE64(bufptr, rPS(id - 32).PS0AsU64());
      bufptr += 16;
    }
    else
    {
      switch (id)
      {
      case 64:
        WriteHexBE32(bufptr, PC);
        bufptr += 8;
        break;
      case 65:
        WriteHexBE32(bufptr, MSR.Hex);
        bufptr += 8;
        break;
      case 66:
        WriteHexBE32(bufptr, PowerPC::ppcState.cr.Get());
        bufptr += 8;
        break;
      case 67:
        WriteHexBE32(bufptr, LR);
        bufptr += 8;
        break;
      case 68:
        WriteHexBE32(bufptr, CTR);
        bufptr += 8;
        break;
      case 69:
        WriteHexBE32(bufptr, XER);
        bufptr += 8;
        break;
      case 70:
        WriteHexBE32(bufptr, 0x0BADC0DE);
        bufptr += 8;
        break;
      case 71:
        WriteHexBE32(bufptr, FPSCR.Hex);
        bufptr += 8;
        break;
      }
    }
  }

  GDBReply(reinterpret_cast<char*>(bfr));
}

void GDBThread::GDBWriteRegisters()
{
  if (!m_connected)
    return;

  GDBAck();

  const u8* bufptr = m_cmd_bfr + 1;

  for (u32 id = 0; id <= 71; ++id)
  {
    if (0 <= id && id <= 31)
    {
      GPR(id) = ReadHexBE32(bufptr);
      bufptr += 8;
    }
    else if (32 <= id && id <= 63)
    {
      rPS(id - 32).SetPS1(ReadHexBE64(bufptr));
      bufptr += 16;
      rPS(id - 32).SetPS0(ReadHexBE64(bufptr));
      bufptr += 16;
    }
    else
    {
      switch (id)
      {
      case 64:
        PC = ReadHexBE32(bufptr);
        bufptr += 8;
        break;
      case 65:
        MSR.Hex = ReadHexBE32(bufptr);
        bufptr += 8;
        break;
      case 66:
        PowerPC::ppcState.cr.Set(ReadHexBE32(bufptr));
        bufptr += 8;
        break;
      case 67:
        LR = ReadHexBE32(bufptr);
        bufptr += 8;
        break;
      case 68:
        CTR = ReadHexBE32(bufptr);
        bufptr += 8;
        break;
      case 69:
        XER = ReadHexBE32(bufptr);
        bufptr += 8;
        break;
      case 70:
        // do nothing, we don't have MQ
        bufptr += 8;
        break;
      case 71:
        FPSCR.Hex = ReadHexBE32(bufptr);
        bufptr += 8;
        break;
      }
    }
  }

  GDBReply("OK");
}

void GDBThread::GDBHandleSetThread()
{
  if (!m_connected)
    return;

  GDBAck();

  if (memcmp(m_cmd_bfr, "Hg0", 3) == 0 || memcmp(m_cmd_bfr, "Hc-1", 4) == 0 ||
      memcmp(m_cmd_bfr, "Hc0", 4) == 0 || memcmp(m_cmd_bfr, "Hc1", 4) == 0)
  {
    return GDBReply("OK");
  }

  GDBReply("E01");
}

void GDBThread::GDBKill()
{
  if (!m_connected)
    return;

  GDBAck();

  GDBDeinit();

  Core::Stop();
  NOTICE_LOG(GDB_THREAD, "killed by gdb");
}

void GDBThread::GDBReadMemory()
{
  if (!m_connected)
    return;

  u8 reply[GDB_BFR_MAX - 4] = {};

  GDBAck();

  u32 i = 1;
  u32 addr = 0;
  while (m_cmd_bfr[i] != ',')
    addr = (addr << 4) | HexToChar(m_cmd_bfr[i++]);

  i++;

  u32 len = 0;
  while (i < m_cmd_len)
    len = (len << 4) | HexToChar(m_cmd_bfr[i++]);
  DEBUG_LOG(GDB_THREAD, "gdb: read memory: %08x bytes from %08x\n", len, addr);

  if (len * 2 > sizeof reply)
    GDBReply("E01");

  MemToHex(reply, addr, len);
  GDBReply(reinterpret_cast<char*>(reply));
}

void GDBThread::GDBWriteMemory()
{
  if (!m_connected)
    return;

  GDBAck();

  u32 i = 1;
  u32 addr = 0;
  while (m_cmd_bfr[i] != ',')
    addr = (addr << 4) | HexToChar(m_cmd_bfr[i++]);

  i++;

  u32 len = 0;
  while (m_cmd_bfr[i] != ':')
    len = (len << 4) | HexToChar(m_cmd_bfr[i++]);
  DEBUG_LOG(GDB_THREAD, "gdb: write memory: %08x bytes to %08x\n", len, addr);

  HexToMem(addr, m_cmd_bfr + i, len);
  GDBReply("OK");
}

void GDBThread::GDBReadRegister()
{
  if (!m_connected)
    return;

  u8 reply[64] = {};
  u32 id = 0;

  GDBAck();

  u32 i = 1;
  while (i < m_cmd_len)
    id = (id << 4) | HexToChar(m_cmd_bfr[i++]);

  if (0 <= id && id <= 31)
  {
    WriteHexBE32(reply, GPR(id));
  }
  else if (32 <= id && id <= 63)
  {
    WriteHexBE64(reply, rPS(id - 32).PS1AsU64());
    WriteHexBE64(reply + 16, rPS(id - 32).PS0AsU64());
  }
  else
  {
    switch (id)
    {
    case 64:
      WriteHexBE32(reply, PC);
      break;
    case 65:
      WriteHexBE32(reply, MSR.Hex);
      break;
    case 66:
      WriteHexBE32(reply, PowerPC::ppcState.cr.Get());
      break;
    case 67:
      WriteHexBE32(reply, LR);
      break;
    case 68:
      WriteHexBE32(reply, CTR);
      break;
    case 69:
      WriteHexBE32(reply, XER);
      break;
    case 70:
      WriteHexBE32(reply, 0x0BADC0DE);
      break;
    case 71:
      WriteHexBE32(reply, FPSCR.Hex);
      break;
    default:
      return GDBReply("E01");
      break;
    }
  }

  GDBReply(reinterpret_cast<char*>(reply));
}

void GDBThread::GDBWriteRegister()
{
  if (!m_connected)
    return;

  u32 id = 0;

  GDBAck();

  u32 i = 1;
  while (m_cmd_bfr[i] != '=')
    id = (id << 4) | HexToChar(m_cmd_bfr[i++]);
  ++i;

  const u8* bufptr = m_cmd_bfr + i;

  if (0 <= id && id <= 31)
  {
    GPR(id) = ReadHexBE32(bufptr);
  }
  else if (32 <= id && id <= 63)
  {
    rPS(id - 32).SetPS1(ReadHexBE64(bufptr));
    rPS(id - 32).SetPS0(ReadHexBE64(bufptr + 16));
  }
  else
  {
    switch (id)
    {
    case 64:
      PC = ReadHexBE32(bufptr);
      break;
    case 65:
      MSR.Hex = ReadHexBE32(bufptr);
      break;
    case 66:
      PowerPC::ppcState.cr.Set(ReadHexBE32(bufptr));
      break;
    case 67:
      LR = ReadHexBE32(bufptr);
      break;
    case 68:
      CTR = ReadHexBE32(bufptr);
      break;
    case 69:
      XER = ReadHexBE32(bufptr);
      break;
    case 70:
      // do nothing, we don't have MQ
      break;
    case 71:
      FPSCR.Hex = ReadHexBE32(bufptr);
      break;
    default:
      return GDBReply("E01");
      break;
    }
  }

  GDBReply("OK");
}

void GDBThread::GDBHandleQuery()
{
  if (!m_connected)
    return;

  INFO_LOG(GDB_THREAD, "gdb: query '%s'\n", m_cmd_bfr + 1);
  GDBAck();
  GDBReply("");
}

void GDBThread::GDBStep()
{
  if (!m_connected)
    return;

  GDBAck();

  if (CPU::State::Stepping != CPU::GetState())
    return;

  PowerPC::breakpoints.ClearAllTemporary();
  JitInterface::InvalidateICache(PC, 4, true);

  Common::Event sync_event;

  sync_event.Reset();
  CPU::StepOpcode(&sync_event);

  sync_event.Wait();

  GDBSignal(SignalType::Trap);
}

void GDBThread::GDBAddBreakpoint()
{
  if (!m_connected)
    return;

  u32 type = 0;
  u32 i = 1;

  GDBAck();

  while (m_cmd_bfr[i] != ',')
    type = (type << 4) | HexToChar(m_cmd_bfr[i++]);
  i++;

  GDBBPType breakpoint_type = GDBBPType::None;
  switch (type)
  {
  case 0:
  case 1:
    breakpoint_type = GDBBPType::Execute;
    break;
  case 2:
    breakpoint_type = GDBBPType::Write;
    break;
  case 3:
    breakpoint_type = GDBBPType::Read;
    break;
  case 4:
    breakpoint_type = GDBBPType::Access;
    break;
  default:
    return GDBReply("E01");
  }

  u32 addr = 0;
  u32 len = 0;

  while (m_cmd_bfr[i] != ',')
    addr = (addr << 4) | HexToChar(m_cmd_bfr[i++]);
  i++;

  while (i < m_cmd_len)
    len = (len << 4) | HexToChar(m_cmd_bfr[i++]);

  AddBreakpoint(breakpoint_type, addr, len);
  GDBReply("OK");
}

void GDBThread::GDBRemoveBreakpoint()
{
  if (!m_connected)
    return;

  u32 type = 0;
  u32 i = 1;

  GDBAck();

  while (m_cmd_bfr[i] != ',')
    type = (type << 4) | HexToChar(m_cmd_bfr[i++]);
  i++;

  GDBBPType breakpoint_type = GDBBPType::None;
  switch (type)
  {
  case 0:
  case 1:
    breakpoint_type = GDBBPType::Execute;
    break;
  case 2:
    breakpoint_type = GDBBPType::Write;
    break;
  case 3:
    breakpoint_type = GDBBPType::Read;
    break;
  case 4:
    breakpoint_type = GDBBPType::Access;
    break;
  default:
    return GDBReply("E01");
  }

  u32 addr = 0;
  u32 len = 0;

  while (m_cmd_bfr[i] != ',')
    addr = (addr << 4) | HexToChar(m_cmd_bfr[i++]);
  i++;

  while (i < m_cmd_len)
    len = (len << 4) | HexToChar(m_cmd_bfr[i++]);

  RemoveBreakpoint(breakpoint_type, addr, len);
  GDBReply("OK");
}

void GDBThread::GDBPause()
{
  if (!m_connected)
    return;

  GDBAck();

  if (CPU::State::Stepping != CPU::GetState())
  {
    CPU::EnableStepping(true);

    UpdateState(CPU::GetState());
  }
}

void GDBThread::GDBSignal(SignalType signal, u64 addr, MemCheckCondition cond)
{
  m_last_signal = signal;
  m_last_signal_address = addr;
  m_last_signal_condition = cond;

  GDBHandleSignal();
}

void GDBThread::AddBreakpoint(GDBBPType type, u32 addr, u32 len)
{
  bool is_mem_check = false;

  TMemCheck mem_check;

  mem_check.start_address = addr;
  mem_check.end_address = addr + ((0 < len) ? len - 1 : 0);
  mem_check.is_ranged = 1 < len;
  mem_check.is_break_on_read = false;
  mem_check.is_break_on_write = false;
  mem_check.log_on_hit = true;
  mem_check.break_on_hit = true;

  switch (type)
  {
  case GDBBPType::Execute:
  {
    is_mem_check = false;
  }
  break;
  case GDBBPType::Write:
  {
    is_mem_check = true;
    mem_check.is_break_on_write = true;
  }
  break;
  case GDBBPType::Read:
  {
    is_mem_check = true;
    mem_check.is_break_on_read = true;
  }
  break;
  case GDBBPType::Access:
  {
    is_mem_check = true;
    mem_check.is_break_on_read = true;
    mem_check.is_break_on_write = true;
  }
  break;
  }

  if (is_mem_check)
  {
    PowerPC::memchecks.Add(mem_check);
  }
  else
  {
    PowerPC::breakpoints.Add(addr & ~3);
  }

  INFO_LOG(GDB_THREAD, "gdb: added a %d breakpoint: %08x bytes at %08X\n", static_cast<int>(type),
           len, addr);
}

void GDBThread::RemoveBreakpoint(GDBBPType type, u32 addr, u32 len)
{
  bool is_mem_check = false;

  switch (type)
  {
  case GDBBPType::Execute:
  {
    is_mem_check = false;
  }
  break;
  case GDBBPType::Write:
  case GDBBPType::Read:
  case GDBBPType::Access:
  {
    is_mem_check = true;
  }
  break;
  }

  if (is_mem_check)
  {
    PowerPC::memchecks.Remove(addr);
  }
  else
  {
    PowerPC::breakpoints.Remove(addr);
  }

  INFO_LOG(GDB_THREAD, "gdb: removed a %d breakpoint: %08x bytes at %08X\n", static_cast<int>(type),
           len, addr);
}
