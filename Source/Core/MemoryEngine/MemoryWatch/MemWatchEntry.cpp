#include "MemWatchEntry.h"

#include <bitset>
#include <cstring>
#include <iomanip>
#include <ios>
#include <limits>
#include <sstream>

#include "../Common/MemoryCommon.h"

MemWatchEntry::MemWatchEntry(const QString label, const u32 consoleAddress,
                             const Common::MemType type, const Common::MemBase base,
                             const bool isUnsigned, const size_t length,
                             const bool isBoundToPointer)
    : m_label(label), m_consoleAddress(consoleAddress), m_type(type), m_base(base),
      m_isUnsigned(isUnsigned), m_boundToPointer(isBoundToPointer), m_length(length)
{
  m_memory = new char[getSizeForType(m_type, m_length)];
}

MemWatchEntry::MemWatchEntry()
{
  m_type = Common::MemType::type_byte;
  m_base = Common::MemBase::base_decimal;
  m_memory = new char[sizeof(u8)];
  *m_memory = 0;
  m_isUnsigned = false;
  m_consoleAddress = 0x80000000;
}

MemWatchEntry::MemWatchEntry(MemWatchEntry* entry)
    : m_label(entry->m_label), m_consoleAddress(entry->m_consoleAddress), m_type(entry->m_type),
      m_base(entry->m_base), m_isUnsigned(entry->m_isUnsigned),
      m_boundToPointer(entry->m_boundToPointer), m_pointerOffsets(entry->m_pointerOffsets),
      m_isValidPointer(entry->m_isValidPointer), m_length(entry->m_length)
{
  m_memory = new char[getSizeForType(entry->getType(), entry->getLength())];
  std::memcpy(m_memory, entry->getMemory(), getSizeForType(entry->getType(), entry->getLength()));
}

MemWatchEntry::~MemWatchEntry()
{
  delete[] m_memory;
  delete[] m_freezeMemory;
}

QString MemWatchEntry::getLabel() const
{
  return m_label;
}

size_t MemWatchEntry::getLength() const
{
  return m_length;
}

Common::MemType MemWatchEntry::getType() const
{
  return m_type;
}

u32 MemWatchEntry::getConsoleAddress() const
{
  return m_consoleAddress;
}

bool MemWatchEntry::isLocked() const
{
  return m_lock;
}

bool MemWatchEntry::isBoundToPointer() const
{
  return m_boundToPointer;
}

Common::MemBase MemWatchEntry::getBase() const
{
  return m_base;
}

int MemWatchEntry::getPointerOffset(const int index) const
{
  return m_pointerOffsets.at(index);
}

std::vector<int> MemWatchEntry::getPointerOffsets() const
{
  return m_pointerOffsets;
}

size_t MemWatchEntry::getPointerLevel() const
{
  return m_pointerOffsets.size();
}

char* MemWatchEntry::getMemory() const
{
  return m_memory;
}

bool MemWatchEntry::isUnsigned() const
{
  return m_isUnsigned;
}

void MemWatchEntry::setLabel(const QString& label)
{
  m_label = label;
}

void MemWatchEntry::setConsoleAddress(const u32 address)
{
  m_consoleAddress = address;
}

void MemWatchEntry::setTypeAndLength(const Common::MemType type, const size_t length)
{
  size_t oldSize = getSizeForType(m_type, m_length);
  m_type = type;
  m_length = length;
  size_t newSize = getSizeForType(m_type, m_length);
  if (oldSize != newSize)
  {
    delete[] m_memory;
    m_memory = new char[newSize];
    std::fill(m_memory, m_memory + newSize, 0);
  }
}

void MemWatchEntry::setBase(const Common::MemBase base)
{
  m_base = base;
}

void MemWatchEntry::setLock(const bool doLock)
{
  m_lock = doLock;
  if (m_lock)
  {
    if (readMemoryFromRAM() == Common::MemOperationReturnCode::OK)
    {
      m_freezeMemSize = getSizeForType(m_type, m_length);
      m_freezeMemory = new char[m_freezeMemSize];
      std::memcpy(m_freezeMemory, m_memory, m_freezeMemSize);
    }
  }
  else if (m_freezeMemory != nullptr)
  {
    m_freezeMemSize = 0;
    delete[] m_freezeMemory;
    m_freezeMemory = nullptr;
  }
}

void MemWatchEntry::setSignedUnsigned(const bool isUnsigned)
{
  m_isUnsigned = isUnsigned;
}

void MemWatchEntry::setBoundToPointer(const bool boundToPointer)
{
  m_boundToPointer = boundToPointer;
}

void MemWatchEntry::setPointerOffset(const int pointerOffset, int index)
{
  m_pointerOffsets[index] = pointerOffset;
}

void MemWatchEntry::removeOffset()
{
  m_pointerOffsets.pop_back();
}

void MemWatchEntry::addOffset(const int offset)
{
  m_pointerOffsets.push_back(offset);
}

Common::MemOperationReturnCode MemWatchEntry::freeze()
{
  Common::MemOperationReturnCode writeCode = writeMemoryToRAM(m_freezeMemory, m_freezeMemSize);
  return writeCode;
}

u32 MemWatchEntry::getAddressForPointerLevel(const int level)
{
  if (!m_boundToPointer && level > m_pointerOffsets.size() && level > 0)
    return 0;

  u32 address = m_consoleAddress;
  char addressBuffer[sizeof(u32)] = {0};
  for (int i = 0; i < level; ++i)
  {
    Common::readFromRAM(addressBuffer, address, sizeof(u32), true);
    std::memcpy(&address, addressBuffer, sizeof(u32));
    if (Common::isValidAddress(address))
      address += m_pointerOffsets.at(i);
    else
      return 0;
  }
  return address;
}

std::string MemWatchEntry::getAddressStringForPointerLevel(const int level)
{
  u32 address = getAddressForPointerLevel(level);
  if (address == 0)
  {
    return "???";
  }
  else
  {
    std::stringstream ss;
    ss << std::hex << std::uppercase << address;
    return ss.str();
  }
}

Common::MemOperationReturnCode MemWatchEntry::readMemoryFromRAM()
{
  u32 realConsoleAddress = m_consoleAddress;
  if (m_boundToPointer)
  {
    char realConsoleAddressBuffer[sizeof(u32)] = {0};
    for (int offset : m_pointerOffsets)
    {
      Common::readFromRAM(realConsoleAddressBuffer, realConsoleAddress, sizeof(u32), true);
      std::memcpy(&realConsoleAddress, realConsoleAddressBuffer, sizeof(u32));
      if (Common::isValidAddress(realConsoleAddress))
        realConsoleAddress += offset;
      else
      {
        m_isValidPointer = false;
        return Common::MemOperationReturnCode::invalidPointer;
      }
    }
    // Resolve sucessful
    m_isValidPointer = true;
  }

  Common::readFromRAM(m_memory, realConsoleAddress, getSizeForType(m_type, m_length),
                      shouldBeBSwappedForType(m_type));
  return Common::MemOperationReturnCode::OK;
}

Common::MemOperationReturnCode MemWatchEntry::writeMemoryToRAM(const char* memory,
                                                               const size_t size)
{
  u32 realConsoleAddress = m_consoleAddress;
  if (m_boundToPointer)
  {
    char realConsoleAddressBuffer[sizeof(u32)] = {0};
    for (int offset : m_pointerOffsets)
    {
      Common::readFromRAM(realConsoleAddressBuffer, realConsoleAddress, sizeof(u32), true);
      std::memcpy(&realConsoleAddress, realConsoleAddressBuffer, sizeof(u32));
      if (Common::isValidAddress(realConsoleAddress))
        realConsoleAddress += offset;
      else
      {
        m_isValidPointer = false;
        return Common::MemOperationReturnCode::invalidPointer;
      }
    }
    // Resolve sucessful
    m_isValidPointer = true;
  }

  Common::writeToRAM((void*)memory, realConsoleAddress, size, shouldBeBSwappedForType(m_type));
  return Common::MemOperationReturnCode::OK;
}

std::string MemWatchEntry::getStringFromMemory() const
{
  if (m_boundToPointer && !m_isValidPointer)
    return "???";
  return Common::formatMemoryToString(m_memory, m_type, m_length, m_base, m_isUnsigned);
}

Common::MemOperationReturnCode MemWatchEntry::writeMemoryFromString(const std::string& inputString)
{
  Common::MemOperationReturnCode writeReturn = Common::MemOperationReturnCode::OK;
  size_t sizeToWrite = 0;
  char* buffer =
      Common::formatStringToMemory(writeReturn, sizeToWrite, inputString, m_base, m_type, m_length);
  if (writeReturn != Common::MemOperationReturnCode::OK)
    return writeReturn;

  writeReturn = writeMemoryToRAM(buffer, sizeToWrite);
  if (writeReturn == Common::MemOperationReturnCode::OK)
  {
    if (m_lock)
      std::memcpy(m_freezeMemory, buffer, m_freezeMemSize);
    delete[] buffer;
    return writeReturn;
  }
  else
  {
    delete[] buffer;
    return writeReturn;
  }
}
