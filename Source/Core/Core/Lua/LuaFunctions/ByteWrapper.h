#pragma once

#include "common/CommonTypes.h"
#include <stdexcept>
#include <format>

extern "C" {
#include "src/lapi.h"
#include "src/lua.h"
#include "src/lua.hpp"
#include "src/luaconf.h"
}

class ByteWrapper
{
public:

  enum ByteType
  {
    UNDEFINED,
    UNSIGNED_8,
    SIGNED_8,
    UNSIGNED_16,
    SIGNED_16,
    UNSIGNED_32,
    SIGNED_32,
    UNSIGNED_64,
    SIGNED_64,
    FLOAT,
    DOUBLE,
    WRAPPER //This type is just included in the enum to make it clear in the parseType() function when the user wants a ByteWrapper type object - no actual ByteWrapper object should have this as a type value.
  };

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
  ByteType byteType; // represents the type.
  s8 numBytesUsedByType;
  s8 totalBytesAllocated;
  bool createdFromMemoryAddress;  // True if ByteWrapper was initialized from a memory address, and
                                  // false otherwise. If the ByteWrapper was initialized from a memory address, then the most significant byte is read out as the first byte in read_u8() methods. Otherwise, the least significant byte is the first byte in read_u8() methods.
  //To give a more concrete example, let's say that the ByteWrapper was initialized from memory addresses containing the value (in order) of 0X0102030405060708
  //In this case, a call to get_value("u8") would return 0X01.
  //Suppose that the ByteWrapper was initialized from the value 42. In this case, a call to get_value("u8") would return 42.


  ByteWrapper();
  static ByteWrapper* CreateByteWrapperFromU8(u8, bool);
  static ByteWrapper* CreateByteWrapperFromU16(u16, bool);
  static ByteWrapper* CreateByteWrapperFromU32(u32, bool);
  static ByteWrapper* CreateByteWrapperFromU64(u64, bool);
  static ByteWrapper* CreateByteWrapperFromLongLong(long long, bool);
  static ByteWrapper* CreateBytewrapperFromDouble(double, bool);
  static ByteWrapper* CreateByteWrapperFromCopy(ByteWrapper*);

  static std::string getByteTypeAsString(ByteType inputType);
  static ByteType parseType(const char*);
  static bool typeSizeCheck(lua_State* luaState, ByteWrapper* byteWrapperPointer, ByteWrapper::ByteType parsedType, const char* errorMessage);
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
  void setType(ByteType);

  private:

    ByteWrapper(u8, bool);
    ByteWrapper(u16, bool);
    ByteWrapper(u32, bool);
    ByteWrapper(u64, bool);

    template<typename T, typename V>
    OPERATIONS comparison_helper(T firstVal, V secondVal) const;

    
  template <typename T>
  T non_comparison_helper(T firstVal, T secondVal, OPERATIONS operation) const;

    bool doComparisonOperation(const ByteWrapper& otherByteWrapper, OPERATIONS operation) const;
    ByteWrapper doNonComparisonOperation(const ByteWrapper& otherByteWrapper, OPERATIONS operation) const;

    ByteWrapper doUnaryOperation(OPERATIONS operation) const;
    template <typename T>
    T unary_operation_helper(T val, OPERATIONS operation) const;
};
