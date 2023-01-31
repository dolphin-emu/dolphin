#pragma once

#include <string>
#include <vector>

#include <QString>

#include "../Common/CommonTypes.h"
#include "../Common/MemoryCommon.h"

class MemWatchEntry
{
public:
  MemWatchEntry();
  MemWatchEntry(const QString label, const u32 consoleAddress, const Common::MemType type,
                const Common::MemBase = Common::MemBase::base_decimal,
                const bool m_isUnsigned = false, const size_t length = 1,
                const bool isBoundToPointer = false);
  MemWatchEntry(MemWatchEntry* entry);
  ~MemWatchEntry();

  QString getLabel() const;
  Common::MemType getType() const;
  u32 getConsoleAddress() const;
  bool isLocked() const;
  bool isBoundToPointer() const;
  Common::MemBase getBase() const;
  size_t getLength() const;
  char* getMemory() const;
  bool isUnsigned() const;
  int getPointerOffset(const int index) const;
  std::vector<int> getPointerOffsets() const;
  size_t getPointerLevel() const;
  void setLabel(const QString& label);
  void setConsoleAddress(const u32 address);
  void setTypeAndLength(const Common::MemType type, const size_t length = 1);
  void setBase(const Common::MemBase base);
  void setLock(const bool doLock);
  void setSignedUnsigned(const bool isUnsigned);
  void setBoundToPointer(const bool boundToPointer);
  void setPointerOffset(const int pointerOffset, const int index);
  void addOffset(const int offset);
  void removeOffset();

  Common::MemOperationReturnCode freeze();

  u32 getAddressForPointerLevel(const int level);
  std::string getAddressStringForPointerLevel(const int level);
  Common::MemOperationReturnCode readMemoryFromRAM();

  std::string getStringFromMemory() const;
  Common::MemOperationReturnCode writeMemoryFromString(const std::string& inputString);

private:
  Common::MemOperationReturnCode writeMemoryToRAM(const char* memory, const size_t size);

  QString m_label;
  u32 m_consoleAddress;
  bool m_lock = false;
  Common::MemType m_type;
  Common::MemBase m_base;
  bool m_isUnsigned;
  bool m_boundToPointer = false;
  std::vector<int> m_pointerOffsets;
  bool m_isValidPointer = false;
  char* m_memory;
  char* m_freezeMemory = nullptr;
  size_t m_freezeMemSize = 0;
  size_t m_length = 1;
};
