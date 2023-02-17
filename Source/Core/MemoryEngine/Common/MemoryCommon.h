#pragma once

#include <cstddef>
#include <string>

#include "CommonTypes.h"

namespace Common
{

enum class MemType
{
  type_byte = 0,
  type_halfword,
  type_word,
  type_float,
  type_double,
  type_string,
  type_byteArray,
  type_num
};

enum class MemBase
{
  base_decimal = 0,
  base_hexadecimal,
  base_octal,
  base_binary,
  base_none // Placeholder when the base doesn't matter (ie. string)
};

enum class MemOperationReturnCode
{
  uninitialized,
  invalidInput,
  operationFailed,
  inputTooLong,
  invalidPointer,
  OK
};

size_t getSizeForType(const MemType type, const size_t length);
bool shouldBeBSwappedForType(const MemType type);
int getNbrBytesAlignementForType(const MemType type);
char* formatStringToMemory(MemOperationReturnCode& returnCode, size_t& actualLength,
                           const std::string inputString, const MemBase base, const MemType type,
                           const size_t length);
std::string formatMemoryToString(const char* memory, const MemType type, const size_t length,
                                 const MemBase base, const bool isUnsigned,
                                 const bool withBSwap = false);

u32 getMEM1();
u32 getMEM1Size();
u32 getMEM1End();
bool isMEM2Present();
u32 getMEM2();
u32 getMEM2Size();
u32 getMEM2End();
bool isARAMAccessible();
u32 getARAM();
u32 getARAMSize();
u32 getARAMEnd();
u32 totalRAMSize();
bool hasMemory();
bool isValidAddress(const u32 address);
u32 addrToOffset(u32 addr);
u32 offsetToAddr(u32 offset);
void readFromRAM(void* data, u32 address, size_t size, bool withBSwap = false);
void writeToRAM(void* data, u32 address, size_t size, bool withBSwap = false);
void readAllRAM(void *data);
} // namespace Common
