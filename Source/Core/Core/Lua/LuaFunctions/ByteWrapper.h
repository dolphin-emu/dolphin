#pragma once

#include "common/CommonTypes.h"
#include <stdexcept>
#include <format>
#include "NumberType.h"

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

class ByteWrapper
{
public:

  enum OPERATIONS
  {
    UNDEFINED_OPERATION,
    EQUALS_EQUALS,
    NOT_EQUALS,
    LESS_THAN,
    LESS_THAN_EQUALS,
    GREATER_THAN,
    GREATER_THAN_EQUALS,
    BITSHIFT_LEFT,
    BITSHIFT_RIGHT,
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR,
    BITWISE_NOT,
    LOGICAL_AND,
    LOGICAL_OR,
    LOGICAL_NOT
  };

  u64 bytes;
  NumberType byteType; // represents the type.
  s8 numBytesUsedByType;
  s8 totalBytesAllocated;


  ByteWrapper();
  static ByteWrapper* CreateByteWrapperFromU8(u8);
  static ByteWrapper* CreateByteWrapperFromU16(u16);
  static ByteWrapper* CreateByteWrapperFromU32(u32);
  static ByteWrapper* CreateByteWrapperFromU64(u64);
  static ByteWrapper* CreateByteWrapperFromLongLong(long long);
  static ByteWrapper* CreateByteWrapperFromDouble(double);
  static ByteWrapper* CreateByteWrapperFromCopy(ByteWrapper*);

  static bool typeSizeCheck(ByteWrapper* byteWrapperPointer, NumberType desiredType);
  // If the number represented by the first wrapper equals the number represented by the 2nd wrapper, then true is returned.
  // Otherwise, false is returned.
  bool operator==(const ByteWrapper& otherByteWrapper) const;
  bool operator!=(const ByteWrapper& otherByteWrapper) const;
  bool operator>(const ByteWrapper& otherByteWrapper) const;
  bool operator<(const ByteWrapper& otherByteWrapper) const;
  bool operator>=(const ByteWrapper& otherByteWrapper) const;
  bool operator<=(const ByteWrapper& otherByteWrapper) const;

  ByteWrapper operator&(const ByteWrapper& otherByteWrapper) const;
  ByteWrapper operator|(const ByteWrapper& otherByteWrapper) const;
  ByteWrapper operator^(const ByteWrapper& otherByteWrapper) const;
  ByteWrapper operator<<(const ByteWrapper& otherByteWrapper) const;
  ByteWrapper operator>>(const ByteWrapper& otherByteWrapper) const;
  ByteWrapper operator&&(const ByteWrapper& otherByteWrapper) const;
  ByteWrapper operator||(const ByteWrapper& otherByteWrapper) const;

  ByteWrapper operator~() const;
  ByteWrapper operator!() const;

  u8 getByteSize() const { return numBytesUsedByType; }
  u8 getValueAsU8() const;
  s8 getValueAsS8() const;
  u16 getValueAsU16() const;
  s16 getValueAsS16() const;
  u32 getValueAsU32() const;
  s32 getValueAsS32() const;
  u64 getValueAsU64() const;
  s64 getValueAsS64() const;
  float getValueAsFloat() const;
  double getValueAsDouble() const;
  void setType(NumberType);

  bool doComparisonOperation(const ByteWrapper& otherByteWrapper, OPERATIONS operation) const;
  ByteWrapper doNonComparisonOperation(const ByteWrapper& otherByteWrapper, OPERATIONS operation) const;

  private:

    ByteWrapper(u8);
    ByteWrapper(u16);
    ByteWrapper(u32);
    ByteWrapper(u64);

    template<typename T, typename V>
    OPERATIONS comparison_helper(T firstVal, V secondVal) const;

    
  template <typename T>
  T non_comparison_helper(T firstVal, T secondVal, OPERATIONS operation) const;

    ByteWrapper doUnaryOperation(OPERATIONS operation) const;
    template <typename T>
    T unary_operation_helper(T val, OPERATIONS operation) const;
};
