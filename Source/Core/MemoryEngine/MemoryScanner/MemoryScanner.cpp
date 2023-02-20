#include "MemoryScanner.h"

#include "../Common/MemoryCommon.h"

MemScanner::MemScanner() : m_resultsConsoleAddr(std::vector<u32>())
{
}

MemScanner::~MemScanner()
{
  delete[] m_scanRAMCache;
}

Common::MemOperationReturnCode MemScanner::firstScan(const MemScanner::ScanFiter filter,
                                                     const std::string& searchTerm1,
                                                     const std::string& searchTerm2)
{
  m_scanRAMCache = nullptr;
  u32 ramSize = Common::totalRAMSize();
  if (ramSize == 0)
    return Common::MemOperationReturnCode::uninitialized;
  m_scanRAMCache = new char[ramSize];
  Common::readAllRAM(m_scanRAMCache);

  if (filter == ScanFiter::unknownInitial)
  {
    int alignementDivision =
        m_enforceMemAlignement ? Common::getNbrBytesAlignementForType(m_memType) : 1;
    m_resultCount = ((ramSize / alignementDivision) -
                     Common::getSizeForType(m_memType, static_cast<size_t>(1)));
    m_wasUnknownInitialValue = true;
    m_memSize = 1;
    m_scanStarted = true;
    return Common::MemOperationReturnCode::OK;
  }

  Common::MemOperationReturnCode scanReturn = Common::MemOperationReturnCode::OK;
  size_t termActualLength = 0;
  size_t termMaxLength = 0;
  if (m_memType == Common::MemType::type_string)
    // This is just to have the string formatted with the appropriate length, byte arrays don't need
    // this because they get copied byte per byte
    termMaxLength = searchTerm1.length();
  else
    // Have no restriction on the length for the rest
    termMaxLength = ramSize;

  std::string formattedSearchTerm1;
  if (m_memType == Common::MemType::type_byteArray)
    formattedSearchTerm1 = addSpacesToBytesArrays(searchTerm1);
  else
    formattedSearchTerm1 = searchTerm1;

  char* memoryToCompare1 = Common::formatStringToMemory(
      scanReturn, termActualLength, formattedSearchTerm1, m_memBase, m_memType, termMaxLength);
  if (scanReturn != Common::MemOperationReturnCode::OK)
  {
    delete[] memoryToCompare1;
    delete[] m_scanRAMCache;
    m_scanRAMCache = nullptr;
    return scanReturn;
  }

  char* memoryToCompare2 = nullptr;
  if (filter == ScanFiter::between)
  {
    memoryToCompare2 = Common::formatStringToMemory(scanReturn, termActualLength, searchTerm2,
                                                    m_memBase, m_memType, ramSize);
    if (scanReturn != Common::MemOperationReturnCode::OK)
    {
      delete[] memoryToCompare1;
      delete[] memoryToCompare2;
      delete[] m_scanRAMCache;
      m_scanRAMCache = nullptr;
      return scanReturn;
    }
  }

  m_memSize = Common::getSizeForType(m_memType, termActualLength);

  char* noOffset = new char[m_memSize];
  std::memset(noOffset, 0, m_memSize);

  int increment = m_enforceMemAlignement ? Common::getNbrBytesAlignementForType(m_memType) : 1;
  for (u32 i = 0; i < (ramSize - m_memSize); i += increment)
  {
    char* memoryCandidate = &m_scanRAMCache[i];
    bool isResult = false;
    switch (filter)
    {
    case ScanFiter::exact:
    {
      if (m_memType == Common::MemType::type_string || m_memType == Common::MemType::type_byteArray)
        isResult = (std::memcmp(memoryCandidate, memoryToCompare1, m_memSize) == 0);
      else
        isResult = (compareMemoryAsNumbers(memoryCandidate, memoryToCompare1, noOffset, false,
                                           false, m_memSize) == MemScanner::CompareResult::equal);
      break;
    }
    case ScanFiter::between:
    {
      MemScanner::CompareResult result1 = compareMemoryAsNumbers(memoryCandidate, memoryToCompare1,
                                                                 noOffset, false, false, m_memSize);
      MemScanner::CompareResult result2 = compareMemoryAsNumbers(memoryCandidate, memoryToCompare2,
                                                                 noOffset, false, false, m_memSize);
      isResult = ((result1 == MemScanner::CompareResult::bigger ||
                   result1 == MemScanner::CompareResult::equal) &&
                  (result2 == MemScanner::CompareResult::smaller ||
                   result2 == MemScanner::CompareResult::equal));
      break;
    }
    case ScanFiter::biggerThan:
    {
      isResult = (compareMemoryAsNumbers(memoryCandidate, memoryToCompare1, noOffset, false, false,
                                         m_memSize) == MemScanner::CompareResult::bigger);
      break;
    }
    case ScanFiter::smallerThan:
    {
      isResult = (compareMemoryAsNumbers(memoryCandidate, memoryToCompare1, noOffset, false, false,
                                         m_memSize) == MemScanner::CompareResult::smaller);
      break;
    }
    default:;
    }

    if (isResult)
      m_resultsConsoleAddr.push_back(Common::offsetToAddr(i));
  }
  delete[] noOffset;
  delete[] memoryToCompare1;
  delete[] memoryToCompare2;
  m_resultCount = m_resultsConsoleAddr.size();
  m_scanStarted = true;
  return Common::MemOperationReturnCode::OK;
}

Common::MemOperationReturnCode MemScanner::nextScan(const MemScanner::ScanFiter filter,
                                                    const std::string& searchTerm1,
                                                    const std::string& searchTerm2)
{
  char* newerRAMCache = nullptr;
  u32 ramSize = Common::totalRAMSize();
  if (ramSize == 0)
    return Common::MemOperationReturnCode::uninitialized;
  newerRAMCache = new char[ramSize];
  Common::readAllRAM(newerRAMCache);

  Common::MemOperationReturnCode scanReturn = Common::MemOperationReturnCode::OK;
  size_t termActualLength = 0;
  size_t termMaxLength = 0;
  if (m_memType == Common::MemType::type_string)
    // This is just to have the string formatted with the appropriate length, byte arrays don't need
    // this because they get copied byte per byte
    termMaxLength = searchTerm1.length();
  else
    // Have no restriction on the length for the rest
    termMaxLength = ramSize;

  char* memoryToCompare1 = nullptr;
  if (filter != ScanFiter::increased && filter != ScanFiter::decreased &&
      filter != ScanFiter::changed && filter != ScanFiter::unchanged)
  {
    std::string formattedSearchTerm1;
    if (m_memType == Common::MemType::type_byteArray)
      formattedSearchTerm1 = addSpacesToBytesArrays(searchTerm1);
    else
      formattedSearchTerm1 = searchTerm1;

    memoryToCompare1 = Common::formatStringToMemory(
        scanReturn, termActualLength, formattedSearchTerm1, m_memBase, m_memType, termMaxLength);
    if (scanReturn != Common::MemOperationReturnCode::OK)
      return scanReturn;
  }

  char* memoryToCompare2 = nullptr;
  if (filter == ScanFiter::between)
  {
    memoryToCompare2 = Common::formatStringToMemory(scanReturn, termActualLength, searchTerm2,
                                                    m_memBase, m_memType, ramSize);
    if (scanReturn != Common::MemOperationReturnCode::OK)
      return scanReturn;
  }

  m_memSize = Common::getSizeForType(m_memType, termActualLength);

  char* noOffset = new char[m_memSize];
  std::memset(noOffset, 0, m_memSize);

  std::vector<u32> newerResults = std::vector<u32>();
  bool aramAccessible = Common::isARAMAccessible();

  if (m_wasUnknownInitialValue)
  {
    m_wasUnknownInitialValue = false;

    int increment = m_enforceMemAlignement ? Common::getNbrBytesAlignementForType(m_memType) : 1;
    for (u32 i = 0; i < (ramSize - m_memSize); i += increment)
    {
      if (isHitNextScan(filter, memoryToCompare1, memoryToCompare2, noOffset, newerRAMCache,
                        m_memSize, i))
        newerResults.push_back(Common::offsetToAddr(i));
    }
  }
  else
  {
    for (auto i : m_resultsConsoleAddr)
    {
      u32 offset = Common::addrToOffset(i);
      if (isHitNextScan(filter, memoryToCompare1, memoryToCompare2, noOffset, newerRAMCache,
                        m_memSize, offset))
      {
        newerResults.push_back(i);
      }
    }
  }

  delete[] noOffset;
  m_resultsConsoleAddr.clear();
  std::swap(m_resultsConsoleAddr, newerResults);
  delete[] m_scanRAMCache;
  m_scanRAMCache = nullptr;
  m_scanRAMCache = newerRAMCache;
  m_resultCount = m_resultsConsoleAddr.size();
  return Common::MemOperationReturnCode::OK;
}

void MemScanner::reset()
{
  m_resultsConsoleAddr.clear();
  m_wasUnknownInitialValue = false;
  delete[] m_scanRAMCache;
  m_scanRAMCache = nullptr;
  m_resultCount = 0;
  m_scanStarted = false;
}

inline bool MemScanner::isHitNextScan(const MemScanner::ScanFiter filter,
                                      const char* memoryToCompare1, const char* memoryToCompare2,
                                      const char* noOffset, const char* newerRAMCache,
                                      const size_t realSize, const u32 consoleOffset) const
{
  char* olderMemory = &m_scanRAMCache[consoleOffset];
  const char* newerMemory = &newerRAMCache[consoleOffset];

  switch (filter)
  {
  case ScanFiter::exact:
  {
    if (m_memType == Common::MemType::type_string || m_memType == Common::MemType::type_byteArray)
      return (std::memcmp(newerMemory, memoryToCompare1, realSize) == 0);
    else
      return (compareMemoryAsNumbers(newerMemory, memoryToCompare1, noOffset, false, false,
                                     realSize) == MemScanner::CompareResult::equal);
  }
  case ScanFiter::between:
  {
    MemScanner::CompareResult result1 =
        compareMemoryAsNumbers(newerMemory, memoryToCompare1, noOffset, false, false, realSize);
    MemScanner::CompareResult result2 =
        compareMemoryAsNumbers(newerMemory, memoryToCompare2, noOffset, false, false, realSize);
    return ((result1 == MemScanner::CompareResult::bigger ||
             result1 == MemScanner::CompareResult::equal) &&
            (result2 == MemScanner::CompareResult::smaller ||
             result2 == MemScanner::CompareResult::equal));
  }
  case ScanFiter::biggerThan:
  {
    return (compareMemoryAsNumbers(newerMemory, memoryToCompare1, noOffset, false, false,
                                   realSize) == MemScanner::CompareResult::bigger);
  }
  case ScanFiter::smallerThan:
  {
    return (compareMemoryAsNumbers(newerMemory, memoryToCompare1, noOffset, false, false,
                                   realSize) == MemScanner::CompareResult::smaller);
  }
  case ScanFiter::increasedBy:
  {
    return (compareMemoryAsNumbers(newerMemory, olderMemory, memoryToCompare1, false, true,
                                   realSize) == MemScanner::CompareResult::equal);
  }
  case ScanFiter::decreasedBy:
  {
    return (compareMemoryAsNumbers(newerMemory, olderMemory, memoryToCompare1, true, true,
                                   realSize) == MemScanner::CompareResult::equal);
  }
  case ScanFiter::increased:
  {
    return (compareMemoryAsNumbers(newerMemory, olderMemory, noOffset, false, true, realSize) ==
            MemScanner::CompareResult::bigger);
  }
  case ScanFiter::decreased:
  {
    return (compareMemoryAsNumbers(newerMemory, olderMemory, noOffset, false, true, realSize) ==
            MemScanner::CompareResult::smaller);
  }
  case ScanFiter::changed:
  {
    MemScanner::CompareResult result =
        compareMemoryAsNumbers(newerMemory, olderMemory, noOffset, false, true, realSize);
    return (result == MemScanner::CompareResult::bigger ||
            result == MemScanner::CompareResult::smaller);
  }
  case ScanFiter::unchanged:
  {
    return (compareMemoryAsNumbers(newerMemory, olderMemory, noOffset, false, true, realSize) ==
            MemScanner::CompareResult::equal);
  }
  default:
  {
    return false;
  }
  }
}

inline MemScanner::CompareResult
MemScanner::compareMemoryAsNumbers(const char* first, const char* second, const char* offset,
                                   bool offsetInvert, bool bswapSecond, size_t length) const
{
  switch (m_memType)
  {
  case Common::MemType::type_byte:
    if (m_memIsSigned)
      return compareMemoryAsNumbersWithType<s8>(first, second, offset, offsetInvert, bswapSecond);
    return compareMemoryAsNumbersWithType<u8>(first, second, offset, offsetInvert, bswapSecond);
  case Common::MemType::type_halfword:
    if (m_memIsSigned)
      return compareMemoryAsNumbersWithType<s16>(first, second, offset, offsetInvert, bswapSecond);
    return compareMemoryAsNumbersWithType<u16>(first, second, offset, offsetInvert, bswapSecond);
  case Common::MemType::type_word:
    if (m_memIsSigned)
      return compareMemoryAsNumbersWithType<s32>(first, second, offset, offsetInvert, bswapSecond);
    return compareMemoryAsNumbersWithType<u32>(first, second, offset, offsetInvert, bswapSecond);
  case Common::MemType::type_float:
    return compareMemoryAsNumbersWithType<float>(first, second, offset, offsetInvert, bswapSecond);
  case Common::MemType::type_double:
    return compareMemoryAsNumbersWithType<double>(first, second, offset, offsetInvert, bswapSecond);
  default:
    return MemScanner::CompareResult::nan;
  }
}

void MemScanner::setType(const Common::MemType type)
{
  m_memType = type;
}

void MemScanner::setBase(const Common::MemBase base)
{
  m_memBase = base;
}

void MemScanner::setEnforceMemAlignement(const bool enforceAlignement)
{
  m_enforceMemAlignement = enforceAlignement;
}

void MemScanner::setIsSigned(const bool isSigned)
{
  m_memIsSigned = isSigned;
}

int MemScanner::getTermsNumForFilter(const MemScanner::ScanFiter filter) const
{
  if (filter == MemScanner::ScanFiter::between)
    return 2;
  else if (filter == MemScanner::ScanFiter::exact || filter == MemScanner::ScanFiter::increasedBy ||
           filter == MemScanner::ScanFiter::decreasedBy ||
           filter == MemScanner::ScanFiter::biggerThan ||
           filter == MemScanner::ScanFiter::smallerThan)
    return 1;
  return 0;
}

bool MemScanner::typeSupportsAdditionalOptions(const Common::MemType type) const
{
  return (type == Common::MemType::type_byte || type == Common::MemType::type_halfword ||
          type == Common::MemType::type_word);
}

std::vector<u32> MemScanner::getResultsConsoleAddr() const
{
  return m_resultsConsoleAddr;
}

std::string MemScanner::getFormattedScannedValueAt(const int index) const
{
  bool aramAccessible = Common::isARAMAccessible();
  u32 offset = Common::addrToOffset(m_resultsConsoleAddr.at(index));
  return Common::formatMemoryToString(&m_scanRAMCache[offset], m_memType, m_memSize, m_memBase,
                                      !m_memIsSigned, Common::shouldBeBSwappedForType(m_memType));
}

std::string MemScanner::getFormattedCurrentValueAt(const int index) const
{
  if (Common::isValidAddress(m_resultsConsoleAddr.at(index)))
  {
    u32 size = Common::getSizeForType(m_memType, m_memSize);
    char* cache = new char[size];
    Common::readFromRAM(cache, m_resultsConsoleAddr.at(index), size);
    std::string s =
        Common::formatMemoryToString(cache, m_memType, m_memSize, m_memBase, !m_memIsSigned,
                                     Common::shouldBeBSwappedForType(m_memType));
    delete[] cache;
    return s;
  }
  return "";
}

void MemScanner::removeResultAt(int index)
{
  m_resultsConsoleAddr.erase(m_resultsConsoleAddr.begin() + index);
  m_resultCount--;
}

std::string MemScanner::addSpacesToBytesArrays(const std::string& bytesArray) const
{
  std::string result(bytesArray);
  int spacesAdded = 0;
  for (int i = 2; i < bytesArray.length(); i += 2)
  {
    if (bytesArray[i] != ' ')
    {
      result.insert(i + spacesAdded, 1, ' ');
      spacesAdded++;
    }
    else
    {
      i++;
    }
  }
  return result;
}

size_t MemScanner::getResultCount() const
{
  return m_resultCount;
}

bool MemScanner::hasScanStarted() const
{
  return m_scanStarted;
}

Common::MemType MemScanner::getType() const
{
  return m_memType;
}

Common::MemBase MemScanner::getBase() const
{
  return m_memBase;
}

size_t MemScanner::getLength() const
{
  return m_memSize;
}

bool MemScanner::getIsUnsigned() const
{
  return !m_memIsSigned;
}
