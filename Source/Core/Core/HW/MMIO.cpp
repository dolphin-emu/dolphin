// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/MMIO.h"

#include <functional>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/HW/MMIOHandlers.h"

namespace MMIO
{
// Base classes for the two handling method hierarchies. Note that a single
// class can inherit from both.
//
// At the moment the only common element between all the handling method is
// that they should be able to accept a visitor of the appropriate type.
template <typename T>
class ReadHandlingMethod
{
public:
  virtual ~ReadHandlingMethod() = default;
  virtual void AcceptReadVisitor(ReadHandlingMethodVisitor<T>& v) const = 0;
};
template <typename T>
class WriteHandlingMethod
{
public:
  virtual ~WriteHandlingMethod() = default;
  virtual void AcceptWriteVisitor(WriteHandlingMethodVisitor<T>& v) const = 0;
};

// Constant: handling method holds a single integer and passes it to the
// visitor. This is a read only handling method: storing to a constant does not
// mean anything.
template <typename T>
class ConstantHandlingMethod : public ReadHandlingMethod<T>
{
public:
  explicit ConstantHandlingMethod(T value) : value_(value) {}
  virtual ~ConstantHandlingMethod() = default;
  void AcceptReadVisitor(ReadHandlingMethodVisitor<T>& v) const override
  {
    v.VisitConstant(value_);
  }

private:
  T value_;
};
template <typename T>
ReadHandlingMethod<T>* Constant(T value)
{
  return new ConstantHandlingMethod<T>(value);
}

// Nop: extremely simple write handling method that does nothing at all, only
// respond to visitors and dispatch to the correct method. This is write only
// since reads should always at least return a value.
template <typename T>
class NopHandlingMethod : public WriteHandlingMethod<T>
{
public:
  NopHandlingMethod() {}
  virtual ~NopHandlingMethod() = default;
  void AcceptWriteVisitor(WriteHandlingMethodVisitor<T>& v) const override { v.VisitNop(); }
};
template <typename T>
WriteHandlingMethod<T>* Nop()
{
  return new NopHandlingMethod<T>();
}

// Direct: handling method holds a pointer to the value where to read/write the
// data from, as well as a mask that is used to restrict reading/writing only
// to a given set of bits.
template <typename T>
class DirectHandlingMethod : public ReadHandlingMethod<T>, public WriteHandlingMethod<T>
{
public:
  DirectHandlingMethod(T* addr, u32 mask) : addr_(addr), mask_(mask) {}
  virtual ~DirectHandlingMethod() = default;
  void AcceptReadVisitor(ReadHandlingMethodVisitor<T>& v) const override
  {
    v.VisitDirect(addr_, mask_);
  }

  void AcceptWriteVisitor(WriteHandlingMethodVisitor<T>& v) const override
  {
    v.VisitDirect(addr_, mask_);
  }

private:
  T* addr_;
  u32 mask_;
};
template <typename T>
ReadHandlingMethod<T>* DirectRead(const T* addr, u32 mask)
{
  return new DirectHandlingMethod<T>(const_cast<T*>(addr), mask);
}
template <typename T>
ReadHandlingMethod<T>* DirectRead(volatile const T* addr, u32 mask)
{
  return new DirectHandlingMethod<T>((T*)addr, mask);
}
template <typename T>
WriteHandlingMethod<T>* DirectWrite(T* addr, u32 mask)
{
  return new DirectHandlingMethod<T>(addr, mask);
}
template <typename T>
WriteHandlingMethod<T>* DirectWrite(volatile T* addr, u32 mask)
{
  return new DirectHandlingMethod<T>((T*)addr, mask);
}

// Complex: holds a lambda that is called when a read or a write is executed.
// This gives complete control to the user as to what is going to happen during
// that read or write, but reduces the optimization potential.
template <typename T>
class ComplexHandlingMethod : public ReadHandlingMethod<T>, public WriteHandlingMethod<T>
{
public:
  explicit ComplexHandlingMethod(std::function<T(u32)> read_lambda)
      : read_lambda_(read_lambda), write_lambda_(InvalidWriteLambda())
  {
  }

  explicit ComplexHandlingMethod(std::function<void(u32, T)> write_lambda)
      : read_lambda_(InvalidReadLambda()), write_lambda_(write_lambda)
  {
  }

  virtual ~ComplexHandlingMethod() = default;
  void AcceptReadVisitor(ReadHandlingMethodVisitor<T>& v) const override
  {
    v.VisitComplex(&read_lambda_);
  }

  void AcceptWriteVisitor(WriteHandlingMethodVisitor<T>& v) const override
  {
    v.VisitComplex(&write_lambda_);
  }

private:
  std::function<T(u32)> InvalidReadLambda() const
  {
    return [](u32) {
      _dbg_assert_msg_(MEMMAP, 0, "Called the read lambda on a write "
                                  "complex handler.");
      return 0;
    };
  }

  std::function<void(u32, T)> InvalidWriteLambda() const
  {
    return [](u32, T) {
      _dbg_assert_msg_(MEMMAP, 0, "Called the write lambda on a read "
                                  "complex handler.");
    };
  }

  std::function<T(u32)> read_lambda_;
  std::function<void(u32, T)> write_lambda_;
};
template <typename T>
ReadHandlingMethod<T>* ComplexRead(std::function<T(u32)> lambda)
{
  return new ComplexHandlingMethod<T>(lambda);
}
template <typename T>
WriteHandlingMethod<T>* ComplexWrite(std::function<void(u32, T)> lambda)
{
  return new ComplexHandlingMethod<T>(lambda);
}

// Invalid: specialization of the complex handling type with lambdas that
// display error messages.
template <typename T>
ReadHandlingMethod<T>* InvalidRead()
{
  return ComplexRead<T>([](u32 addr) {
    ERROR_LOG(MEMMAP, "Trying to read %zu bits from an invalid MMIO (addr=%08x)", 8 * sizeof(T),
              addr);
    return -1;
  });
}
template <typename T>
WriteHandlingMethod<T>* InvalidWrite()
{
  return ComplexWrite<T>([](u32 addr, T val) {
    ERROR_LOG(MEMMAP, "Trying to write %zu bits to an invalid MMIO (addr=%08x, val=%08x)",
              8 * sizeof(T), addr, (u32)val);
  });
}

// Converters to larger and smaller size. Probably the most complex of these
// handlers to implement. They do not define new handling method types but
// instead will internally use the types defined above.
template <typename T>
struct SmallerAccessSize
{
};
template <>
struct SmallerAccessSize<u16>
{
  typedef u8 value;
};
template <>
struct SmallerAccessSize<u32>
{
  typedef u16 value;
};

template <typename T>
struct LargerAccessSize
{
};
template <>
struct LargerAccessSize<u8>
{
  typedef u16 value;
};
template <>
struct LargerAccessSize<u16>
{
  typedef u32 value;
};

template <typename T>
ReadHandlingMethod<T>* ReadToSmaller(Mapping* mmio, u32 high_part_addr, u32 low_part_addr)
{
  typedef typename SmallerAccessSize<T>::value ST;

  ReadHandler<ST>* high_part = &mmio->GetHandlerForRead<ST>(high_part_addr);
  ReadHandler<ST>* low_part = &mmio->GetHandlerForRead<ST>(low_part_addr);

  // TODO(delroth): optimize
  return ComplexRead<T>([=](u32 addr) {
    return ((T)high_part->Read(high_part_addr) << (8 * sizeof(ST))) | low_part->Read(low_part_addr);
  });
}

template <typename T>
WriteHandlingMethod<T>* WriteToSmaller(Mapping* mmio, u32 high_part_addr, u32 low_part_addr)
{
  typedef typename SmallerAccessSize<T>::value ST;

  WriteHandler<ST>* high_part = &mmio->GetHandlerForWrite<ST>(high_part_addr);
  WriteHandler<ST>* low_part = &mmio->GetHandlerForWrite<ST>(low_part_addr);

  // TODO(delroth): optimize
  return ComplexWrite<T>([=](u32 addr, T val) {
    high_part->Write(high_part_addr, val >> (8 * sizeof(ST)));
    low_part->Write(low_part_addr, (ST)val);
  });
}

template <typename T>
ReadHandlingMethod<T>* ReadToLarger(Mapping* mmio, u32 larger_addr, u32 shift)
{
  typedef typename LargerAccessSize<T>::value LT;

  ReadHandler<LT>* large = &mmio->GetHandlerForRead<LT>(larger_addr);

  // TODO(delroth): optimize
  return ComplexRead<T>(
      [large, shift](u32 addr) { return large->Read(addr & ~(sizeof(LT) - 1)) >> shift; });
}

// Inplementation of the ReadHandler and WriteHandler class. There is a lot of
// redundant code between these two classes but trying to abstract it away
// brings more trouble than it fixes.
template <typename T>
ReadHandler<T>::ReadHandler()
{
}

template <typename T>
ReadHandler<T>::ReadHandler(ReadHandlingMethod<T>* method) : m_Method(nullptr)
{
  ResetMethod(method);
}

template <typename T>
ReadHandler<T>::~ReadHandler()
{
}

template <typename T>
void ReadHandler<T>::Visit(ReadHandlingMethodVisitor<T>& visitor)
{
  if (!m_Method)
    InitializeInvalid();

  m_Method->AcceptReadVisitor(visitor);
}

template <typename T>
void ReadHandler<T>::ResetMethod(ReadHandlingMethod<T>* method)
{
  m_Method.reset(method);

  struct FuncCreatorVisitor : public ReadHandlingMethodVisitor<T>
  {
    virtual ~FuncCreatorVisitor() = default;

    std::function<T(u32)> ret;

    void VisitConstant(T value) override
    {
      ret = [value](u32) { return value; };
    }

    void VisitDirect(const T* addr, u32 mask) override
    {
      ret = [addr, mask](u32) { return *addr & mask; };
    }

    void VisitComplex(const std::function<T(u32)>* lambda) override { ret = *lambda; }
  };

  FuncCreatorVisitor v;
  Visit(v);
  m_ReadFunc = v.ret;
}

template <typename T>
WriteHandler<T>::WriteHandler()
{
}

template <typename T>
WriteHandler<T>::WriteHandler(WriteHandlingMethod<T>* method) : m_Method(nullptr)
{
  ResetMethod(method);
}

template <typename T>
WriteHandler<T>::~WriteHandler()
{
}

template <typename T>
void WriteHandler<T>::Visit(WriteHandlingMethodVisitor<T>& visitor)
{
  if (!m_Method)
    InitializeInvalid();

  m_Method->AcceptWriteVisitor(visitor);
}

template <typename T>
void WriteHandler<T>::ResetMethod(WriteHandlingMethod<T>* method)
{
  m_Method.reset(method);

  struct FuncCreatorVisitor : public WriteHandlingMethodVisitor<T>
  {
    virtual ~FuncCreatorVisitor() = default;

    std::function<void(u32, T)> ret;

    void VisitNop() override
    {
      ret = [](u32, T) {};
    }

    void VisitDirect(T* ptr, u32 mask) override
    {
      ret = [ptr, mask](u32, T val) { *ptr = val & mask; };
    }

    void VisitComplex(const std::function<void(u32, T)>* lambda) override { ret = *lambda; }
  };

  FuncCreatorVisitor v;
  Visit(v);
  m_WriteFunc = v.ret;
}

// Define all the public specializations that are exported in MMIOHandlers.h.
#define MaybeExtern
MMIO_PUBLIC_SPECIALIZATIONS()
#undef MaybeExtern
}
