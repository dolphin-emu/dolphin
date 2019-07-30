#include "Core/Gecko2AR.h"

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"

#include <cstdio>
#include <string>
#include <vector>

struct ARCode
{
  union
  {
    uint32_t cmd;
    struct
    {
      uint32_t gcaddr : 25;
      uint32_t size : 2;
      uint32_t type : 3;
      uint32_t subtype : 2;
    };
  };
  uint32_t value;
};

struct GKCode
{
  union
  {
    uint32_t cmd;
    struct
    {
      uint32_t gcaddr : 25;
      uint32_t subtype : 3;
      uint32_t pointer : 1;
      uint32_t type : 3;
    };
  };
  uint32_t value;
};

static uint32_t g_spare_address;
using GKVector = std::vector<Gecko::GeckoCode::Code>;

// #define DEBUG_Gecko2AR
#undef CopyMemory
#if defined(DEBUG_Gecko2AR)
#include <android/log.h>
#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, "Gecko2AR", __VA_ARGS__)
#else
#define LOG_INFO(...)
#endif

namespace AR
{
// Data Types
enum class DataType
{
  BIT8 = 0x00,
  BIT16 = 0x01,
  BIT32 = 0x02,
  BIT32_FLOAT = 0x03,
};

// Conditional Line Counts
enum class Conditional
{
  IF_EQUAL = 0x01,
  IF_NOT_EQUAL = 0x02,
  IF_LESS_SIGNED = 0x03,
  IF_GREATER_SIGNED = 0x04,
  IF_LESS_UNSIGNED = 0x05,
  IF_GREATER_UNSIGNED = 0x06,
  IF_AND = 0x07,  // bitwise AND

  SKIP_ONE_LINE = 0x00,
  SKIP_TWO_LINES = 0x01,
  SKIP_ALL_LINES_UNTIL = 0x02,
  SKIP_ALL_LINES = 0x03,
};

const uint32_t MASK_BYTE = 0xFFU;
const uint32_t MASK_SHORT = 0xFFFFU;
const uint32_t MASK_GCADDR = 0x1FFFFFFU;

static void CopyMemory(uint32_t dst, uint32_t src, uint32_t size, bool pointer,
                       std::vector<ARCode>& cheat)
{
  LOG_INFO("# AR CopyMemory\n");
  ARCode ar{};
  // zero code
  ar.cmd = 0;
  // copy dst
  ar.value = (dst & MASK_GCADDR) | 0x86000000U;
  cheat.push_back(ar);

  // copy src
  ar.cmd = src;
  // is pointer ? and copy size
  ar.value = (pointer ? 0x01000000U : 0) | size;

  cheat.push_back(ar);
}

static void WriteMemory(uint32_t dst, uint32_t val, DataType size, bool pointer,
                        std::vector<ARCode>& cheat)
{
  LOG_INFO("# AR WriteMemory\n");
  ARCode ar{};
  ar.type = 0;
  ar.subtype = pointer ? 1 : 0;
  ar.size = static_cast<uint32_t>(size);
  ar.gcaddr = dst;
  switch (size)
  {
  case DataType::BIT8:
    ar.value = val & MASK_BYTE;
    break;
  case DataType::BIT16:
    ar.value = val & MASK_SHORT;
    break;
  case DataType::BIT32:
  case DataType::BIT32_FLOAT:
    ar.value = val;
    break;
  }
  cheat.push_back(ar);
}

static void WriteAndFill(uint32_t dst, uint32_t val, uint32_t count, DataType size, bool pointer,
                         std::vector<ARCode>& cheat)
{
  LOG_INFO("# AR WriteAndFill\n");
  ARCode ar{};
  ar.type = 0;
  ar.subtype = pointer ? 1 : 0;
  ar.size = static_cast<uint32_t>(size);
  ar.gcaddr = dst;

  if (!pointer)
  {
    if (size == DataType::BIT8)
    {
      ar.value = count << 8U | (val & MASK_BYTE);
    }
    else if (size == DataType::BIT16)
    {
      ar.value = count << 16U | (val & MASK_SHORT);
    }
    cheat.push_back(ar);
  }
  else
  {
    // ar code doesn't support fill pointer address
    // so copy n times
    for (uint32_t i = 0; i < count + 1; ++i)
    {
      // offset and value
      if (size == DataType::BIT8)
      {
        ar.value = i << 8U | (val & MASK_BYTE);
      }
      else if (size == DataType::BIT16)
      {
        ar.value = i << 16U | (val & MASK_SHORT);
      }
      cheat.push_back(ar);
    }
  }
}

static void ValueAdd(uint32_t dst, uint32_t val, DataType size, std::vector<ARCode>& cheat)
{
  LOG_INFO("# AR ValueAdd\n");
  ARCode ar{};
  ar.type = 0;
  ar.subtype = 2;
  ar.size = static_cast<uint32_t>(size);
  ar.gcaddr = dst;
  switch (size)
  {
  case DataType::BIT8:
    ar.value = val & MASK_BYTE;
    break;
  case DataType::BIT16:
    ar.value = val & MASK_SHORT;
    break;
  case DataType::BIT32:
  case DataType::BIT32_FLOAT:
    ar.value = val;
    break;
  }
  cheat.push_back(ar);
}
}  // namespace AR

class GeckoRegister
{
public:
  GeckoRegister() = default;
  ~GeckoRegister() = default;

  void Reset()
  {
    m_is_owned = false;
    m_is_pointer = false;
    m_address = 0x80000000U;
    m_holder = 0;
  }

  void Load(uint32_t addr)
  {
    m_is_pointer = true;
    m_holder = addr;
  }

  void Set(uint32_t addr, std::vector<ARCode>& cheat)
  {
    if (addr >= 0x90000000U)
    {
      uint32_t value = addr;
      addr = g_spare_address;
      g_spare_address += 4;
      AR::WriteMemory(addr, value, AR::DataType::BIT32, false, cheat);
      Load(addr);
    }
    else
    {
      m_is_pointer = false;
      m_address = addr;
    }
  }

  void Add(const GeckoRegister& reg, std::vector<ARCode>& cheat)
  {
    if (m_is_pointer && reg.m_is_pointer)
    {
      // don't support in action replay
      return;
    }
    else if (m_is_pointer)
    {
      Add(reg.m_address, cheat);
    }
    else if (reg.m_is_pointer)
    {
      Load(reg.m_holder);
      Add(m_address, cheat);
    }
    else
    {
      m_address += reg.m_address;
    }
  }

  void Add(uint32_t offset, std::vector<ARCode>& cheat)
  {
    if (m_is_pointer)
    {
      if (offset == 0)
        return;

      if (!m_is_owned)
      {
        uint32_t addr = g_spare_address;
        g_spare_address += 4;
        AR::CopyMemory(addr, m_holder, 4, false, cheat);
        m_holder = addr;
        m_is_owned = true;
      }
      AR::ValueAdd(m_holder, offset, AR::DataType::BIT32, cheat);
    }
    else
    {
      m_address += offset;
    }
  }

  GeckoRegister Clone(std::vector<ARCode>& cheat) const
  {
    GeckoRegister reg;
    if (m_is_pointer)
    {
      reg.Load(m_holder);
    }
    else
    {
      reg.Set(m_address, cheat);
    }
    return reg;
  }

  void Clone(const GeckoRegister& reg, std::vector<ARCode>& cheat)
  {
    m_is_pointer = reg.m_is_pointer;
    if (reg.m_is_pointer)
    {
      Load(reg.m_holder);
    }
    else
    {
      m_address = reg.m_address;
    }
  }

  uint32_t GetAddress() { return m_is_pointer ? m_holder : (m_address & ~1U); }

  uint32_t GetAddressWithOffset(uint32_t offset, std::vector<ARCode>& cheat) const
  {
    if (m_is_pointer)
    {
      GeckoRegister reg;
      reg.Load(m_holder);
      reg.Add(offset, cheat);
      return reg.m_holder;
    }
    else
    {
      return (m_address + offset) & ~1U;
    }
  }

  bool IsPointer() const { return m_is_pointer; }

private:
  uint32_t m_holder = false;
  uint32_t m_address = 0x80000000U;
  bool m_is_pointer = false;
  bool m_is_owned = false;
};

class GeckoConverter
{
public:
  GeckoConverter() = default;
  ~GeckoConverter() = default;

  GeckoRegister GetStepRegister(const GKCode& gk);
  uint32_t GetGCAddress(const GKCode& gk);
  bool IsPointer(const GKCode& gk);

  bool Convert(const GKVector& lines);

  bool LoadRegister(const GKCode& gk);
  bool SetRegister(const GKCode& gk);
  bool StoreRegister(const GKCode& gk);

  bool MemoryWriteOps(const GKCode& gk, const GKVector& lines, uint32_t& i);
  bool ConditionCodeOps(const GKCode& gk);
  bool RegiesterActionOps(const GKCode& gk);

  void InsertASM(const GKCode& gk, const GKVector& lines, uint32_t& i);
  void AddressRangeCheck(const GKCode& gk);
  void FullTerminator(const GKCode& gk);
  void OnOffSwitch();

  void GetAREntrys(std::vector<ActionReplay::AREntry>& ops) const;

private:
  GeckoRegister m_ba;
  GeckoRegister m_po;
  std::vector<ARCode> m_cheat;
};

void GeckoConverter::GetAREntrys(std::vector<ActionReplay::AREntry>& ops) const
{
  for (const auto& ar : m_cheat)
  {
    ops.emplace_back(ar.cmd, ar.value);
  }
}

GeckoRegister GeckoConverter::GetStepRegister(const GKCode& gk)
{
  return gk.pointer == 0 ? m_ba.Clone(m_cheat) : m_po.Clone(m_cheat);
}

uint32_t GeckoConverter::GetGCAddress(const GKCode& gk)
{
  return gk.pointer == 0 ? m_ba.GetAddressWithOffset(gk.gcaddr, m_cheat) :
                           m_po.GetAddressWithOffset(gk.gcaddr, m_cheat);
}

bool GeckoConverter::IsPointer(const GKCode& gk)
{
  return (gk.pointer == 0 && m_ba.IsPointer()) || (gk.pointer == 1 && m_po.IsPointer());
}

bool GeckoConverter::MemoryWriteOps(const GKCode& gk, const GKVector& lines, uint32_t& i)
{
  switch (gk.subtype)
  {
  case 0:
  {
    LOG_INFO("# 8bits write and fill\n");
    // 8bits write and fill
    uint32_t count = (gk.value & 0xFFFF0000U) >> 16U;
    uint32_t value = gk.value & AR::MASK_BYTE;
    AR::WriteAndFill(GetGCAddress(gk), value, count, AR::DataType::BIT8, IsPointer(gk), m_cheat);
    break;
  }
  case 1:
  {
    LOG_INFO("# 16bits write and fill\n");
    // 16bits write and fill
    uint32_t count = (gk.value & 0xFFFF0000U) >> 16U;
    uint32_t value = gk.value & AR::MASK_SHORT;
    AR::WriteAndFill(GetGCAddress(gk), value, count, AR::DataType::BIT16, IsPointer(gk), m_cheat);
    break;
  }
  case 2:
  {
    LOG_INFO("# 32bits write\n");
    // 32bits write
    AR::WriteMemory(GetGCAddress(gk), gk.value, AR::DataType::BIT32, IsPointer(gk), m_cheat);
    break;
  }
  case 3:
  {
    LOG_INFO("# string write\n");
    // string write
    uint32_t count = gk.value;
    GeckoRegister reg = GetStepRegister(gk);
    bool is_pointer = IsPointer(gk);
    bool first_part = true;
    uint32_t data = lines[++i].address;
    for (uint32_t k = 0; k < count; ++k)
    {
      uint32_t value = (data >> ((24U - (k & 3U)) << 3U)) & AR::MASK_BYTE;
      k == 0 ? reg.Add(gk.gcaddr, m_cheat) : reg.Add(k, m_cheat);
      AR::WriteMemory(reg.GetAddress(), value, AR::DataType::BIT8, is_pointer, m_cheat);
      if ((k & 3U) == 3U)
      {
        if (first_part)
        {
          data = lines[i].data;
        }
        else if (k + 1 < count)
        {
          data = lines[++i].address;
        }
        first_part = !first_part;
      }
    }
    break;
  }
  case 4:
  {
    LOG_INFO("# Slider/Multi Skip (Serial)\n");
    // Slider/Multi Skip (Serial)
    uint32_t value = gk.value;
    uint32_t data = lines[++i].address;
    uint32_t size = (data >> 28U) & 0xFU;
    uint32_t amount = (data >> 16U) & 0xFFFU;
    uint32_t address_increment = data & 0xFFFFU;
    uint32_t value_increment = lines[i].data;
    GeckoRegister reg = GetStepRegister(gk);
    bool is_pointer = IsPointer(gk);
    for (uint32_t k = 0; k < amount; ++k)
    {
      k == 0 ? reg.Add(gk.gcaddr, m_cheat) : reg.Add(address_increment, m_cheat);
      AR::WriteMemory(reg.GetAddress(), value, static_cast<AR::DataType>(size), is_pointer,
                      m_cheat);
      value += value_increment;
    }
    break;
  }
  default:
    return false;
  }
  return true;
}

bool GeckoConverter::ConditionCodeOps(const GKCode& gk)
{
  if (gk.gcaddr & 0x1U && !m_cheat.empty())
  {
    // Endif
    m_cheat.push_back({0, 0x40000000U});
  }

  ARCode ar{};

  switch (gk.subtype)
  {
  case 0:
  case 4:
    LOG_INFO("# 32 or 16 bits(endif, then) If equal\n");
    // 32 or 16 bits(endif, then) If equal
    ar.type = static_cast<uint32_t>(AR::Conditional::IF_EQUAL);
    ar.subtype = static_cast<uint32_t>(AR::Conditional::SKIP_ALL_LINES_UNTIL);
    ar.size = static_cast<uint32_t>(gk.subtype == 0 ? AR::DataType::BIT32 : AR::DataType::BIT16);
    ar.gcaddr = GetGCAddress(gk);
    ar.value = gk.value;
    m_cheat.push_back(ar);
    break;
  case 1:
  case 5:
    LOG_INFO("# 32 or 16 bits(endif, then) If not equal\n");
    // 32 or 16 bits(endif, then) If not equal
    ar.type = static_cast<uint32_t>(AR::Conditional::IF_NOT_EQUAL);
    ar.subtype = static_cast<uint32_t>(AR::Conditional::SKIP_ALL_LINES_UNTIL);
    ar.size = static_cast<uint32_t>(gk.subtype == 1 ? AR::DataType::BIT32 : AR::DataType::BIT16);
    ar.gcaddr = GetGCAddress(gk);
    ar.value = gk.value;
    m_cheat.push_back(ar);
    break;
  case 2:
  case 6:
    LOG_INFO("# 32 or 16 bits (endif, then) If greater than (unsigned)\n");
    // 32 or 16 bits (endif, then) If greater than (unsigned)
    ar.type = static_cast<uint32_t>(AR::Conditional::IF_GREATER_UNSIGNED);
    ar.subtype = static_cast<uint32_t>(AR::Conditional::SKIP_ALL_LINES_UNTIL);
    ar.size = static_cast<uint32_t>(gk.subtype == 2 ? AR::DataType::BIT32 : AR::DataType::BIT16);
    ar.gcaddr = GetGCAddress(gk);
    ar.value = gk.value;
    m_cheat.push_back(ar);
    break;
  case 3:
  case 7:
    LOG_INFO("# 32 or 16 bits (endif, then) If lower than (unsigned)\n");
    // 32 or 16 bits (endif, then) If lower than (unsigned)
    ar.type = static_cast<uint32_t>(AR::Conditional::IF_LESS_UNSIGNED);
    ar.subtype = static_cast<uint32_t>(AR::Conditional::SKIP_ALL_LINES_UNTIL);
    ar.size = static_cast<uint32_t>(gk.subtype == 3 ? AR::DataType::BIT32 : AR::DataType::BIT16);
    ar.gcaddr = GetGCAddress(gk);
    ar.value = gk.value;
    m_cheat.push_back(ar);
    break;
  default:
    return false;
  }
  return true;
}

bool GeckoConverter::RegiesterActionOps(const GKCode& gk)
{
  switch (gk.subtype)
  {
  case 0:
  {
    LOG_INFO("# Load into Base Address\n");
    // Load into Base Address
    if (!LoadRegister(gk))
    {
      return false;
    }
    break;
  }
  case 1:
  {
    LOG_INFO("# Set Base Address to\n");
    // Set Base Address to
    if (!SetRegister(gk))
    {
      return false;
    }
    break;
  }
  case 2:
  {
    LOG_INFO("# Store Base Address at\n");
    // Store Base Address at
    if (!StoreRegister(gk))
    {
      return false;
    }
    break;
  }
  case 3:
    // Put next code's location into the Base Address
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  case 4:
  {
    LOG_INFO("# Load into Pointer Address\n");
    // Load into Pointer Address
    if (!LoadRegister(gk))
    {
      return false;
    }
    break;
  }
  case 5:
  {
    LOG_INFO("# Set Pointer Address to\n");
    // Set Pointer Address to
    if (!SetRegister(gk))
    {
      return false;
    }
    break;
  }
  case 6:
    LOG_INFO("# Store Pointer Address at\n");
    // Store Pointer Address at
    if (!StoreRegister(gk))
    {
      return false;
    }
    break;
  case 7:
    // Put next code's location into the Pointer Address
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  default:
    return false;
  }
  return true;
}

bool GeckoConverter::LoadRegister(const GKCode& gk)
{
  const bool is_ba = gk.subtype < 4;
  const uint32_t type = (gk.cmd & 0xFFF000U) >> 12U;
  GeckoRegister& dst = is_ba ? m_ba : m_po;

  switch (type)
  {
  case 0x000:
    // dst = [XXXXXXXX]
    dst.Load(gk.value);
    break;
  case 0x001:
    // dst = [grN+XXXXXXXX]
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  case 0x010:
    if (gk.pointer == 0)
    {
      if (!m_ba.IsPointer())
      {
        // dst = [ba + XXXXXXXX]
        dst.Load(m_ba.GetAddress() + gk.value);
      }
      else if (is_ba)
      {
        // ba = [ba + XXXXXXXX]
        dst.Add(gk.value, m_cheat);
      }
      else
      {
        // po = [ba + XXXXXXXX]
        dst.Clone(m_ba, m_cheat);
        dst.Add(gk.value, m_cheat);
      }
    }
    else
    {
      if (!m_po.IsPointer())
      {
        // dst = [po + XXXXXXXX]
        dst.Load(m_po.GetAddress() + gk.value);
      }
      else if (is_ba)
      {
        // ba = [po + XXXXXXXX]
        dst.Clone(m_po, m_cheat);
        dst.Add(gk.value, m_cheat);
      }
      else
      {
        // po = [po + XXXXXXXX]
        dst.Add(gk.value, m_cheat);
      }
    }
    break;
  case 0x011:
    // dst = [ba+grN+XXXXXXXX]
    // dst = [po+grN+XXXXXXXX]
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  case 0x100:
  {
    // dst += [XXXXXXXX]
    if (dst.IsPointer())
    {
      LOG_INFO("# %08X[%d] subtype: %d, pointer: %d, unsupport pointer add\n", gk.cmd, gk.type,
               gk.subtype, gk.pointer);
      return false;
    }
    GeckoRegister reg;
    reg.Load(gk.value);
    dst.Add(reg, m_cheat);
    break;
  }
  case 0x101:
    // dst += [grN+XXXXXXXX]
    return false;
  case 0x110:
    if (gk.pointer == 0)
    {
      if (m_ba.IsPointer())
      {
        LOG_INFO("# %08X[%d] subtype: %d, pointer: %d, unsupport pointer add\n", gk.cmd, gk.type,
                 gk.subtype, gk.pointer);
        return false;
      }
      // dst += [ba+XXXXXXXX]
      uint32_t old = dst.GetAddress();
      dst.Load(m_ba.GetAddress() + gk.value);
      dst.Add(old, m_cheat);
    }
    else
    {
      if (m_po.IsPointer())
      {
        LOG_INFO("# %08X[%d] subtype: %d, pointer: %d, unsupport pointer add\n", gk.cmd, gk.type,
                 gk.subtype, gk.pointer);
        return false;
      }
      // dst += [po + XXXXXXXX]
      uint32_t old = dst.GetAddress();
      dst.Load(m_po.GetAddress() + gk.value);
      dst.Add(old, m_cheat);
    }
    break;
  case 0x111:
    // dst += [ba+grN+XXXXXXXX]
    // dst += [po+grN+XXXXXXXX]
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  default:
    return false;
  }

  return true;
}

bool GeckoConverter::SetRegister(const GKCode& gk)
{
  const bool is_ba = gk.subtype < 4;
  const uint32_t type = (gk.cmd & 0xFFF000U) >> 12U;
  GeckoRegister& dst = is_ba ? m_ba : m_po;

  switch (type)
  {
  case 0x000:
    // dst = XXXXXXXX
    dst.Set(gk.value, m_cheat);
    break;
  case 0x001:
    // dst = grN+XXXXXXXX
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  case 0x010:
    if ((gk.pointer == 0 && is_ba) || (gk.pointer == 1 && !is_ba))
    {
      // ba = ba+XXXXXXXX
      // po = po+XXXXXXXX
      dst.Add(gk.value, m_cheat);
    }
    else
    {
      // ba = po+XXXXXXXX
      // po = ba+XXXXXXXX
      dst.Clone(gk.pointer == 0 ? m_ba : m_po, m_cheat);
      dst.Add(gk.value, m_cheat);
    }
    break;
  case 0x011:
    // dst = ba+grN+XXXXXXXX
    // dst = po+grN+XXXXXXXX
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  case 0x100:
    // dst += XXXXXXXX
    dst.Add(gk.value, m_cheat);
    break;
  case 0x101:
    // dst += grN+XXXXXXXX
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  case 0x110:
    if (gk.pointer == 0)
    {
      if (m_ba.IsPointer())
      {
        LOG_INFO("# %08X[%d] subtype: %d, pointer: %d, unsupport pointer add\n", gk.cmd, gk.type,
                 gk.subtype, gk.pointer);
        return false;
      }
      // ba += ba+XXXXXXXX
      // po += ba+XXXXXXXX
      dst.Add(m_ba.GetAddress() + gk.value, m_cheat);
    }
    else
    {
      if (m_po.IsPointer())
      {
        LOG_INFO("# %08X[%d] subtype: %d, pointer: %d, unsupport pointer add\n", gk.cmd, gk.type,
                 gk.subtype, gk.pointer);
        return false;
      }
      // ba += po+XXXXXXXX
      // po += po+XXXXXXXX
      dst.Add(m_po.GetAddress() + gk.value, m_cheat);
    }
    break;
  case 0x111:
    // dst += ba+grN+XXXXXXXX
    // dst += po+grN+XXXXXXXX
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  default:
    return false;
  }

  return true;
}

bool GeckoConverter::StoreRegister(const GKCode& gk)
{
  const bool is_ba = gk.subtype < 4;
  const uint32_t type = (gk.cmd & 0xFFF000U) >> 12U;
  GeckoRegister& dst = is_ba ? m_ba : m_po;

  switch (type)
  {
  case 0x000:
    // [XXXXXXXX] = dst
    AR::CopyMemory(gk.value, dst.GetAddress(), 4, false, m_cheat);
    break;
  case 0x001:
    // [XXXXXXXX+grN] = dst
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  case 0x010:
    if (gk.pointer == 0)
    {
      // [XXXXXXXX+ba] = dst
      GeckoRegister reg = m_ba.Clone(m_cheat);
      reg.Add(gk.value, m_cheat);
      AR::WriteMemory(reg.GetAddress(), dst.GetAddress(), AR::DataType::BIT32, false, m_cheat);
    }
    else
    {
      // [XXXXXXXX+po] = dst
      GeckoRegister reg = m_po.Clone(m_cheat);
      reg.Add(gk.value, m_cheat);
      AR::WriteMemory(reg.GetAddress(), dst.GetAddress(), AR::DataType::BIT32, false, m_cheat);
    }
    break;
  case 0x011:
    // [XXXXXXXX+ba+grN] = dst
    // [XXXXXXXX+po+grN] = dst
    LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
             gk.pointer);
    return false;
  default:
    return false;
  }

  return true;
}

void GeckoConverter::InsertASM(const GKCode& gk, const GKVector& lines, uint32_t& i)
{
  LOG_INFO("# InsertASM\n");
  GeckoRegister reg = GetStepRegister(gk);
  const bool is_pointer = reg.IsPointer();
  const uint32_t k = i + 1;
  bool flag = true;
  while (flag)
  {
    uint32_t value1 = lines[++i].address;
    uint32_t value2 = lines[i].data;
    if (value2 != 0)
    {
      k == i ? reg.Add(gk.gcaddr, m_cheat) : reg.Add(4, m_cheat);
      AR::WriteMemory(reg.GetAddress(), value1, AR::DataType::BIT32, is_pointer, m_cheat);
      reg.Add(4, m_cheat);
      AR::WriteMemory(reg.GetAddress(), value2, AR::DataType::BIT32, is_pointer, m_cheat);
    }
    else
    {
      flag = false;
      if (value1 != 0x60000000U)
      {
        k == i ? reg.Add(gk.gcaddr, m_cheat) : reg.Add(4, m_cheat);
        AR::WriteMemory(reg.GetAddress(), value1, AR::DataType::BIT32, is_pointer, m_cheat);
      }
    }
  }
}

void GeckoConverter::AddressRangeCheck(const GKCode& gk)
{
  GeckoRegister& reg = gk.pointer == 0 ? m_ba : m_po;
  uint32_t lower_bounds = gk.value & 0xFFFF0000U;
  uint32_t upper_bounds = (gk.value & 0xFFFFU) << 16U;

  uint32_t gcaddr = reg.GetAddress();
  if (reg.IsPointer())
  {
    static uint32_t simple_address = 0;
    if (simple_address == 0)
    {
      simple_address = g_spare_address;
      g_spare_address += 4;
    }
    LOG_INFO("# AddressRangeCheck write pointer\n");
    // write pointer to point self
    AR::WriteMemory(simple_address, simple_address, AR::DataType::BIT32, false, m_cheat);
    LOG_INFO("# AddressRangeCheck copy pointer\n");
    // copy memory with pointer support
    AR::CopyMemory(simple_address, gcaddr, 4, false, m_cheat);
    gcaddr = simple_address;
  }

  LOG_INFO("# AddressRangeCheck\n");
  ARCode ar{};
  ar.subtype = static_cast<uint32_t>(AR::Conditional::SKIP_ALL_LINES);
  ar.size = static_cast<uint32_t>(AR::DataType::BIT32);
  ar.gcaddr = gcaddr;

  // check lower bounds
  ar.type = static_cast<uint32_t>(AR::Conditional::IF_GREATER_UNSIGNED);
  ar.value = lower_bounds;
  m_cheat.push_back(ar);

  // check upper bounds
  ar.type = static_cast<uint32_t>(AR::Conditional::IF_LESS_UNSIGNED);
  ar.value = upper_bounds;
  m_cheat.push_back(ar);
}

void GeckoConverter::OnOffSwitch()
{
  LOG_INFO("# OnOffSwitch\n");
  uint32_t gcaddr = g_spare_address;
  g_spare_address += 4;

  // add 0x1
  AR::ValueAdd(gcaddr, 0x1, AR::DataType::BIT8, m_cheat);

  // if and
  ARCode ar{};
  ar.type = static_cast<uint32_t>(AR::Conditional::IF_AND);
  ar.subtype = static_cast<uint32_t>(AR::Conditional::SKIP_ALL_LINES);
  ar.size = static_cast<uint32_t>(AR::DataType::BIT8);
  ar.gcaddr = gcaddr;
  ar.value = 0x1;
  m_cheat.push_back(ar);
}

void GeckoConverter::FullTerminator(const GKCode& gk)
{
  LOG_INFO("# FullTerminator\n");
  uint32_t ba_addr = gk.value & 0xFFFF0000U;
  uint32_t po_addr = (gk.value & 0xFFFFU) << 16U;

  if (ba_addr != 0)
  {
    m_ba.Set(ba_addr, m_cheat);
  }

  if (po_addr != 0)
  {
    m_po.Set(po_addr, m_cheat);
  }
}

bool GeckoConverter::Convert(const GKVector& lines)
{
  m_cheat.clear();
  m_ba.Reset();
  m_po.Reset();

  for (uint32_t i = 0; i < lines.size(); ++i)
  {
    GKCode gk{};
    gk.cmd = lines[i].address;
    gk.value = lines[i].data;

    switch (gk.type)
    {
    case 0:
      if (!MemoryWriteOps(gk, lines, i))
      {
        return false;
      }
      break;
    case 1:
      if (!ConditionCodeOps(gk))
      {
        return false;
      }
      break;
    case 2:
      if (!RegiesterActionOps(gk))
      {
        return false;
      }
      break;
    case 3:
      // Set Repeat
      // Execute Repeat
      // Return
      // Goto
      // Gosub
      LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
               gk.pointer);
      return false;
    case 4:
      // Set Gecko Register to
      // Load into Gecko Register
      // Store Gecko Register at
      // Gecko Register / Direct Value Operations
      // Gecko Register Operations
      // Memory Copy 1
      // Memory Copy 2
      LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
               gk.pointer);
      return false;
    case 5:
      // 16 bits (endif, then) If equal (Gecko Register)
      // 16 bits (endif, then) If not equal
      // 16 bits (endif, then) If greater
      // 16 bits (endif, then) If lower
      // 16 bits (endif, then) If counter value equal (Counter If Codes)
      // 16 bits (endif, then) If counter value not equal
      // 16 bits (endif, then) If counter value greater
      // 16 bits (endif, then) If counter value lower
      LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
               gk.pointer);
      return false;
    case 6:
      if (gk.subtype == 1)
      {
        // Insert ASM
        InsertASM(gk, lines, i);
      }
      else if (gk.subtype == 6)
      {
        // On/Off Switch
        OnOffSwitch();
      }
      else if (gk.subtype == 7)
      {
        // Address Range Check
        AddressRangeCheck(gk);
      }
      else
      {
        LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
                 gk.pointer);
        return false;
      }
      break;
    case 7:
      if (gk.subtype == 0)
      {
        if (gk.pointer == 0)
        {
          // Full Terminator
          FullTerminator(gk);
        }
        else
        {
          // End of Codes
          m_cheat.push_back({});
        }
      }
      else if (gk.subtype == 1 && gk.pointer == 0)
      {
        // Endif (+else)
        FullTerminator(gk);
      }
      else
      {
        LOG_INFO("# %08X[%d] unsupport subtype: %d, pointer: %d\n", gk.cmd, gk.type, gk.subtype,
                 gk.pointer);
        return false;
      }
      break;
    default:
      return false;
    }
  }

  return true;
}

namespace Gecko2AR
{
void SetActiveCodes(const std::vector<Gecko::GeckoCode>& gkcodes)
{
  GeckoConverter gecko2ar;
  std::vector<ActionReplay::ARCode> arcodes;
  std::string filename = StringFromFormat("%s%s.txt", File::GetUserPath(D_DUMP_IDX).c_str(),
                                          SConfig::GetInstance().GetGameID().c_str());

  std::ofstream stream;
  File::OpenFStream(stream, filename, std::ios_base::out);

  for (const auto& gk : gkcodes)
  {
    if (!gk.enabled)
    {
      continue;
    }

    ActionReplay::ARCode ar;
    ar.name = gk.name;
    ar.user_defined = true;
    stream << "$" << ar.name << std::endl;
    LOG_INFO("# Name: %s\n", ar.name.c_str());
    g_spare_address = 0x80003200U;
    if (gecko2ar.Convert(gk.codes))
    {
      ar.active = true;
      gecko2ar.GetAREntrys(ar.ops);

      for (const auto& o : ar.ops)
      {
        stream << StringFromFormat("%08X %08X", o.cmd_addr, o.value) << std::endl;
      }
      ActionReplay::AddCode(ar);
    }
    else
    {
      stream << "# error" << std::endl;
    }
    stream << std::endl;
  }
}

}  // namespace Gecko2AR
