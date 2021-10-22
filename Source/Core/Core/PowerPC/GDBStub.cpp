// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#include <optional>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
typedef SSIZE_T ssize_t;
#define SHUT_RDWR SD_BOTH
#else
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include "Common/Logging/Log.h"
#include "Common/SocketContext.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/Host.h"
#include "Core/PowerPC/GDBStub.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCCache.h"
#include "Core/PowerPC/PowerPC.h"

namespace
{
std::optional<Common::SocketContext> s_socket_context;
}  // namespace

#define GDB_BFR_MAX 10000
#define GDB_MAX_BP 10

#define GDB_STUB_START '$'
#define GDB_STUB_END '#'
#define GDB_STUB_ACK '+'
#define GDB_STUB_NAK '-'

static int tmpsock = -1;
static int sock = -1;

static u8 cmd_bfr[GDB_BFR_MAX];
static u32 cmd_len;

static u32 sig = 0;
static u32 send_signal = 0;
static u32 step_break = 0;

typedef struct
{
  u32 active;
  u32 addr;
  u32 len;
} gdb_bp_t;

static gdb_bp_t bp_x[GDB_MAX_BP];
static gdb_bp_t bp_r[GDB_MAX_BP];
static gdb_bp_t bp_w[GDB_MAX_BP];
static gdb_bp_t bp_a[GDB_MAX_BP];

static const char* CommandBufferAsString()
{
  return reinterpret_cast<const char*>(cmd_bfr);
}

// private helpers
static u8 hex2char(u8 hex)
{
  if (hex >= '0' && hex <= '9')
    return hex - '0';
  else if (hex >= 'a' && hex <= 'f')
    return hex - 'a' + 0xa;
  else if (hex >= 'A' && hex <= 'F')
    return hex - 'A' + 0xa;

  ERROR_LOG_FMT(GDB_STUB, "Invalid nibble: {} ({:02x})", static_cast<char>(hex), hex);
  return 0;
}

static u8 nibble2hex(u8 n)
{
  n &= 0xf;
  if (n < 0xa)
    return '0' + n;
  else
    return 'A' + n - 0xa;
}

static void mem2hex(u8* dst, u8* src, u32 len)
{
  while (len-- > 0)
  {
    const u8 tmp = *src++;
    *dst++ = nibble2hex(tmp >> 4);
    *dst++ = nibble2hex(tmp);
  }
}

static void hex2mem(u8* dst, u8* src, u32 len)
{
  while (len-- > 0)
  {
    *dst++ = (hex2char(*src) << 4) | hex2char(*(src + 1));
    src += 2;
  }
}

static u8 gdb_read_byte()
{
  u8 c = '+';

  const ssize_t res = recv(sock, (char*)&c, 1, MSG_WAITALL);
  if (res != 1)
  {
    ERROR_LOG_FMT(GDB_STUB, "recv failed : {}", res);
    gdb_deinit();
  }

  return c;
}

static u8 gdb_calc_chksum()
{
  u32 len = cmd_len;
  u8* ptr = cmd_bfr;
  u8 c = 0;

  while (len-- > 0)
    c += *ptr++;

  return c;
}

static gdb_bp_t* gdb_bp_ptr(u32 type)
{
  switch (type)
  {
  case GDB_BP_TYPE_X:
    return bp_x;
  case GDB_BP_TYPE_R:
    return bp_x;
  case GDB_BP_TYPE_W:
    return bp_x;
  case GDB_BP_TYPE_A:
    return bp_x;
  default:
    return nullptr;
  }
}

static gdb_bp_t* gdb_bp_empty_slot(u32 type)
{
  gdb_bp_t* p;
  u32 i;

  p = gdb_bp_ptr(type);
  if (p == nullptr)
    return nullptr;

  for (i = 0; i < GDB_MAX_BP; i++)
  {
    if (p[i].active == 0)
      return &p[i];
  }

  return nullptr;
}

static gdb_bp_t* gdb_bp_find(u32 type, u32 addr, u32 len)
{
  gdb_bp_t* p;
  u32 i;

  p = gdb_bp_ptr(type);
  if (p == nullptr)
    return nullptr;

  for (i = 0; i < GDB_MAX_BP; i++)
  {
    if (p[i].active == 1 && p[i].addr == addr && p[i].len == len)
      return &p[i];
  }

  return nullptr;
}

static void gdb_bp_remove(u32 type, u32 addr, u32 len)
{
  gdb_bp_t* p;

  do
  {
    p = gdb_bp_find(type, addr, len);
    if (p != nullptr)
    {
      DEBUG_LOG_FMT(GDB_STUB, "gdb: removed a breakpoint: {:08x} bytes at {:08x}", len, addr);
      p->active = 0;
      memset(p, 0, sizeof(gdb_bp_t));
    }
  } while (p != nullptr);
}

static int gdb_bp_check(u32 addr, u32 type)
{
  gdb_bp_t* p;
  u32 i;

  p = gdb_bp_ptr(type);
  if (p == nullptr)
    return 0;

  for (i = 0; i < GDB_MAX_BP; i++)
  {
    if (p[i].active == 1 && (addr >= p[i].addr && addr < p[i].addr + p[i].len))
      return 1;
  }

  return 0;
}

static void gdb_nak()
{
  const char nak = GDB_STUB_NAK;
  const ssize_t res = send(sock, &nak, 1, 0);

  if (res != 1)
    ERROR_LOG_FMT(GDB_STUB, "send failed");
}

static void gdb_ack()
{
  const char ack = GDB_STUB_ACK;
  const ssize_t res = send(sock, &ack, 1, 0);

  if (res != 1)
    ERROR_LOG_FMT(GDB_STUB, "send failed");
}

static void gdb_read_command()
{
  cmd_len = 0;
  memset(cmd_bfr, 0, sizeof cmd_bfr);

  u8 c = gdb_read_byte();
  if (c == '+')
  {
    // ignore ack
    return;
  }
  else if (c == 0x03)
  {
    CPU::Break();
    gdb_signal(GDB_SIGTRAP);
    return;
  }
  else if (c != GDB_STUB_START)
  {
    DEBUG_LOG_FMT(GDB_STUB, "gdb: read invalid byte {:02x}", c);
    return;
  }

  while ((c = gdb_read_byte()) != GDB_STUB_END)
  {
    cmd_bfr[cmd_len++] = c;
    if (cmd_len == sizeof cmd_bfr)
    {
      ERROR_LOG_FMT(GDB_STUB, "gdb: cmd_bfr overflow");
      gdb_nak();
      return;
    }
  }

  u8 chk_read = hex2char(gdb_read_byte()) << 4;
  chk_read |= hex2char(gdb_read_byte());

  const u8 chk_calc = gdb_calc_chksum();

  if (chk_calc != chk_read)
  {
    ERROR_LOG_FMT(GDB_STUB,
                  "gdb: invalid checksum: calculated {:02x} and read {:02x} for ${}# (length: {})",
                  chk_calc, chk_read, CommandBufferAsString(), cmd_len);
    cmd_len = 0;

    gdb_nak();
    return;
  }

  DEBUG_LOG_FMT(GDB_STUB, "gdb: read command {} with a length of {}: {}",
                static_cast<char>(cmd_bfr[0]), cmd_len, CommandBufferAsString());
  gdb_ack();
}

static int gdb_data_available()
{
  struct timeval t;
  fd_set _fds, *fds = &_fds;

  FD_ZERO(fds);
  FD_SET(sock, fds);

  t.tv_sec = 0;
  t.tv_usec = 20;

  if (select(sock + 1, fds, nullptr, nullptr, &t) < 0)
  {
    ERROR_LOG_FMT(GDB_STUB, "select failed");
    return 0;
  }

  if (FD_ISSET(sock, fds))
    return 1;
  return 0;
}

static void gdb_reply(const char* reply)
{
  if (!gdb_active())
    return;

  memset(cmd_bfr, 0, sizeof cmd_bfr);

  cmd_len = (u32)strlen(reply);
  if (cmd_len + 4 > sizeof cmd_bfr)
    ERROR_LOG_FMT(GDB_STUB, "cmd_bfr overflow in gdb_reply");

  memcpy(cmd_bfr + 1, reply, cmd_len);

  cmd_len++;
  const u8 chk = gdb_calc_chksum();
  cmd_len--;
  cmd_bfr[0] = GDB_STUB_START;
  cmd_bfr[cmd_len + 1] = GDB_STUB_END;
  cmd_bfr[cmd_len + 2] = nibble2hex(chk >> 4);
  cmd_bfr[cmd_len + 3] = nibble2hex(chk);

  DEBUG_LOG_FMT(GDB_STUB, "gdb: reply (len: {}): {}", cmd_len, CommandBufferAsString());

  const char* ptr = (const char*)cmd_bfr;
  u32 left = cmd_len + 4;
  while (left > 0)
  {
    const int n = send(sock, ptr, left, 0);
    if (n < 0)
    {
      ERROR_LOG_FMT(GDB_STUB, "gdb: send failed");
      return gdb_deinit();
    }
    left -= n;
    ptr += n;
  }
}

static void gdb_handle_query()
{
  DEBUG_LOG_FMT(GDB_STUB, "gdb: query '{}'", CommandBufferAsString() + 1);

  if (!strcmp((const char*)(cmd_bfr + 1), "TStatus"))
  {
    return gdb_reply("T0");
  }

  gdb_reply("");
}

static void gdb_handle_set_thread()
{
  if (memcmp(cmd_bfr, "Hg0", 3) == 0 || memcmp(cmd_bfr, "Hc-1", 4) == 0 ||
      memcmp(cmd_bfr, "Hc0", 4) == 0 || memcmp(cmd_bfr, "Hc1", 4) == 0)
    return gdb_reply("OK");
  gdb_reply("E01");
}

static void gdb_handle_signal()
{
  char bfr[128];
  memset(bfr, 0, sizeof bfr);
  sprintf(bfr, "T%02x%02x:%08x;%02x:%08x;", sig, 64, PC, 1, GPR(1));
  gdb_reply(bfr);
}

static void wbe32hex(u8* p, u32 v)
{
  u32 i;
  for (i = 0; i < 8; i++)
    p[i] = nibble2hex(v >> (28 - 4 * i));
}

static void wbe64hex(u8* p, u64 v)
{
  u32 i;
  for (i = 0; i < 16; i++)
    p[i] = nibble2hex(v >> (60 - 4 * i));
}

static u32 re32hex(u8* p)
{
  u32 i;
  u32 res = 0;

  for (i = 0; i < 8; i++)
    res = (res << 4) | hex2char(p[i]);

  return res;
}

static u64 re64hex(u8* p)
{
  u32 i;
  u64 res = 0;

  for (i = 0; i < 16; i++)
    res = (res << 4) | hex2char(p[i]);

  return res;
}

static void gdb_read_register()
{
  static u8 reply[64];
  u32 id;

  memset(reply, 0, sizeof reply);
  id = hex2char(cmd_bfr[1]);
  if (cmd_bfr[2] != '\0')
  {
    id <<= 4;
    id |= hex2char(cmd_bfr[2]);
  }

  if (id < 32)
  {
    wbe32hex(reply, GPR(id));
  }
  else if (id >= 32 && id < 64)
  {
    wbe64hex(reply, rPS(id - 32).PS0AsU64());
  }
  else
  {
    switch (id)
    {
    case 64:
      wbe32hex(reply, PC);
      break;
    case 65:
      wbe32hex(reply, MSR.Hex);
      break;
    case 66:
      wbe32hex(reply, PowerPC::ppcState.cr.Get());
      break;
    case 67:
      wbe32hex(reply, LR);
      break;
    case 68:
      wbe32hex(reply, CTR);
      break;
    case 69:
      wbe32hex(reply, PowerPC::ppcState.spr[SPR_XER]);
      break;
    case 70:
      wbe32hex(reply, 0x0BADC0DE);
      break;
    case 71:
      wbe32hex(reply, FPSCR.Hex);
      break;
    default:
      return gdb_reply("E01");
      break;
    }
  }

  gdb_reply((char*)reply);
}

static void gdb_read_registers()
{
  static u8 bfr[GDB_BFR_MAX - 4];
  u8* bufptr = bfr;
  u32 i;

  memset(bfr, 0, sizeof bfr);

  for (i = 0; i < 32; i++)
  {
    wbe32hex(bufptr + i * 8, GPR(i));
  }
  bufptr += 32 * 8;

  gdb_reply((char*)bfr);
}

static void gdb_write_registers()
{
  u32 i;
  u8* bufptr = cmd_bfr;

  for (i = 0; i < 32; i++)
  {
    GPR(i) = re32hex(bufptr + i * 8);
  }
  bufptr += 32 * 8;

  gdb_reply("OK");
}

static void gdb_write_register()
{
  u32 id;

  u8* bufptr = cmd_bfr + 3;

  id = hex2char(cmd_bfr[1]);
  if (cmd_bfr[2] != '=')
  {
    ++bufptr;
    id <<= 4;
    id |= hex2char(cmd_bfr[2]);
  }

  if (id < 32)
  {
    GPR(id) = re32hex(bufptr);
  }
  else if (id >= 32 && id < 64)
  {
    rPS(id - 32).SetPS0(re64hex(bufptr));
  }
  else
  {
    switch (id)
    {
    case 64:
      PC = re32hex(bufptr);
      break;
    case 65:
      MSR.Hex = re32hex(bufptr);
      break;
    case 66:
      PowerPC::ppcState.cr.Set(re32hex(bufptr));
      break;
    case 67:
      LR = re32hex(bufptr);
      break;
    case 68:
      CTR = re32hex(bufptr);
      break;
    case 69:
      PowerPC::ppcState.spr[SPR_XER] = re32hex(bufptr);
      break;
    case 70:
      // do nothing, we dont have MQ
      break;
    case 71:
      FPSCR.Hex = re32hex(bufptr);
      break;
    default:
      return gdb_reply("E01");
      break;
    }
  }

  gdb_reply("OK");
}

static void gdb_read_mem()
{
  static u8 reply[GDB_BFR_MAX - 4];
  u32 addr, len;
  u32 i;

  i = 1;
  addr = 0;
  while (cmd_bfr[i] != ',')
    addr = (addr << 4) | hex2char(cmd_bfr[i++]);
  i++;

  len = 0;
  while (i < cmd_len)
    len = (len << 4) | hex2char(cmd_bfr[i++]);
  DEBUG_LOG_FMT(GDB_STUB, "gdb: read memory: {:08x} bytes from {:08x}", len, addr);

  if (len * 2 > sizeof reply)
    gdb_reply("E01");
  u8* data = Memory::GetPointer(addr);
  if (!data)
    return gdb_reply("E0");
  mem2hex(reply, data, len);
  reply[len * 2] = '\0';
  gdb_reply((char*)reply);
}

static void gdb_write_mem()
{
  u32 addr, len;
  u32 i;

  i = 1;
  addr = 0;
  while (cmd_bfr[i] != ',')
    addr = (addr << 4) | hex2char(cmd_bfr[i++]);
  i++;

  len = 0;
  while (cmd_bfr[i] != ':')
    len = (len << 4) | hex2char(cmd_bfr[i++]);
  DEBUG_LOG_FMT(GDB_STUB, "gdb: write memory: {:08x} bytes to {:08x}", len, addr);

  u8* dst = Memory::GetPointer(addr);
  if (!dst)
    return gdb_reply("E00");
  hex2mem(dst, cmd_bfr + i + 1, len);
  gdb_reply("OK");
}

// forces a break on next instruction check
void gdb_break()
{
  step_break = 1;
  send_signal = 1;
}

static void gdb_step()
{
  gdb_break();
}

static void gdb_continue()
{
  send_signal = 1;
}

bool gdb_add_bp(u32 type, u32 addr, u32 len)
{
  gdb_bp_t* bp;
  bp = gdb_bp_empty_slot(type);
  if (bp == nullptr)
    return false;

  bp->active = 1;
  bp->addr = addr;
  bp->len = len;

  DEBUG_LOG_FMT(GDB_STUB, "gdb: added {} breakpoint: {:08x} bytes at {:08x}", type, bp->len,
                bp->addr);
  return true;
}

static void _gdb_add_bp()
{
  u32 type;
  u32 i, addr = 0, len = 0;

  type = hex2char(cmd_bfr[1]);
  switch (type)
  {
  case 0:
  case 1:
    type = GDB_BP_TYPE_X;
    break;
  case 2:
    type = GDB_BP_TYPE_W;
    break;
  case 3:
    type = GDB_BP_TYPE_R;
    break;
  case 4:
    type = GDB_BP_TYPE_A;
    break;
  default:
    return gdb_reply("E01");
  }

  i = 3;
  while (cmd_bfr[i] != ',')
    addr = addr << 4 | hex2char(cmd_bfr[i++]);
  i++;

  while (i < cmd_len)
    len = len << 4 | hex2char(cmd_bfr[i++]);

  if (!gdb_add_bp(type, addr, len))
    return gdb_reply("E02");
  gdb_reply("OK");
}

static void gdb_remove_bp()
{
  u32 type, addr, len, i;

  type = hex2char(cmd_bfr[1]);
  switch (type)
  {
  case 0:
  case 1:
    type = GDB_BP_TYPE_X;
    break;
  case 2:
    type = GDB_BP_TYPE_W;
    break;
  case 3:
    type = GDB_BP_TYPE_R;
    break;
  case 4:
    type = GDB_BP_TYPE_A;
    break;
  default:
    return gdb_reply("E01");
  }

  addr = 0;
  len = 0;

  i = 3;
  while (cmd_bfr[i] != ',')
    addr = (addr << 4) | hex2char(cmd_bfr[i++]);
  i++;

  while (i < cmd_len)
    len = (len << 4) | hex2char(cmd_bfr[i++]);

  gdb_bp_remove(type, addr, len);
  gdb_reply("OK");
}

void gdb_handle_exception()
{
  while (gdb_active())
  {
    if (!gdb_data_available())
      continue;
    gdb_read_command();
    if (cmd_len == 0)
      continue;

    switch (cmd_bfr[0])
    {
    case 'q':
      gdb_handle_query();
      break;
    case 'H':
      gdb_handle_set_thread();
      break;
    case '?':
      gdb_handle_signal();
      break;
    case 'k':
      gdb_deinit();
      INFO_LOG_FMT(GDB_STUB, "killed by gdb");
      return;
    case 'g':
      gdb_read_registers();
      break;
    case 'G':
      gdb_write_registers();
      break;
    case 'p':
      gdb_read_register();
      break;
    case 'P':
      gdb_write_register();
      break;
    case 'm':
      gdb_read_mem();
      break;
    case 'M':
      gdb_write_mem();
      PowerPC::ppcState.iCache.Reset();
      Host_UpdateDisasmDialog();
      break;
    case 's':
      gdb_step();
      return;
    case 'C':
    case 'c':
      gdb_continue();
      return;
    case 'z':
      gdb_remove_bp();
      break;
    case 'Z':
      _gdb_add_bp();
      break;
    default:
      gdb_reply("");
      break;
    }
  }
}

// exported functions

static void gdb_init_generic(int domain, const sockaddr* server_addr, socklen_t server_addrlen,
                             sockaddr* client_addr, socklen_t* client_addrlen);

#ifndef _WIN32
void gdb_init_local(const char* socket)
{
  unlink(socket);

  sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, socket);

  gdb_init_generic(PF_LOCAL, (const sockaddr*)&addr, sizeof(addr), NULL, NULL);
}
#endif

void gdb_init(u32 port)
{
  sockaddr_in saddr_server = {};
  sockaddr_in saddr_client;

  saddr_server.sin_family = AF_INET;
  saddr_server.sin_port = htons(port);
  saddr_server.sin_addr.s_addr = INADDR_ANY;

  socklen_t client_addrlen = sizeof(saddr_client);

  gdb_init_generic(PF_INET, (const sockaddr*)&saddr_server, sizeof(saddr_server),
                   (sockaddr*)&saddr_client, &client_addrlen);

  saddr_client.sin_addr.s_addr = ntohl(saddr_client.sin_addr.s_addr);
}

static void gdb_init_generic(int domain, const sockaddr* server_addr, socklen_t server_addrlen,
                             sockaddr* client_addr, socklen_t* client_addrlen)
{
  s_socket_context.emplace();
  memset(bp_x, 0, sizeof bp_x);
  memset(bp_r, 0, sizeof bp_r);
  memset(bp_w, 0, sizeof bp_w);
  memset(bp_a, 0, sizeof bp_a);

  tmpsock = socket(domain, SOCK_STREAM, 0);
  if (tmpsock == -1)
    ERROR_LOG_FMT(GDB_STUB, "Failed to create gdb socket");

  int on = 1;
  if (setsockopt(tmpsock, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof on) < 0)
    ERROR_LOG_FMT(GDB_STUB, "Failed to setsockopt");

  if (bind(tmpsock, server_addr, server_addrlen) < 0)
    ERROR_LOG_FMT(GDB_STUB, "Failed to bind gdb socket");

  if (listen(tmpsock, 1) < 0)
    ERROR_LOG_FMT(GDB_STUB, "Failed to listen to gdb socket");

  INFO_LOG_FMT(GDB_STUB, "Waiting for gdb to connect...");

  sock = accept(tmpsock, client_addr, client_addrlen);
  if (sock < 0)
    ERROR_LOG_FMT(GDB_STUB, "Failed to accept gdb client");
  INFO_LOG_FMT(GDB_STUB, "Client connected.");

#ifdef _WIN32
  closesocket(tmpsock);
#else
  close(tmpsock);
#endif
  tmpsock = -1;
}

void gdb_deinit()
{
  if (tmpsock != -1)
  {
    shutdown(tmpsock, SHUT_RDWR);
    tmpsock = -1;
  }
  if (sock != -1)
  {
    shutdown(sock, SHUT_RDWR);
    sock = -1;
  }

  s_socket_context.reset();
}

bool gdb_active()
{
  return tmpsock != -1 || sock != -1;
}

int gdb_signal(u32 s)
{
  if (sock == -1)
    return 1;

  sig = s;

  if (send_signal)
  {
    gdb_handle_signal();
    send_signal = 0;
  }

  return 0;
}

int gdb_bp_x(u32 addr)
{
  if (sock == -1)
    return 0;

  if (step_break)
  {
    step_break = 0;

    DEBUG_LOG_FMT(GDB_STUB, "Step was successful.");
    return 1;
  }

  return gdb_bp_check(addr, GDB_BP_TYPE_X);
}

int gdb_bp_r(u32 addr)
{
  if (sock == -1)
    return 0;

  return gdb_bp_check(addr, GDB_BP_TYPE_R);
}

int gdb_bp_w(u32 addr)
{
  if (sock == -1)
    return 0;

  return gdb_bp_check(addr, GDB_BP_TYPE_W);
}

int gdb_bp_a(u32 addr)
{
  if (sock == -1)
    return 0;

  return gdb_bp_check(addr, GDB_BP_TYPE_A);
}
