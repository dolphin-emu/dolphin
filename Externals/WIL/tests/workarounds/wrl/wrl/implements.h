//
// Copyright (C) Microsoft Corporation
// All rights reserved.
//
// Code in Details namespace is for internal usage within the library code
//

#ifndef _WRL_IMPLEMENTS_H_
#define _WRL_IMPLEMENTS_H_

#ifdef _MSC_VER
#pragma once
#endif  // _MSC_VER

#pragma region includes

#include <inspectable.h>
#include <roapi.h>
#ifdef BUILD_WINDOWS
#include <winrt.h>
#endif
#include <activation.h>
#include <WinString.h>

#include <new.h>
#include <weakreference.h>
#include <objbase.h>    // IMarshal
#include <cguid.h>      // CLSID_StdGlobalInterfaceTable
#include <intrin.h>

#include <wrl\def.h>
#include <wrl\client.h>

#if (NTDDI_VERSION >= NTDDI_WINBLUE)
#include "roerrorapi.h"
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)

// Set packing
#include <pshpack8.h>

#pragma endregion

#ifndef __WRL_NO_DEFAULT_LIB__
#pragma comment(lib, "ole32.lib") // For CoTaskMemAlloc
#endif

#pragma region disable warnings

#pragma warning(push)
#pragma warning(disable: 4584) // 'class1' : base-class 'class2' is already a base-class of 'class3'
#pragma warning(disable: 4481) // nonstandard extension used: override specifier 'override'

#pragma endregion // disable warnings

namespace Microsoft {
namespace WRL {

// Indicator for RuntimeClass,Implements and ChainInterfaces that T interface
// will be not accessible on IID list
// Example:
// struct MyRuntimeClass : RuntimeClass<CloakedIid<IMyCloakedInterface>> {}
template<typename T>
struct CloakedIid : T
{
};

enum RuntimeClassType
{
    WinRt                   = 0x0001,
    ClassicCom              = 0x0002,
    WinRtClassicComMix      = WinRt | ClassicCom,
    InhibitWeakReference    = 0x0004,
    Delegate                = ClassicCom,
    InhibitFtmBase          = 0x0008,
    InhibitRoOriginateError = 0x0010
};

template <unsigned int flags>
struct RuntimeClassFlags
{
    static const unsigned int value = flags;
};

namespace Details
{
// Empty struct used for validating template parameter types in Implements
struct ImplementsBase
{
};

} // namespace Details

// MixIn modifier allows to combine QI from
// a class that doesn't have default constructor on it
template<typename Derived, typename MixInType, bool hasImplements = __is_base_of(Details::ImplementsBase, MixInType)>
struct MixIn
{
};

// ComposableBase template to allow deriving from a RuntimeClass
// Optionally allows specifying the base factory and statics interface
template <typename FactoryInterface = IInspectable>
class ComposableBase
{
};
// Back-compat indicator for RuntimeClass to not support IWeakReferenceSource
typedef RuntimeClassFlags<WinRt | InhibitWeakReference>    InhibitWeakReferencePolicy;

template<unsigned int RuntimeClassTypeT>
struct ErrorHelper
{
    static void OriginateError(HRESULT hr, HSTRING message)
    {
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
        ::RoOriginateError(hr, message);
#else
        UNREFERENCED_PARAMETER(hr);
        UNREFERENCED_PARAMETER(message);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
    }
};

template<>
struct ErrorHelper<InhibitRoOriginateError>
{
    static void OriginateError(HRESULT hr, HSTRING message)
    {
        UNREFERENCED_PARAMETER(hr);
        UNREFERENCED_PARAMETER(message);
        // No-Op
    }
};

namespace Details
{

//Forward declaration
struct CreatorMap;

// Sections automatically generate a list of pointers to CreatorMap through the linker
// Sections a and z are used as a terminators
#pragma section("minATL$__a", read)
// Section f is used to put com objects to creator map
#pragma section("minATL$__f", read)
// Section m divides COM entries from WinRT entries
#pragma section("minATL$__m", read)
// Section r is used to put WinRT objects to creator map
#pragma section("minATL$__r", read)
#pragma section("minATL$__z", read)

extern "C"
{
// Location of the first and last entries for the linker generated list of pointers to CreatorMapEntry
__declspec(selectany) __declspec(allocate("minATL$__a")) const CreatorMap* __pobjectentryfirst = nullptr;
// Section m divides COM objects from WinRT objects
// - sections between a and m we store COM object info
// - sections between m+1 and z we store WinRT object info
__declspec(selectany) __declspec(allocate("minATL$__m")) const CreatorMap* __pobjectentrymid = nullptr;
__declspec(selectany) __declspec(allocate("minATL$__z")) const CreatorMap* __pobjectentrylast = nullptr;
}

// Base class used by all module classes.
class __declspec(novtable) ModuleBase
{
private:
    // Lock that synchronize access and termination of factories
    static void* moduleLock_;

    static_assert(sizeof(moduleLock_) == sizeof(SRWLOCK), "cacheLock must have the same size as SRWLOCK");
protected:
    static volatile unsigned long objectCount_;
public:
    static ModuleBase *module_;

    ModuleBase() throw()
    {
#ifdef _DEBUG
        // WRLs support for activatable classes requires there is only one instance of Module<>, this assert
        // ensures there is only one. Since Module<> is templatized, using different template parameters will
        // result in multiple instances, avoid this by making sure all code in a component uses the same parameters.
        // Note that the C++ CX runtime creates an instance; Module<InProc, Platform::Details::InProcModule>,
        // so mixing it with non CX code can result in this assert.
        // WRL supports static and dynamically allocated Module<>, choose dynamic by defining __WRL_DISABLE_STATIC_INITIALIZE__
        // and allocate that instance with new but only once, for example in the main() entry point of an application.
        __WRL_ASSERT__(::InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(&module_), this, nullptr) == nullptr &&
            "The module was already instantiated");

        SRWLOCK initSRWLOCK = SRWLOCK_INIT;
        __WRL_ASSERT__(reinterpret_cast<SRWLOCK*>(&moduleLock_)->Ptr == initSRWLOCK.Ptr && "Different value for moduleLock_ than SRWLOCK_INIT");
        (initSRWLOCK);
#else
        module_ = this;
#endif
    }

    ModuleBase(const ModuleBase&) = delete;
    ModuleBase& operator=(const ModuleBase&) = delete;

    virtual ~ModuleBase() throw()
    {
#ifdef _DEBUG
        __WRL_ASSERT__(::InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(&module_), nullptr, this) == this &&
            "The module was already instantiated");
#else
        module_ = nullptr;
#endif
    }

    // Number of active objects in the module
    STDMETHOD_(unsigned long, IncrementObjectCount)() = 0;
    STDMETHOD_(unsigned long, DecrementObjectCount)() = 0;

    STDMETHOD_(unsigned long, GetObjectCount)() const
    {
        return objectCount_;
    }

    STDMETHOD_(const CreatorMap**, GetFirstEntryPointer)() const
    {
        return &__pobjectentryfirst;
    }

    STDMETHOD_(const CreatorMap**, GetMidEntryPointer)() const
    {
        return &__pobjectentrymid;
    }

    STDMETHOD_(const CreatorMap**, GetLastEntryPointer)() const
    {
        return &__pobjectentrylast;
    }

    STDMETHOD_(SRWLOCK*, GetLock)() const
    {
        return reinterpret_cast<SRWLOCK*>(&moduleLock_);
    }

    STDMETHOD(RegisterWinRTObject)(_In_opt_z_ const wchar_t*, _In_z_ const wchar_t** activatableClassIds, _Inout_ RO_REGISTRATION_COOKIE* cookie, unsigned int) = 0;
    STDMETHOD(UnregisterWinRTObject)(_In_opt_z_ const wchar_t*, _In_ RO_REGISTRATION_COOKIE) = 0;
    STDMETHOD(RegisterCOMObject)(_In_opt_z_ const wchar_t*, _In_ IID*, _In_ IClassFactory**, _Inout_ DWORD*, unsigned int) = 0;
    STDMETHOD(UnregisterCOMObject)(_In_opt_z_ const wchar_t*, _Inout_ DWORD*, unsigned int) = 0;
};

__declspec(selectany) volatile unsigned long ModuleBase::objectCount_ = 0;
// moduleLock_ value must be equal SRWLOCK_INIT which is nullptr
__declspec(selectany) void* ModuleBase::moduleLock_ = nullptr;
__declspec(selectany) ModuleBase *ModuleBase::module_ = nullptr;

#pragma region helper types
// Empty struct used as default template parameter
class Nil
{
};

// Used on RuntimeClass to protect it from being constructed with new
class DontUseNewUseMake
{
private:
    void* operator new(size_t) throw()
    {
        __WRL_ASSERT__(false);
        return 0;
    }

public:
    void* operator new(size_t, _In_ void* placement) throw()
    {
        return placement;
    }
};

// RuntimeClassBase is used for detection of RuntimeClass in Make method
class RuntimeClassBase
{
};

// RuntimeClassBaseT provides helper methods for QI and getting IIDs
template <unsigned int RuntimeClassTypeT>
class RuntimeClassBaseT : private RuntimeClassBase
{
protected:
    template<typename T>
    static HRESULT AsIID(_In_ T* implements, REFIID riid, _Outptr_result_nullonfailure_ void **ppvObject) throw()
    {
        *ppvObject = nullptr;
#pragma warning(push)
// Conditional expression is constant
#pragma warning(disable: 4127)
// Potential comparison of a constant with another constant
#pragma warning(disable: 6326)
// Conditional check using template parameter is constant and can be used to optimize the code
        bool isRefDelegated = false;
        // Prefer InlineIsEqualGUID over other forms since it's better perf on 4-byte aligned data, which is almost always the case.
        if (InlineIsEqualGUID(riid, __uuidof(IUnknown)) || ((RuntimeClassTypeT & WinRt) != 0 && InlineIsEqualGUID(riid, __uuidof(IInspectable))))
#pragma warning(pop)
        {
            *ppvObject = implements->CastToUnknown();
            static_cast<IUnknown*>(*ppvObject)->AddRef();
            return S_OK;
        }

        HRESULT hr = implements->CanCastTo(riid, ppvObject, &isRefDelegated);
        if (SUCCEEDED(hr) && !isRefDelegated)
        {
            static_cast<IUnknown*>(*ppvObject)->AddRef();
        }

#pragma warning(suppress: 6102) // '*ppvObject' is used but may not be initialized
        _Analysis_assume_(SUCCEEDED(hr) || (*ppvObject == nullptr));

        return hr;
    }
    template<typename T>
    static HRESULT GetImplementedIIDS(
        _In_ T* implements,
        _Out_ ULONG *iidCount,
        _When_(*iidCount == 0, _At_(*iids, _Post_null_))
        _When_(*iidCount > 0, _At_(*iids, _Post_notnull_))
        _Result_nullonfailure_ IID **iids) throw()
    {
        *iids = nullptr;
        *iidCount = 0;
        unsigned long count = implements->GetIidCount();

        // If there is no iids the CoTaskMemAlloc don't have to be called
        if (count == 0)
        {
            return S_OK;
        }

        IID* iidArray = reinterpret_cast<IID*>(::CoTaskMemAlloc(sizeof(IID) * count));
        if (iidArray == nullptr)
        {
            return E_OUTOFMEMORY;
        }

        unsigned long index = 0;

        // assign the IIDs to the array
        implements->FillArrayWithIid(&index, iidArray);
        __WRL_ASSERT__(index == count);

        // and return it
        *iidCount = count;
        *iids = iidArray;
        return S_OK;
    }

public:
    HRESULT RuntimeClassInitialize() throw()
    {
        return S_OK;
    }
};

// Base class required to mark FtmBase
class FtmBaseMarker
{
};

// Verifies that I is derived from specified base
template <unsigned int type, typename I, bool doStrictCheck = true, bool isImplementsBased = __is_base_of(ImplementsBase, I)>
struct VerifyInterfaceHelper;

// Specialization for ClassicCom interface
template <typename I, bool doStrictCheck>
struct VerifyInterfaceHelper<ClassicCom, I, doStrictCheck, false>
{
    static void Verify() throw()
    {
#ifdef __WRL_STRICT__
        // Make sure that your interfaces inherit from IUnknown and are not IUnknown and/or IInspectable based
        // The IUnknown is allowed only on RuntimeClass as first template parameter
        static_assert(__is_base_of(IUnknown, I) && !__is_base_of(IInspectable, I) && !(doStrictCheck && IsSame<IUnknown, I>::value),
            "'I' has to derive from 'IUnknown' and not from 'IInspectable'. 'I' must not be IUnknown.");
#else
        static_assert(__is_base_of(IUnknown, I), "'I' has to derive from 'IUnknown'.");
#endif
    }
};

// Specialization for WinRtClassicComMix interface
template <typename I, bool doStrictCheck>
struct VerifyInterfaceHelper<WinRtClassicComMix, I, doStrictCheck, false>
{
    static void Verify() throw()
    {
#ifdef __WRL_STRICT__
        // Make sure that your interfaces inherit from IUnknown and are not IUnknown and/or IInspectable
        // except when IInspectable is the first template parameter
        static_assert(__is_base_of(IUnknown, I) &&
            (doStrictCheck ? !(IsSame<IInspectable, I>::value || IsSame<IUnknown, I>::value) : __is_base_of(IInspectable, I)),
                "'I' has to derive from 'IUnknown' and must not be IUnknown and/or IInspectable.");
#else
        static_assert(__is_base_of(IUnknown, I), "'I' has to derive from 'IUnknown'.");
#endif
    }
};

// Specialization for WinRt interface
template <typename I, bool doStrictCheck>
struct VerifyInterfaceHelper<WinRt, I, doStrictCheck, false>
{
    static void Verify() throw()
    {
#ifdef __WRL_STRICT__
        // IWeakReferenceSource is exception for WinRt and can be used however it cannot be first templated interface
        // Make sure that your interfaces inherit from IInspectable and are not IInspectable
        // The IInspectable is allowed only on RuntimeClass as first template parameter
        static_assert((__is_base_of(IWeakReferenceSource, I) && doStrictCheck) ||
            (__is_base_of(IInspectable, I) && !(doStrictCheck && IsSame<IInspectable, I>::value)),
                "'I' has to derive from 'IWeakReferenceSource' or 'IInspectable' and must not be IInspectable");
#else
        // IWeakReference and IWeakReferneceSource are exceptions for WinRT
        static_assert(__is_base_of(IWeakReference, I) ||
                        __is_base_of(IWeakReferenceSource, I) ||
                            __is_base_of(IInspectable, I), "'I' has to derive from 'IWeakReference', 'IWeakReferenceSource' or 'IInspectable'");
#endif
    }
};

// Specialization for Implements passed as template parameter
template <unsigned int type, typename I>
struct VerifyInterfaceHelper<type, I, true, true>
{
    static void Verify() throw()
    {
#ifdef __WRL_STRICT__
        // Verifies if Implements has correct RuntimeClassFlags setting
        // Allow using FtmBase on classes configured with RuntimeClassFlags<WinRt> (Default configuration)
        static_assert(I::ClassFlags::value == type ||
                type == WinRtClassicComMix ||
                    __is_base_of(::Microsoft::WRL::Details::FtmBaseMarker, I),
            "Implements class must have the same and/or compatibile flags configuration");
#endif
    }
};

// Specialization for Implements passed as first template parameter
template <unsigned int type, typename I>
struct VerifyInterfaceHelper<type, I, false, true>
{
    static void Verify() throw()
    {
#ifdef __WRL_STRICT__
        // Verifies if Implements has correct RuntimeClassFlags setting
        static_assert(I::ClassFlags::value == type || type == WinRtClassicComMix,
            "Implements class must have the same and/or compatible flags configuration."
                "If you use WRL::FtmBase it cannot be specified as first template parameter on RuntimeClass");

        // Besides make sure that the first interface on Implements meet flags requirement
        VerifyInterfaceHelper<type, I::FirstInterface, false>::Verify();
#endif
    }
};

// Interface traits provides casting and filling iids methods helpers
template<typename I0>
struct __declspec(novtable) InterfaceTraits
{
    typedef I0 Base;
    static const unsigned long IidCount = 1;

    template<unsigned int ClassType>
    static void Verify() throw()
    {
        VerifyInterfaceHelper<ClassType & WinRtClassicComMix, Base>::Verify();
    }

    template<typename T>
    static Base* CastToBase(_In_ T* ptr) throw()
    {
        return static_cast<Base*>(ptr);
    }

    template<typename T>
    static IUnknown* CastToUnknown(_In_ T* ptr) throw()
    {
        return static_cast<IUnknown*>(static_cast<Base*>(ptr));
    }

    template <typename T>
    _Success_(return == true)
    static bool CanCastTo(_In_ T* ptr, REFIID riid, _Outptr_ void **ppv) throw()
    {
        // Prefer InlineIsEqualGUID over other forms since it's better perf on 4-byte aligned data, which is almost always the case.
        if (InlineIsEqualGUID(riid, __uuidof(Base)))
        {
            *ppv = static_cast<Base*>(ptr);
            return true;
        }

        return false;
    }

    static void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        *(iids + *index) = __uuidof(Base);
        (*index)++;
    }
};

// Specialization of traits for cloaked interface
template<typename CloakedType>
struct __declspec(novtable) InterfaceTraits<CloakedIid<CloakedType>>
{
    typedef CloakedType Base;
    static const unsigned long IidCount = 0;

    template<unsigned int ClassType>
    static void Verify() throw()
    {
        VerifyInterfaceHelper<ClassType & WinRtClassicComMix, Base>::Verify();
    }

    template<typename T>
    static Base* CastToBase(_In_ T* ptr) throw()
    {
        return static_cast<Base*>(ptr);
    }

    template<typename T>
    static IUnknown* CastToUnknown(_In_ T* ptr) throw()
    {
        return static_cast<IUnknown*>(static_cast<Base*>(ptr));
    }

    template <typename T>
    _Success_(return == true)
    static bool CanCastTo(_In_ T* ptr, REFIID riid, _Outptr_ void **ppv) throw()
    {
        // Prefer InlineIsEqualGUID over other forms since it's better perf on 4-byte aligned data, which is almost always the case.
        if (InlineIsEqualGUID(riid, __uuidof(Base)))
        {
            *ppv = static_cast<Base*>(ptr);
            return true;
        }

        return false;
    }

    // Cloaked specialization makes it always IID list empty
    static void FillArrayWithIid(_Inout_ unsigned long*, _Inout_ IID*) throw()
    {
    }
};

// Specialization for Nil parameter
template<>
struct __declspec(novtable) InterfaceTraits<Nil>
{
    typedef Nil Base;
    static const unsigned long IidCount = 0;

    template<unsigned int ClassType>
    static void Verify() throw()
    {
    }

    static void FillArrayWithIid(_Inout_ unsigned long *, _Inout_ IID*) throw()
    {
    }

    template <typename T>
    _Success_(return == true)
    static bool CanCastTo(_In_ T*, REFIID, _Outptr_ void **) throw()
    {
        return false;
    }
};

// Verify inheritance
template <typename I, typename Base>
struct VerifyInheritanceHelper
{
    static void Verify() throw()
    {
        static_assert(Details::IsBaseOfStrict<typename InterfaceTraits<Base>::Base, typename InterfaceTraits<I>::Base>::value, "'I' needs to inherit from 'Base'.");
    }
};

template <typename I>
struct VerifyInheritanceHelper<I, Nil>
{
    static void Verify() throw()
    {
    }
};

#pragma endregion //  helper types

} // namespace Details

// note: Due to potential shutdown ordering issues, the results of GetModuleBase
// should always be checked for null on reference counting and cleanup operations.
inline Details::ModuleBase* GetModuleBase() throw()
{
    return Details::ModuleBase::module_;
}

// ChainInterfaces - template allows specifying a derived COM interface along with its class hierarchy to allow QI for the base interfaces
template <typename I0, typename I1, typename I2 = Details::Nil, typename I3 = Details::Nil,
        typename I4 = Details::Nil, typename I5 = Details::Nil, typename I6 = Details::Nil,
        typename I7 = Details::Nil, typename I8 = Details::Nil, typename I9 = Details::Nil>
struct ChainInterfaces : I0
{
protected:
    template<unsigned int ClassType>
    static void Verify() throw()
    {
        Details::InterfaceTraits<I0>::template Verify<ClassType>();
        Details::InterfaceTraits<I1>::template Verify<ClassType>();
        Details::InterfaceTraits<I2>::template Verify<ClassType>();
        Details::InterfaceTraits<I3>::template Verify<ClassType>();
        Details::InterfaceTraits<I4>::template Verify<ClassType>();
        Details::InterfaceTraits<I5>::template Verify<ClassType>();
        Details::InterfaceTraits<I6>::template Verify<ClassType>();
        Details::InterfaceTraits<I7>::template Verify<ClassType>();
        Details::InterfaceTraits<I8>::template Verify<ClassType>();
        Details::InterfaceTraits<I9>::template Verify<ClassType>();

        Details::VerifyInheritanceHelper<I0, I1>::Verify();
        Details::VerifyInheritanceHelper<I0, I2>::Verify();
        Details::VerifyInheritanceHelper<I0, I3>::Verify();
        Details::VerifyInheritanceHelper<I0, I4>::Verify();
        Details::VerifyInheritanceHelper<I0, I5>::Verify();
        Details::VerifyInheritanceHelper<I0, I6>::Verify();
        Details::VerifyInheritanceHelper<I0, I7>::Verify();
        Details::VerifyInheritanceHelper<I0, I8>::Verify();
        Details::VerifyInheritanceHelper<I0, I9>::Verify();
    }

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv) throw()
    {
        typename Details::InterfaceTraits<I0>::Base* ptr = Details::InterfaceTraits<I0>::CastToBase(this);

        return (Details::InterfaceTraits<I0>::CanCastTo(this, riid, ppv) ||
            Details::InterfaceTraits<I1>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I2>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I3>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I4>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I5>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I6>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I7>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I8>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I9>::CanCastTo(ptr, riid, ppv)) ? S_OK : E_NOINTERFACE;
    }

    IUnknown* CastToUnknown() throw()
    {
        return Details::InterfaceTraits<I0>::CastToUnknown(this);
    }

    static const unsigned long IidCount =
        Details::InterfaceTraits<I0>::IidCount +
        Details::InterfaceTraits<I1>::IidCount +
        Details::InterfaceTraits<I2>::IidCount +
        Details::InterfaceTraits<I3>::IidCount +
        Details::InterfaceTraits<I4>::IidCount +
        Details::InterfaceTraits<I5>::IidCount +
        Details::InterfaceTraits<I6>::IidCount +
        Details::InterfaceTraits<I7>::IidCount +
        Details::InterfaceTraits<I8>::IidCount +
        Details::InterfaceTraits<I9>::IidCount;

    static void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        Details::InterfaceTraits<I0>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I1>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I2>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I3>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I4>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I5>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I6>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I7>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I8>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I9>::FillArrayWithIid(index, iids);
    }
};

template <typename DerivedType, typename BaseType, bool hasImplements, typename I1, typename I2, typename I3,
        typename I4, typename I5, typename I6,
        typename I7, typename I8, typename I9>
struct ChainInterfaces<MixIn<DerivedType, BaseType, hasImplements>, I1, I2, I3, I4, I5, I6, I7, I8, I9>
{
    static_assert(!hasImplements, "Cannot use ChainInterfaces<MixIn<...>> to Mix a class implementing interfaces using \"Implements\"");

protected:
    template<unsigned int ClassType>
    static void Verify() throw()
    {
        Details::InterfaceTraits<BaseType>::template Verify<ClassType>();
        Details::InterfaceTraits<I1>::template Verify<ClassType>();
        Details::InterfaceTraits<I2>::template Verify<ClassType>();
        Details::InterfaceTraits<I3>::template Verify<ClassType>();
        Details::InterfaceTraits<I4>::template Verify<ClassType>();
        Details::InterfaceTraits<I5>::template Verify<ClassType>();
        Details::InterfaceTraits<I6>::template Verify<ClassType>();
        Details::InterfaceTraits<I7>::template Verify<ClassType>();
        Details::InterfaceTraits<I8>::template Verify<ClassType>();
        Details::InterfaceTraits<I9>::template Verify<ClassType>();

        Details::VerifyInheritanceHelper<BaseType, I1>::Verify();
        Details::VerifyInheritanceHelper<BaseType, I2>::Verify();
        Details::VerifyInheritanceHelper<BaseType, I3>::Verify();
        Details::VerifyInheritanceHelper<BaseType, I4>::Verify();
        Details::VerifyInheritanceHelper<BaseType, I5>::Verify();
        Details::VerifyInheritanceHelper<BaseType, I6>::Verify();
        Details::VerifyInheritanceHelper<BaseType, I7>::Verify();
        Details::VerifyInheritanceHelper<BaseType, I8>::Verify();
        Details::VerifyInheritanceHelper<BaseType, I9>::Verify();
    }

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv) throw()
    {
        BaseType* ptr = static_cast<BaseType*>(static_cast<DerivedType*>(this));

        return (
            Details::InterfaceTraits<I1>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I2>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I3>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I4>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I5>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I6>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I7>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I8>::CanCastTo(ptr, riid, ppv) ||
            Details::InterfaceTraits<I9>::CanCastTo(ptr, riid, ppv)) ? S_OK : E_NOINTERFACE;
    }

    // It's not possible to cast to IUnknown when Base interface inherit more interfaces
    // The RuntimeClass is taking always the first interface as IUnknown thus it's required to
    // list IInspectable or IUnknown class before MixIn<Derived, MixInType> parameter, such as:
    // struct MyRuntimeClass : RuntimeClass<IInspectable, ChainInterfaces<MixIn<MyRuntimeClass,MyIndependentImplementation>, IFoo, IBar>, MyIndependentImplementation  {}
    IUnknown* CastToUnknown() throw() = delete;

    static const unsigned long IidCount =
        Details::InterfaceTraits<I1>::IidCount +
        Details::InterfaceTraits<I2>::IidCount +
        Details::InterfaceTraits<I3>::IidCount +
        Details::InterfaceTraits<I4>::IidCount +
        Details::InterfaceTraits<I5>::IidCount +
        Details::InterfaceTraits<I6>::IidCount +
        Details::InterfaceTraits<I7>::IidCount +
        Details::InterfaceTraits<I8>::IidCount +
        Details::InterfaceTraits<I9>::IidCount;

    static void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        Details::InterfaceTraits<I1>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I2>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I3>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I4>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I5>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I6>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I7>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I8>::FillArrayWithIid(index, iids);
        Details::InterfaceTraits<I9>::FillArrayWithIid(index, iids);
    }
};

namespace Details
{

#pragma region Implements helper templates

// Helper template used by Implements. This template traverses a list of interfaces and adds them as base class and information
// to enable QI. doStrictCheck is typically false only for the first interface, allowing IInspectable to be explicitly specified
// only as the first interface.
template <typename RuntimeClassFlagsT, bool doStrictCheck, typename ...TInterfaces>
struct __declspec(novtable) ImplementsHelper;

template <typename T>
struct __declspec(novtable) ImplementsMarker
{};

template <typename I0, bool isImplements>
struct __declspec(novtable) MarkImplements;

template <typename I0>
struct __declspec(novtable) MarkImplements<I0, false>
{
    typedef I0 Type;
};

template <typename I0>
struct __declspec(novtable) MarkImplements<I0, true>
{
    typedef ImplementsMarker<I0> Type;
};

template <typename I0>
struct __declspec(novtable) MarkImplements<CloakedIid<I0>, true>
{
    // Cloaked Implements type will be handled in the nested processing.
    // Applying the ImplementsMarker too early will bypass Cloaked behavior.
    typedef CloakedIid<I0> Type;
};

template <typename DerivedType, typename BaseType, bool hasImplements>
struct __declspec(novtable) MarkImplements<MixIn<DerivedType, BaseType, hasImplements>, true>
{
    // Implements type in mix-ins will be handled in the nested processing.
    typedef MixIn<DerivedType, BaseType, hasImplements> Type;
};

// AdjustImplements pre-processes the type list for more efficient builds.
template <typename RuntimeClassFlagsT, bool doStrictCheck, typename ...Bases>
struct __declspec(novtable) AdjustImplements;

template <typename RuntimeClassFlagsT, bool doStrictCheck, typename I0, typename ...Bases>
struct __declspec(novtable) AdjustImplements<RuntimeClassFlagsT, doStrictCheck, I0, Bases...>
{
    typedef ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, typename MarkImplements<I0, __is_base_of(ImplementsBase, I0)>::Type, Bases...> Type;
};

// Use AdjustImplements to remove instances of "Details::Nil" from the type list.
template <typename RuntimeClassFlagsT, bool doStrictCheck, typename ...Bases>
struct __declspec(novtable) AdjustImplements<RuntimeClassFlagsT, doStrictCheck, typename Details::Nil, Bases...>
{
    typedef typename AdjustImplements<RuntimeClassFlagsT, doStrictCheck, Bases...>::Type Type;
};


template <typename RuntimeClassFlagsT, bool doStrictCheck>
struct __declspec(novtable) AdjustImplements<RuntimeClassFlagsT, doStrictCheck>
{
    typedef ImplementsHelper<RuntimeClassFlagsT, doStrictCheck> Type;
};


// Specialization handles unadorned interfaces
template <typename RuntimeClassFlagsT, bool doStrictCheck, typename I0, typename ...TInterfaces>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, I0, TInterfaces...> :
    I0,
    AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type
{
    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TOtherInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class RuntimeClassBaseT;

protected:

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv, bool *pRefDelegated = nullptr) throw()
    {
        VerifyInterfaceHelper<RuntimeClassFlagsT::value & WinRtClassicComMix, I0, doStrictCheck>::Verify();
        // Prefer InlineIsEqualGUID over other forms since it's better perf on 4-byte aligned data, which is almost always the case.
        if (InlineIsEqualGUID(riid, __uuidof(I0)))
        {
            *ppv = reinterpret_cast<I0*>(reinterpret_cast<void*>(this));
            return S_OK;
        }
        return AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type::CanCastTo(riid, ppv, pRefDelegated);
    }

    IUnknown* CastToUnknown() throw()
    {
        return reinterpret_cast<I0*>(reinterpret_cast<void*>(this));
    }

    unsigned long GetIidCount() throw()
    {
        return 1 + AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type::GetIidCount();
    }

    // FillArrayWithIid
    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        *(iids + *index) = __uuidof(I0);
        (*index)++;
        AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type::FillArrayWithIid(index, iids);
    }
};


// Selector is used to "tag" base interfaces to be used in casting, since a runtime class may indirectly derive from
// the same interface or Implements<> template multiple times
template <typename base, typename disciminator>
struct  __declspec(novtable) Selector : public base
{
};

// Specialization handles types that derive from ImplementsHelper (e.g. nested Implements).
template <typename RuntimeClassFlagsT, bool doStrictCheck, typename I0, typename ...TInterfaces>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, ImplementsMarker<I0>, TInterfaces...> :
    Selector<I0, ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, ImplementsMarker<I0>, TInterfaces...>>,
    Selector<typename AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type, ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, ImplementsMarker<I0>, TInterfaces...>>
{
    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TOtherInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class RuntimeClassBaseT;

protected:
    typedef Selector<I0, ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, ImplementsMarker<I0>, TInterfaces...>> CurrentType;
    typedef Selector<typename AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type, ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, ImplementsMarker<I0>, TInterfaces...>> BaseType;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv, bool *pRefDelegated = nullptr) throw()
    {
        VerifyInterfaceHelper<RuntimeClassFlagsT::value & WinRtClassicComMix, I0, doStrictCheck>::Verify();
        HRESULT hr = CurrentType::CanCastTo(riid, ppv);
        if (hr == E_NOINTERFACE)
        {
            hr = BaseType::CanCastTo(riid, ppv, pRefDelegated);
        }
        return hr;
    }

    IUnknown* CastToUnknown() throw()
    {
        // First in list wins.
        return CurrentType::CastToUnknown();
    }

    unsigned long GetIidCount() throw()
    {
        return CurrentType::GetIidCount() + BaseType::GetIidCount();
    }

    // FillArrayWithIid
    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        CurrentType::FillArrayWithIid(index, iids);
        BaseType::FillArrayWithIid(index, iids);
    }
};

// CloakedIid instance. Since the first "real" interface should be checked against doStrictCheck,
// pass this through unchanged. Two specializations for cloaked prevent the need to use the Selector
// used in the Implements<> case. The same can't be done there because some type ambiguities are unavoidable.
template <typename RuntimeClassFlagsT, bool doStrictCheck, typename I0, typename I1, typename ...TInterfaces>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, CloakedIid<I0>, I1, TInterfaces...> :
    AdjustImplements<RuntimeClassFlagsT, doStrictCheck, I0>::Type,
    AdjustImplements<RuntimeClassFlagsT, true, I1, TInterfaces...>::Type
{
    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TOtherInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;

protected:

    typedef typename AdjustImplements<RuntimeClassFlagsT, doStrictCheck, I0>::Type CurrentType;
    typedef typename AdjustImplements<RuntimeClassFlagsT, true, I1, TInterfaces...>::Type BaseType;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv, bool *pRefDelegated = nullptr) throw()
    {
        VerifyInterfaceHelper<RuntimeClassFlagsT::value & WinRtClassicComMix, I0, doStrictCheck>::Verify();

        HRESULT hr = CurrentType::CanCastTo(riid, ppv, pRefDelegated);
        if (SUCCEEDED(hr))
        {
            return S_OK;
        }
        return BaseType::CanCastTo(riid, ppv, pRefDelegated);
    }

    IUnknown* CastToUnknown() throw()
    {
        return CurrentType::CastToUnknown();
    }

    // Don't expose the cloaked IID(s), but continue processing the rest of the interfaces
    unsigned long GetIidCount() throw()
    {
        return BaseType::GetIidCount();
    }

    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        BaseType::FillArrayWithIid(index, iids);
    }
};

template <typename RuntimeClassFlagsT, bool doStrictCheck, typename I0>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, CloakedIid<I0>> :
    AdjustImplements<RuntimeClassFlagsT, doStrictCheck, I0>::Type
{
    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;

protected:

    typedef typename AdjustImplements<RuntimeClassFlagsT, doStrictCheck, I0>::Type CurrentType;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv, bool *pRefDelegated = nullptr) throw()
    {
        VerifyInterfaceHelper<RuntimeClassFlagsT::value & WinRtClassicComMix, I0, doStrictCheck>::Verify();

        return CurrentType::CanCastTo(riid, ppv, pRefDelegated);
    }

    IUnknown* CastToUnknown() throw()
    {
        return CurrentType::CastToUnknown();
    }

    // Don't expose the cloaked IID(s), but continue processing the rest of the interfaces
    unsigned long GetIidCount() throw()
    {
        return 0;
    }

    void FillArrayWithIid(_Inout_ unsigned long * /*index*/, _Inout_ IID* /*iids*/) throw()
    {
        // no-op
    }
};


// terminal case specialization.
template <typename RuntimeClassFlagsT, bool doStrictCheck>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck>
{
    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class RuntimeClassBaseT;

protected:
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;

    HRESULT CanCastTo(_In_ REFIID /*riid*/, _Outptr_ void ** /*ppv*/, bool * /*pRefDelegated*/ = nullptr) throw()
    {
        return E_NOINTERFACE;
    }

    // IUnknown* CastToUnknown() throw(); // not defined for terminal case.

    unsigned long GetIidCount() throw()
    {
        return 0;
    }

    void FillArrayWithIid(_Inout_ unsigned long * /*index*/, _Inout_ IID* /*iids*/) throw()
    {
    }
};

// Specialization handles chaining interfaces
template <typename RuntimeClassFlagsT, bool doStrictCheck, typename C0, typename C1, typename C2, typename C3, typename C4, typename C5, typename C6, typename C7, typename C8, typename C9, typename ...TInterfaces>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, ChainInterfaces<C0, C1, C2, C3, C4, C5, C6, C7, C8, C9>, TInterfaces...> :
    ChainInterfaces<C0, C1, C2, C3, C4, C5, C6, C7, C8, C9>,
    AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type
{
    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TOtherInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class RuntimeClassBaseT;

protected:
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;
    typedef typename AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type BaseType;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv, bool *pRefDelegated = nullptr) throw()
    {
        ChainInterfaces<C0, C1, C2, C3, C4, C5, C6, C7, C8, C9>::template Verify<RuntimeClassFlagsT::value>();

        HRESULT hr = ChainInterfaces<C0, C1, C2, C3, C4, C5, C6, C7, C8, C9>::CanCastTo(riid, ppv);
        if (FAILED(hr))
        {
            hr = BaseType::CanCastTo(riid, ppv, pRefDelegated);
        }

        return hr;
    }

    IUnknown* CastToUnknown() throw()
    {
        return ChainInterfaces<C0, C1, C2, C3, C4, C5, C6, C7, C8, C9>::CastToUnknown();
    }

    unsigned long GetIidCount() throw()
    {
        return ChainInterfaces<C0, C1, C2, C3, C4, C5, C6, C7, C8, C9>::IidCount + BaseType::GetIidCount();
    }

    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        ChainInterfaces<C0, C1, C2, C3, C4, C5, C6, C7, C8, C9>::FillArrayWithIid(index, iids);
        BaseType::FillArrayWithIid(index, iids);
    }
};


// Mixin specialization
template <typename RuntimeClassFlagsT, typename DerivedType, typename BaseMixInType, bool hasImplements, typename ...TInterfaces, bool doStrictCheck>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, MixIn<DerivedType, BaseMixInType, hasImplements>, TInterfaces...> :
    AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type
{
    static_assert(hasImplements, "Cannot use MixIn to with a class not deriving from \"Implements\"");

    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TOtherInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class RuntimeClassBaseT;

protected:
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;
    typedef typename AdjustImplements<RuntimeClassFlagsT, true, TInterfaces...>::Type BaseType;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv, bool *pRefDelegated = nullptr) throw()
    {
        VerifyInterfaceHelper<RuntimeClassFlagsT::value & WinRtClassicComMix, BaseMixInType, doStrictCheck>::Verify();

        HRESULT hr = static_cast<BaseMixInType*>(static_cast<DerivedType*>(this))->CanCastTo(riid, ppv);
        if (FAILED(hr))
        {
            hr = BaseType::CanCastTo(riid, ppv, pRefDelegated);
        }

        return hr;
    }

    IUnknown* CastToUnknown() throw()
    {
        return static_cast<BaseMixInType*>(static_cast<DerivedType*>(this))->CastToUnknown();
    }

    unsigned long GetIidCount() throw()
    {
        return static_cast<BaseMixInType*>(static_cast<DerivedType*>(this))->GetIidCount() +
            BaseType::GetIidCount();
    }

    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        static_cast<BaseMixInType*>(static_cast<DerivedType*>(this))->FillArrayWithIid(index, iids);
        BaseType::FillArrayWithIid(index, iids);
    }
};

// Specialization handles inheriting COM objects. ComposableBase must be the last non-nil interface in the list.
// Trailing nil's are allowed for compatibility with some tools that pad out the list.
template <typename I0, typename ...>
struct AreAllNil
{
    static const bool value = false;
};

template <typename ...TInterfaces>
struct AreAllNil<Microsoft::WRL::Details::Nil, TInterfaces...>
{
    static const bool value = AreAllNil<TInterfaces...>::value;
};

template <>
struct AreAllNil<Microsoft::WRL::Details::Nil>
{
    static const bool value = true;
};

template <typename RuntimeClassFlagsT, typename FactoryInterface, bool doStrictCheck, typename ...TInterfaces>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, ComposableBase<FactoryInterface>, TInterfaces...> :
    ImplementsHelper<RuntimeClassFlagsT, true, ComposableBase<FactoryInterface>>
{
    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TOtherInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class RuntimeClassBaseT;

protected:
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;

    typedef ImplementsHelper<RuntimeClassFlagsT, true, ComposableBase<FactoryInterface>> BaseType;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv, bool *pRefDelegated = nullptr) throw()
    {
        static_assert(AreAllNil<TInterfaces...>::value, "ComposableBase should be the last template parameter to RuntimeClass");
        return BaseType::CanCastTo(riid, ppv, pRefDelegated);
    }

    IUnknown* CastToUnknown() throw()
    {
        static_assert(AreAllNil<TInterfaces...>::value, "ComposableBase should be the last template parameter to RuntimeClass");
        return BaseType::CastToUnknown();
    }

    unsigned long GetIidCount() throw()
    {
        static_assert(AreAllNil<TInterfaces...>::value, "ComposableBase should be the last template parameter to RuntimeClass");
        return BaseType::GetIidCount();
    }

    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        static_assert(AreAllNil<TInterfaces...>::value, "ComposableBase should be the last template parameter to RuntimeClass");
        BaseType::FillArrayWithIid(index, iids);
    }
};

template <typename RuntimeClassFlagsT, typename FactoryInterface, bool doStrictCheck>
struct __declspec(novtable) ImplementsHelper<RuntimeClassFlagsT, doStrictCheck, ComposableBase<FactoryInterface>>
{
    template <typename OtherRuntimeClassFlagsT, bool OtherDoStrictCheck, typename ...TInterfaces> friend struct ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class RuntimeClassBaseT;

protected:
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv, bool *pRefDelegated) throw()
    {
        *pRefDelegated = true;
        return composableBase_.CopyTo(riid, ppv);
    }

    IUnknown* CastToUnknown() throw()
    {
        return nullptr;
    }

    unsigned long GetIidCount() throw()
    {
        return iidCount_;
    }

    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        for(unsigned long i = 0; i < iidCount_; i++)
        {
            *(iids + *index) = *(iidsCached_ + i);
            (*index)++;
        }
    }

    ImplementsHelper() throw() : iidsCached_(nullptr), iidCount_(0)
    {
    }

    ~ImplementsHelper() throw()
    {
        ::CoTaskMemFree(iidsCached_);
        iidsCached_ = nullptr;
        iidCount_ = 0;
    }

public:
    HRESULT SetComposableBasePointers(_In_ IInspectable* base, _In_opt_ FactoryInterface* baseFactory = nullptr) throw()
    {
        if (composableBase_ != nullptr)
        {
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
            ErrorHelper<RuntimeClassFlagsT::value & InhibitRoOriginateError>::OriginateError(E_UNEXPECTED, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
            return E_UNEXPECTED;
        }

        HRESULT hr = base->GetIids(&iidCount_, &iidsCached_);
        if (SUCCEEDED(hr))
        {
            composableBase_ = base;
            composableBaseFactory_ = baseFactory;
        }
        return hr;
    }

    ComPtr<IInspectable> GetComposableBase() throw()
    {
        return composableBase_;
    }

    ComPtr<FactoryInterface> GetComposableBaseFactory() throw()
    {
        return composableBaseFactory_;
    }

private:
    ComPtr<IInspectable> composableBase_;
    ComPtr<FactoryInterface> composableBaseFactory_;
    IID *iidsCached_;
    unsigned long iidCount_;
};

#pragma endregion // Implements helper templates

} // namespace Details

// Implements - template implementing QI using the information provided through its template parameters
// Each template parameter has to be one of the following:
// * COM Interface
// * A class that implements one or more COM interfaces
// * ChainInterfaces template
template <typename I0, typename ...TInterfaces>
struct __declspec(novtable) Implements :
    Details::AdjustImplements<RuntimeClassFlags<WinRt>, true, I0, TInterfaces...>::Type,
    Details::ImplementsBase
{
public:
    typedef RuntimeClassFlags<WinRt> ClassFlags;
    typedef I0 FirstInterface;
protected:
    typedef typename Details::AdjustImplements<RuntimeClassFlags<WinRt>, true, I0, TInterfaces...>::Type BaseType;
    template <typename RuntimeClassFlagsT, bool doStrictCheck, typename ...TOtherInterfaces> friend struct Details::ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv) throw()
    {
        return BaseType::CanCastTo(riid, ppv);
    }

    IUnknown* CastToUnknown() throw()
    {
        return BaseType::CastToUnknown();
    }

    unsigned long GetIidCount() throw()
    {
        return BaseType::GetIidCount();
    }

    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        BaseType::FillArrayWithIid(index, iids);
    }
};

template <int flags, typename I0, typename ...TInterfaces>
struct __declspec(novtable) Implements<RuntimeClassFlags<flags>, I0, TInterfaces...> :
    Details::AdjustImplements<RuntimeClassFlags<flags>, true, I0, TInterfaces...>::Type,
    Details::ImplementsBase
{
public:
    typedef RuntimeClassFlags<flags> ClassFlags;
    typedef I0 FirstInterface;
protected:

    typedef typename Details::AdjustImplements<RuntimeClassFlags<flags>, true, I0, TInterfaces...>::Type BaseType;
    template <typename RuntimeClassFlagsT, bool doStrictCheck, typename ...TOtherInterfaces> friend struct Details::ImplementsHelper;
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv) throw()
    {
        return BaseType::CanCastTo(riid, ppv);
    }

    IUnknown* CastToUnknown() throw()
    {
        return BaseType::CastToUnknown();
    }

    unsigned long GetIidCount() throw()
    {
        return BaseType::GetIidCount();
    }

    void FillArrayWithIid(_Inout_ unsigned long *index, _Inout_ IID* iids) throw()
    {
        BaseType::FillArrayWithIid(index, iids);
    }
};

class FtmBase :
    public Implements<
        ::Microsoft::WRL::RuntimeClassFlags<WinRtClassicComMix>,
        ::Microsoft::WRL::CloakedIid< ::IMarshal> >,
    // Inheriting from FtmBaseMarker allows using FtmBase on classes configured with RuntimeClassFlags<WinRt> (Default configuration)
    private ::Microsoft::WRL::Details::FtmBaseMarker
{
    // defining type 'Super' for other compilers since '__super' is a VC++-specific language extension
    using Super = Implements<
      ::Microsoft::WRL::RuntimeClassFlags<WinRtClassicComMix>,
      ::Microsoft::WRL::CloakedIid< ::IMarshal> >;
protected:
    template <typename RuntimeClassFlagsT, bool doStrictCheck, typename ...TInterfaces> friend struct Details::ImplementsHelper;

    HRESULT CanCastTo(REFIID riid, _Outptr_ void **ppv) throw()
    {
        // Prefer InlineIsEqualGUID over other forms since it's better perf on 4-byte aligned data, which is almost always the case.
        if (InlineIsEqualGUID(riid, __uuidof(::IAgileObject)))
        {

            *ppv = Super::CastToUnknown();
            return S_OK;
        }

        return Super::CanCastTo(riid, ppv);
    }

public:
    FtmBase() throw()
    {
        ComPtr<IUnknown> unknown;
        if (SUCCEEDED(::CoCreateFreeThreadedMarshaler(nullptr, &unknown)))
        {
            unknown.As(&marshaller_);
        }
    }

    // IMarshal Methods
#pragma warning(suppress: 6101) // PREFast cannot see through the smart-pointer invocation
    STDMETHOD(GetUnmarshalClass)(_In_ REFIID riid,
                                   _In_opt_ void *pv,
                                   _In_ DWORD dwDestContext,
                                   _Reserved_ void *pvDestContext,
                                   _In_ DWORD mshlflags,
                                   _Out_ CLSID *pCid) override
    {
        if (marshaller_)
        {
            return marshaller_->GetUnmarshalClass(riid, pv, dwDestContext, pvDestContext, mshlflags, pCid);
        }
        return E_OUTOFMEMORY;
    }

#pragma warning(suppress: 6101) // PREFast cannot see through the smart-pointer invocation
    STDMETHOD(GetMarshalSizeMax)(_In_ REFIID riid, _In_opt_ void *pv, _In_ DWORD dwDestContext,
                                   _Reserved_ void *pvDestContext, _In_ DWORD mshlflags, _Out_ DWORD *pSize) override
    {
        if (marshaller_)
        {
            return marshaller_->GetMarshalSizeMax(riid, pv, dwDestContext, pvDestContext, mshlflags, pSize);
        }
        return E_OUTOFMEMORY;
    }

    STDMETHOD(MarshalInterface)(_In_ IStream *pStm, _In_ REFIID riid, _In_opt_ void *pv, _In_ DWORD dwDestContext,
                                  _Reserved_ void *pvDestContext, _In_ DWORD mshlflags) override
    {
        if (marshaller_)
        {
            return marshaller_->MarshalInterface(pStm, riid, pv, dwDestContext, pvDestContext, mshlflags);
        }
        return E_OUTOFMEMORY;
    }

#pragma warning(suppress: 6101) // PREFast cannot see through the smart-pointer invocation
    STDMETHOD(UnmarshalInterface)(_In_ IStream *pStm, _In_ REFIID riid, _Outptr_ void **ppv) override
    {
        if (marshaller_)
        {
            return marshaller_->UnmarshalInterface(pStm, riid, ppv);
        }
        return E_OUTOFMEMORY;
    }

    STDMETHOD(ReleaseMarshalData)(_In_ IStream *pStm) override
    {
        if (marshaller_)
        {
            return marshaller_->ReleaseMarshalData(pStm);
        }
        return E_OUTOFMEMORY;
    }

    STDMETHOD(DisconnectObject)(_In_ DWORD dwReserved) override
    {
        if (marshaller_)
        {
            return marshaller_->DisconnectObject(dwReserved);
        }
        return E_OUTOFMEMORY;
    }

    static HRESULT CreateGlobalInterfaceTable(_Out_ IGlobalInterfaceTable **git) throw()
    {
        *git = nullptr;
        return ::CoCreateInstance(CLSID_StdGlobalInterfaceTable,
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(IGlobalInterfaceTable),
            reinterpret_cast<void**>(git));
    }

    ::Microsoft::WRL::ComPtr<IMarshal> marshaller_;  // Holds a reference to the free threaded marshaler
};

namespace Details
{

#ifdef _PERF_COUNTERS
class __declspec(novtable) PerfCountersBase
{
public:
    ULONG GetAddRefCount() throw()
    {
        return addRefCount_;
    }

    ULONG GetReleaseCount() throw()
    {
        return releaseCount_;
    }

    ULONG GetQueryInterfaceCount() throw()
    {
        return queryInterfaceCount_;
    }

    void ResetPerfCounters() throw()
    {
        addRefCount_ = 0;
        releaseCount_ = 0;
        queryInterfaceCount_ = 0;
    }

protected:
    PerfCountersBase() throw() :
        addRefCount_(0),
        releaseCount_(0),
        queryInterfaceCount_(0)
    {
    }

    void IncrementAddRefCount() throw()
    {
        InterlockedIncrement(&addRefCount_);
    }

    void IncrementReleaseCount() throw()
    {
        InterlockedIncrement(&releaseCount_);
    }

    void IncrementQueryInterfaceCount() throw()
    {
        InterlockedIncrement(&queryInterfaceCount_);
    }

private:
    volatile unsigned long addRefCount_;
    volatile unsigned long releaseCount_;
    volatile unsigned long queryInterfaceCount_;
};
#endif

#if defined(_X86_) || defined(_AMD64_)

#define UnknownIncrementReference InterlockedIncrement
#define UnknownDecrementReference InterlockedDecrement
#define UnknownBarrierAfterInterlock()
#define UnknownInterlockedCompareExchangePointer InterlockedCompareExchangePointer
#define UnknownInterlockedCompareExchangePointerForIncrement InterlockedCompareExchangePointer
#define UnknownInterlockedCompareExchangePointerForRelease InterlockedCompareExchangePointer
#define UnknownInterlockedCompareExchangeForIncrement InterlockedCompareExchange
#define UnknownInterlockedCompareExchangeForRelease InterlockedCompareExchange

#elif defined(_ARM_)

#define UnknownIncrementReference InterlockedIncrementNoFence
#define UnknownDecrementReference InterlockedDecrementRelease
#define UnknownBarrierAfterInterlock() __dmb(_ARM_BARRIER_ISH)
#define UnknownInterlockedCompareExchangePointer InterlockedCompareExchangePointer
#define UnknownInterlockedCompareExchangePointerForIncrement InterlockedCompareExchangePointerNoFence
#define UnknownInterlockedCompareExchangePointerForRelease InterlockedCompareExchangePointerRelease
#define UnknownInterlockedCompareExchangeForIncrement InterlockedCompareExchangeNoFence
#define UnknownInterlockedCompareExchangeForRelease InterlockedCompareExchangeRelease

#elif defined(_ARM64_)

#define UnknownIncrementReference InterlockedIncrementNoFence
#define UnknownDecrementReference InterlockedDecrementRelease
#define UnknownBarrierAfterInterlock() __dmb(_ARM64_BARRIER_ISH)
#define UnknownInterlockedCompareExchangePointer InterlockedCompareExchangePointer
#define UnknownInterlockedCompareExchangePointerForIncrement InterlockedCompareExchangePointerNoFence
#define UnknownInterlockedCompareExchangePointerForRelease InterlockedCompareExchangePointerRelease
#define UnknownInterlockedCompareExchangeForIncrement InterlockedCompareExchangeNoFence
#define UnknownInterlockedCompareExchangeForRelease InterlockedCompareExchangeRelease

#else

#error Unsupported architecture.

#endif

// Since variadic templates can't have a parameter pack after default arguments, provide a convenient helper for defaults.
#define DETAILS_RTCLASS_FLAGS_ARGUMENTS(RuntimeClassFlagsT) \
    RuntimeClassFlagsT, \
    (RuntimeClassFlagsT::value & InhibitWeakReference) == 0, \
    (RuntimeClassFlagsT::value & WinRt) == WinRt, \
    __WRL_IMPLEMENTS_FTM_BASE__(RuntimeClassFlagsT::value) \

template <class RuntimeClassFlagsT, bool implementsWeakReferenceSource, bool implementsInspectable, bool implementsFtmBase, typename ...TInterfaces>
class __declspec(novtable) RuntimeClassImpl;

#pragma warning(push)
// PREFast cannot see through template instantiation for AsIID()
#pragma warning(disable: 6388)

// Reference counting functions that check overflow. If overflow is detected, ref count value will stop at LONG_MAX, and the object being
// reference-counted will be leaked.
inline unsigned long SafeUnknownIncrementReference(long volatile &refcount) throw()
{
    long oldValue = refcount;
    while (oldValue != LONG_MAX && (UnknownInterlockedCompareExchangeForIncrement(&refcount, oldValue + 1, oldValue) != oldValue))
    {
        oldValue = refcount;
    }

    if (oldValue != LONG_MAX)
    {
        return oldValue + 1;
    }
    else
    {
        return LONG_MAX;
    }
}

inline unsigned long SafeUnknownDecrementReference(long volatile &refcount) throw()
{
    long oldValue = refcount;
    while (oldValue != LONG_MAX && (UnknownInterlockedCompareExchangeForRelease(&refcount, oldValue - 1, oldValue) != oldValue))
    {
        oldValue = refcount;
    }

    return oldValue - 1;
}

template <class RuntimeClassFlagsT, bool implementsWeakReferenceSource, bool implementsFtmBase, typename ...TInterfaces>
class __declspec(novtable) RuntimeClassImpl<RuntimeClassFlagsT, implementsWeakReferenceSource, false, implementsFtmBase, TInterfaces...> :
    public Details::AdjustImplements<RuntimeClassFlagsT, false, TInterfaces...>::Type,
    public RuntimeClassBaseT<RuntimeClassFlagsT::value>,
    protected RuntimeClassFlags<InhibitWeakReference>,
    public DontUseNewUseMake
#ifdef _PERF_COUNTERS
    , public PerfCountersBase
#endif
{
public:
    typedef RuntimeClassFlagsT ClassFlags;

    STDMETHOD(QueryInterface)(REFIID riid, _Outptr_result_nullonfailure_ void **ppvObject)
    {
#ifdef _PERF_COUNTERS
        IncrementQueryInterfaceCount();
#endif
        return Super::AsIID(this, riid, ppvObject);
    }

    STDMETHOD_(ULONG, AddRef)()
    {
        return InternalAddRef();
    }

    STDMETHOD_(ULONG, Release)()
    {
        ULONG ref = InternalRelease();
        if (ref == 0)
        {
            delete this;

            auto modulePtr = ::Microsoft::WRL::GetModuleBase();
            if (modulePtr != nullptr)
            {
                modulePtr->DecrementObjectCount();
            }
        }

        return ref;
    }

protected:
    using Super = RuntimeClassBaseT<RuntimeClassFlagsT::value>;

    RuntimeClassImpl() throw() : refcount_(1)
    {
    }

    virtual ~RuntimeClassImpl() throw()
    {
        // Set refcount_ to -(LONG_MAX/2) to protect destruction and
        // also catch mismatched Release in debug builds
        refcount_ = -(LONG_MAX/2);
    }

    unsigned long InternalAddRef() throw()
    {
#ifdef _PERF_COUNTERS
        IncrementAddRefCount();
#endif
        return SafeUnknownIncrementReference(refcount_);
    }

    unsigned long InternalRelease() throw()
    {
#ifdef _PERF_COUNTERS
        IncrementReleaseCount();
#endif
        // A release fence is required to ensure all guarded memory accesses are
        // complete before any thread can begin destroying the object.
        unsigned long newValue = SafeUnknownDecrementReference(refcount_);
        if (newValue == 0)
        {
            // An acquire fence is required before object destruction to ensure
            // that the destructor cannot observe values changing on other threads.
            UnknownBarrierAfterInterlock();
        }
        return newValue;
    }

    unsigned long GetRefCount() const throw()
    {
        return refcount_;
    }

    friend class WeakReferenceImpl;

private:
    volatile long refcount_;
};

template<typename I, bool isImplementsBased = __is_base_of(ImplementsBase, I)>
struct HasIInspectable;

template<typename I>
struct HasIInspectable<I, false>
{
    static const bool isIInspectable = __is_base_of(IInspectable, I);
};

template<typename I>
struct HasIInspectable<I, true>
{
    static const bool isIInspectable = HasIInspectable<typename I::FirstInterface>::isIInspectable;
};

#ifdef __WRL_STRICT__
template<typename I0, bool isIInspectable = true>
#else
template<typename I0, bool isIInspectable = HasIInspectable<I0>::isIInspectable>
#endif
struct IInspectableInjector;

template<typename I0>
struct IInspectableInjector<I0, true>
{
    typedef Details::Nil InspectableIfNeeded;
};

template<typename I0>
struct IInspectableInjector<I0, false>
{
    typedef IInspectable InspectableIfNeeded;
};

// Implements IInspectable in ILst
template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
class __declspec(novtable) RuntimeClassImpl<RuntimeClassFlagsT, false, true, false, I0, TInterfaces...> :
    public Details::AdjustImplements<RuntimeClassFlagsT, false, typename IInspectableInjector<I0>::InspectableIfNeeded, I0, TInterfaces...>::Type,
    public RuntimeClassBaseT<RuntimeClassFlagsT::value>,
    protected RuntimeClassFlags<InhibitWeakReference>,
    public DontUseNewUseMake
#ifdef _PERF_COUNTERS
    , public PerfCountersBase
#endif
{
public:
    typedef RuntimeClassFlagsT ClassFlags;

    STDMETHOD(QueryInterface)(REFIID riid, _Outptr_result_nullonfailure_ void **ppvObject)
    {
#ifdef _PERF_COUNTERS
        IncrementQueryInterfaceCount();
#endif
        return Super::AsIID(this, riid, ppvObject);
    }

    STDMETHOD_(ULONG, AddRef)()
    {
        return InternalAddRef();
    }

    STDMETHOD_(ULONG, Release)()
    {
        ULONG ref = InternalRelease();
        if (ref == 0)
        {
            delete this;

            auto modulePtr = ::Microsoft::WRL::GetModuleBase();
            if (modulePtr != nullptr)
            {
                modulePtr->DecrementObjectCount();
            }
        }

        return ref;
    }

    // IInspectable methods
    STDMETHOD(GetIids)(
        _Out_ ULONG *iidCount,
        _When_(*iidCount == 0, _At_(*iids, _Post_null_))
        _When_(*iidCount > 0, _At_(*iids, _Post_notnull_))
        _Result_nullonfailure_ IID **iids)
    {
        return Super::GetImplementedIIDS(this, iidCount, iids);
    }

#if !defined(__WRL_STRICT__) || !defined(__WRL_FORCE_INSPECTABLE_CLASS_MACRO__)
    STDMETHOD(GetRuntimeClassName)(_Out_ HSTRING* runtimeClassName)
    {
        *runtimeClassName = nullptr;

        __WRL_ASSERT__(false && "Use InspectableClass macro to set runtime class name and trust level.");

#if (NTDDI_VERSION >= NTDDI_WINBLUE)
        ErrorHelper<RuntimeClassFlagsT::value & InhibitRoOriginateError>::OriginateError(E_NOTIMPL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
        return E_NOTIMPL;
    }

    STDMETHOD(GetTrustLevel)(_Out_ ::TrustLevel*)
    {
        __WRL_ASSERT__(false && "Use InspectableClass macro to set runtime class name and trust level.");

#if (NTDDI_VERSION >= NTDDI_WINBLUE)
        ErrorHelper<RuntimeClassFlagsT::value & InhibitRoOriginateError>::OriginateError(E_NOTIMPL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
        return E_NOTIMPL;
    }
#endif // !defined(__WRL_STRICT__) || !defined(__WRL_FORCE_INSPECTABLE_CLASS_MACRO__)

protected:
    using Super = RuntimeClassBaseT<RuntimeClassFlagsT::value>;

    RuntimeClassImpl() throw() : refcount_(1)
    {
    }

    virtual ~RuntimeClassImpl() throw()
    {
        // Set refcount_ to -(LONG_MAX/2) to protect destruction and
        // also catch mismatched Release in debug builds
        refcount_ = -(LONG_MAX/2);
    }

    unsigned long InternalAddRef() throw()
    {
#ifdef _PERF_COUNTERS
        IncrementAddRefCount();
#endif
        return SafeUnknownIncrementReference(refcount_);
    }

    unsigned long InternalRelease() throw()
    {
#ifdef _PERF_COUNTERS
        IncrementReleaseCount();
#endif
        // A release fence is required to ensure all guarded memory accesses are
        // complete before any thread can begin destroying the object.
        unsigned long newValue = SafeUnknownDecrementReference(refcount_);
        if (newValue == 0)
        {
            // An acquire fence is required before object destruction to ensure
            // that the destructor cannot observe values changing on other threads.
            UnknownBarrierAfterInterlock();
        }
        return newValue;
    }

    unsigned long GetRefCount() const throw()
    {
        return refcount_;
    }
private:
    volatile long refcount_;
};

class StrongReference
{
public:
    StrongReference(long refCount = 1) throw() : strongRefCount_(refCount) {}

    ~StrongReference() throw()
    {
        // Set refcount_ to -(LONG_MAX/2) to protect destruction and
        // also catch mismatched Release in debug builds
        strongRefCount_ = -(LONG_MAX / 2);
    }

    unsigned long IncrementStrongReference() throw()
    {
        return SafeUnknownIncrementReference(strongRefCount_);
    }

    unsigned long DecrementStrongReference() throw()
    {
        // A release fence is required to ensure all guarded memory accesses are
        // complete before any thread can begin destroying the object.
        unsigned long newValue = SafeUnknownDecrementReference(strongRefCount_);
        if (newValue == 0)
        {
            // An acquire fence is required before object destruction to ensure
            // that the destructor cannot observe values changing on other threads.
            UnknownBarrierAfterInterlock();
        }
        return newValue;
    }

    unsigned long GetStrongReferenceCount() throw()
    {
        return strongRefCount_;
    }

    void SetStrongReference(unsigned long value) throw()
    {
        strongRefCount_ = value;
    }

    long strongRefCount_;
};

// To support storing, encoding and decoding reference-count/pointers regardless of the target platform.
// In a RuntimeClass, the refCount_ member can mean either
//    1. actual reference count
//    2. pointer to the weak reference object which holds the strong count
// The member
//    1. If it is a count, the most significant bit will be OFF
//    2. If it is an encoded pointer to the weak reference, the most significant bit will be turned ON
// To test which mode it is
//    1. Test for negative
//    2. If it is, it is an encoded pointer to the weak reference
//    3. If it is not, it is the actual reference count
// To yield the encoded pointer
//    1. Test the value for negative
//    2. If it is, shift the value to the left and cast it to a WeakReferenceImpl*
//
const UINT_PTR EncodeWeakReferencePointerFlag = static_cast<UINT_PTR>(1) << ((sizeof(UINT_PTR) * 8) - 1);

union ReferenceCountOrWeakReferencePointer
{
    // Represents the count when it is a count (in practice only the least significant 4 bytes)
    UINT_PTR refCount;
    // Pointer size, *signed* to help with ease of casting and such
    INT_PTR rawValue;
    // The hint that this could also be a pointer
    void* ifHighBitIsSetThenShiftLeftToYieldPointerToWeakReference;
};

// Helper methods to test, decode and decode the different representations of ReferenceCountOrWeakReferencePointer
inline bool IsValueAPointerToWeakReference(INT_PTR value)
{
    return value < 0;
}

// Forward declaration
class WeakReferenceImpl;
inline INT_PTR EncodeWeakReferencePointer(Microsoft::WRL::Details::WeakReferenceImpl* value);
inline Microsoft::WRL::Details::WeakReferenceImpl* DecodeWeakReferencePointer(INT_PTR value);

// Helper functions, originally from winnt.h, needed to get the semantics right when fetching values
// in multi-threaded scenarios. This is in order to guarantee the compiler emits exactly one read
// (compiler may decide to re-fetch the value later on, which in some cases can break code)
#if defined(_ARM_)

FORCEINLINE
LONG
ReadULongPtrNoFence (
    _In_ _Interlocked_operand_ DWORD const volatile *Source
    )

{
    LONG Value;

    Value = __iso_volatile_load32((int *)Source);
    return Value;
}

#elif defined(_ARM64_)

FORCEINLINE
LONG64
ReadULongPtrNoFence (
    _In_ _Interlocked_operand_ DWORD64 const volatile *Source
    )

{
    LONG64 Value;

    Value = __iso_volatile_load64((__int64 *)Source);
    return Value;
}

#elif defined(_X86_)

FORCEINLINE
LONG
ReadULongPtrNoFence (
    _In_ _Interlocked_operand_ DWORD const volatile *Source
    )

{
    LONG Value;

    Value = *Source;
    return Value;
}

#elif defined(_AMD64_)

FORCEINLINE
LONG64
ReadULongPtrNoFence (
    _In_ _Interlocked_operand_ DWORD64 const volatile *Source
    )

{
    LONG64 Value;

    Value = *Source;
    return Value;
}

#else

#error Unsupported architecture.

#endif

template <typename T>
inline T ReadValueFromPointerNoFence(_In_ const volatile T* value)
{
    ULONG_PTR currentValue = ReadULongPtrNoFence(reinterpret_cast<const volatile ULONG_PTR *>(value));
    const T* currentPointerToValue = reinterpret_cast<T*>(&currentValue);
    return *currentPointerToValue;
}

inline WeakReferenceImpl* CreateWeakReference(_In_ IUnknown*);

// Implementation of activatable class that implements IWeakReferenceSource
// and delegates reference counting to WeakReferenceImpl object
template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
class __declspec(novtable) RuntimeClassImpl<RuntimeClassFlagsT, true, true, false, I0, TInterfaces...> :
    public Details::AdjustImplements<RuntimeClassFlagsT, false, typename IInspectableInjector<I0>::InspectableIfNeeded, I0, IWeakReferenceSource, TInterfaces...>::Type,
    public RuntimeClassBaseT<RuntimeClassFlagsT::value>,
    public DontUseNewUseMake
#ifdef _PERF_COUNTERS
    , public PerfCountersBase
#endif
{
public:
    typedef RuntimeClassFlagsT ClassFlags;

    RuntimeClassImpl() throw()
    {
        refCount_.rawValue = 1;
    }

    STDMETHOD(QueryInterface)(REFIID riid, _Outptr_result_nullonfailure_ void **ppvObject)
    {
#ifdef _PERF_COUNTERS
        IncrementQueryInterfaceCount();
#endif
        return Super::AsIID(this, riid, ppvObject);
    }

    STDMETHOD_(ULONG, AddRef)()
    {
        return InternalAddRef();
    }

    STDMETHOD_(ULONG, Release)()
    {
        ULONG ref = InternalRelease();
        if (ref == 0)
        {
            delete this;

            auto modulePtr = ::Microsoft::WRL::GetModuleBase();
            if (modulePtr != nullptr)
            {
                modulePtr->DecrementObjectCount();
            }
        }

        return ref;
    }

    // IInspectable methods
    STDMETHOD(GetIids)(
        _Out_ ULONG *iidCount,
        _When_(*iidCount == 0, _At_(*iids, _Post_null_))
        _When_(*iidCount > 0, _At_(*iids, _Post_notnull_))
        _Result_nullonfailure_ IID **iids)
    {
        return Super::GetImplementedIIDS(this, iidCount, iids);
    }

#if !defined(__WRL_STRICT__) || !defined(__WRL_FORCE_INSPECTABLE_CLASS_MACRO__)
    STDMETHOD(GetRuntimeClassName)(_Out_ HSTRING* runtimeClassName)
    {
        *runtimeClassName = nullptr;

        __WRL_ASSERT__(false && "Use InspectableClass macro to set runtime class name and trust level.");

#if (NTDDI_VERSION >= NTDDI_WINBLUE)
        ErrorHelper<RuntimeClassFlagsT::value & InhibitRoOriginateError>::OriginateError(E_NOTIMPL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
        return E_NOTIMPL;
    }

    STDMETHOD(GetTrustLevel)(_Out_ ::TrustLevel*)
    {
        __WRL_ASSERT__(false && "Use InspectableClass macro to set runtime class name and trust level.");
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
        ErrorHelper<RuntimeClassFlagsT::value & InhibitRoOriginateError>::OriginateError(E_NOTIMPL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
        return E_NOTIMPL;
    }
#endif // !defined(__WRL_STRICT__) || !defined(__WRL_FORCE_INSPECTABLE_CLASS_MACRO__)

    STDMETHOD(GetWeakReference)(_Outptr_ IWeakReference **weakReference);

    virtual ~RuntimeClassImpl() throw();

protected:
    template <unsigned int RuntimeClassTypeT> friend class Details::RuntimeClassBaseT;
    using ImplementsHelper = typename Details::AdjustImplements<RuntimeClassFlagsT, false, typename IInspectableInjector<I0>::InspectableIfNeeded, I0, IWeakReferenceSource, TInterfaces...>::Type;
    using Super = RuntimeClassBaseT<RuntimeClassFlagsT::value>;

    unsigned long InternalAddRef() throw();

    unsigned long InternalRelease() throw();

    unsigned long GetRefCount() const throw();

    friend class WeakReferenceImpl;

#ifdef __WRL_UNITTEST__
protected:
#else
private:
#endif
    ReferenceCountOrWeakReferencePointer refCount_;
};

inline INT_PTR EncodeWeakReferencePointer(Microsoft::WRL::Details::WeakReferenceImpl* value)
{
    return ((reinterpret_cast<INT_PTR>(value) >> 1) | EncodeWeakReferencePointerFlag);
}

inline Microsoft::WRL::Details::WeakReferenceImpl* DecodeWeakReferencePointer(INT_PTR value)
{
    return reinterpret_cast<Microsoft::WRL::Details::WeakReferenceImpl*>(static_cast<UINT_PTR>(value) << 1);
}

#pragma warning(pop) // C6388

template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
class __declspec(novtable) RuntimeClassImpl<RuntimeClassFlagsT, false, true, true, I0, TInterfaces...> :
    public RuntimeClassImpl<RuntimeClassFlagsT, false, true, false, I0, TInterfaces...>
{
};

template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
class __declspec(novtable) RuntimeClassImpl<RuntimeClassFlagsT, true, true, true, I0, TInterfaces...> :
    public RuntimeClassImpl<RuntimeClassFlagsT, true, true, false, I0, FtmBase, TInterfaces...>
{
};

// To minimize breaks with code written against WRL before variadic support was added, this form is maintained.
template <typename ...TInterfaces>
struct InterfaceListHelper
{
    typedef InterfaceListHelper<TInterfaces...> TypeT;
};

template <
    typename ILst,
    class RuntimeClassFlagsT,
    bool implementsWeakReferenceSource = (RuntimeClassFlagsT::value & InhibitWeakReference) == 0,
    bool implementsInspectable = (RuntimeClassFlagsT::value & WinRt) == WinRt,
    bool implementsFtmBase = __WRL_IMPLEMENTS_FTM_BASE__(RuntimeClassFlagsT::value)
>
class RuntimeClass;


template <
    typename RuntimeClassFlagsT,
    bool implementsWeakReferenceSource,
    bool implementsInspectable,
    bool implementsFtmBase,
    typename ...TInterfaces
>
class RuntimeClass<InterfaceListHelper<TInterfaces...>, RuntimeClassFlagsT, implementsWeakReferenceSource, implementsInspectable, implementsFtmBase> :
    public RuntimeClassImpl<RuntimeClassFlagsT, implementsWeakReferenceSource, implementsInspectable, implementsFtmBase, TInterfaces...>
{
protected:
#pragma warning(suppress: 6101) // Function only used internally and the value of 'ppvObject' is only used if *handled is true
    HRESULT CustomQueryInterface(REFIID /*riid*/, _Outptr_result_nullonfailure_ void** /*ppvObject*/, _Out_ bool *handled)
    {
        *handled = false;
        return S_OK;
    }
};

} // namespace Details

// The RuntimeClass IUnknown methods
// It inherits from Details::RuntimeClass that provides helper methods for reference counting and
// collecting IIDs
template <typename ...TInterfaces>
class RuntimeClass :
    public Details::RuntimeClassImpl<DETAILS_RTCLASS_FLAGS_ARGUMENTS(RuntimeClassFlags<WinRt>), TInterfaces...>
{
    RuntimeClass(const RuntimeClass&);
    RuntimeClass& operator=(const RuntimeClass&);
protected:
#pragma warning(suppress: 6101) // Function only used internally and the value of 'ppvObject' is only used if *handled is true
    HRESULT CustomQueryInterface(REFIID /*riid*/, _Outptr_result_nullonfailure_ void** /*ppvObject*/, _Out_ bool *handled)
    {
        *handled = false;
        return S_OK;
    }
public:
    RuntimeClass() throw()
    {
        auto modulePtr = ::Microsoft::WRL::GetModuleBase();
        if (modulePtr != nullptr)
        {
            modulePtr->IncrementObjectCount();
        }
    }
    typedef RuntimeClass RuntimeClassT;
};

template <unsigned int classFlags, typename ...TInterfaces>
class RuntimeClass<RuntimeClassFlags<classFlags>, TInterfaces...> :
    public Details::RuntimeClassImpl<DETAILS_RTCLASS_FLAGS_ARGUMENTS(RuntimeClassFlags<classFlags>), TInterfaces...>
{
    RuntimeClass(const RuntimeClass&);
    RuntimeClass& operator=(const RuntimeClass&);
protected:
#pragma warning(suppress: 6101) // Function only used internally and the value of 'ppvObject' is only used if *handled is true
    HRESULT CustomQueryInterface(REFIID /*riid*/, _Outptr_result_nullonfailure_ void** /*ppvObject*/, _Out_ bool *handled)
    {
        *handled = false;
        return S_OK;
    }
public:
    RuntimeClass() throw()
    {
        auto modulePtr = ::Microsoft::WRL::GetModuleBase();
        if (modulePtr != nullptr)
        {
            modulePtr->IncrementObjectCount();
        }
    }
    typedef RuntimeClass RuntimeClassT;
};

namespace Details
{
    // Weak reference implementation
    class WeakReferenceImpl final :
        public ::Microsoft::WRL::RuntimeClass<RuntimeClassFlags<ClassicCom>, IWeakReference>,
        public StrongReference
    {
    public:
    WeakReferenceImpl(_In_ IUnknown* unk) throw() : StrongReference(LONG_MAX / 2), unknown_(unk)
    {
        // Set ref count to 2 to avoid unnecessary interlocked increment operation while returning
        // WeakReferenceImpl from GetWeakReference method. One reference is hold by the object the second is hold
        // by the caller of GetWeakReference method.
        refcount_ = 2;
    }

    virtual ~WeakReferenceImpl() throw()
    {
    }

    STDMETHOD(Resolve)(REFIID riid, _Outptr_result_maybenull_ _Result_nullonfailure_ IInspectable **ppvObject)
    {
        *ppvObject = nullptr;

        for(;;)
        {
            long ref = this->strongRefCount_;
            if (ref == 0)
            {
                return S_OK;
            }

            // InterlockedCompareExchange calls _InterlockedCompareExchange intrinsic thus we call directly _InterlockedCompareExchange to save the call
            if (::_InterlockedCompareExchange(&this->strongRefCount_, ref + 1, ref) == ref)
            {
#ifdef _PERF_COUNTERS
                // This artificially manipulates the strong ref count via AddRef to account for the resolve
                // interlocked operation above when tallying reference counting operations.
                unknown_->AddRef();
                ::_InterlockedDecrement(&this->strongRefCount_);
#endif
                break;
            }
        }

        HRESULT hr = unknown_->QueryInterface(riid, reinterpret_cast<void**>(ppvObject));
        unknown_->Release();
        return hr;
    }


private:
    IUnknown *unknown_;
};

template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
RuntimeClassImpl<RuntimeClassFlagsT, true, true, false, I0, TInterfaces...>::~RuntimeClassImpl() throw()
{
    if (IsValueAPointerToWeakReference(refCount_.rawValue))
    {
        WeakReferenceImpl* weakRef = DecodeWeakReferencePointer(refCount_.rawValue);
        weakRef->Release();
        weakRef = nullptr;
    }
}

template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
unsigned long RuntimeClassImpl<RuntimeClassFlagsT, true, true, false, I0, TInterfaces...>::GetRefCount() const throw()
{
    ReferenceCountOrWeakReferencePointer currentValue = ReadValueFromPointerNoFence<ReferenceCountOrWeakReferencePointer>(&refCount_);

    if (IsValueAPointerToWeakReference(currentValue.rawValue))
    {
        WeakReferenceImpl* weakRef = DecodeWeakReferencePointer(currentValue.rawValue);
        return weakRef->GetStrongReferenceCount();
    }
    else
    {
        return static_cast<unsigned long>(currentValue.refCount);
    }
}

template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
unsigned long RuntimeClassImpl<RuntimeClassFlagsT, true, true, false, I0, TInterfaces...>::InternalAddRef() throw()
{
#ifdef _PERF_COUNTERS
    IncrementAddRefCount();
#endif

    ReferenceCountOrWeakReferencePointer currentValue = ReadValueFromPointerNoFence<ReferenceCountOrWeakReferencePointer>(&refCount_);

    for (;;)
    {
        if (!IsValueAPointerToWeakReference(currentValue.rawValue))
        {
            if (static_cast<long>(currentValue.refCount) == LONG_MAX)
            {
                return LONG_MAX;
            }

            UINT_PTR updateValue = currentValue.refCount + 1;

#ifdef __WRL_UNITTEST__
            OnBeforeInternalAddRefIncrement();
#endif

            INT_PTR previousValue = reinterpret_cast<INT_PTR>(UnknownInterlockedCompareExchangePointerForIncrement(reinterpret_cast<PVOID*>(&(refCount_.refCount)), reinterpret_cast<PVOID>(updateValue), reinterpret_cast<PVOID>(currentValue.refCount)));
            if (previousValue == currentValue.rawValue)
            {
                return static_cast<unsigned long>(updateValue);
            }

            currentValue.rawValue = previousValue;
        }
        else
        {
            WeakReferenceImpl* weakRef = DecodeWeakReferencePointer(currentValue.rawValue);
            return weakRef->IncrementStrongReference();
        }
    }
}

template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
unsigned long RuntimeClassImpl<RuntimeClassFlagsT, true, true, false, I0, TInterfaces...>::InternalRelease() throw()
{
#ifdef _PERF_COUNTERS
    IncrementReleaseCount();
#endif

    ReferenceCountOrWeakReferencePointer currentValue = ReadValueFromPointerNoFence<ReferenceCountOrWeakReferencePointer>(&refCount_);

    for (;;)
    {
        if (!IsValueAPointerToWeakReference(currentValue.rawValue))
        {
            if (static_cast<long>(currentValue.refCount) == LONG_MAX)
            {
                return LONG_MAX - 1;
            }

            UINT_PTR updateValue = currentValue.refCount - 1;

#ifdef __WRL_UNITTEST__
            OnBeforeInternalReleaseDecrement();
#endif

            INT_PTR previousValue = reinterpret_cast<INT_PTR>(UnknownInterlockedCompareExchangePointerForIncrement(reinterpret_cast<PVOID*>(&(refCount_.refCount)), reinterpret_cast<PVOID>(updateValue), reinterpret_cast<PVOID>(currentValue.refCount)));
            if (previousValue == currentValue.rawValue)
            {
                return static_cast<unsigned long>(updateValue);
            }

            currentValue.rawValue = previousValue;
        }
        else
        {
            WeakReferenceImpl* weakRef = DecodeWeakReferencePointer(currentValue.rawValue);
            return weakRef->DecrementStrongReference();
        }
    }
}

template <class RuntimeClassFlagsT, typename I0, typename ...TInterfaces>
COM_DECLSPEC_NOTHROW HRESULT RuntimeClassImpl<RuntimeClassFlagsT, true, true, false, I0, TInterfaces...>::GetWeakReference(_Outptr_ IWeakReference **weakReference)
{
    WeakReferenceImpl* weakRef = nullptr;
    INT_PTR encodedWeakRef = 0;
    ReferenceCountOrWeakReferencePointer currentValue = ReadValueFromPointerNoFence<ReferenceCountOrWeakReferencePointer>(&refCount_);

    *weakReference = nullptr;

    if (IsValueAPointerToWeakReference(currentValue.rawValue))
    {
        weakRef = DecodeWeakReferencePointer(currentValue.rawValue);

        weakRef->AddRef();
        *weakReference = weakRef;
        return S_OK;
    }

    // WeakReferenceImpl is created with ref count 2 to avoid interlocked increment
    weakRef = CreateWeakReference(ImplementsHelper::CastToUnknown());
    if (weakRef == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    encodedWeakRef = EncodeWeakReferencePointer(weakRef);

    for (;;)
    {
        INT_PTR previousValue = 0;

        weakRef->SetStrongReference(static_cast<unsigned long>(currentValue.refCount));

#ifdef __WRL_UNITTEST__
        OnBeforeGetWeakReferenceSwap();
#endif

        previousValue = reinterpret_cast<INT_PTR>(UnknownInterlockedCompareExchangePointer(reinterpret_cast<PVOID*>(&(this->refCount_.ifHighBitIsSetThenShiftLeftToYieldPointerToWeakReference)), reinterpret_cast<PVOID>(encodedWeakRef), currentValue.ifHighBitIsSetThenShiftLeftToYieldPointerToWeakReference));
        if (previousValue == currentValue.rawValue)
        {
            // No need to call AddRef in this case, WeakReferenceImpl is created with ref count 2 to avoid interlocked increment
            *weakReference = weakRef;
            return S_OK;
        }
        else if (IsValueAPointerToWeakReference(previousValue))
        {
            // Another thread beat this call to create the weak reference.

            delete weakRef;

            weakRef = DecodeWeakReferencePointer(previousValue);
            weakRef->AddRef();
            *weakReference = weakRef;
            return S_OK;
        }

        // Another thread won via an AddRef or Release.
        // Let's try again
        currentValue.rawValue = previousValue;
    }
}

// Memory allocation for object that doesn't support weak references
// It only allocates memory
template<typename T>
class MakeAllocator
{
public:
    MakeAllocator() throw() : buffer_(nullptr)
    {
    }

    ~MakeAllocator() throw()
    {
        if (buffer_ != nullptr)
        {
            delete buffer_;
        }
    }

    void* Allocate() throw()
    {
        __WRL_ASSERT__(buffer_ == nullptr);
        // Allocate memory with operator new(size, nothrow) only
        // This will allow developer to override one operator only
        // to enable different memory allocation model
#ifdef __cpp_aligned_new
        if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        {
            return buffer_ = (char*) operator new (sizeof(T), static_cast<std::align_val_t>(alignof(T)), ::std::nothrow);
        }
#endif // /std:c++17 or later
        return buffer_ = (char*) operator new (sizeof(T), ::std::nothrow);
    }

    void Detach() throw()
    {
        buffer_ = nullptr;
    }
private:
    char* buffer_;
};

} //Details

#pragma region make overloads

namespace Details {

// Make and MakeAndInitialize functions must not be marked as throw() as the constructor is allowed to throw exceptions.
template <typename T, typename ...TArgs>
ComPtr<T> Make(TArgs&&... args)
{
    static_assert(__is_base_of(Details::RuntimeClassBase, T), "Make can only instantiate types that derive from RuntimeClass");
    ComPtr<T> object;
    Details::MakeAllocator<T> allocator;
    void *buffer = allocator.Allocate();
    if (buffer != nullptr)
    {
        auto ptr = new (buffer)T(Details::Forward<TArgs>(args)...);
        object.Attach(ptr);
        allocator.Detach();
    }
    return object;
}

#pragma warning(push)
#pragma warning(disable:6387 6388 28196) // PREFast does not understand call to ComPtr<T>::CopyTo() is safe here

template <typename T, typename I, typename ...TArgs>
HRESULT MakeAndInitialize(_Outptr_result_nullonfailure_ I** result, TArgs&&... args)
{
    static_assert(__is_base_of(Details::RuntimeClassBase, T), "Make can only instantiate types that derive from RuntimeClass");
    static_assert(__is_base_of(I, T), "The 'T' runtime class doesn't implement 'I' interface");
    *result = nullptr;
    Details::MakeAllocator<T> allocator;
    void *buffer = allocator.Allocate();
    if (buffer == nullptr) { return E_OUTOFMEMORY; }
    auto ptr = new (buffer)T;
    ComPtr<T> object;
    object.Attach(ptr);
    allocator.Detach();
    HRESULT hr = object->RuntimeClassInitialize(Details::Forward<TArgs>(args)...);
    if (FAILED(hr)) { return hr; }
    return object.CopyTo(result);
}

#pragma warning(pop) // C6387 C6388 C28196

template <typename T, typename I, typename ...TArgs>
HRESULT MakeAndInitialize(_Inout_ ComPtrRef<ComPtr<I>> ppvObject, TArgs&&... args)
{
    return MakeAndInitialize<T>(ppvObject.ReleaseAndGetAddressOf(), Details::Forward<TArgs>(args)...);
}

} //end of Details

using Details::MakeAndInitialize;
using Details::Make;

#pragma endregion // make overloads

namespace Details
{
    inline WeakReferenceImpl* CreateWeakReference(_In_ IUnknown* unk)
    {
        return Make<WeakReferenceImpl>(unk).Detach();
    }
}

#define InspectableClass(runtimeClassName, trustLevel) \
    public: \
        static _Null_terminated_ const wchar_t* STDMETHODCALLTYPE InternalGetRuntimeClassName() throw() \
        { \
            static_assert((RuntimeClassT::ClassFlags::value & ::Microsoft::WRL::WinRtClassicComMix) == ::Microsoft::WRL::WinRt || \
                (RuntimeClassT::ClassFlags::value & ::Microsoft::WRL::WinRtClassicComMix) == ::Microsoft::WRL::WinRtClassicComMix, \
                    "'InspectableClass' macro must not be used with ClassicCom clasess."); \
            static_assert(__is_base_of(::Microsoft::WRL::Details::RuntimeClassBase, RuntimeClassT), "'InspectableClass' macro can only be used with ::Windows::WRL::RuntimeClass types"); \
            static_assert(!__is_base_of(IActivationFactory, RuntimeClassT), "Incorrect usage of IActivationFactory interface. Make sure that your RuntimeClass doesn't implement IActivationFactory interface use ::Windows::WRL::ActivationFactory instead or 'InspectableClass' macro is not used on ::Windows::WRL::ActivationFactory"); \
            return runtimeClassName; \
        } \
        static ::TrustLevel STDMETHODCALLTYPE InternalGetTrustLevel() throw() \
        { \
            return trustLevel; \
        } \
        STDMETHOD(GetRuntimeClassName)(_Out_ HSTRING* runtimeName) override \
        { \
            *runtimeName = nullptr; \
            HRESULT hr = S_OK; \
            auto name = InternalGetRuntimeClassName(); \
            if (name != nullptr) \
            { \
                hr = ::WindowsCreateString(name, static_cast<UINT32>(::wcslen(name)), runtimeName); \
            } \
            return hr; \
        } \
        STDMETHOD(GetTrustLevel)(_Out_ ::TrustLevel* trustLvl) override \
        { \
            *trustLvl = trustLevel; \
            return S_OK; \
        } \
        STDMETHOD(GetIids)(_Out_ ULONG *iidCount, \
            _When_(*iidCount == 0, _At_(*iids, _Post_null_)) \
            _When_(*iidCount > 0, _At_(*iids, _Post_notnull_)) \
            _Result_nullonfailure_ IID **iids) override \
        { \
            return RuntimeClassT::GetIids(iidCount, iids); \
        } \
        STDMETHOD(QueryInterface)(REFIID riid, _Outptr_result_nullonfailure_ void **ppvObject) override \
        { \
            bool handled = false; \
            HRESULT hr = this->CustomQueryInterface(riid, ppvObject, &handled); \
            if (FAILED(hr) || handled) return hr; \
            return RuntimeClassT::QueryInterface(riid, ppvObject); \
        } \
        STDMETHOD_(ULONG, Release)() override \
        { \
            return RuntimeClassT::Release(); \
        } \
        STDMETHOD_(ULONG, AddRef)() override \
        { \
            return RuntimeClassT::AddRef(); \
        } \
    private:

#define MixInHelper() \
    public: \
        STDMETHOD(QueryInterface)(REFIID riid, _Outptr_result_nullonfailure_ void **ppvObject) \
        { \
            static_assert((RuntimeClassT::ClassFlags::value & ::Microsoft::WRL::WinRt) == 0, "'MixInClass' macro must not be used with WinRt clasess."); \
            static_assert(__is_base_of(::Microsoft::WRL::Details::RuntimeClassBase, RuntimeClassT), "'MixInHelper' macro can only be used with ::Windows::WRL::RuntimeClass types"); \
            static_assert(!__is_base_of(IClassFactory, RuntimeClassT), "Incorrect usage of IClassFactory interface. Make sure that your RuntimeClass doesn't implement IClassFactory interface use ::Windows::WRL::ClassFactory instead or 'MixInHelper' macro is not used on ::Windows::WRL::ClassFactory"); \
            return RuntimeClassT::QueryInterface(riid, ppvObject); \
        } \
        STDMETHOD_(ULONG, Release)() \
        { \
            return RuntimeClassT::Release(); \
        } \
        STDMETHOD_(ULONG, AddRef)() \
        { \
            return RuntimeClassT::AddRef(); \
        } \
    private:

// Please make sure that those macros are in sync with those ones from 'wrl/module.h'
#ifndef WrlCreatorMapIncludePragmaEx
#define WrlCreatorMapIncludePragmaEx(className, group)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'WrlCreatorMapIncludePragmaEx' macro");
#endif

#ifndef WrlCreatorMapIncludePragma
#define WrlCreatorMapIncludePragma(className)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'WrlCreatorMapIncludePragma' macro");
#endif

#ifndef ActivatableClassWithFactoryEx
#define ActivatableClassWithFactoryEx(className, factory, groupId)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'ActivatableClassWithFactoryEx' macro");
#endif

#ifndef ActivatableClassWithFactory
#define ActivatableClassWithFactory(className, factory)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'ActivatableClassWithFactory' macro");
#endif

#ifndef ActivatableClass
#define ActivatableClass(className)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'ActivatableClass' macro");
#endif

#ifndef ActivatableStaticOnlyFactoryEx
#define ActivatableStaticOnlyFactoryEx(factory, serverName)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'ActivatableStaticOnlyFactoryEx' macro");
#endif

#ifndef ActivatableStaticOnlyFactory
#define ActivatableStaticOnlyFactory(factory)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'ActivatableStaticOnlyFactory' macro");
#endif

#ifndef CoCreatableClassWithFactoryEx
#define CoCreatableClassWithFactoryEx(className, factory, groupId)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'CoCreatableClassWithFactory' macro");
#endif

#ifndef CoCreatableClassWithFactory
#define CoCreatableClassWithFactory(className, factory)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'CoCreatableClassWithFactory' macro");
#endif

#ifndef CoCreatableClass
#define CoCreatableClass(className)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'CoCreatableClass' macro");
#endif

#ifndef CoCreatableClassWrlCreatorMapInclude
#define CoCreatableClassWrlCreatorMapInclude(className)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'CoCreatableClassWrlCreatorMapInclude' macro");
#endif

#ifndef CoCreatableClassWrlCreatorMapIncludeEx
#define CoCreatableClassWrlCreatorMapIncludeEx(className, groupId)  static_assert(false, "It's required to include 'wrl/module.h' to be able to use 'CoCreatableClassWrlCreatorMapInclude' macro");
#endif

#undef UnknownIncrementReference
#undef UnknownDecrementReference
#undef UnknownBarrierAfterInterlock
#undef UnknownInterlockedCompareExchangePointer
#undef UnknownInterlockedCompareExchangePointerForIncrement
#undef UnknownInterlockedCompareExchangePointerForRelease

}}    // namespace Microsoft::WRL

#pragma warning(pop)

// Restore packing
#include <poppack.h>

#endif // _WRL_IMPLEMENTS_H_
