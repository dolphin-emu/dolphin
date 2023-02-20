#include "MemoryCommon.h"

#include <bitset>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

#include "Common/Swap.h"
#include "CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace Common
{
size_t getSizeForType(const MemType type, const size_t length)
{
  switch (type)
  {
  case MemType::type_byte:
    return sizeof(u8);
  case MemType::type_halfword:
    return sizeof(u16);
  case MemType::type_word:
    return sizeof(u32);
  case MemType::type_float:
    return sizeof(float);
  case MemType::type_double:
    return sizeof(double);
  case MemType::type_string:
    return length;
  case MemType::type_byteArray:
    return length;
  default:
    return 0;
  }
}

bool shouldBeBSwappedForType(const MemType type)
{
  switch (type)
  {
  case MemType::type_byte:
    return false;
  case MemType::type_halfword:
    return true;
  case MemType::type_word:
    return true;
  case MemType::type_float:
    return true;
  case MemType::type_double:
    return true;
  case MemType::type_string:
    return false;
  case MemType::type_byteArray:
    return false;
  default:
    return false;
  }
}

int getNbrBytesAlignementForType(const MemType type)
{
  switch (type)
  {
  case MemType::type_byte:
    return 1;
  case MemType::type_halfword:
    return 2;
  case MemType::type_word:
    return 4;
  case MemType::type_float:
    return 4;
  case MemType::type_double:
    return 4;
  case MemType::type_string:
    return 1;
  case MemType::type_byteArray:
    return 1;
  default:
    return 1;
  }
}

char* formatStringToMemory(MemOperationReturnCode& returnCode, size_t& actualLength,
                           const std::string inputString, const MemBase base, const MemType type,
                           const size_t length)
{
  if (inputString.length() == 0)
  {
    returnCode = MemOperationReturnCode::invalidInput;
    return nullptr;
  }

  std::stringstream ss(inputString);
  switch (base)
  {
  case MemBase::base_octal:
    ss >> std::oct;
    break;
  case MemBase::base_decimal:
    ss >> std::dec;
    break;
  case MemBase::base_hexadecimal:
    ss >> std::hex;
    break;
  default:;
  }

  size_t size = getSizeForType(type, length);
  char* buffer = new char[size];

  switch (type)
  {
  case MemType::type_byte:
  {
    u8 theByte = 0;
    if (base == MemBase::base_binary)
    {
      unsigned long long input = 0;
      input = std::bitset<sizeof(u8) * 8>(inputString).to_ullong();
      theByte = static_cast<u8>(input);
    }
    else
    {
      int theByteInt = 0;
      ss >> theByteInt;
      if (ss.fail())
      {
        delete[] buffer;
        buffer = nullptr;
        returnCode = MemOperationReturnCode::invalidInput;
        return buffer;
      }
      theByte = static_cast<u8>(theByteInt);
    }

    std::memcpy(buffer, &theByte, size);
    actualLength = sizeof(u8);
    break;
  }

  case MemType::type_halfword:
  {
    u16 theHalfword = 0;
    if (base == MemBase::base_binary)
    {
      unsigned long long input = 0;
      input = std::bitset<sizeof(u16) * 8>(inputString).to_ullong();
      theHalfword = static_cast<u16>(input);
    }
    else
    {
      ss >> theHalfword;
      if (ss.fail())
      {
        delete[] buffer;
        buffer = nullptr;
        returnCode = MemOperationReturnCode::invalidInput;
        return buffer;
      }
    }

    std::memcpy(buffer, &theHalfword, size);
    actualLength = sizeof(u16);
    break;
  }

  case MemType::type_word:
  {
    u32 theWord = 0;
    if (base == MemBase::base_binary)
    {
      unsigned long long input = 0;
      // try
      //{
      input = std::bitset<sizeof(u32) * 8>(inputString).to_ullong();
      //}
      // catch (std::invalid_argument)
      //{
      //  delete[] buffer;
      //  buffer = nullptr;
      //  returnCode = MemOperationReturnCode::invalidInput;
      //  return buffer;
      //}
      theWord = static_cast<u32>(input);
    }
    else
    {
      ss >> theWord;
      if (ss.fail())
      {
        delete[] buffer;
        buffer = nullptr;
        returnCode = MemOperationReturnCode::invalidInput;
        return buffer;
      }
    }

    std::memcpy(buffer, &theWord, size);
    actualLength = sizeof(u32);
    break;
  }

  case MemType::type_float:
  {
    float theFloat = 0.0f;
    // 9 digits is the max number of digits in a flaot that can recover any binary format
    ss >> std::setprecision(9) >> theFloat;
    if (ss.fail())
    {
      delete[] buffer;
      buffer = nullptr;
      returnCode = MemOperationReturnCode::invalidInput;
      return buffer;
    }
    std::memcpy(buffer, &theFloat, size);
    actualLength = sizeof(float);
    break;
  }

  case MemType::type_double:
  {
    double theDouble = 0.0;
    // 17 digits is the max number of digits in a double that can recover any binary format
    ss >> std::setprecision(17) >> theDouble;
    if (ss.fail())
    {
      delete[] buffer;
      buffer = nullptr;
      returnCode = MemOperationReturnCode::invalidInput;
      return buffer;
    }
    std::memcpy(buffer, &theDouble, size);
    actualLength = sizeof(double);
    break;
  }

  case MemType::type_string:
  {
    if (inputString.length() > length)
    {
      delete[] buffer;
      buffer = nullptr;
      returnCode = MemOperationReturnCode::inputTooLong;
      return buffer;
    }
    std::memcpy(buffer, inputString.c_str(), length);
    actualLength = length;
    break;
  }

  case MemType::type_byteArray:
  {
    std::vector<std::string> bytes;
    std::string next;
    for (auto i : inputString)
    {
      if (i == ' ')
      {
        if (!next.empty())
        {
          bytes.push_back(next);
          next.clear();
        }
      }
      else
      {
        next += i;
      }
    }
    if (!next.empty())
    {
      bytes.push_back(next);
      next.clear();
    }

    if (bytes.size() > length)
    {
      delete[] buffer;
      buffer = nullptr;
      returnCode = MemOperationReturnCode::inputTooLong;
      return buffer;
    }

    int index = 0;
    for (const auto& i : bytes)
    {
      std::stringstream byteStream(i);
      ss >> std::hex;
      u8 theByte = 0;
      int theByteInt = 0;
      ss >> theByteInt;
      if (ss.fail())
      {
        delete[] buffer;
        buffer = nullptr;
        returnCode = MemOperationReturnCode::invalidInput;
        return buffer;
      }
      theByte = static_cast<u8>(theByteInt);
      std::memcpy(&(buffer[index]), &theByte, sizeof(u8));
      index++;
    }
    actualLength = bytes.size();
  }
  default:;
  }
  return buffer;
}

std::string formatMemoryToString(const char* memory, const MemType type, const size_t length,
                                 const MemBase base, const bool isUnsigned, const bool withBSwap)
{
  std::stringstream ss;
  switch (base)
  {
  case Common::MemBase::base_octal:
    ss << std::oct;
    break;
  case Common::MemBase::base_decimal:
    ss << std::dec;
    break;
  case Common::MemBase::base_hexadecimal:
    ss << std::hex << std::uppercase;
    break;
  default:;
  }

  switch (type)
  {
  case Common::MemType::type_byte:
  {
    if (isUnsigned || base == Common::MemBase::base_binary)
    {
      u8 unsignedByte = 0;
      std::memcpy(&unsignedByte, memory, sizeof(u8));
      if (base == Common::MemBase::base_binary)
        return std::bitset<sizeof(u8) * 8>(unsignedByte).to_string();
      // This has to be converted to an integer type because printing a uint8_t would resolve to a
      // char and print a single character.
      ss << static_cast<unsigned int>(unsignedByte);
      return ss.str();
    }
    else
    {
      s8 aByte = 0;
      std::memcpy(&aByte, memory, sizeof(s8));
      // This has to be converted to an integer type because printing a uint8_t would resolve to a
      // char and print a single character.  Additionaly, casting a signed type to a larger signed
      // type will extend the sign to match the size of the destination type, this is required for
      // signed values in decimal, but must be bypassed for other bases, this is solved by first
      // casting to u8 then to signed int.
      if (base == Common::MemBase::base_decimal)
        ss << static_cast<int>(aByte);
      else
        ss << static_cast<int>(static_cast<u8>(aByte));
      return ss.str();
    }
  }
  case Common::MemType::type_halfword:
  {
    char* memoryCopy = new char[sizeof(u16)];
    std::memcpy(memoryCopy, memory, sizeof(u16));
    if (withBSwap)
    {
      u16 halfword = 0;
      std::memcpy(&halfword, memoryCopy, sizeof(u16));
      halfword = Common::swap16(halfword);
      std::memcpy(memoryCopy, &halfword, sizeof(u16));
    }

    if (isUnsigned || base == Common::MemBase::base_binary)
    {
      u16 unsignedHalfword = 0;
      std::memcpy(&unsignedHalfword, memoryCopy, sizeof(u16));
      if (base == Common::MemBase::base_binary)
      {
        delete[] memoryCopy;
        return std::bitset<sizeof(u16) * 8>(unsignedHalfword).to_string();
      }
      ss << unsignedHalfword;
      delete[] memoryCopy;
      return ss.str();
    }
    s16 aHalfword = 0;
    std::memcpy(&aHalfword, memoryCopy, sizeof(s16));
    ss << aHalfword;
    delete[] memoryCopy;
    return ss.str();
  }
  case Common::MemType::type_word:
  {
    char* memoryCopy = new char[sizeof(u32)];
    std::memcpy(memoryCopy, memory, sizeof(u32));
    if (withBSwap)
    {
      u32 word = 0;
      std::memcpy(&word, memoryCopy, sizeof(u32));
      word = Common::swap32(word);
      std::memcpy(memoryCopy, &word, sizeof(u32));
    }

    if (isUnsigned || base == Common::MemBase::base_binary)
    {
      u32 unsignedWord = 0;
      std::memcpy(&unsignedWord, memoryCopy, sizeof(u32));
      if (base == Common::MemBase::base_binary)
      {
        delete[] memoryCopy;
        return std::bitset<sizeof(u32) * 8>(unsignedWord).to_string();
      }
      ss << unsignedWord;
      delete[] memoryCopy;
      return ss.str();
    }
    s32 aWord = 0;
    std::memcpy(&aWord, memoryCopy, sizeof(s32));
    ss << aWord;
    delete[] memoryCopy;
    return ss.str();
  }
  case Common::MemType::type_float:
  {
    char* memoryCopy = new char[sizeof(u32)];
    std::memcpy(memoryCopy, memory, sizeof(u32));
    if (withBSwap)
    {
      u32 word = 0;
      std::memcpy(&word, memoryCopy, sizeof(u32));
      word = Common::swap32(word);
      std::memcpy(memoryCopy, &word, sizeof(u32));
    }

    float aFloat = 0.0f;
    std::memcpy(&aFloat, memoryCopy, sizeof(float));
    // With 9 digits of precision, it is possible to convert a float back and forth to its binary
    // representation without any loss
    ss << std::setprecision(9) << aFloat;
    delete[] memoryCopy;
    return ss.str();
  }
  case Common::MemType::type_double:
  {
    char* memoryCopy = new char[sizeof(u64)];
    std::memcpy(memoryCopy, memory, sizeof(u64));
    if (withBSwap)
    {
      u64 doubleword = 0;
      std::memcpy(&doubleword, memoryCopy, sizeof(u64));
      doubleword = Common::swap64(doubleword);
      std::memcpy(memoryCopy, &doubleword, sizeof(u64));
    }

    double aDouble = 0.0;
    std::memcpy(&aDouble, memoryCopy, sizeof(double));
    // With 17 digits of precision, it is possible to convert a double back and forth to its binary
    // representation without any loss
    ss << std::setprecision(17) << aDouble;
    delete[] memoryCopy;
    return ss.str();
  }
  case Common::MemType::type_string:
  {
    size_t actualLength = 0;
    while (actualLength < length && *(memory + actualLength) != 0x00)
      ++actualLength;
    return std::string(memory, actualLength);
  }
  case Common::MemType::type_byteArray:
  {
    // Force Hexadecimal, no matter the base
    ss << std::hex << std::uppercase;
    for (size_t i = 0; i < length; ++i)
    {
      u8 aByte = 0;
      std::memcpy(&aByte, memory + i, sizeof(u8));
      ss << std::setfill('0') << std::setw(2) << static_cast<int>(aByte) << " ";
    }
    std::string str = ss.str();
    // Remove the space at the end
    str.pop_back();
    return str;
  }
  default:
    return "";
    break;
  }
}

u32 getMEM1()
{
  return 0x80000000;
}
u32 getMEM1Size()
{
  return Core::System::GetInstance().GetMemory().GetRamSizeReal();
}
u32 getMEM1End()
{
  return getMEM1() + Core::System::GetInstance().GetMemory().GetRamSizeReal();
}

bool isMEM2Present()
{
  return Core::System::GetInstance().GetMemory().GetEXRAM();
}

u32 getMEM2()
{
  return 0x90000000;
}
u32 getMEM2Size()
{
  return Core::System::GetInstance().GetMemory().GetExRamSizeReal();
}
u32 getMEM2End()
{
  return getMEM2() + Core::System::GetInstance().GetMemory().GetExRamSizeReal();
}

bool isARAMAccessible()
{  // TODO: check that DME's ARAM indeed corresponds to Dolphin's FakeVMEM.
  return Core::System::GetInstance().GetMemory().GetFakeVMEM();
}

u32 getARAM()
{
  return 0x7E000000;
}
u32 getARAMSize()
{
  return Core::System::GetInstance().GetMemory().GetFakeVMemSize();
}
u32 getARAMEnd()
{
  return getARAM() + Core::System::GetInstance().GetMemory().GetFakeVMemSize();
}

u32 totalRAMSize()
{
  Memory::MemoryManager& mem = Core::System::GetInstance().GetMemory();
  u32 size = mem.GetRamSizeReal();
  if (mem.GetEXRAM())
    size += mem.GetExRamSizeReal();
  if (mem.GetFakeVMEM())
    size += mem.GetFakeVMemSize();
  return size;
}

bool hasMemory()
{
  return Core::System::GetInstance().GetMemory().IsInitialized();
}

bool isValidAddress(const u32 address)
{
  // We could use Memory::MemoryManager::GetPointer but it may panic, so the code from DME may be
  // better.
  Memory::MemoryManager& mem = Core::System::GetInstance().GetMemory();

  if (address >= getMEM1() && address < getMEM1() + mem.GetRamSizeReal())
    return true;

  if (mem.GetEXRAM() && address >= getMEM2() && address < getMEM2() + mem.GetExRamSizeReal())
    return true;

  if (mem.GetFakeVMEM() && address >= getARAM() && address < getARAM() + mem.GetFakeVMemSize())
    return true;

  return false;
}

u32 addrToOffset(u32 addr)
{
  Memory::MemoryManager& mem = Core::System::GetInstance().GetMemory();

  // ARAM address
  if (addr >= getARAM() && addr < getARAM() + mem.GetFakeVMemSize())
    return addr - getARAM();

  // MEM1 address
  if (addr >= getMEM1() && addr < getMEM1() + mem.GetRamSizeReal())
    return addr - getMEM1() + (mem.GetFakeVMEM() ? mem.GetFakeVMemSize() : 0);

  // MEM2 address
  if (addr >= getMEM2() && addr < getMEM2() + mem.GetExRamSizeReal())
    return addr - getMEM2() + (mem.GetFakeVMEM() ? mem.GetFakeVMemSize() : 0) +
           mem.GetRamSizeReal();

  // invalid address
  return mem.GetFakeVMemSize() + mem.GetRamSizeReal() + mem.GetExRamSizeReal();
}

u32 offsetToAddr(u32 offset)
{
  Memory::MemoryManager& mem = Core::System::GetInstance().GetMemory();

  if (mem.GetFakeVMEM())
  {
    if (offset < mem.GetFakeVMemSize())
      return offset + getARAM();
    offset -= mem.GetFakeVMemSize();
  }
  if (offset < mem.GetRamSizeReal())
    return offset + getMEM1();
  return offset - mem.GetRamSizeReal() + getMEM2();
}

void readFromRAM(void* data, u32 address, size_t size, bool withBSwap)
{
  // We may come here without the memory being initialized if the
  // emulation has just stopped and a timer asks for a refresh.
  if (!Core::System::GetInstance().GetMemory().GetRAM())
    return;

  if (getARAM() <= address && address + size <= getARAMEnd())
    std::memcpy(data, Core::System::GetInstance().GetMemory().GetFakeVMEM() + address - getARAM(),
                size);
  else
    Core::System::GetInstance().GetMemory().CopyFromEmu(data, address, size);

  if (withBSwap)
  {
    switch (size)
    {
    case 2:
    {
      u16 halfword = 0;
      std::memcpy(&halfword, data, sizeof(u16));
      halfword = Common::swap16(halfword);
      std::memcpy(data, &halfword, sizeof(u16));
      break;
    }
    case 4:
    {
      u32 word = 0;
      std::memcpy(&word, data, sizeof(u32));
      word = Common::swap32(word);
      std::memcpy(data, &word, sizeof(u32));
      break;
    }
    case 8:
    {
      u64 doubleword = 0;
      std::memcpy(&doubleword, data, sizeof(u64));
      doubleword = Common::swap64(doubleword);
      std::memcpy(data, &doubleword, sizeof(u64));
      break;
    }
    }
  }
}

void writeToRAM(void* data, u32 address, size_t size, bool withBSwap)
{
  if (withBSwap)
  {
    char* buffer = new char[size];
    switch (size)
    {
    case 2:
    {
      u16 halfword = 0;
      std::memcpy(&halfword, data, sizeof(u16));
      halfword = Common::swap16(halfword);
      std::memcpy(buffer, &halfword, sizeof(u16));
      break;
    }
    case 4:
    {
      u32 word = 0;
      std::memcpy(&word, data, sizeof(u32));
      word = Common::swap32(word);
      std::memcpy(buffer, &word, sizeof(u32));
      break;
    }
    case 8:
    {
      u64 doubleword = 0;
      std::memcpy(&doubleword, data, sizeof(u64));
      doubleword = Common::swap64(doubleword);
      std::memcpy(buffer, &doubleword, sizeof(u64));
      break;
    }
    }
    writeToRAM(buffer, address, size, false);
    delete[] buffer;
  }
  // We may come here without the memory being initialized if the
  // emulation has just stopped and a timer asks for a refresh.
  else if (Core::System::GetInstance().GetMemory().GetRAM())
  {
    if (getARAM() <= address && address + size <= getARAMEnd())
      std::memcpy(Core::System::GetInstance().GetMemory().GetFakeVMEM() + address - getARAM(), data,
                  size);
    else
      Core::System::GetInstance().GetMemory().CopyToEmu(address, data, size);
  }
}

void readAllRAM(void* data)
{
  Memory::MemoryManager& mem = Core::System::GetInstance().GetMemory();
  if (mem.GetFakeVMEM())
  {
    std::memcpy(data, mem.GetFakeVMEM(), mem.GetFakeVMemSize());
    data = (u8*)data + mem.GetFakeVMemSize();
  }
  if (mem.GetRAM())
  {
    mem.CopyFromEmu(data, getMEM1(), mem.GetRamSizeReal());
    data = (u8*)data + mem.GetRamSizeReal();
  }
  if (mem.GetEXRAM())
    mem.CopyFromEmu(data, getMEM2(), mem.GetExRamSizeReal());
}
}  // namespace Common
