#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "../Common/CommonTypes.h"
#include "../Common/MemoryCommon.h"
#include "../../Common/Swap.h"

class MemScanner
{
public:
  enum class ScanFiter
  {
    exact = 0,
    increasedBy,
    decreasedBy,
    between,
    biggerThan,
    smallerThan,
    increased,
    decreased,
    changed,
    unchanged,
    unknownInitial
  };

  enum class CompareResult
  {
    bigger,
    smaller,
    equal,
    nan
  };

  MemScanner();
  ~MemScanner();
  Common::MemOperationReturnCode firstScan(const ScanFiter filter, const std::string& searchTerm1,
                                           const std::string& searchTerm2);
  Common::MemOperationReturnCode nextScan(const ScanFiter filter, const std::string& searchTerm1,
                                          const std::string& searchTerm2);
  void reset();
  inline CompareResult compareMemoryAsNumbers(const char* first, const char* second,
                                              const char* offset, bool offsetInvert,
                                              bool bswapSecond, size_t length) const;

  template <typename T>
  inline T convertMemoryToType(const char* memory, bool invert) const
  {
    T theType;
    std::memcpy(&theType, memory, sizeof(T));
    if (invert)
      theType *= -1;
    return theType;
  }

  template <typename T>
  inline CompareResult compareMemoryAsNumbersWithType(const char* first, const char* second,
                                                      const char* offset, bool offsetInvert,
                                                      bool bswapSecond) const
  {
    T firstByte;
    T secondByte;
    std::memcpy(&firstByte, first, sizeof(T));
    std::memcpy(&secondByte, second, sizeof(T));
    size_t size = sizeof(T);
    switch (size)
    {
    case 2:
    {
      u16 firstHalfword = 0;
      std::memcpy(&firstHalfword, &firstByte, sizeof(u16));
      firstHalfword = Common::swap16(firstHalfword);
      std::memcpy(&firstByte, &firstHalfword, sizeof(u16));
      if (bswapSecond)
      {
        std::memcpy(&firstHalfword, &secondByte, sizeof(u16));
        firstHalfword = Common::swap16(firstHalfword);
        std::memcpy(&secondByte, &firstHalfword, sizeof(u16));
      }
      break;
    }
    case 4:
    {
      u32 firstWord = 0;
      std::memcpy(&firstWord, &firstByte, sizeof(u32));
      firstWord = Common::swap32(firstWord);
      std::memcpy(&firstByte, &firstWord, sizeof(u32));
      if (bswapSecond)
      {
        std::memcpy(&firstWord, &secondByte, sizeof(u32));
        firstWord = Common::swap32(firstWord);
        std::memcpy(&secondByte, &firstWord, sizeof(u32));
      }
      break;
    }
    case 8:
    {
      u64 firstDoubleword = 0;
      std::memcpy(&firstDoubleword, &firstByte, sizeof(u64));
      firstDoubleword = Common::swap64(firstDoubleword);
      std::memcpy(&firstByte, &firstDoubleword, sizeof(u64));
      if (bswapSecond)
      {
        std::memcpy(&firstDoubleword, &secondByte, sizeof(u64));
        firstDoubleword = Common::swap64(firstDoubleword);
        std::memcpy(&secondByte, &firstDoubleword, sizeof(u64));
      }
      break;
    }
    }

    if (firstByte != firstByte)
      return CompareResult::nan;

    if (firstByte < (secondByte + convertMemoryToType<T>(offset, offsetInvert)))
      return CompareResult::smaller;
    else if (firstByte > (secondByte + convertMemoryToType<T>(offset, offsetInvert)))
      return CompareResult::bigger;
    else
      return CompareResult::equal;
  }

  void setType(const Common::MemType type);
  void setBase(const Common::MemBase base);
  void setEnforceMemAlignement(const bool enforceAlignement);
  void setIsSigned(const bool isSigned);

  std::vector<u32> getResultsConsoleAddr() const;
  size_t getResultCount() const;
  int getTermsNumForFilter(const ScanFiter filter) const;
  Common::MemType getType() const;
  Common::MemBase getBase() const;
  size_t getLength() const;
  bool getIsUnsigned() const;
  std::string getFormattedScannedValueAt(const int index) const;
  std::string getFormattedCurrentValueAt(int index) const;
  void removeResultAt(int index);
  bool typeSupportsAdditionalOptions(const Common::MemType type) const;
  bool hasScanStarted() const;

private:
  inline bool isHitNextScan(const ScanFiter filter, const char* memoryToCompare1,
                            const char* memoryToCompare2, const char* noOffset,
                            const char* newerRAMCache, const size_t realSize,
                            const u32 consoleOffset) const;
  std::string addSpacesToBytesArrays(const std::string& bytesArray) const;

  Common::MemType m_memType = Common::MemType::type_byte;
  Common::MemBase m_memBase = Common::MemBase::base_decimal;
  size_t m_memSize;
  bool m_enforceMemAlignement = true;
  bool m_memIsSigned = false;
  std::vector<u32> m_resultsConsoleAddr;
  bool m_wasUnknownInitialValue = false;
  size_t m_resultCount = 0;
  char* m_scanRAMCache = nullptr;
  bool m_scanStarted = false;
};
