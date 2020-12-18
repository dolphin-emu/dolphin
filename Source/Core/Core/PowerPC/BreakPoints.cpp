// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/BreakPoints.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <expr.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

struct ExprDeleter
{
  void operator()(expr* expression) const { expr_destroy(expression, nullptr); }
};

using ExprPointer = std::unique_ptr<expr, ExprDeleter>;

class ExprVarList
{
public:
  ExprVarList() = default;
  ExprVarList(const ExprVarList&) = delete;
  ExprVarList& operator=(const ExprVarList&) = delete;

  ExprVarList(ExprVarList&& other) noexcept : m_vars{std::exchange(other.m_vars, {})} {}

  ExprVarList& operator=(ExprVarList&& other) noexcept
  {
    std::swap(m_vars, other.m_vars);
    return *this;
  }

  ~ExprVarList() { expr_destroy(nullptr, &m_vars); }

  expr_var* GetHead() { return m_vars.head; }
  expr_var_list* GetAddress() { return &m_vars; }

private:
  expr_var_list m_vars = {};
};

static std::optional<int> ParseGPR(const char* name)
{
  if (std::strlen(name) >= 2 && name[0] == 'r' && std::isdigit(name[1]))
  {
    char* end = nullptr;
    int index = std::strtol(&name[1], &end, 10);
    if (index < 32 && *end == '\0')
      return index;
  }
  return {};
}

template <typename T>
static T HostRead(u32 address);

template <typename T>
static void HostWrite(T var, u32 address);

template <>
u8 HostRead(u32 address)
{
  return PowerPC::HostRead_U8(address);
}

template <>
u16 HostRead(u32 address)
{
  return PowerPC::HostRead_U16(address);
}

template <>
u32 HostRead(u32 address)
{
  return PowerPC::HostRead_U32(address);
}

template <>
u64 HostRead(u32 address)
{
  return PowerPC::HostRead_U64(address);
}

template <>
void HostWrite(u8 var, u32 address)
{
  PowerPC::HostWrite_U8(var, address);
}

template <>
void HostWrite(u16 var, u32 address)
{
  PowerPC::HostWrite_U16(var, address);
}

template <>
void HostWrite(u32 var, u32 address)
{
  PowerPC::HostWrite_U32(var, address);
}

template <>
void HostWrite(u64 var, u32 address)
{
  PowerPC::HostWrite_U64(var, address);
}

template <typename T, typename U = T>
double HostReadFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;
  u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 0)));
  return Common::BitCast<T>(HostRead<U>(address));
}

template <typename T, typename U = T>
double HostWriteFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 2)
    return 0;
  T var = static_cast<T>(expr_eval(&vec_nth(args, 0)));
  u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 1)));
  HostWrite<U>(Common::BitCast<U>(var), address);
  return var;
}

static expr_func g_expr_funcs[] = {{"read_u8", HostReadFunc<u8>},
                                   {"read_s8", HostReadFunc<s8, u8>},
                                   {"read_u16", HostReadFunc<u16>},
                                   {"read_s16", HostReadFunc<s16, u16>},
                                   {"read_u32", HostReadFunc<u32>},
                                   {"read_s32", HostReadFunc<s32, u32>},
                                   {"read_f32", HostReadFunc<float, u32>},
                                   {"read_f64", HostReadFunc<double, u64>},
                                   {"write_u8", HostWriteFunc<u8>},
                                   {"write_u16", HostWriteFunc<u16>},
                                   {"write_u32", HostWriteFunc<u32>},
                                   {"write_f32", HostWriteFunc<float, u32>},
                                   {"write_f64", HostWriteFunc<double, u64>},
                                   {}};

static double EvaluateExpression(const std::string& expression_string)
{
  ExprVarList vars;
  ExprPointer expression{expr_create(expression_string.c_str(), expression_string.length(),
                                     vars.GetAddress(), g_expr_funcs)};
  if (!expression)
    return false;

  for (auto* v = vars.GetHead(); v != nullptr; v = v->next)
  {
    auto index = ParseGPR(v->name);
    if (index)
      v->value = GPR(*index);
  }

  return expr_eval(expression.get());
}

static bool EvaluateCondition(const std::string& condition)
{
  return condition.empty() || EvaluateExpression(condition) != 0;
}

bool BreakPoints::IsAddressBreakPoint(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                     [address](const auto& bp) { return bp.address == address; });
}

bool BreakPoints::IsTempBreakPoint(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(), [address](const auto& bp) {
    return bp.address == address && bp.is_temporary;
  });
}

bool BreakPoints::IsBreakPointBreakOnHit(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(), [address](const auto& bp) {
    return bp.address == address && bp.break_on_hit && EvaluateCondition(bp.condition);
  });
}

bool BreakPoints::IsBreakPointLogOnHit(u32 address) const
{
  return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                     [address](const auto& bp) { return bp.address == address && bp.log_on_hit; });
}

BreakPoints::TBreakPointsStr BreakPoints::GetStrings() const
{
  TBreakPointsStr bp_strings;
  for (const TBreakPoint& bp : m_breakpoints)
  {
    if (!bp.is_temporary)
    {
      std::ostringstream ss;
      ss << "$" << fmt::format("{:08x}", bp.address) << " ";
      if (bp.is_enabled)
        ss << "n";
      if (bp.log_on_hit)
        ss << "l";
      if (bp.break_on_hit)
        ss << "b";
      if (!bp.condition.empty())
        ss << "c " << bp.condition;
      bp_strings.emplace_back(ss.str());
    }
  }

  return bp_strings;
}

void BreakPoints::AddFromStrings(const TBreakPointsStr& bp_strings)
{
  for (const std::string& bp_string : bp_strings)
  {
    TBreakPoint bp;
    std::stringstream ss;
    ss << std::hex << bp_string;
    if (ss.peek() == '$')
      ss.ignore();
    ss >> bp.address;
    std::string flags;
    ss >> flags;
    bp.is_enabled = flags.find('n') != std::string::npos;
    bp.log_on_hit = flags.find('l') != std::string::npos;
    bp.break_on_hit = flags.find('b') != std::string::npos;
    if (flags.find('c') != std::string::npos)
    {
      ss >> std::ws;
      std::getline(ss, bp.condition);
    }
    bp.is_temporary = false;
    Add(bp);
  }
}

void BreakPoints::Add(const TBreakPoint& bp)
{
  if (IsAddressBreakPoint(bp.address))
    return;

  m_breakpoints.push_back(bp);

  JitInterface::InvalidateICache(bp.address, 4, true);
}

void BreakPoints::Add(u32 address, bool temp)
{
  BreakPoints::Add(address, temp, true, false, {});
}

void BreakPoints::Add(u32 address, bool temp, bool break_on_hit, bool log_on_hit,
                      std::string condition)
{
  // Only add new addresses
  if (IsAddressBreakPoint(address))
    return;

  TBreakPoint bp;  // breakpoint settings
  bp.is_enabled = true;
  bp.is_temporary = temp;
  bp.break_on_hit = break_on_hit;
  bp.log_on_hit = log_on_hit;
  bp.address = address;
  bp.condition = std::move(condition);

  m_breakpoints.push_back(bp);

  JitInterface::InvalidateICache(address, 4, true);
}

void BreakPoints::Remove(u32 address)
{
  const auto iter = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                                 [address](const auto& bp) { return bp.address == address; });

  if (iter == m_breakpoints.cend())
    return;

  m_breakpoints.erase(iter);
  JitInterface::InvalidateICache(address, 4, true);
}

void BreakPoints::Clear()
{
  for (const TBreakPoint& bp : m_breakpoints)
  {
    JitInterface::InvalidateICache(bp.address, 4, true);
  }

  m_breakpoints.clear();
}

void BreakPoints::ClearAllTemporary()
{
  auto bp = m_breakpoints.begin();
  while (bp != m_breakpoints.end())
  {
    if (bp->is_temporary)
    {
      JitInterface::InvalidateICache(bp->address, 4, true);
      bp = m_breakpoints.erase(bp);
    }
    else
    {
      ++bp;
    }
  }
}

MemChecks::TMemChecksStr MemChecks::GetStrings() const
{
  TMemChecksStr mc_strings;
  for (const TMemCheck& mc : m_mem_checks)
  {
    std::ostringstream ss;
    ss << std::hex << mc.start_address;
    ss << " " << (mc.is_ranged ? mc.end_address : mc.start_address) << " "
       << (mc.is_ranged ? "n" : "") << (mc.is_break_on_read ? "r" : "")
       << (mc.is_break_on_write ? "w" : "") << (mc.log_on_hit ? "l" : "")
       << (mc.break_on_hit ? "p" : "");
    mc_strings.push_back(ss.str());
  }

  return mc_strings;
}

void MemChecks::AddFromStrings(const TMemChecksStr& mc_strings)
{
  for (const std::string& mc_string : mc_strings)
  {
    TMemCheck mc;
    std::stringstream ss;
    ss << std::hex << mc_string;
    ss >> mc.start_address;
    mc.is_ranged = mc_string.find('n') != mc_string.npos;
    mc.is_break_on_read = mc_string.find('r') != mc_string.npos;
    mc.is_break_on_write = mc_string.find('w') != mc_string.npos;
    mc.log_on_hit = mc_string.find('l') != mc_string.npos;
    mc.break_on_hit = mc_string.find('b') != mc_string.npos;
    if (mc.is_ranged)
      ss >> mc.end_address;
    else
      mc.end_address = mc.start_address;
    Add(mc);
  }
}

void MemChecks::Add(const TMemCheck& memory_check)
{
  if (GetMemCheck(memory_check.start_address) != nullptr)
    return;

  bool had_any = HasAny();
  Core::RunAsCPUThread([&] {
    m_mem_checks.push_back(memory_check);
    // If this is the first one, clear the JIT cache so it can switch to
    // watchpoint-compatible code.
    if (!had_any)
      JitInterface::ClearCache();
    PowerPC::DBATUpdated();
  });
}

void MemChecks::Remove(u32 address)
{
  const auto iter =
      std::find_if(m_mem_checks.cbegin(), m_mem_checks.cend(),
                   [address](const auto& check) { return check.start_address == address; });

  if (iter == m_mem_checks.cend())
    return;

  Core::RunAsCPUThread([&] {
    m_mem_checks.erase(iter);
    if (!HasAny())
      JitInterface::ClearCache();
    PowerPC::DBATUpdated();
  });
}

void MemChecks::Clear()
{
  Core::RunAsCPUThread([&] {
    m_mem_checks.clear();
    JitInterface::ClearCache();
    PowerPC::DBATUpdated();
  });
}

TMemCheck* MemChecks::GetMemCheck(u32 address, size_t size)
{
  const auto iter =
      std::find_if(m_mem_checks.begin(), m_mem_checks.end(), [address, size](const auto& mc) {
        return mc.end_address >= address && address + size - 1 >= mc.start_address;
      });

  // None found
  if (iter == m_mem_checks.cend())
    return nullptr;

  return &*iter;
}

bool MemChecks::OverlapsMemcheck(u32 address, u32 length) const
{
  if (!HasAny())
    return false;

  const u32 page_end_suffix = length - 1;
  const u32 page_end_address = address | page_end_suffix;

  return std::any_of(m_mem_checks.cbegin(), m_mem_checks.cend(), [&](const auto& mc) {
    return ((mc.start_address | page_end_suffix) == page_end_address ||
            (mc.end_address | page_end_suffix) == page_end_address) ||
           ((mc.start_address | page_end_suffix) < page_end_address &&
            (mc.end_address | page_end_suffix) > page_end_address);
  });
}

bool TMemCheck::Action(Common::DebugInterface* debug_interface, u32 value, u32 addr, bool write,
                       size_t size, u32 pc)
{
  if ((write && is_break_on_write) || (!write && is_break_on_read))
  {
    if (log_on_hit)
    {
      NOTICE_LOG_FMT(MEMMAP, "MBP {:08x} ({}) {}{} {:x} at {:08x} ({})", pc,
                     debug_interface->GetDescription(pc), write ? "Write" : "Read", size * 8, value,
                     addr, debug_interface->GetDescription(addr));
    }
    if (break_on_hit)
      return true;
  }
  return false;
}
