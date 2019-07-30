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

// Use these visitors interfaces if you need to write code that performs
// different actions based on the handling method used by a handler. Write your
// visitor implementing that interface, then use handler->VisitHandlingMethod
// to run the proper function.
template <typename T>
class ReadHandlerVisitor
{
public:
  virtual void VisitConstant(T value) = 0;
  virtual void VisitDirect(const T* addr, u32 mask) = 0;
  virtual void VisitComplex(const std::function<T(u32)>* lambda) = 0;
};

template <typename T>
class WriteHandlerVisitor
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
  virtual ~ReadHandler() = default;
  virtual T Read(u32 addr) = 0;
  virtual void Visit(ReadHandlerVisitor<T>& visitor) = 0;
};

template <typename T>
class WriteHandler
{
public:
  virtual ~WriteHandler() = default;
  virtual void Write(u32 addr, T val) = 0;
  virtual void Visit(WriteHandlerVisitor<T>& visitor) = 0;
};


// Constant: handling method holds a single integer and passes it to the
// visitor. This is a read only handling method: storing to a constant does not
// mean anything.
template <typename T>
class ConstantReadHandler : public ReadHandler<T>
{
public:
  explicit ConstantReadHandler(T value) : value_(value) {}

  T Read(u32 addr) override
  {
    return value_;
  }

  void Visit(ReadHandlerVisitor<T>& visitor) override
  {
    visitor.VisitConstant(value_);
  }

private:
  T value_;
};

template <typename T>
ReadHandler<T>* Constant(T value)
{
  return new ConstantReadHandler<T>(value);
}

// Nop: extremely simple write handling method that does nothing at all, only
// respond to visitors and dispatch to the correct method. This is write only
// since reads should always at least return a value.
template <typename T>
class NopWriteHandler : public WriteHandler<T>
{
public:

  void Visit(WriteHandlerVisitor<T>& visitor) override
  {
    visitor.VisitNop();
  }

  void Write(u32 addr, T val) override
  {

  }
};

template <typename T>
WriteHandler<T>* Nop()
{
  return new NopWriteHandler<T>();
}

// Direct: handling method holds a pointer to the value where to read/write the
// data from, as well as a mask that is used to restrict reading/writing only
// to a given set of bits.
template <typename T>
class DirectHandler : public ReadHandler<T>, public WriteHandler<T>
{
public:
  DirectHandler(T* addr, u32 mask) : addr_(addr), mask_(mask) {}

  T Read(u32 _) override
  {
    return *addr_ & mask_;
  }

  void Visit(ReadHandlerVisitor<T>& visitor) override
  {
    visitor.VisitDirect(addr_, mask_);
  }

  void Write(u32 _, T val) override
  {
    *addr_ = val & mask_;
  }

  void Visit(WriteHandlerVisitor<T>& visitor) override
  {
    visitor.VisitDirect(addr_, mask_);
  }

private:
  T * addr_;
  u32 mask_;
};

template <typename T>
ReadHandler<T>* DirectRead(const T* addr, u32 mask = 0xFFFFFFFF)
{
  return new DirectHandler<T>(const_cast<T*>(addr), mask);
}
template <typename T>
ReadHandler<T>* DirectRead(volatile const T* addr, u32 mask = 0xFFFFFFFF)
{
  return new DirectHandler<T>((T*)addr, mask);
}

template <typename T>
WriteHandler<T>* DirectWrite(T* addr, u32 mask = 0xFFFFFFFF)
{
  return new DirectHandler<T>(addr, mask);
}
template <typename T>
WriteHandler<T>* DirectWrite(volatile T* addr, u32 mask = 0xFFFFFFFF)
{
  return new DirectHandler<T>((T*)addr, mask);
}

// Complex: holds a lambda that is called when a read or a write is executed.
// This gives complete control to the user as to what is going to happen during
// that read or write, but reduces the optimization potential.
template <typename T>
class ComplexHandler : public ReadHandler<T>, public WriteHandler<T>
{
public:
  explicit ComplexHandler(std::function<T(u32)> read_lambda)
    : read_lambda_(read_lambda), write_lambda_(InvalidWriteLambda())
  {
  }

  explicit ComplexHandler(std::function<void(u32, T)> write_lambda)
    : read_lambda_(InvalidReadLambda()), write_lambda_(write_lambda)
  {
  }

  virtual ~ComplexHandler() = default;

  T Read(u32 addr) override
  {
    return read_lambda_(addr);
  }

  void Visit(ReadHandlerVisitor<T>& visitor) override
  {
    visitor.VisitComplex(&read_lambda_);
  }

  void Write(u32 addr, T val) override
  {
    write_lambda_(addr, val);
  }

  void Visit(WriteHandlerVisitor<T>& visitor) override
  {
    visitor.VisitComplex(&write_lambda_);
  }

private:
  std::function<T(u32)> InvalidReadLambda() const
  {
    return [](u32) {
      DEBUG_ASSERT_MSG(MEMMAP, 0,
        "Called the read lambda on a write "
        "complex handler.");
      return 0;
    };
  }

  std::function<void(u32, T)> InvalidWriteLambda() const
  {
    return [](u32, T) {
      DEBUG_ASSERT_MSG(MEMMAP, 0,
        "Called the write lambda on a read "
        "complex handler.");
    };
  }

  std::function<T(u32)> read_lambda_;
  std::function<void(u32, T)> write_lambda_;
};
template <typename T>
ReadHandler<T>* ComplexRead(std::function<T(u32)> lambda)
{
  return new ComplexHandler<T>(lambda);
}
template <typename T>
WriteHandler<T>* ComplexWrite(std::function<void(u32, T)> lambda)
{
  return new ComplexHandler<T>(lambda);
}

// Invalid: specialization of the complex handling type with lambdas that
// display error messages.
template <typename T>
ReadHandler<T>* InvalidRead()
{
  return ComplexRead<T>([](u32 addr) {
    ERROR_LOG(MEMMAP, "Trying to read %zu bits from an invalid MMIO (addr=%08x)", 8 * sizeof(T),
      addr);
    return -1;
  });
}

template <typename T>
WriteHandler<T>* InvalidWrite()
{
  return ComplexWrite<T>([](u32 addr, T val) {
    ERROR_LOG(MEMMAP, "Trying to write %zu bits to an invalid MMIO (addr=%08x, val=%08x)",
      8 * sizeof(T), addr, (u32)val);
  });
}

}  // namespace MMIO
