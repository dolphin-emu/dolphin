// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>

#include "Common/CommonTypes.h"

// All the templated and very repetitive MMIO-related code is isolated in this
// file for easier reading. It mostly contains code related to handling methods
// (including the declaration of the public functions for creating handling
// method objects), visitors for these handling methods, and interface of the
// handler classes.
//
// This code is very genericized (aka. lots of templates) in order to handle
// u8/u16/u32 with the same code while providing type safety: it is impossible
// to mix code from these types, and the type system enforces it.

namespace MMIO
{
class Mapping;

// Read and write handling methods are separated for type safety. On top of
// that, some handling methods require different arguments for reads and writes
// (Complex, for example).
template <typename T>
class ReadHandlingMethod;
template <typename T>
class WriteHandlingMethod;

// Constant: use when the value read on this MMIO is always the same. This is
// only for reads.
template <typename T>
ReadHandlingMethod<T>* Constant(T value);

// Nop: use for writes that shouldn't have any effect and shouldn't log an
// error either.
template <typename T>
WriteHandlingMethod<T>* Nop();

// Direct: use when all the MMIO does is read/write the given value to/from a
// global variable, with an optional mask applied on the read/written value.
template <typename T>
ReadHandlingMethod<T>* DirectRead(const T* addr, u32 mask = 0xFFFFFFFF);
template <typename T>
ReadHandlingMethod<T>* DirectRead(volatile const T* addr, u32 mask = 0xFFFFFFFF);
template <typename T>
WriteHandlingMethod<T>* DirectWrite(T* addr, u32 mask = 0xFFFFFFFF);
template <typename T>
WriteHandlingMethod<T>* DirectWrite(volatile T* addr, u32 mask = 0xFFFFFFFF);

// Complex: use when no other handling method fits your needs. These allow you
// to directly provide a function that will be called when a read/write needs
// to be done.
template <typename T>
ReadHandlingMethod<T>* ComplexRead(std::function<T(u32)>);
template <typename T>
WriteHandlingMethod<T>* ComplexWrite(std::function<void(u32, T)>);

// Invalid: log an error and return -1 in case of a read. These are the default
// handlers set for all MMIO types.
template <typename T>
ReadHandlingMethod<T>* InvalidRead();
template <typename T>
WriteHandlingMethod<T>* InvalidWrite();

// {Read,Write}To{Smaller,Larger}: these functions are not themselves handling
// methods but will try to combine accesses to two handlers into one new
// handler object.
//
// This is used for example when 32 bit reads have the exact same handling as
// 16 bit. Handlers need to be registered for both 32 and 16, and it would be
// repetitive and unoptimal to require users to write the same handling code in
// both cases. Instead, an MMIO module can simply define all handlers in terms
// of 16 bit reads, then use ReadToSmaller<u32> to convert u32 reads to u16
// reads.
//
// Internally, these size conversion functions have some magic to make the
// combined handlers as fast as possible. For example, if the two underlying
// u16 handlers for a u32 reads are Direct to consecutive memory addresses,
// they can be transformed into a Direct u32 access.
//
// Warning: unlike the other handling methods, *ToSmaller are obviously not
// available for u8, and *ToLarger are not available for u32.
template <typename T>
ReadHandlingMethod<T>* ReadToSmaller(Mapping* mmio, u32 high_part_addr, u32 low_part_addr);
template <typename T>
WriteHandlingMethod<T>* WriteToSmaller(Mapping* mmio, u32 high_part_addr, u32 low_part_addr);
template <typename T>
ReadHandlingMethod<T>* ReadToLarger(Mapping* mmio, u32 larger_addr, u32 shift);

// Use these visitors interfaces if you need to write code that performs
// different actions based on the handling method used by a handler. Write your
// visitor implementing that interface, then use handler->VisitHandlingMethod
// to run the proper function.
template <typename T>
class ReadHandlingMethodVisitor
{
public:
  virtual void VisitConstant(T value) = 0;
  virtual void VisitDirect(const T* addr, u32 mask) = 0;
  virtual void VisitComplex(const std::function<T(u32)>* lambda) = 0;
};
template <typename T>
class WriteHandlingMethodVisitor
{
public:
  virtual void VisitNop() = 0;
  virtual void VisitDirect(T* addr, u32 mask) = 0;
  virtual void VisitComplex(const std::function<void(u32, T)>* lambda) = 0;
};

// These classes are INTERNAL. Do not use outside of the MMIO implementation
// code. Unfortunately, because we want to make Read() and Write() fast and
// inlinable, we need to provide some of the implementation of these two
// classes here and can't just use a forward declaration.
template <typename T>
class ReadHandler
{
public:
  ReadHandler();

  // Takes ownership of "method".
  ReadHandler(ReadHandlingMethod<T>* method);

  ~ReadHandler();

  // Entry point for read handling method visitors.
  void Visit(ReadHandlingMethodVisitor<T>& visitor);

  T Read(u32 addr)
  {
    // Check if the handler has already been initialized. For real
    // handlers, this will always be the case, so this branch should be
    // easily predictable.
    if (!m_Method)
      InitializeInvalid();

    return m_ReadFunc(addr);
  }

  // Internal method called when changing the internal method object. Its
  // main role is to make sure the read function is updated at the same time.
  void ResetMethod(ReadHandlingMethod<T>* method);

private:
  // Initialize this handler to an invalid handler. Done lazily to avoid
  // useless initialization of thousands of unused handler objects.
  void InitializeInvalid() { ResetMethod(InvalidRead<T>()); }
  std::unique_ptr<ReadHandlingMethod<T>> m_Method;
  std::function<T(u32)> m_ReadFunc;
};
template <typename T>
class WriteHandler
{
public:
  WriteHandler();

  // Takes ownership of "method".
  WriteHandler(WriteHandlingMethod<T>* method);

  ~WriteHandler();

  // Entry point for write handling method visitors.
  void Visit(WriteHandlingMethodVisitor<T>& visitor);

  void Write(u32 addr, T val)
  {
    // Check if the handler has already been initialized. For real
    // handlers, this will always be the case, so this branch should be
    // easily predictable.
    if (!m_Method)
      InitializeInvalid();

    m_WriteFunc(addr, val);
  }

  // Internal method called when changing the internal method object. Its
  // main role is to make sure the write function is updated at the same
  // time.
  void ResetMethod(WriteHandlingMethod<T>* method);

private:
  // Initialize this handler to an invalid handler. Done lazily to avoid
  // useless initialization of thousands of unused handler objects.
  void InitializeInvalid() { ResetMethod(InvalidWrite<T>()); }
  std::unique_ptr<WriteHandlingMethod<T>> m_Method;
  std::function<void(u32, T)> m_WriteFunc;
};

// Boilerplate boilerplate boilerplate.
//
// This is used to be able to avoid putting the templates implementation in the
// header files and slow down compilation times. Instead, we declare 3
// specializations in the header file as already implemented in another
// compilation unit: u8, u16, u32.
//
// The "MaybeExtern" is there because that same macro is used for declaration
// (where MaybeExtern = "extern") and definition (MaybeExtern = "").
#define MMIO_GENERIC_PUBLIC_SPECIALIZATIONS(MaybeExtern, T)                                        \
  MaybeExtern template ReadHandlingMethod<T>* Constant<T>(T value);                                \
  MaybeExtern template WriteHandlingMethod<T>* Nop<T>();                                           \
  MaybeExtern template ReadHandlingMethod<T>* DirectRead(const T* addr, u32 mask);                 \
  MaybeExtern template ReadHandlingMethod<T>* DirectRead(volatile const T* addr, u32 mask);        \
  MaybeExtern template WriteHandlingMethod<T>* DirectWrite(T* addr, u32 mask);                     \
  MaybeExtern template WriteHandlingMethod<T>* DirectWrite(volatile T* addr, u32 mask);            \
  MaybeExtern template ReadHandlingMethod<T>* ComplexRead<T>(std::function<T(u32)>);               \
  MaybeExtern template WriteHandlingMethod<T>* ComplexWrite<T>(std::function<void(u32, T)>);       \
  MaybeExtern template ReadHandlingMethod<T>* InvalidRead<T>();                                    \
  MaybeExtern template WriteHandlingMethod<T>* InvalidWrite<T>();                                  \
  MaybeExtern template class ReadHandler<T>;                                                       \
  MaybeExtern template class WriteHandler<T>

#define MMIO_SPECIAL_PUBLIC_SPECIALIZATIONS(MaybeExtern)                                           \
  MaybeExtern template ReadHandlingMethod<u16>* ReadToSmaller(Mapping* mmio, u32 high_part_addr,   \
                                                              u32 low_part_addr);                  \
  MaybeExtern template ReadHandlingMethod<u32>* ReadToSmaller(Mapping* mmio, u32 high_part_addr,   \
                                                              u32 low_part_addr);                  \
  MaybeExtern template WriteHandlingMethod<u16>* WriteToSmaller(Mapping* mmio, u32 high_part_addr, \
                                                                u32 low_part_addr);                \
  MaybeExtern template WriteHandlingMethod<u32>* WriteToSmaller(Mapping* mmio, u32 high_part_addr, \
                                                                u32 low_part_addr);                \
  MaybeExtern template ReadHandlingMethod<u8>* ReadToLarger(Mapping* mmio, u32 larger_addr,        \
                                                            u32 shift);                            \
  MaybeExtern template ReadHandlingMethod<u16>* ReadToLarger(Mapping* mmio, u32 larger_addr,       \
                                                             u32 shift)

#define MMIO_PUBLIC_SPECIALIZATIONS()                                                              \
  MMIO_GENERIC_PUBLIC_SPECIALIZATIONS(MaybeExtern, u8);                                            \
  MMIO_GENERIC_PUBLIC_SPECIALIZATIONS(MaybeExtern, u16);                                           \
  MMIO_GENERIC_PUBLIC_SPECIALIZATIONS(MaybeExtern, u32);                                           \
  MMIO_SPECIAL_PUBLIC_SPECIALIZATIONS(MaybeExtern);

#define MaybeExtern extern
MMIO_PUBLIC_SPECIALIZATIONS()
#undef MaybeExtern
}
