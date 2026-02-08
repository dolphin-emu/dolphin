
#include <ocidl.h> // Bring in IObjectWithSite

#include <wil/com.h>
#include <wrl/implements.h>

#include "common.h"

#include <Bits.h>

using namespace Microsoft::WRL;

// avoid including #include <shobjidl.h>, it fails to compile in noprivateapis
EXTERN_C const CLSID CLSID_ShellLink;
class DECLSPEC_UUID("00021401-0000-0000-C000-000000000046") ShellLink;

// Uncomment this line to do a more exhaustive test of the concepts covered by this file.  By
// default we don't fully compile every combination of tests as this test can substantially impact
// build times with template expansion.

// #define WIL_EXHAUSTIVE_TEST

// Helper objects / functions
class __declspec(uuid("a817e7a2-43fa-11d0-9e44-00aa00b6770a"))
IUnknownFake : public IUnknown
{
public:
    STDMETHOD_(ULONG, AddRef)()
    {
        AddRefCounter++;
        return 0;
    }
    STDMETHOD_(ULONG, Release)()
    {
        ReleaseCounter++;
        return 0;
    }
    STDMETHOD(QueryInterface)(REFIID riid, _Outptr_result_nullonfailure_ void **ppvObject)
    {
        if (riid == __uuidof(IUnknown))
        {
            *ppvObject = this;
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    bool ReturnTRUE()
    {
        return true;
    }
    static void Clear()
    {
        AddRefCounter = 0;
        ReleaseCounter = 0;
    }
    static int GetAddRef()
    {
        int res = AddRefCounter;
        AddRefCounter = 0;
        return res;
    }
    static int GetRelease()
    {
        int res = ReleaseCounter;
        ReleaseCounter = 0;
        return res;
    }
protected:
    static int AddRefCounter;
    static int ReleaseCounter;
};

int IUnknownFake::AddRefCounter = 0;
int IUnknownFake::ReleaseCounter = 0;

class __declspec(uuid("a817e7a2-43fa-11d0-9e44-00aa00b6770b"))
IUnknownFake2 : public IUnknownFake {};

TEST_CASE("ComTests::Test_Constructors", "[com][com_ptr]")
{
    IUnknownFake::Clear();
    IUnknownFake helper;

    SECTION("Null/default construction")
    {
        wil::com_ptr_nothrow<IUnknown> ptr; //default constructor
        REQUIRE(ptr.get() == nullptr);

        wil::com_ptr_nothrow<IUnknown> ptr2(nullptr); //default explicit null constructor
        REQUIRE(ptr2.get() == nullptr);

        IUnknown* nullPtr = nullptr;
        wil::com_ptr_nothrow<IUnknown> ptr3(nullPtr);
        REQUIRE(ptr3.get() == nullptr);
    }

    SECTION("Valid pointer construction")
    {
        wil::com_ptr_nothrow<IUnknown> ptr(&helper); // explicit
        REQUIRE(IUnknownFake::GetAddRef() == 1);
        REQUIRE(ptr.get() == &helper);
    }

    SECTION("Copy construction")
    {
        wil::com_ptr_nothrow<IUnknown> ptr(&helper);
        wil::com_ptr_nothrow<IUnknown> ptrCopy(ptr); // assign the same pointer
        REQUIRE(IUnknownFake::GetAddRef() == 2);
        REQUIRE(ptrCopy.get() == ptr.get());

        IUnknownFake2 helper2;
        wil::com_ptr_nothrow<IUnknownFake2> ptr2(&helper2);
        wil::com_ptr_nothrow<IUnknownFake> ptrCopy2(ptr2);
        REQUIRE(IUnknownFake::GetAddRef() == 2);
        REQUIRE(ptrCopy2.get() == &helper2);
    }

    SECTION("Move construction")
    {
        IUnknownFake helper3;
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper3);
        wil::com_ptr_nothrow<IUnknownFake> ptrMove(reinterpret_cast<wil::com_ptr_nothrow<IUnknownFake>&&>(ptr));
        REQUIRE(IUnknownFake::GetAddRef() == 1);
        REQUIRE(ptrMove.get() == &helper3);
        REQUIRE(ptr.get() == nullptr);

        IUnknownFake2 helper4;
        wil::com_ptr_nothrow<IUnknownFake2> ptr2(&helper4);
        wil::com_ptr_nothrow<IUnknownFake> ptrMove2(reinterpret_cast<wil::com_ptr_nothrow<IUnknownFake2>&&>(ptr2));
        REQUIRE(IUnknownFake::GetAddRef() == 1);
        REQUIRE(ptrMove2.get() == &helper4);
        REQUIRE(ptr2.get() == nullptr);
    }

#if defined(__cpp_deduction_guides) && (__cpp_deduction_guides >= 201907L)
    SECTION("CTAD pointer construction")
    {
        wil::com_ptr_nothrow ptr(&helper); // explicit
        REQUIRE(IUnknownFake::GetAddRef() == 1);
        REQUIRE(ptr.get() == &helper);
    }
#endif
}

TEST_CASE("ComTests::Test_Make", "[com][com_ptr]")
{
    IUnknownFake::Clear();
    IUnknownFake helper;

    auto ptr = wil::make_com_ptr_nothrow(&helper); // CTAD workaround for pre-C++20
    REQUIRE(IUnknownFake::GetAddRef() == 1);
    REQUIRE(ptr.get() == &helper);
}

TEST_CASE("ComTests::Test_Assign", "[com][com_ptr]")
{
    IUnknownFake::Clear();
    IUnknownFake helper;

    SECTION("Null pointer assignment")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        ptr = nullptr;
        REQUIRE(ptr.get() == nullptr);
        REQUIRE(IUnknownFake::GetRelease() == 1);
    }

    IUnknownFake::Clear();
    IUnknownFake helper2;

    SECTION("Different pointer assignment")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        wil::com_ptr_nothrow<IUnknownFake> ptr2(&helper2);

        ptr = static_cast<const wil::com_ptr_nothrow<IUnknownFake>&>(ptr2);

        REQUIRE(ptr.get() == &helper2);
        REQUIRE(ptr2.get() == &helper2);
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 3);
    }

    SECTION("Self assignment")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);

        IUnknownFake::Clear();
        ptr = ptr;

        REQUIRE(ptr.get() == &helper);
        // wil::com_ptr<T> can do self-assignment without blowing up -- and chooses NOT to preserve the this comparison for performance
        // as this should be a rare/never operation...
        // REQUIRE(IUnknownFake::GetRelease() == 0);
        // REQUIRE(IUnknownFake::GetAddRef() == 0);

        ptr = std::move(ptr);
        REQUIRE(ptr.get() == &helper);
    }

    IUnknownFake2 helper3;

    SECTION("Assign pointer with different interface")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        wil::com_ptr_nothrow<IUnknownFake2> ptr2(&helper3);

        IUnknownFake::Clear();
        ptr = static_cast<const wil::com_ptr_nothrow<IUnknownFake2>&>(ptr2);

        REQUIRE(ptr.get() == &helper3);
        REQUIRE(ptr2.get() == &helper3);
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 1);
    }

    SECTION("Move assignment")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        wil::com_ptr_nothrow<IUnknownFake> ptr2(&helper2);

        IUnknownFake::Clear();
        ptr = static_cast<wil::com_ptr_nothrow<IUnknownFake>&&>(ptr2);

        REQUIRE(ptr.get() == &helper2);
        REQUIRE(ptr2.get() == nullptr);
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
    }

    SECTION("Move assign with different interface")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        wil::com_ptr_nothrow<IUnknownFake2> ptr2(&helper3);

        IUnknownFake::Clear();
        ptr = static_cast<wil::com_ptr_nothrow<IUnknownFake2>&&>(ptr2);

        REQUIRE(ptr.get() == &helper3);
        REQUIRE(ptr2.get() == nullptr);
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
    }
}

TEST_CASE("ComTests::Test_Operators", "[com][com_ptr]")
{
    IUnknownFake::Clear();
    IUnknownFake helper;
    IUnknownFake helper2;
    IUnknownFake2 helper3;

    wil::com_ptr_nothrow<IUnknownFake> ptrNULL; //NULL one
    wil::com_ptr_nothrow<IUnknownFake> ptrLT(&helper);
    wil::com_ptr_nothrow<IUnknownFake> ptrGT(&helper2);
    wil::com_ptr_nothrow<IUnknownFake2> ptrDiff(&helper3);

    SECTION("equal operator")
    {
        REQUIRE_FALSE(ptrNULL == ptrLT);
        REQUIRE(ptrNULL == ptrNULL);
        REQUIRE(ptrLT == ptrLT);
        REQUIRE_FALSE(ptrDiff == ptrLT);
        REQUIRE_FALSE(ptrLT == ptrGT);
    }

    SECTION("not equals operator")
    {
        REQUIRE(ptrNULL != ptrLT);
        REQUIRE_FALSE(ptrNULL != ptrNULL);
        REQUIRE_FALSE(ptrLT != ptrLT);
        REQUIRE(ptrDiff != ptrLT);
        REQUIRE(ptrLT != ptrGT);
    }

    SECTION("less-than operator")
    {
        REQUIRE_FALSE(ptrNULL < ptrNULL);
        REQUIRE(ptrNULL < ptrLT);
        REQUIRE(ptrNULL < ptrLT);

        if (ptrLT.get() < ptrGT.get())
        {
            REQUIRE(ptrLT < ptrGT);
        }
        else
        {
            REQUIRE(ptrGT < ptrLT);
        }
    }
}

TEST_CASE("ComTests::Test_Conversion", "[com][com_ptr]")
{
    IUnknownFake::Clear();
    IUnknownFake helper;

    wil::com_ptr_nothrow<IUnknownFake> nullPtr;
    wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);

    REQUIRE_FALSE(nullPtr);
    REQUIRE(ptr);
}

TEST_CASE("ComTests::Test_Address", "[com][com_ptr]")
{
    IUnknownFake::Clear();
    IUnknownFake helper;

    IUnknownFake** pFakePtr;
    SECTION("addressof")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);

        IUnknownFake::Clear();
        pFakePtr = ptr.addressof();
        REQUIRE(IUnknownFake::GetRelease() == 0);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
        REQUIRE((*pFakePtr) == &helper);
    }

    SECTION("put")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        IUnknownFake::Clear();

        pFakePtr = ptr.put();
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
        REQUIRE((*pFakePtr) == nullptr);
        REQUIRE(ptr == nullptr);
    }

    SECTION("put_void")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        IUnknownFake::Clear();

        void** pvFakePtr = ptr.put_void();
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
        REQUIRE((*pvFakePtr) == nullptr);
        REQUIRE(ptr == nullptr);
    }

    SECTION("put_unknown")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        IUnknownFake::Clear();

        IUnknown** puFakePtr = ptr.put_unknown();
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
        REQUIRE((*puFakePtr) == nullptr);
        REQUIRE(ptr == nullptr);
    }

    SECTION("Address operator")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);
        IUnknownFake::Clear();

        pFakePtr = &ptr;
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
        REQUIRE((*pFakePtr) == nullptr);
        REQUIRE(ptr == nullptr);
    }
}

TEST_CASE("ComTests::Test_Helpers", "[com][com_ptr]")
{
    IUnknownFake::Clear();
    IUnknownFake helper;
    IUnknownFake helper2;
    IUnknownFake *ptrHelper;
    wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);

    SECTION("detach")
    {
        IUnknownFake::Clear(); //clear addref counter
        ptrHelper = ptr.detach();
        REQUIRE(ptr.get() == nullptr);
        REQUIRE(ptrHelper == &helper);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
    }

    SECTION("attach")
    {
        ptrHelper = &helper;
        wil::com_ptr_nothrow<IUnknownFake> ptr2(&helper2); //have some non null pointer

        IUnknownFake::Clear(); //clear addref counter
        ptr2.attach(ptrHelper);
        REQUIRE(ptr2.get() == ptrHelper);
        REQUIRE(IUnknownFake::GetRelease() == 1);
        REQUIRE(IUnknownFake::GetAddRef() == 0);
    }

    SECTION("get")
    {
        wil::com_ptr_nothrow<IUnknown> ptr2;
        REQUIRE(ptr2.get() == nullptr);

        IUnknownFake helper3;
        wil::com_ptr_nothrow<IUnknownFake> ptr4(&helper3);
        REQUIRE(ptr4.get() == &helper3);
    }

    SECTION("l-value swap")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr2(&helper);
        wil::com_ptr_nothrow<IUnknownFake> ptr3(&helper2);

        ptr2.swap(ptr3);
        REQUIRE(ptr2.get() == &helper2);
        REQUIRE(ptr3.get() == &helper);
    }

    SECTION("r-value swap")
    {
        wil::com_ptr_nothrow<IUnknownFake> ptr2(&helper);
        wil::com_ptr_nothrow<IUnknownFake> ptr3(&helper2);

        ptr2.swap(wistd::move(ptr3));
        REQUIRE(ptr2.get() == &helper2);
        REQUIRE(ptr3.get() == &helper);
    }
}

TEST_CASE("ComTests::Test_As", "[com][com_ptr]")
{
    IUnknownFake::Clear();

    IUnknownFake helper;
    wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);

    SECTION("query by IID")
    {
        wil::com_ptr_nothrow<IUnknown> ptr2;
        // REQUIRE(S_OK == ptr.AsIID(__uuidof(IUnknown), &ptr2));
        REQUIRE(S_OK == ptr.query_to(__uuidof(IUnknown), reinterpret_cast<void**>(&ptr2)));
        REQUIRE(ptr2 != nullptr);
    }

    SECTION("query by invalid IID")
    {
        wil::com_ptr_nothrow<IUnknown> ptr2;
        // REQUIRE(S_OK != ptr.AsIID(__uuidof(IDispatch), &ptr2));
        REQUIRE(S_OK != ptr.query_to(__uuidof(IDispatch), reinterpret_cast<void**>(&ptr2)));
        REQUIRE(ptr2 == nullptr);
    }

    SECTION("same interface query")
    {
        // wil::com_ptr optimizes same-type assignment to just call AddRef
        IUnknownFake2 helper2;
        wil::com_ptr_nothrow<IUnknownFake2> ptr2(&helper2);
        wil::com_ptr_nothrow<IUnknownFake2> ptr3;
        REQUIRE(S_OK == ptr2.query_to<IUnknownFake2>(&ptr3));
        REQUIRE(ptr3 != nullptr);
    }

    SECTION("base interface query")
    {
        IUnknownFake2 helper2;
        wil::com_ptr_nothrow<IUnknownFake2> ptr2(&helper2);
        wil::com_ptr_nothrow<IUnknown> ptr3;
        REQUIRE(S_OK == ptr2.query_to<IUnknown>(&ptr3));
        REQUIRE(ptr3 != nullptr);
    }
}

TEST_CASE("ComTests::Test_CopyTo", "[com][com_ptr]")
{
    IUnknownFake::Clear();

    IUnknownFake helper;
    IUnknownFake2 helper2;
    wil::com_ptr_nothrow<IUnknownFake> ptr(&helper);

    SECTION("copy by IID")
    {
        wil::com_ptr_nothrow<IUnknown> ptr2;
        REQUIRE(S_OK == ptr.copy_to(__uuidof(IUnknown), reinterpret_cast<void **>(&ptr2)));
        REQUIRE(ptr2 != nullptr);
    }

    SECTION("copy by invalid IID")
    {
        wil::com_ptr_nothrow<IUnknown> ptr2;
        REQUIRE(S_OK != ptr.copy_to(__uuidof(IDispatch), reinterpret_cast<void **>(&ptr2)));
        REQUIRE(ptr2 == nullptr);
    }

    SECTION("same interface copy")
    {
        wil::com_ptr_nothrow<IUnknownFake2> ptr2(&helper2);
        wil::com_ptr_nothrow<IUnknownFake2> ptr3;
        REQUIRE(S_OK == ptr2.copy_to(&ptr3));
        REQUIRE(ptr3 != nullptr);
    }

    SECTION("base interface copy")
    {
        wil::com_ptr_nothrow<IUnknownFake2> ptr2(&helper2);
        wil::com_ptr_nothrow<IUnknown> ptr3;
        REQUIRE(S_OK == ptr2.copy_to(ptr3.addressof()));
        REQUIRE(ptr3 != nullptr);
    }
}

// Helper used to verify correctness of IID_PPV_ARGS support
void IID_PPV_ARGS_Test_Helper(REFIID iid, void** pv)
{
    __analysis_assume(pv != nullptr);
    REQUIRE(pv != nullptr);
    REQUIRE(*pv == nullptr);
    *pv = reinterpret_cast<void*>(0x01); // Set check value

    REQUIRE(iid == __uuidof(IUnknown));
}

TEST_CASE("ComTests::Test_IID_PPV_ARGS", "[com][com_ptr]")
{
    wil::com_ptr_nothrow<IUnknown> unk;
    IID_PPV_ARGS_Test_Helper(IID_PPV_ARGS(&unk));
    //Test if we got the correct check value back
    REQUIRE(unk.get() == reinterpret_cast<void*>(0x01));
    // Make sure that we will not try to release some garbage
    auto avoidWarning = unk.detach();
    (void)avoidWarning;
}

// Helps with testing wil::com_ptr<const ExtensionHelper> configuration when the operator -> is used
class ExtensionHelper
{
public:
    HRESULT Extend() const
    {
        return S_OK;
    }
    STDMETHOD_(ULONG, AddRef)() const
    {
        return 0;
    }
    STDMETHOD_(ULONG, Release)() const
    {
        return 0;
    }
};

TEST_CASE("ComTests::Test_ConstPointer", "[com][com_ptr]")
{
    IUnknownFake::Clear();
    IUnknownFake helper;

    const wil::com_ptr_nothrow<IUnknown> spUnk(&helper);
    wil::com_ptr_nothrow<IUnknown> spUnkHelper;
    wil::com_ptr_nothrow<IInspectable> spInspectable;

    REQUIRE(spUnk.get() != nullptr);
    REQUIRE(spUnk);
    spUnk.addressof();
    spUnk.copy_to(spUnkHelper.addressof());
    spUnk.copy_to(spInspectable.addressof());
    spUnk.copy_to(IID_PPV_ARGS(&spInspectable));

    spUnk.query_to(&spUnkHelper);
    spUnk.query_to(&spInspectable);
    spUnk.query_to(__uuidof(IUnknown), reinterpret_cast<void**>(&spUnkHelper));

    const ExtensionHelper extHelper;
    wil::com_ptr_nothrow<const ExtensionHelper> spExt(&extHelper);
    REQUIRE(spExt->Extend() == S_OK);
}

// Make sure that the pointer can be defined just with forward declaration of the class
TEST_CASE("ComTests::Test_ComPtrWithForwardDeclaration", "[com][com_ptr]")
{
    class MyClass;

    wil::com_ptr_nothrow<MyClass> spClass;

    class MyClass : public IUnknown
    {
    public:
        STDMETHOD_(ULONG, AddRef)()
        {
            return 0;
        }
        STDMETHOD_(ULONG, Release)()
        {
            return 0;
        }
    };
}

//*****************************************************************************
// various com_ptr tests
//*****************************************************************************

interface __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20a00"))
ITest : public IUnknown
{
   STDMETHOD_(void, Test)() = 0;
};

interface __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20a01"))
IDerivedTest : public ITest
{
   STDMETHOD_(void, TestDerived)() = 0;
};

interface __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20a02"))
ITestInspectable : public IInspectable
{
   STDMETHOD_(void, TestInspctable)() = 0;
};

interface __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20a03"))
IDerivedTestInspectable : public ITestInspectable
{
   STDMETHOD_(void, TestInspctableDerived)() = 0;
};

interface __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20a04"))
INever : public IUnknown
{
   STDMETHOD_(void, Never)() = 0;
};

interface __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20a05"))
IAlways : public IUnknown
{
   STDMETHOD_(void, Always)() = 0;
};

class __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20b00")) // non-implemented to allow QI for the class to be attempted (and fail)
ComObject : witest::AllocatedObject,
    public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
                                        Microsoft::WRL::ChainInterfaces<IDerivedTest, ITest>,
                                        IAlways>{
public:
    COM_DECLSPEC_NOTHROW IFACEMETHODIMP_(void) Test() {}
    COM_DECLSPEC_NOTHROW IFACEMETHODIMP_(void) TestDerived() {}
    COM_DECLSPEC_NOTHROW IFACEMETHODIMP_(void) Always() {}
};

class __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20b01")) // non-implemented to allow QI for the class to be attempted (and fail)
WinRtObject : witest::AllocatedObject,
    public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
                                        ITest, IDerivedTest, ITestInspectable, IDerivedTestInspectable, IAlways, Microsoft::WRL::FtmBase>
{
public:
    COM_DECLSPEC_NOTHROW IFACEMETHODIMP_(void) Test() {}
    COM_DECLSPEC_NOTHROW IFACEMETHODIMP_(void) TestDerived() {}
    COM_DECLSPEC_NOTHROW IFACEMETHODIMP_(void) TestInspctable() {}
    COM_DECLSPEC_NOTHROW IFACEMETHODIMP_(void) TestInspctableDerived() {}
    COM_DECLSPEC_NOTHROW IFACEMETHODIMP_(void) Always() {}
};

class NoCom : witest::AllocatedObject
{
public:
    ULONG __stdcall AddRef()
    {
        return m_ref++;
    }

    ULONG __stdcall Release()
    {
        auto retVal = (--m_ref);
        if (retVal == 0)
        {
            delete this;
        }
        return retVal;
    }

private:
    ULONG m_ref = 1;
};

template <typename T, typename U, typename = wistd::enable_if_t<!wistd::is_same_v<T, U>>>
T* cast_object(U*)
{
    FAIL_FAST();
}

template <typename T>
T* cast_object(T* ptr)
{
    return ptr;
}

template <typename IFace, typename Object>
static IFace* make_object()
{
    auto obj = Microsoft::WRL::Make<Object>();

    IFace* result = nullptr;
    if (FAILED(obj.Get()->QueryInterface(__uuidof(IFace), reinterpret_cast<void**>(&result))))
    {
        // The QI only fails when we're asking for a CFoo from a CFoo (equivalent types)... in this
        // case just return the original pointer -- the reinterpret_cast is needed as the code is shared
        // and the other (nonuniform) cases also compile it (but do not execute it).
        result = cast_object<IFace>(obj.Detach());
    }

    return result;
}

template <>
NoCom* make_object<NoCom, NoCom>()
{
    return new NoCom();
}

template <typename Ptr>
void TestSmartPointer(const Ptr& ptr1, const Ptr& ptr2)
{
    SECTION("swap (method and global)")
    {
        auto p1 = ptr1;
        auto p2 = ptr2;
        p1.swap(p2);  // l-value
        REQUIRE(((p1 == ptr2) && (p2 == ptr1)));
        p1.swap(wistd::move(p2)); // r-value
        REQUIRE(((p1 == ptr1) && (p2 == ptr2)));
        wil::swap(p1, p2);
        REQUIRE(((p1 == ptr2) && (p2 == ptr1)));
    }

    SECTION("WRL swap (method and global)")
    {
        auto p1 = ptr1;
        Microsoft::WRL::ComPtr<typename Ptr::element_type> p2 = ptr2.get();
        p1.swap(p2);  // l-value
        REQUIRE(((p1 == ptr2) && (p2 == ptr1)));
        p1.swap(wistd::move(p2)); // r-value
        REQUIRE(((p1 == ptr1) && (p2 == ptr2)));
        wil::swap(p1, p2);
        REQUIRE(((p1 == ptr2) && (p2 == ptr1)));
        wil::swap(p2, p1);
        REQUIRE(((p1 == ptr1) && (p2 == ptr2)));
    }

    SECTION("reset")
    {
        auto p = ptr1;
        p.reset();
        REQUIRE_FALSE(p);
        p = ptr1;
        p.reset(nullptr);
        REQUIRE_FALSE(p);
    }

    SECTION("attach / detach")
    {
        auto p1 = ptr1;
        auto p2 = ptr2;
        p1.attach(p2.detach());
        REQUIRE(((p1.get() == ptr2.get()) && !p2));
    }

    SECTION("addressof")
    {
        auto p1 = ptr1;
        auto p2 = ptr2;
        p1.addressof(); // Doesn't reset
        REQUIRE(p1.get() == ptr1.get());
        p1.reset();
        *(p1.addressof()) = p2.detach();
        REQUIRE(p1.get() == ptr2.get());
    }

    SECTION("put")
    {
        auto p1 = ptr1;
        auto p2 = ptr2;
        p1.put();
        REQUIRE_FALSE(p1);
        *p1.put() = p2.detach();
        REQUIRE(p1.get() == ptr2.get());
    }

    SECTION("operator&")
    {
        auto p1 = ptr1;
        auto p2 = ptr2;
        &p1;
        REQUIRE_FALSE(p1);
        *(&p1) = p2.detach();
        REQUIRE(p1.get() == ptr2.get());
    }

    SECTION("exercise const methods on the const param (ensure const)")
    {
        auto address = ptr1.addressof();
        REQUIRE(*address == ptr1.get());
        (void)static_cast<bool>(ptr1);
        ptr1.get();
        auto deref = ptr1.operator->();
        (void)deref;
        if (ptr1)
        {
            auto& ref = ptr1.operator*();
            (void)ref;
        }
    }
}

template <typename IFace>
static void TestPointerCombination(IFace* p1, IFace* p2)
{
#ifdef WIL_ENABLE_EXCEPTIONS
    TestSmartPointer(wil::com_ptr<IFace>(p1), wil::com_ptr<IFace>(p2));
#endif
    TestSmartPointer(wil::com_ptr_failfast<IFace>(p1), wil::com_ptr_failfast<IFace>(p2));
    TestSmartPointer(wil::com_ptr_nothrow<IFace>(p1), wil::com_ptr_nothrow<IFace>(p2));
}

template <typename IFace, typename Object>
static void TestPointer()
{
    auto p1 = make_object<IFace, Object>();
    auto p2 = make_object<IFace, Object>();
    IFace* nullPtr = nullptr;
    TestPointerCombination(p1, p2);
    TestPointerCombination(nullPtr, p2);
    TestPointerCombination(p1, nullPtr);
    TestPointerCombination(nullPtr, nullPtr);
    TestPointerCombination(p1, p1); // same object

    p1->Release();
    p2->Release();
}

TEST_CASE("ComTests::Test_MemberFunctions", "[com][com_ptr]")
{
    // avoid overwhelming debug logging, perhaps the COM helpers are over reporting
    auto restoreDebugString = wil::g_fResultOutputDebugString;
    wil::g_fResultOutputDebugString = false;

    TestPointer<NoCom, NoCom>();

    TestPointer<ComObject, ComObject>();
    TestPointer<IUnknown, ComObject>();
    TestPointer<ITest, ComObject>();
    TestPointer<IDerivedTest, ComObject>();
    TestPointer<IAlways, ComObject>();

    TestPointer<WinRtObject, WinRtObject>();
    TestPointer<IUnknown, WinRtObject>();
    TestPointer<IInspectable, WinRtObject>();
    TestPointer<ITest, WinRtObject>();
    TestPointer<IDerivedTest, WinRtObject>();
    TestPointer<ITestInspectable, WinRtObject>();
    TestPointer<IDerivedTestInspectable, WinRtObject>();
    TestPointer<IAlways, WinRtObject>();

    REQUIRE_FALSE(witest::g_objectCount.Leaked());
    wil::g_fResultOutputDebugString = restoreDebugString;
}

template <typename Ptr1, typename Ptr2>
static void TestSmartPointerConversion(const Ptr1& ptr1, const Ptr2& ptr2)
{
    const Microsoft::WRL::ComPtr<typename Ptr1::element_type> wrl1 = ptr1.get();
    const Microsoft::WRL::ComPtr<typename Ptr1::element_type> wrl2 = ptr2.get();

    SECTION("global comparison operators")
    {
        auto p1 = ptr1.get();
        auto p2 = ptr2.get();

        // com_ptr to com_ptr
        REQUIRE((ptr1 == ptr2) == (p1 == p2));
        REQUIRE((ptr1 != ptr2) == (p1 != p2));
        REQUIRE((ptr1 < ptr2) == (p1 < p2));
        REQUIRE((ptr1 <= ptr2) == (p1 <= p2));
        REQUIRE((ptr1 > ptr2) == (p1 > p2));
        REQUIRE((ptr1 >= ptr2) == (p1 >= p2));

        // com_ptr to ComPtr
        REQUIRE((wrl1 == ptr2) == (p1 == p2));
        REQUIRE((wrl1 != ptr2) == (p1 != p2));
        REQUIRE((wrl1 < ptr2) == (p1 < p2));
        REQUIRE((wrl1 <= ptr2) == (p1 <= p2));
        REQUIRE((wrl1 > ptr2) == (p1 > p2));
        REQUIRE((wrl1 >= ptr2) == (p1 >= p2));

        REQUIRE((ptr1 == wrl2) == (p1 == p2));
        REQUIRE((ptr1 != wrl2) == (p1 != p2));
        REQUIRE((ptr1 < wrl2) == (p1 < p2));
        REQUIRE((ptr1 <= wrl2) == (p1 <= p2));
        REQUIRE((ptr1 > wrl2) == (p1 > p2));
        REQUIRE((ptr1 >= wrl2) == (p1 >= p2));

        // com_ptr to raw pointer
        REQUIRE((ptr1 == p2) == (p1 == p2));
        REQUIRE((ptr1 != p2) == (p1 != p2));
        REQUIRE((ptr1 < p2) == (p1 < p2));
        REQUIRE((ptr1 <= p2) == (p1 <= p2));
        REQUIRE((ptr1 > p2) == (p1 > p2));
        REQUIRE((ptr1 >= p2) == (p1 >= p2));

        REQUIRE((p1 == ptr2) == (p1 == p2));
        REQUIRE((p1 != ptr2) == (p1 != p2));
        REQUIRE((p1 < ptr2) == (p1 < p2));
        REQUIRE((p1 <= ptr2) == (p1 <= p2));
        REQUIRE((p1 > ptr2) == (p1 > p2));
        REQUIRE((p1 >= ptr2) == (p1 >= p2));
    }

    SECTION("construct from raw pointer")
    {
        Ptr1 p1(ptr2.get());
        Ptr1 p2 = ptr2.get();
        REQUIRE(((p1 == ptr2) && (p2 == ptr2)));
    }

    SECTION("construct from com_ptr ref<>")
    {
        Ptr1 p1(ptr2);
        Ptr1 p2 = (ptr2);
        REQUIRE(((p1 == ptr2) && (p2 == ptr2)));
    }

    SECTION("r-value construct from com_ptr ref<>")
    {
        auto move1 = ptr2;
        auto move2 = ptr2;
        Ptr1 p1(wistd::move(move1));
        Ptr1 p2 = wistd::move(move2);
        REQUIRE(((p1 == ptr2) && (p2 == ptr2)));
    }

    SECTION("assign from raw pointer")
    {
        Ptr1 p = ptr1;
        p = (ptr2.get());
        REQUIRE(p == ptr2);
    }

    SECTION("assign from com_ptr ref<>")
    {
        Ptr1 p = ptr1;
        p = ptr2;
        REQUIRE(p == ptr2);
    }

    SECTION("r-value assign from com_ptr ref<>")
    {
        Ptr1 p = ptr1;
        p = Ptr2(ptr2);
        REQUIRE(p == ptr2);
    }

    SECTION("construct from ComPtr ref<>")
    {
        Ptr1 p1(wrl2);
        Ptr1 p2 = (wrl2);
        REQUIRE(((p1 == wrl2) && (p2 == wrl2)));
    }

    SECTION("r-value construct from ComPtr ref<>")
    {
        auto move1 = wrl2;
        auto move2 = wrl2;
        Ptr1 p1(wistd::move(move1));
        Ptr1 p2 = wistd::move(move2);
        REQUIRE(((p1 == wrl2) && (p2 == wrl2)));
    }

    SECTION("assign from ComPtr ref<>")
    {
        Ptr1 p = ptr1;
        p = wrl2;
        REQUIRE(p == wrl2);
    }

    SECTION("r-value assign from ComPtr ref<>")
    {
        Ptr1 p = ptr1;
        p = decltype(wrl2)(wrl2);
        REQUIRE(p == wrl2);
    }
}

template <typename IFace1, typename IFace2>
static void TestPointerConversionCombination(IFace1* p1, IFace2* p2)
{
#ifdef WIL_ENABLE_EXCEPTIONS
    TestSmartPointerConversion(wil::com_ptr<IFace1>(p1), wil::com_ptr_nothrow<IFace2>(p2));
#endif
    TestSmartPointerConversion(wil::com_ptr_failfast<IFace1>(p1), wil::com_ptr_nothrow<IFace2>(p2));
    TestSmartPointerConversion(wil::com_ptr_nothrow<IFace1>(p1), wil::com_ptr_nothrow<IFace2>(p2));

#ifdef WIL_EXHAUSTIVE_TEST
#ifdef WIL_ENABLE_EXCEPTIONS
    TestSmartPointerConversion(wil::com_ptr<IFace1>(p1), wil::com_ptr<IFace2>(p2));
    TestSmartPointerConversion(wil::com_ptr_failfast<IFace1>(p1), wil::com_ptr<IFace2>(p2));
    TestSmartPointerConversion(wil::com_ptr_nothrow<IFace1>(p1), wil::com_ptr<IFace2>(p2));

    TestSmartPointerConversion(wil::com_ptr<IFace1>(p1), wil::com_ptr_failfast<IFace2>(p2));
#endif
    TestSmartPointerConversion(wil::com_ptr_failfast<IFace1>(p1), wil::com_ptr_failfast<IFace2>(p2));
    TestSmartPointerConversion(wil::com_ptr_nothrow<IFace1>(p1), wil::com_ptr_failfast<IFace2>(p2));
#endif
}

template <typename IFace1, typename IFace2, typename Object>
static void TestPointerConversion()
{
    auto p1 = make_object<IFace1, Object>();
    auto p2 = make_object<IFace2, Object>();
    IFace1* nullPtr1 = nullptr;
    IFace2* nullPtr2 = nullptr;
    TestPointerConversionCombination(p1, p2);
    TestPointerConversionCombination(nullPtr1, p2);
    TestPointerConversionCombination(p1, nullPtr2);
    TestPointerConversionCombination(nullPtr1, nullPtr2);
    TestPointerConversionCombination(static_cast<IFace1*>(p2), p2); // same object

    p1->Release();
    p2->Release();
}

TEST_CASE("ComTests::Test_PointerConversion", "[com][com_ptr]")
{
    // avoid overwhelming debug logging, perhaps the COM helpers are over reporting
    auto restoreDebugString = wil::g_fResultOutputDebugString;
    wil::g_fResultOutputDebugString = false;

    TestPointerConversion<NoCom, NoCom, NoCom>();

    TestPointerConversion<ComObject, ComObject, ComObject>();
    TestPointerConversion<IUnknown, ITest, ComObject>();
    TestPointerConversion<IUnknown, IDerivedTest, ComObject>();
    TestPointerConversion<ITest, IDerivedTest, ComObject>();

#ifdef WIL_EXHAUSTIVE_TEST
    TestPointerConversion<IUnknown, IUnknown, ComObject>();
    TestPointerConversion<ITest, ITest, ComObject>();
    TestPointerConversion<IDerivedTest, IDerivedTest, ComObject>();
    TestPointerConversion<IAlways, IAlways, ComObject>();
    TestPointerConversion<IUnknown, IAlways, ComObject>();

    TestPointerConversion<WinRtObject, WinRtObject, WinRtObject>();
    TestPointerConversion<IUnknown, IUnknown, WinRtObject>();
    TestPointerConversion<IUnknown, ITest, WinRtObject>();
    TestPointerConversion<IUnknown, IDerivedTest, WinRtObject>();
    TestPointerConversion<IUnknown, ITestInspectable, WinRtObject>();
    TestPointerConversion<IUnknown, IDerivedTestInspectable, WinRtObject>();
    TestPointerConversion<IUnknown, IAlways, WinRtObject>();
    TestPointerConversion<IInspectable, IInspectable, WinRtObject>();
    TestPointerConversion<IInspectable, ITestInspectable, WinRtObject>();
    TestPointerConversion<IInspectable, IDerivedTestInspectable, WinRtObject>();
    TestPointerConversion<ITest, ITest, WinRtObject>();
    TestPointerConversion<ITest, IDerivedTest, WinRtObject>();
    TestPointerConversion<ITestInspectable, ITestInspectable, WinRtObject>();
    TestPointerConversion<ITestInspectable, IDerivedTestInspectable, WinRtObject>();
    TestPointerConversion<IDerivedTest, IDerivedTest, WinRtObject>();
    TestPointerConversion<IDerivedTestInspectable, IDerivedTestInspectable, WinRtObject>();
    TestPointerConversion<IAlways, IAlways, WinRtObject>();
#endif

    REQUIRE_FALSE(witest::g_objectCount.Leaked());
    wil::g_fResultOutputDebugString = restoreDebugString;
}

template <typename TargetIFace, typename Ptr>
void TestGlobalQueryIidPpv(wistd::true_type, const Ptr& source)     // interface
{
    using DestPtr = wil::com_ptr_nothrow<TargetIFace>;
    wil::com_ptr_nothrow<INever> never;

    SECTION("com_query_to(iid, ppv)")
    {
        if (source)
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            wil::com_query_to(source, IID_PPV_ARGS(&dest1));
            REQUIRE_ERROR(wil::com_query_to(source, IID_PPV_ARGS(&never)));
            REQUIRE((dest1 && !never));
#endif

            DestPtr dest2, dest3;
            wil::com_query_to_failfast(source, IID_PPV_ARGS(&dest2));
            REQUIRE_ERROR(wil::com_query_to_failfast(source, IID_PPV_ARGS(&never)));
            wil::com_query_to_nothrow(source, IID_PPV_ARGS(&dest3));
            REQUIRE_ERROR(wil::com_query_to_nothrow(source, IID_PPV_ARGS(&never)));
            REQUIRE((dest2 && dest3 && !never));
        }
        else
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            REQUIRE_CRASH(wil::com_query_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE_CRASH(wil::com_query_to(source, IID_PPV_ARGS(&never)));
#endif

            DestPtr dest2, dest3;
            REQUIRE_CRASH(wil::com_query_to_failfast(source, IID_PPV_ARGS(&dest2)));
            REQUIRE_CRASH(wil::com_query_to_failfast(source, IID_PPV_ARGS(&never)));
            REQUIRE_CRASH(wil::com_query_to_nothrow(source, IID_PPV_ARGS(&dest3)));
            REQUIRE_CRASH(wil::com_query_to_nothrow(source, IID_PPV_ARGS(&never)));
        }
    }

    SECTION("try_com_query_to(iid, ppv)")
    {
        if (source)
        {
            DestPtr dest1;
            REQUIRE(wil::try_com_query_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE_FALSE(wil::try_com_query_to(source, IID_PPV_ARGS(&never)));
            REQUIRE((dest1 && !never));
        }
        else
        {
            DestPtr dest1;
            REQUIRE_CRASH(wil::try_com_query_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE_CRASH(wil::try_com_query_to(source, IID_PPV_ARGS(&never)));
        }
    }

    SECTION("com_copy_to(iid, ppv)")
    {
    if (source)
    {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            wil::com_copy_to(source, IID_PPV_ARGS(&dest1));
            REQUIRE_ERROR(wil::com_copy_to(source, IID_PPV_ARGS(&never)));
            REQUIRE((dest1 && !never));
#endif

            DestPtr dest2, dest3;
            wil::com_copy_to_failfast(source, IID_PPV_ARGS(&dest2));
            REQUIRE_ERROR(wil::com_copy_to_failfast(source, IID_PPV_ARGS(&never)));
            wil::com_copy_to_nothrow(source, IID_PPV_ARGS(&dest3));
            REQUIRE_ERROR(wil::com_copy_to_nothrow(source, IID_PPV_ARGS(&never)));
            REQUIRE((dest2 && dest3 && !never));
        }
        else
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            wil::com_copy_to(source, IID_PPV_ARGS(&dest1));
            wil::com_copy_to(source, IID_PPV_ARGS(&never));
#endif

            DestPtr dest2, dest3;
            wil::com_copy_to_failfast(source, IID_PPV_ARGS(&dest2));
            wil::com_copy_to_failfast(source, IID_PPV_ARGS(&never));
            wil::com_copy_to_nothrow(source, IID_PPV_ARGS(&dest3));
            wil::com_copy_to_nothrow(source, IID_PPV_ARGS(&never));
        }
    }

    SECTION("try_com_copy_to(iid, ppv)")
    {
        if (source)
        {
            DestPtr dest1;
            REQUIRE(wil::try_com_copy_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE_FALSE(wil::try_com_copy_to(source, IID_PPV_ARGS(&never)));
            REQUIRE((dest1 && !never));
        }
        else
        {
            DestPtr dest1;
            REQUIRE_FALSE(wil::try_com_copy_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE_FALSE(wil::try_com_copy_to(source, IID_PPV_ARGS(&never)));
        }
    }
}

template <typename TargetIFace, typename Ptr>
void TestGlobalQueryIidPpv(wistd::false_type, const Ptr&)     // class
{
    // we can't compile against iid, ppv with a class
}

template <typename TargetIFace, typename Ptr>
static void TestGlobalQuery(const Ptr& source)
{
    using DestPtr = wil::com_ptr_nothrow<TargetIFace>;
    wil::com_ptr_nothrow<INever> never;

    SECTION("com_query")
    {
        if (source)
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE(wil::com_query<TargetIFace>(source));
            REQUIRE_ERROR(wil::com_query<INever>(source));
#endif

            REQUIRE(wil::com_query_failfast<TargetIFace>(source));
            REQUIRE_ERROR(wil::com_query_failfast<INever>(source));
        }
        else
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE_CRASH(wil::com_query<TargetIFace>(source));
            REQUIRE_CRASH(wil::com_query<INever>(source));
#endif

            REQUIRE_CRASH(wil::com_query_failfast<TargetIFace>(source));
            REQUIRE_CRASH(wil::com_query_failfast<INever>(source));
        }
    }

    SECTION("com_query_to(U**)")
    {
        if (source)
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            wil::com_query_to(source, &dest1);
            REQUIRE_ERROR(wil::com_query_to(source, &never));
            REQUIRE((dest1 && !never));
#endif

            DestPtr dest2, dest3;
            wil::com_query_to_failfast(source, &dest2);
            REQUIRE_ERROR(wil::com_query_to_failfast(source, &never));
            wil::com_query_to_nothrow(source, &dest3);
            REQUIRE_ERROR(wil::com_query_to_nothrow(source, &never));
            REQUIRE((dest2 && dest3 && !never));
        }
        else
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            REQUIRE_CRASH(wil::com_query_to(source, &dest1));
            REQUIRE_CRASH(wil::com_query_to(source, &never));
#endif

            DestPtr dest2, dest3;
            REQUIRE_CRASH(wil::com_query_to_failfast(source, &dest2));
            REQUIRE_CRASH(wil::com_query_to_failfast(source, &never));
            REQUIRE_CRASH(wil::com_query_to_nothrow(source, &dest3));
            REQUIRE_CRASH(wil::com_query_to_nothrow(source, &never));
        }
    }

    SECTION("try_com_query")
    {
        if (source)
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE(wil::try_com_query<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_query<INever>(source));
#endif

            REQUIRE(wil::try_com_query_failfast<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_query_failfast<INever>(source));
            REQUIRE(wil::try_com_query_nothrow<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_query_nothrow<INever>(source));
        }
        else
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE_CRASH(wil::try_com_query<TargetIFace>(source));
            REQUIRE_CRASH(wil::try_com_query<INever>(source));
#endif

            REQUIRE_CRASH(wil::try_com_query_failfast<TargetIFace>(source));
            REQUIRE_CRASH(wil::try_com_query_failfast<INever>(source));
            REQUIRE_CRASH(wil::try_com_query_nothrow<TargetIFace>(source));
            REQUIRE_CRASH(wil::try_com_query_nothrow<INever>(source));
        }
    }

    SECTION("try_com_query_to(U**)")
    {
        if (source)
        {
            DestPtr dest1;
            REQUIRE(wil::try_com_query_to(source, &dest1));
            REQUIRE_FALSE(wil::try_com_query_to(source, &never));
            REQUIRE((dest1 && !never));
        }
        else
        {
            DestPtr dest1;
            REQUIRE_CRASH(wil::try_com_query_to(source, &dest1));
            REQUIRE_CRASH(wil::try_com_query_to(source, &never));
        }
    }

    SECTION("com_copy")
    {
        if (source)
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE(wil::com_copy<TargetIFace>(source));
            REQUIRE_ERROR(wil::com_copy<INever>(source));
#endif

            REQUIRE(wil::com_copy_failfast<TargetIFace>(source));
            REQUIRE_ERROR(wil::com_copy_failfast<INever>(source));
        }
        else
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE_FALSE(wil::com_copy<TargetIFace>(source));
            REQUIRE_FALSE(wil::com_copy<INever>(source));
#endif

            REQUIRE_FALSE(wil::com_copy_failfast<TargetIFace>(source));
            REQUIRE_FALSE(wil::com_copy_failfast<INever>(source));
        }
    }

    SECTION("com_copy_to(U**)")
    {
        if (source)
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            wil::com_copy_to(source, &dest1);
            REQUIRE_ERROR(wil::com_copy_to(source, &never));
            REQUIRE((dest1 && !never));
#endif

            DestPtr dest2, dest3;
            wil::com_copy_to_failfast(source, &dest2);
            REQUIRE_ERROR(wil::com_copy_to_failfast(source, &never));
            wil::com_copy_to_nothrow(source, &dest3);
            REQUIRE_ERROR(wil::com_copy_to_nothrow(source, &never));
            REQUIRE((dest2 && dest3 && !never));
        }
        else
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            wil::com_copy_to(source, &dest1);
            wil::com_copy_to(source, &never);
#endif

            DestPtr dest2, dest3;
            wil::com_copy_to_failfast(source, &dest2);
            wil::com_copy_to_failfast(source, &never);
            wil::com_copy_to_nothrow(source, &dest3);
            wil::com_copy_to_nothrow(source, &never);
        }
    }

    SECTION("try_com_copy")
    {
        if (source)
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE(wil::try_com_copy<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_copy<INever>(source));
#endif
            REQUIRE(wil::try_com_copy_failfast<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_copy_failfast<INever>(source));
            REQUIRE(wil::try_com_copy_nothrow<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_copy_nothrow<INever>(source));
        }
        else
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            REQUIRE_FALSE(wil::try_com_copy<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_copy<INever>(source));
#endif
            REQUIRE_FALSE(wil::try_com_copy_failfast<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_copy_failfast<INever>(source));
            REQUIRE_FALSE(wil::try_com_copy_nothrow<TargetIFace>(source));
            REQUIRE_FALSE(wil::try_com_copy_nothrow<INever>(source));
        }
    }

    SECTION("try_com_copy_to(U**)")
    {
        if (source)
        {
            DestPtr dest1;
            REQUIRE(wil::try_com_copy_to(source, &dest1));
            REQUIRE_FALSE(wil::try_com_copy_to(source, &never));
            REQUIRE((dest1 && !never));
        }
        else
        {
            DestPtr dest1;
            REQUIRE_FALSE(wil::try_com_copy_to(source, &dest1));
            REQUIRE_FALSE(wil::try_com_copy_to(source, &never));
        }
    }

    TestGlobalQueryIidPpv<TargetIFace, Ptr>(typename wistd::is_abstract<TargetIFace>::type(), source);
}

// Test fluent query functions for types that support them (exception and fail fast)
template <typename IFace, typename Ptr>
void TestSmartPointerQueryFluent(wistd::true_type, const Ptr& source)     // void return (non-error based)
{
    SECTION("query")
    {
        if (source)
        {
            REQUIRE(source.template query<IFace>());
            REQUIRE_ERROR(source.template query<INever>());
        }
        else
        {
            REQUIRE_CRASH(source.template query<IFace>());
            REQUIRE_CRASH(source.template query<INever>());
        }
    }

    SECTION("copy")
    {
        if (source)
        {
            REQUIRE(source.template copy<IFace>());
            REQUIRE_ERROR(source.template copy<INever>());
        }
        else
        {
            REQUIRE_FALSE(source.template copy<IFace>());
            REQUIRE_FALSE(source.template copy<INever>());
        }
    }
}

// "Test" fluent query functions for error-based types (by doing nothing)
template <typename IFace, typename Ptr>
void TestSmartPointerQueryFluent(wistd::false_type, const Ptr& /*source*/)     // error-code based return
{
    // error code based code cannot call the fluent error methods
}

// Test iid, ppv queries for types that support them (interfaces yes, classes no)
template <typename IFace, typename Ptr>
void TestSmartPointerQueryIidPpv(wistd::true_type, const Ptr& source)       // interface
{
    wil::com_ptr_nothrow<INever> never;
    using DestPtr = wil::com_ptr_nothrow<IFace>;

    SECTION("query_to(iid, ppv)")
    {
        if (source)
        {
            DestPtr dest;
            source.query_to(IID_PPV_ARGS(&dest));
            REQUIRE_ERROR(source.query_to(IID_PPV_ARGS(&never)));
            REQUIRE((dest && !never));
        }
        else
        {
            DestPtr dest;
            REQUIRE_CRASH(source.query_to(IID_PPV_ARGS(&dest)));
            REQUIRE_CRASH(source.query_to(IID_PPV_ARGS(&never)));
            REQUIRE((!dest && !never));
        }
    }

    SECTION("try_query_to(iid, ppv)")
    {
        if (source)
        {
            DestPtr dest;
            REQUIRE(source.try_query_to(IID_PPV_ARGS(&dest)));
            REQUIRE(!source.try_query_to(IID_PPV_ARGS(&never)));
            REQUIRE((dest && !never));
        }
        else
        {
            DestPtr dest;
            REQUIRE_CRASH(source.try_query_to(IID_PPV_ARGS(&dest)));
            REQUIRE_CRASH(source.try_query_to(IID_PPV_ARGS(&never)));
            REQUIRE((!dest && !never));
        }
    }

    SECTION("copy_to(iid, ppv)")
    {
        if (source)
        {
            DestPtr dest;
            source.copy_to(IID_PPV_ARGS(&dest));
            REQUIRE_ERROR(source.copy_to(IID_PPV_ARGS(&never)));
            REQUIRE((dest && !never));
        }
        else
        {
            DestPtr dest;
            source.copy_to(IID_PPV_ARGS(&dest));
            source.copy_to(IID_PPV_ARGS(&never));
            REQUIRE((!dest && !never));
        }
    }

    SECTION("try_copy_to(iid, ppv)")
    {
        if (source)
        {
            DestPtr dest;
            REQUIRE(source.try_copy_to(IID_PPV_ARGS(&dest)));
            REQUIRE(!source.try_copy_to(IID_PPV_ARGS(&never)));
            REQUIRE((dest && !never));
        }
        else
        {
            DestPtr dest;
            REQUIRE(!source.try_copy_to(IID_PPV_ARGS(&dest)));
            REQUIRE(!source.try_copy_to(IID_PPV_ARGS(&never)));
            REQUIRE((!dest && !never));
        }
    }
}

// "Test" iid, ppv queries for types that support them for a class (unsupported same (interfaces yes, classes no)
template <typename IFace, typename Ptr>
void TestSmartPointerQueryIidPpv(wistd::false_type, const Ptr& /*source*/)      // class
{
    // we can't compile against iid, ppv with a class
}

// Test the various query and copy methods against the given source pointer (trying produce the given dest pointer)
template <typename IFace, typename Ptr>
void TestSmartPointerQuery(const Ptr& source)
{
    wil::com_ptr_nothrow<INever> never;
    using DestPtr = wil::com_ptr_nothrow<IFace>;

    SECTION("query_to(U**)")
    {
        if (source)
        {
            DestPtr dest;
            source.query_to(&dest);
            REQUIRE_ERROR(source.query_to(&never));
            REQUIRE((dest && !never));
        }
        else
        {
            DestPtr dest;
            REQUIRE_CRASH(source.query_to(&dest));
            REQUIRE_CRASH(source.query_to(&never));
            REQUIRE((!dest && !never));
        }
    }

    SECTION("try_query")
    {
        if (source)
        {
            REQUIRE(source.template try_query<IFace>());
            REQUIRE_FALSE(source.template try_query<INever>());
        }
        else
        {
            REQUIRE_CRASH(source.template try_query<IFace>());
            REQUIRE_CRASH(source.template try_query<INever>());
        }
    }

    SECTION("try_query_to(U**)")
    {
        if (source)
        {
            DestPtr dest;
            REQUIRE(source.try_query_to(&dest));
            REQUIRE_FALSE(source.try_query_to(&never));
            REQUIRE((dest && !never));
        }
        else
        {
            DestPtr dest;
            REQUIRE_CRASH(source.try_query_to(&dest));
            REQUIRE_CRASH(source.try_query_to(&never));
            REQUIRE((!dest && !never));
        }
    }

    SECTION("copy_to(U**)")
    {
        if (source)
        {
            DestPtr dest;
            source.copy_to(&dest);
            REQUIRE_ERROR(source.copy_to(&never));
            REQUIRE((dest && !never));
        }
        else
        {
            DestPtr dest;
            source.copy_to(&dest);
            source.copy_to(&never);
            REQUIRE((!dest && !never));
        }
    }

    SECTION("try_copy")
    {
        if (source)
        {
            REQUIRE(source.template try_copy<IFace>());
            REQUIRE_FALSE(source.template try_copy<INever>());
        }
        else
        {
            REQUIRE_FALSE(source.template try_copy<IFace>());
            REQUIRE_FALSE(source.template try_copy<INever>());
        }
    }

    SECTION("try_copy_to(U**)")
    {
        if (source)
        {
            DestPtr dest;
            REQUIRE(source.try_copy_to(&dest));
            REQUIRE_FALSE(source.try_copy_to(&never));
            REQUIRE((dest && !never));
        }
        else
        {
            DestPtr dest;
            REQUIRE_FALSE(source.try_copy_to(&dest));
            REQUIRE_FALSE(source.try_copy_to(&never));
            REQUIRE((!dest && !never));
        }
    }

    TestSmartPointerQueryFluent<IFace, Ptr>(typename wistd::is_same<void, typename Ptr::result>::type(), source);

    TestSmartPointerQueryIidPpv<IFace, Ptr>(typename wistd::is_abstract<IFace>::type(), source);
}

template <typename TargetIFace, typename IFace>
static void TestQueryCombination(IFace* ptr)
{
    TestGlobalQuery<TargetIFace>(ptr);
#ifdef WIL_EXHAUSTIVE_TEST
#ifdef WIL_ENABLE_EXCEPTIONS
    TestGlobalQuery<TargetIFace>(wil::com_ptr<IFace>(ptr));
#endif
    TestGlobalQuery<TargetIFace>(wil::com_ptr_failfast<IFace>(ptr));
#endif
    TestGlobalQuery<TargetIFace>(wil::com_ptr_nothrow<IFace>(ptr));
    TestGlobalQuery<TargetIFace>(Microsoft::WRL::ComPtr<IFace>(ptr));

#ifdef WIL_ENABLE_EXCEPTIONS
    TestSmartPointerQuery<TargetIFace>(wil::com_ptr<IFace>(ptr));
#endif
    TestSmartPointerQuery<TargetIFace>(wil::com_ptr_failfast<IFace>(ptr));
    TestSmartPointerQuery<TargetIFace>(wil::com_ptr_nothrow<IFace>(ptr));
}

template <typename TargetIFace, typename IFace>
static void TestQuery(IFace* ptr)
{
    IFace* nullPtr = nullptr;
    TestQueryCombination<TargetIFace>(ptr);
    TestQueryCombination<TargetIFace>(nullPtr);
}

template <typename IFace, typename TargetIFace, typename Object>
static void TestQuery()
{
    auto ptr = make_object<IFace, Object>();
    TestQuery<TargetIFace>(ptr);
    ptr->Release();
}

TEST_CASE("ComTests::Test_Query", "[com][com_ptr]")
{
    // avoid overwhelming debug logging, perhaps the COM helpers are over reporting
    auto restoreDebugString = wil::g_fResultOutputDebugString;
    wil::g_fResultOutputDebugString = false;

    TestQuery<ComObject, ComObject, ComObject>(); // Same type (no QI)
    TestQuery<ComObject, IUnknown, ComObject>();  // Ambiguous base (must QI)
    TestQuery<ComObject, ITest, ComObject>();     // Non-ambiguous base (no QI)

    // This adds a significant amount of time to the compilation duration, so most tests are disabled by default...
#ifdef WIL_EXHAUSTIVE_TEST
    TestQuery<ComObject, IDerivedTest, ComObject>();                         // ComObject
    TestQuery<ComObject, IAlways, ComObject>();
    TestQuery<IUnknown, IUnknown, ComObject>();                              // IUnknown
    TestQuery<IUnknown, ITest, ComObject>();
    TestQuery<IUnknown, IDerivedTest, ComObject>();
    TestQuery<IUnknown, IAlways, ComObject>();
    TestQuery<ITest, IUnknown, ComObject>();                                 // ITest
    TestQuery<ITest, ITest, ComObject>();
    TestQuery<ITest, IDerivedTest, ComObject>();
    TestQuery<ITest, IAlways, ComObject>();
    TestQuery<IDerivedTest, IUnknown, ComObject>();                          // IDerivedTest
    TestQuery<IDerivedTest, ITest, ComObject>();
    TestQuery<IDerivedTest, IDerivedTest, ComObject>();
    TestQuery<IDerivedTest, IAlways, ComObject>();
    TestQuery<IAlways, IUnknown, ComObject>();                               // IAlways
    TestQuery<IAlways, ITest, ComObject>();
    TestQuery<IAlways, IDerivedTest, ComObject>();
    TestQuery<IAlways, IAlways, ComObject>();

    TestQuery<WinRtObject, WinRtObject, WinRtObject>();                      // WinRtObject
    TestQuery<WinRtObject, IUnknown, WinRtObject>();
    TestQuery<WinRtObject, ITest, WinRtObject>();
    TestQuery<WinRtObject, IInspectable, WinRtObject>();
    TestQuery<WinRtObject, ITestInspectable, WinRtObject>();
    TestQuery<WinRtObject, IDerivedTest, WinRtObject>();
    TestQuery<WinRtObject, IDerivedTestInspectable, WinRtObject>();
    TestQuery<WinRtObject, IAlways, WinRtObject>();
    TestQuery<IUnknown, IUnknown, WinRtObject>();                            // IUnknown
    TestQuery<IUnknown, IInspectable, WinRtObject>();
    TestQuery<IUnknown, ITest, WinRtObject>();
    TestQuery<IUnknown, IDerivedTest, WinRtObject>();
    TestQuery<IUnknown, ITestInspectable, WinRtObject>();
    TestQuery<IUnknown, IDerivedTestInspectable, WinRtObject>();
    TestQuery<IUnknown, IAlways, WinRtObject>();
    TestQuery<IInspectable, IUnknown, WinRtObject>();                        // IInspectable
    TestQuery<IInspectable, IInspectable, WinRtObject>();
    TestQuery<IInspectable, ITest, WinRtObject>();
    TestQuery<IInspectable, IDerivedTest, WinRtObject>();
    TestQuery<IInspectable, ITestInspectable, WinRtObject>();
    TestQuery<IInspectable, IDerivedTestInspectable, WinRtObject>();
    TestQuery<IInspectable, IAlways, WinRtObject>();
    TestQuery<ITest, IUnknown, WinRtObject>();                               // ITest
    TestQuery<ITest, IInspectable, WinRtObject>();
    TestQuery<ITest, ITest, WinRtObject>();
    TestQuery<ITest, IDerivedTest, WinRtObject>();
    TestQuery<ITest, ITestInspectable, WinRtObject>();
    TestQuery<ITest, IDerivedTestInspectable, WinRtObject>();
    TestQuery<ITest, IAlways, WinRtObject>();
    TestQuery<IDerivedTest, IUnknown, WinRtObject>();                        // IDerivedTest
    TestQuery<IDerivedTest, IInspectable, WinRtObject>();
    TestQuery<IDerivedTest, ITest, WinRtObject>();
    TestQuery<IDerivedTest, IDerivedTest, WinRtObject>();
    TestQuery<IDerivedTest, ITestInspectable, WinRtObject>();
    TestQuery<IDerivedTest, IDerivedTestInspectable, WinRtObject>();
    TestQuery<IDerivedTest, IAlways, WinRtObject>();
    TestQuery<ITestInspectable, IUnknown, WinRtObject>();                     // ITestInspectable
    TestQuery<ITestInspectable, IInspectable, WinRtObject>();
    TestQuery<ITestInspectable, ITest, WinRtObject>();
    TestQuery<ITestInspectable, IDerivedTest, WinRtObject>();
    TestQuery<ITestInspectable, ITestInspectable, WinRtObject>();
    TestQuery<ITestInspectable, IDerivedTestInspectable, WinRtObject>();
    TestQuery<ITestInspectable, IAlways, WinRtObject>();
    TestQuery<IDerivedTestInspectable, IUnknown, WinRtObject>();              // IDerivedTestInspectable
    TestQuery<IDerivedTestInspectable, IInspectable, WinRtObject>();
    TestQuery<IDerivedTestInspectable, ITest, WinRtObject>();
    TestQuery<IDerivedTestInspectable, IDerivedTest, WinRtObject>();
    TestQuery<IDerivedTestInspectable, ITestInspectable, WinRtObject>();
    TestQuery<IDerivedTestInspectable, IDerivedTestInspectable, WinRtObject>();
    TestQuery<IDerivedTestInspectable, IAlways, WinRtObject>();
    TestQuery<IAlways, IUnknown, WinRtObject>();                              // IAlways
    TestQuery<IAlways, IInspectable, WinRtObject>();
    TestQuery<IAlways, ITest, WinRtObject>();
    TestQuery<IAlways, IDerivedTest, WinRtObject>();
    TestQuery<IAlways, ITestInspectable, WinRtObject>();
    TestQuery<IAlways, IDerivedTestInspectable, WinRtObject>();
    TestQuery<IAlways, IAlways, WinRtObject>();
#endif

    REQUIRE_FALSE(witest::g_objectCount.Leaked());
    wil::g_fResultOutputDebugString = restoreDebugString;
}

#if (NTDDI_VERSION >= NTDDI_WINBLUE)
template <typename Ptr>
void TestAgile(const Ptr& source)
{
    bool source_valid = (source != nullptr);

    if (source)
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        auto agile1 = wil::com_agile_query(source);
        REQUIRE(agile1);
#endif

        auto agile2 = wil::com_agile_query_failfast(source);
        wil::com_agile_ref_nothrow agile3;
        REQUIRE_SUCCEEDED(wil::com_agile_query_nothrow(source, &agile3));
        REQUIRE((agile2 && agile3));
    }
    else
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE_CRASH(wil::com_agile_query(source));
#endif

        REQUIRE_CRASH(wil::com_agile_query_failfast(source));
        wil::com_agile_ref_nothrow agile3;
        REQUIRE_CRASH(wil::com_agile_query_nothrow(source, &agile3));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    auto agile1 = wil::com_agile_copy(source);
    REQUIRE(static_cast<bool>(agile1) == source_valid);
#endif

    auto agile2 = wil::com_agile_copy_failfast(source);
    wil::com_agile_ref_nothrow agile3;
    REQUIRE_SUCCEEDED(wil::com_agile_copy_nothrow(source, &agile3));
    REQUIRE(static_cast<bool>(agile2) == source_valid);
    REQUIRE(static_cast<bool>(agile3) == source_valid);
}

template <typename IFace>
void TestAgileCombinations()
{
    auto ptr = make_object<IFace, WinRtObject>();

    REQUIRE_SUCCEEDED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    auto exit = wil::scope_exit([] { ::CoUninitialize(); });

    TestAgile(ptr);
    TestAgile(wil::com_ptr_nothrow<IFace>(ptr));
    TestAgile(Microsoft::WRL::ComPtr<IFace>(ptr));

    auto agilePtr = wil::com_agile_query_failfast(ptr);
    TestQuery<ITest>(agilePtr.get());
#ifdef WIL_EXHAUSTIVE_TEST
    TestQuery<IUnknown>(agilePtr.get());
    TestQuery<IInspectable>(agilePtr.get());
    TestQuery<IDerivedTest>(agilePtr.get());
    TestQuery<ITestInspectable>(agilePtr.get());
    TestQuery<IDerivedTestInspectable>(agilePtr.get());
    TestQuery<IAlways>(agilePtr.get());
#endif

    ptr->Release();
}

TEST_CASE("ComTests::Test_Agile", "[com][com_agile_ref]")
{
    // TestAgileCombinations<WinRtObject>();
    TestAgileCombinations<IUnknown>();
    TestAgileCombinations<IInspectable>();
    TestAgileCombinations<ITest>();
#ifdef WIL_EXHAUSTIVE_TEST
    TestAgileCombinations<IDerivedTest>();
    TestAgileCombinations<ITestInspectable>();
    TestAgileCombinations<IDerivedTestInspectable>();
    TestAgileCombinations<IAlways>();
#endif

    REQUIRE_FALSE(witest::g_objectCount.Leaked());
}
#endif

template <typename Ptr>
void TestWeak(const Ptr& source)
{
    bool supports_weak = (source && (wil::try_com_query_nothrow<IInspectable>(source)));

    if (supports_weak && source)
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        auto weak1 = wil::com_weak_query(source);
        REQUIRE(weak1);
#endif

        auto weak2 = wil::com_weak_query_failfast(source);
        wil::com_weak_ref_nothrow weak3;
        REQUIRE_SUCCEEDED(wil::com_weak_query_nothrow(source, &weak3));
        REQUIRE((weak2 && weak3));

#ifdef WIL_ENABLE_EXCEPTIONS
        auto weak1copy = wil::com_weak_copy(source);
        REQUIRE(weak1copy);
#endif
        auto weak2copy = wil::com_weak_copy_failfast(source);
        wil::com_weak_ref_nothrow weak3copy;
        REQUIRE_SUCCEEDED(wil::com_weak_copy_nothrow(source, &weak3copy));
        REQUIRE((weak2copy && weak3copy));
    }
    else if (source)
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE_ERROR(wil::com_weak_query(source));
#endif

        REQUIRE_ERROR(wil::com_weak_query_failfast(source));
        wil::com_weak_ref_nothrow weak3err;
        REQUIRE_ERROR(wil::com_weak_query_nothrow(source, &weak3err));

#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE_ERROR(wil::com_weak_copy(source));
#endif

        REQUIRE_ERROR(wil::com_weak_copy_failfast(source));
        wil::com_weak_ref_nothrow weak3;
        REQUIRE_ERROR(wil::com_weak_copy_nothrow(source, &weak3));
    }
    else // !source
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE_CRASH(wil::com_weak_query(source));
#endif

        REQUIRE_CRASH(wil::com_weak_query_failfast(source));
        wil::com_weak_ref_nothrow weak3crash;
        REQUIRE_CRASH(wil::com_weak_query_nothrow(source, &weak3crash));

#ifdef WIL_ENABLE_EXCEPTIONS
        auto weak1 = wil::com_weak_copy(source);
        REQUIRE(!weak1);
#endif

        auto weak2 = wil::com_weak_copy_failfast(source);
        wil::com_weak_ref_nothrow weak3;
        REQUIRE_SUCCEEDED(wil::com_weak_copy_nothrow(source, &weak3));
        REQUIRE((!weak2 && !weak3));
    }
}

template <typename TargetIFace, typename Ptr>
void TestGlobalQueryWithFailedResolve(const Ptr& source)
{
    // No need to test the null source and wrong interface query
    // since that's covered in the TestGlobalQuery.
    using DestPtr = wil::com_ptr_nothrow<TargetIFace>;

    SECTION("com_query")
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE_ERROR(wil::com_query<TargetIFace>(source));
#endif
        REQUIRE_ERROR(wil::com_query_failfast<TargetIFace>(source));
    }

    SECTION("com_query_to(U**)")
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        DestPtr dest1;
        REQUIRE_ERROR(wil::com_query_to(source, &dest1));
        REQUIRE(!dest1);
#endif

        DestPtr dest2, dest3;
        REQUIRE_ERROR(wil::com_query_to_failfast(source, &dest2));
        REQUIRE_ERROR(wil::com_query_to_nothrow(source, &dest3));
        REQUIRE((!dest2 && !dest3));
    }

    SECTION("try_com_query")
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(!wil::try_com_query<TargetIFace>(source));
#endif
        REQUIRE(!wil::try_com_query_failfast<TargetIFace>(source));
        REQUIRE(!wil::try_com_query_nothrow<TargetIFace>(source));
    }

    SECTION("try_com_query_to(U**)")
    {
        DestPtr dest1;
        REQUIRE(!wil::try_com_query_to(source, &dest1));
        REQUIRE(!dest1);
    }

    SECTION("com_copy")
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE_ERROR(wil::com_copy<TargetIFace>(source));
#endif
        REQUIRE_ERROR(wil::com_copy_failfast<TargetIFace>(source));
    }

    SECTION("com_copy_to(U**)")
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        DestPtr dest1;
        REQUIRE_ERROR(wil::com_copy_to(source, &dest1));
        REQUIRE(!dest1);
#endif

        DestPtr dest2, dest3;
        REQUIRE_ERROR(wil::com_copy_to_failfast(source, &dest2));
        REQUIRE_ERROR(wil::com_copy_to_nothrow(source, &dest3));
        REQUIRE((!dest2 && !dest3));
    }


    SECTION("try_com_copy")
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(!wil::try_com_copy<TargetIFace>(source));
#endif
        REQUIRE(!wil::try_com_copy_failfast<TargetIFace>(source));
        REQUIRE(!wil::try_com_copy_nothrow<TargetIFace>(source));
    }

    SECTION("try_com_copy_to(U**)")
    {
        DestPtr dest1;
        REQUIRE(!wil::try_com_copy_to(source, &dest1));
        REQUIRE(!dest1);
    }

    if (wistd::is_abstract<TargetIFace>::value)
    {
        SECTION("com_query_to(iid, ppv)")
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            REQUIRE_ERROR(wil::com_query_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE(!dest1);
#endif

            DestPtr dest2, dest3;
            REQUIRE_ERROR(wil::com_query_to_failfast(source, IID_PPV_ARGS(&dest2)));
            REQUIRE_ERROR(wil::com_query_to_nothrow(source, IID_PPV_ARGS(&dest3)));
            REQUIRE((!dest2 && !dest3));
        }

        SECTION("try_com_query_to(iid, ppv)")
        {
            DestPtr dest1;
            REQUIRE(!wil::try_com_query_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE(!dest1);
        }

        SECTION("com_copy_to(iid, ppv)")
        {
#ifdef WIL_ENABLE_EXCEPTIONS
            DestPtr dest1;
            REQUIRE_ERROR(wil::com_copy_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE(!dest1);
#endif

            DestPtr dest2, dest3;
            REQUIRE_ERROR(wil::com_copy_to_failfast(source, IID_PPV_ARGS(&dest2)));
            REQUIRE_ERROR(wil::com_copy_to_nothrow(source, IID_PPV_ARGS(&dest3)));
            REQUIRE((!dest2 && !dest3));
        }

        SECTION("try_com_copy_to(iid, ppv)")
        {
            DestPtr dest1;
            REQUIRE(!wil::try_com_copy_to(source, IID_PPV_ARGS(&dest1)));
            REQUIRE(!dest1);
        }
    }
}

template <typename TargetIFace, typename Ptr>
void TestSmartPointerQueryFluentWithFailedResolve(wistd::false_type, const Ptr& /*source*/)
{
}

    template <typename TargetIFace, typename Ptr>
void TestSmartPointerQueryFluentWithFailedResolve(wistd::true_type, const Ptr& source)
{
    REQUIRE_ERROR(source.template query<TargetIFace>());
    REQUIRE_ERROR(source.template copy<TargetIFace>());
}

template <typename TargetIFace, typename Ptr>
void TestSmartPointerQueryWithFailedResolve(const Ptr source)
{
    using DestPtr = wil::com_ptr_nothrow<TargetIFace>;

    SECTION("query_to(U**)")
    {
        DestPtr dest;
        REQUIRE_ERROR(source.query_to(&dest));
        REQUIRE(!dest);
    }

    SECTION("try_query")
    {
        REQUIRE(!source.template try_query<TargetIFace>());
    }

    SECTION("try_query_to(U**)")
    {
        DestPtr dest;
        REQUIRE(!source.try_query_to(&dest));
        REQUIRE(!dest);
    }

    SECTION("copy_to(U**)")
    {
        DestPtr dest;
        REQUIRE_ERROR(source.copy_to(&dest));
        REQUIRE(!dest);
    }

    SECTION("try_copy")
    {
        REQUIRE(!source.template try_copy<TargetIFace>());
    }

    SECTION("try_copy_to(U**)")
    {
        DestPtr dest;
        REQUIRE(!source.try_copy_to(&dest));
        REQUIRE(!dest);
    }

    TestSmartPointerQueryFluentWithFailedResolve<TargetIFace, Ptr>(typename wistd::is_same<void, typename Ptr::result>::type(), source);

    if (wistd::is_abstract<TargetIFace>::value)
    {
        SECTION("query_to(iid, ppv)")
        {
            DestPtr dest;
            REQUIRE_ERROR(source.query_to(IID_PPV_ARGS(&dest)));
            REQUIRE(!dest);
        }

        SECTION("try_query_to(iid, ppv)")
        {
            DestPtr dest;
            REQUIRE(!source.try_query_to(IID_PPV_ARGS(&dest)));
            REQUIRE(!dest);
        }

        SECTION("copy_to(iid, ppv)")
        {
            DestPtr dest;
            REQUIRE_ERROR(source.copy_to(IID_PPV_ARGS(&dest)));
            REQUIRE(!dest);
        }

        SECTION("try_copy_to(iid, ppv)")
        {
            DestPtr dest;
            REQUIRE(!source.try_copy_to(IID_PPV_ARGS(&dest)));
            REQUIRE(!dest);
        }
    }
}

template <typename TargetIFace, typename IFace>
void TestQueryWithFailedResolve(IFace* ptr)
{
    TestGlobalQueryWithFailedResolve<TargetIFace>(ptr);
#ifdef WIL_EXHAUSTIVE_TEST
#ifdef WIL_ENABLE_EXCEPTIONS
    TestGlobalQueryWithFailedResolve<TargetIFace>(wil::com_ptr<IFace>(ptr));
#endif
    TestGlobalQueryWithFailedResolve<TargetIFace>(wil::com_ptr_failfast<IFace>(ptr));
#endif
    TestGlobalQueryWithFailedResolve<TargetIFace>(wil::com_ptr_nothrow<IFace>(ptr));
    TestGlobalQueryWithFailedResolve<TargetIFace>(Microsoft::WRL::ComPtr<IFace>(ptr));

#ifdef WIL_ENABLE_EXCEPTIONS
    TestSmartPointerQueryWithFailedResolve<TargetIFace>(wil::com_ptr<IFace>(ptr));
#endif
    TestSmartPointerQueryWithFailedResolve<TargetIFace>(wil::com_ptr_nothrow<IFace>(ptr));
    TestSmartPointerQueryWithFailedResolve<TargetIFace>(wil::com_ptr_failfast<IFace>(ptr));
}

template <typename IFace>
void TestWeakCombinations()
{
    auto ptr = make_object<IFace, WinRtObject>();

    TestWeak(ptr);
    TestWeak(wil::com_ptr_nothrow<IFace>(ptr));
    TestWeak(Microsoft::WRL::ComPtr<IFace>(ptr));

    auto weakPtr = wil::com_weak_query_failfast(ptr);
    TestQuery<IUnknown>(weakPtr.get()); // Not IInspectable derived
    TestQuery<ITest>(weakPtr.get()); // IInspectable derived

#ifdef WIL_EXHAUSTIVE_TEST
    TestQuery<IInspectable>(weakPtr.get());
    TestQuery<IDerivedTest>(weakPtr.get());
    TestQuery<ITestInspectable>(weakPtr.get());
    TestQuery<IDerivedTestInspectable>(weakPtr.get());
    TestQuery<IAlways>(weakPtr.get());
#endif

    // On the final release of the pointer, the weak reference will no longer resolve
    ptr->Release();
    TestQueryWithFailedResolve<IUnknown>(weakPtr.get());
    TestQueryWithFailedResolve<ITest>(weakPtr.get());
#ifdef WIL_EXHAUSTIVE_TEST
    TestQueryWithFailedResolve<IInspectable>(weakPtr.get());
#endif
}

TEST_CASE("ComTests::Test_Weak", "[com][com_weak_ref]")
{
    // TestWeakCombinations<WinRtObject>();
    TestWeakCombinations<ITest>();
#ifdef WIL_EXHAUSTIVE_TEST
    TestWeakCombinations<IUnknown>();
    TestWeakCombinations<IInspectable>();
    TestWeakCombinations<IDerivedTest>();
    TestWeakCombinations<ITestInspectable>();
    TestWeakCombinations<IDerivedTestInspectable>();
    TestWeakCombinations<IAlways>();
#endif

    REQUIRE_FALSE(witest::g_objectCount.Leaked());
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
TEST_CASE("ComTests::VerifyCoCreate", "[com][CoCreateInstance]")
{
    auto init = wil::CoInitializeEx_failfast();

    // success cases
#ifdef WIL_ENABLE_EXCEPTIONS
    auto link1 = wil::CoCreateInstance<ShellLink>();
    auto link2 = wil::CoCreateInstance(CLSID_ShellLink);
#endif
    auto link3 = wil::CoCreateInstanceFailFast<ShellLink>();
    auto link4 = wil::CoCreateInstanceFailFast(CLSID_ShellLink);
    auto link5 = wil::CoCreateInstanceNoThrow<ShellLink>();
    auto link6 = wil::CoCreateInstanceNoThrow(CLSID_ShellLink);

    // failure
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE_THROWS((wil::CoCreateInstance<ShellLink, IStream>()));
#endif
    // skip this test, assume testing the exception based version is sufficient.
    // auto link2 = wil::CoCreateInstanceFailFast<ShellLink, IStream>();
    REQUIRE_FALSE(static_cast<bool>(wil::CoCreateInstanceNoThrow<ShellLink, IStream>().get()));
}

TEST_CASE("ComTests::VerifyCoGetClassObject", "[com][CoGetClassObject]")
{
    auto init = wil::CoInitializeEx_failfast();

    // success cases
#ifdef WIL_ENABLE_EXCEPTIONS
    auto linkFactory1 = wil::CoGetClassObject<ShellLink>();
    auto linkFactory2 = wil::CoGetClassObject(CLSID_ShellLink);
#endif
    auto linkFactory3 = wil::CoGetClassObjectFailFast<ShellLink>();
    auto linkFactory4 = wil::CoGetClassObjectFailFast(CLSID_ShellLink);
    auto linkFactory5 = wil::CoGetClassObjectNoThrow<ShellLink>();
    auto linkFactory6 = wil::CoGetClassObjectNoThrow(CLSID_ShellLink);

    // failure
#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE_THROWS((wil::CoGetClassObject<ShellLink, IStream>()));
#endif
    // skip this test, assume testing the exception based version is sufficient.
    // auto linkFactory2 = wil::CoGetClassObjectFailFast<ShellLink, IStream>();
    REQUIRE_FALSE(static_cast<bool>(wil::CoGetClassObjectNoThrow<ShellLink, IStream>()));
}
#endif

#if defined(__IBackgroundCopyManager_INTERFACE_DEFINED__) && (__WI_LIBCPP_STD_VER >= 17)
TEST_CASE("ComTests::VerifyCoCreateEx", "[com][CoCreateInstance]")
{
    auto init = wil::CoInitializeEx_failfast();

    {
#ifdef WIL_ENABLE_EXCEPTIONS
        auto [sp1, ps1] = wil::CoCreateInstanceEx<IBackgroundCopyManager, IUnknown>(__uuidof(BackgroundCopyManager), CLSCTX_LOCAL_SERVER);
        REQUIRE((sp1 && ps1));
#endif
        auto [hr, unk] = wil::CoCreateInstanceExNoThrow<IBackgroundCopyManager, IUnknown>(__uuidof(BackgroundCopyManager), CLSCTX_LOCAL_SERVER);
        REQUIRE_SUCCEEDED(hr);
        auto sp = std::get<0>(unk);
        auto ps = std::get<1>(unk);
        REQUIRE((sp && ps));
        auto [sp3, ps3] = wil::CoCreateInstanceExFailFast<IBackgroundCopyManager, IUnknown>(__uuidof(BackgroundCopyManager), CLSCTX_LOCAL_SERVER);
        REQUIRE((sp3 && ps3));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    {
        auto [ps, pf] = wil::CoCreateInstanceEx<IPersistStream, IPersistFile>(__uuidof(ShellLink), CLSCTX_INPROC_SERVER);
        std::ignore = ps->IsDirty();
        std::ignore = pf->IsDirty();
    }
#endif
}

TEST_CASE("ComTests::VerifyCoCreateInstanceExNoThrowMissingInterface", "[com][CoCreateInstance]")
{
    auto init = wil::CoInitializeEx_failfast();

    {
        // IPropertyBag is not implemented
        auto [error, result] = wil::CoCreateInstanceExNoThrow<IBackgroundCopyManager, IUnknown, IPropertyBag>
            (__uuidof(BackgroundCopyManager), CLSCTX_LOCAL_SERVER);
        REQUIRE(error == E_NOINTERFACE);
        REQUIRE(std::get<0>(result).get() == nullptr);
        REQUIRE(std::get<1>(result).get() == nullptr);
        REQUIRE(std::get<2>(result).get() == nullptr);
    }
}

TEST_CASE("ComTests::VerifyTryCoCreateInstanceMissingInterface", "[com][CoCreateInstance]")
{
    auto init = wil::CoInitializeEx_failfast();

    // request some implemented, one not (IPropertyBag), partial results enabled
    {
        auto [sp, pb] = wil::TryCoCreateInstanceEx<IBackgroundCopyManager, IPropertyBag>
            (__uuidof(BackgroundCopyManager), CLSCTX_LOCAL_SERVER);
        REQUIRE(sp != nullptr);
        REQUIRE(pb == nullptr);
    }
    {
        auto [sp, pb] = wil::TryCoCreateInstanceExNoThrow<IBackgroundCopyManager, IPropertyBag>
            (__uuidof(BackgroundCopyManager), CLSCTX_LOCAL_SERVER);
        REQUIRE(sp != nullptr);
        REQUIRE(pb == nullptr);
    }
    {
        auto [sp, pb] = wil::TryCoCreateInstanceExFailFast<IBackgroundCopyManager, IPropertyBag>
            (__uuidof(BackgroundCopyManager), CLSCTX_LOCAL_SERVER);
        REQUIRE(sp != nullptr);
        REQUIRE(pb == nullptr);
    }
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("ComTests::VerifyQueryMultipleInterfaces", "[com][com_multi_query]")
{
    auto init = wil::CoInitializeEx_failfast();

    auto mgr = wil::CoCreateInstance<BackgroundCopyManager>(CLSCTX_LOCAL_SERVER);
    auto [sp, ps] = wil::com_multi_query<IBackgroundCopyManager, IUnknown>(mgr.get());
    REQUIRE(sp);
    REQUIRE(ps);
    auto [sp1, pb] = wil::try_com_multi_query<IBackgroundCopyManager, IPropertyBag>(mgr.get());
    REQUIRE(sp1);
    REQUIRE(!pb);
}
#endif
#endif // __IBackgroundCopyManager_INTERFACE_DEFINED__

#ifdef __IObjectWithSite_INTERFACE_DEFINED__
TEST_CASE("ComTests::VerifyComSetSiteNullIsMoveOnly", "[com][com_set_site]")
{
    wil::unique_set_site_null_call call1;

    // intentional compilation errors for copy construction/assignment
    // wil::unique_set_site_null_call call2 = call1;
    // call2 = call1;

    auto siteSetter = wil::com_set_site(nullptr, nullptr);
    auto siteSetter2 = std::move(siteSetter); // Move construction
    siteSetter2 = std::move(siteSetter); // Move assignment
}

TEST_CASE("ComTests::VerifyComSetSite", "[com][com_set_site]")
{
    class ObjectWithSite WrlFinal : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IObjectWithSite>
    {
    public:
        STDMETHODIMP SetSite(IUnknown* val) noexcept override
        {
            m_site = val;
            return S_OK;
        }

        STDMETHODIMP GetSite(REFIID riid, void** ppv) noexcept override
        {
            m_site.try_copy_to(riid, ppv);
            return S_OK;
        }

    private:
        wil::com_ptr_nothrow<IUnknown> m_site;
    };

    class ServiceObject WrlFinal : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IServiceProvider>
    {
    public:
        ServiceObject(IServiceProvider* site = nullptr)
        {
            m_site = site;
        }

        STDMETHODIMP QueryService(REFIID /*sid*/, REFIID /*riid*/, void** ppv) noexcept override
        {
            *ppv = nullptr;
            return E_NOTIMPL;
        }
    private:
        wil::com_ptr_nothrow<IUnknown> m_site;
    };

    auto objWithSite = Make<ObjectWithSite>();
    auto serviceObj = Make<ServiceObject>();
    auto serviceObj2 = Make<ServiceObject>(serviceObj.Get());

    {
        auto cleanupSite = wil::com_set_site(objWithSite.Get(), serviceObj2.Get());

        wil::com_ptr_nothrow<IUnknown> site;
        REQUIRE_SUCCEEDED(objWithSite->GetSite(IID_PPV_ARGS(&site)));
        REQUIRE(static_cast<bool>(site));

        auto siteCount = 0;
        wil::for_each_site(objWithSite.Get(), [&](IUnknown* /*site*/)
        {
            siteCount++;
        });
        REQUIRE(siteCount == 2);
    }

    wil::com_ptr_nothrow<IUnknown> site;
    REQUIRE_SUCCEEDED(objWithSite->GetSite(IID_PPV_ARGS(&site)));
    REQUIRE_FALSE(static_cast<bool>(site));
}
#endif

class FakeStream : public IStream
{
public:

    STDMETHOD(QueryInterface)(REFIID riid, PVOID* ppv) override
    {
        if ((riid == __uuidof(IStream)) ||
            (riid == __uuidof(ISequentialStream)) ||
            (riid == __uuidof(IUnknown)))
        {
            *ppv = static_cast<IStream*>(this);
            return S_OK;
        }

        return E_NOTIMPL;
    }

    STDMETHOD_(ULONG, AddRef)() override
    {
        return 2;
    }

    STDMETHOD_(ULONG, Release)() override
    {
        return 1;
    }

    unsigned long long Position = 0;
    unsigned long long PositionMax = 0;
    unsigned long MaxReadSize = 0;
    unsigned long MaxWriteSize = 0;
    unsigned long long TotalSize = 0;

    // ISequentialStream
    STDMETHOD(Read)(_Out_writes_bytes_to_(cb, *pcbRead) void *pv, _In_ ULONG cb, _Out_opt_ ULONG *pcbRead) override
    {
        if (pcbRead)
        {
            *pcbRead = min(MaxReadSize, cb);
        }

        ZeroMemory(pv, cb);
        return (MaxReadSize <= cb) ? S_OK : S_FALSE;
    }

    STDMETHOD(Write)(_In_reads_bytes_(cb) const void *, _In_ ULONG cb, _Out_opt_ ULONG *pcbWritten) override
    {
        if (pcbWritten)
        {
            *pcbWritten = min(MaxWriteSize, cb);
        }

        return (MaxWriteSize <= cb) ? S_OK : S_FALSE;
    }

    // IStream
    STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin, _Out_opt_ ULARGE_INTEGER *plibNewPosition)
    {
        if (dwOrigin == STREAM_SEEK_CUR)
        {
            if ((dlibMove.QuadPart < 0) && (static_cast<unsigned long long>(-dlibMove.QuadPart) > Position))
            {
                Position = 0;
            }
            else
            {
                Position += dlibMove.QuadPart;
            }
        }
        else if (dwOrigin == STREAM_SEEK_SET)
        {
            Position = static_cast<unsigned long long>(dlibMove.QuadPart);
        }
        else if (dwOrigin == STREAM_SEEK_END)
        {
            if ((dlibMove.QuadPart < 0) && (static_cast<unsigned long long>(-dlibMove.QuadPart) > Position))
            {
                Position = 0;
            }
            else
            {
                Position = PositionMax + dlibMove.QuadPart;
            }
        }

        Position = min(Position, PositionMax);

        if (plibNewPosition)
        {
            plibNewPosition->QuadPart = Position;
        }

        return S_OK;
    }

    STDMETHOD(Stat)(__RPC__out STATSTG *pstatstg, DWORD) override
    {
        *pstatstg = {};
        pstatstg->cbSize.QuadPart = TotalSize;
        return S_OK;
    }

    STDMETHOD(Revert)(void) override
    {
        return E_NOTIMPL;
    }

    STDMETHOD(SetSize)(ULARGE_INTEGER) override
    {
        return E_NOTIMPL;
    }

    STDMETHOD(Clone)(__RPC__deref_out_opt IStream **ppstm) override
    {
        *ppstm = this;
        return S_OK;
    }

    STDMETHOD(Commit)(DWORD) override
    {
        return E_NOTIMPL;
    }

    STDMETHOD(CopyTo)(_In_ IStream *pstm, ULARGE_INTEGER cb, _Out_opt_ ULARGE_INTEGER *pcbRead, _Out_opt_ ULARGE_INTEGER *pcbWritten) override
    {
        unsigned long didWrite;
        unsigned long didRead;

        FAIL_FAST_IF(cb.HighPart != 0);
        RETURN_IF_FAILED(this->Read(nullptr, cb.LowPart, &didRead));
        RETURN_IF_FAILED(pstm->Write(nullptr, didRead, &didWrite));

        pcbRead->QuadPart = didRead;
        pcbWritten->QuadPart = didWrite;

        return S_OK;
    }

    STDMETHOD(LockRegion)(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override
    {
        return E_NOTIMPL;
    }

    STDMETHOD(UnlockRegion)(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override
    {
        return E_NOTIMPL;
    }

    void SetPosition(unsigned long long position, unsigned long long positionMax)
    {
        Position = position;
        PositionMax = positionMax;
    }

    void SetPosition(unsigned long long position)
    {
        return SetPosition(position, position);
    }
};

TEST_CASE("StreamTests::ReadPartial", "[com][IStream]")
{
    FakeStream stream;
    stream.MaxReadSize = 16;
    BYTE buffer[32];
    ULONG readSize;

    // Reading more than what's available is OK
    REQUIRE_SUCCEEDED(wil::stream_read_partial_nothrow(&stream, buffer, 32, &readSize));
    REQUIRE(stream.MaxReadSize == readSize);

    // Reading less than what's available is OK
    REQUIRE_SUCCEEDED(wil::stream_read_partial_nothrow(&stream, buffer, 5, &readSize));
    REQUIRE(5 == readSize);

#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(stream.MaxReadSize == wil::stream_read_partial(&stream, buffer, 32));
    REQUIRE(5ULL == wil::stream_read_partial(&stream, buffer, 5));
#endif
}

TEST_CASE("StreamTests::Read", "[com][IStream]")
{
    FakeStream stream;
    stream.MaxReadSize = 10;
    BYTE buffer[32];

    // Reading less than available is OK
    REQUIRE_SUCCEEDED(wil::stream_read_nothrow(&stream, buffer, 5));

    // Reading more is not.
    REQUIRE(stream.MaxReadSize < sizeof(buffer));
    REQUIRE_FAILED(wil::stream_read_nothrow(&stream, buffer, sizeof(buffer)));

    struct Header
    {
        ULONG Flags;
        ULONG Other;
    } header;

    // Reading a POD when there's not enough fails
    stream.MaxReadSize = sizeof(header) - 1;
    REQUIRE_FAILED(wil::stream_read_nothrow(&stream, &header));

    // Reading a POD when there is is OK (and prove that the read happened)
    header.Flags = 1;
    header.Other = 2;
    stream.MaxReadSize = sizeof(header);
    REQUIRE_SUCCEEDED(wil::stream_read_nothrow(&stream, &header));
    REQUIRE(0UL == header.Flags);
    REQUIRE(0UL == header.Other);

#ifdef WIL_ENABLE_EXCEPTIONS
    // Reading less than available is OK
    REQUIRE_NOTHROW(wil::stream_read(&stream, buffer, 5));
    REQUIRE_THROWS(wil::stream_read(&stream, buffer, sizeof(buffer)));

    // Reading a POD when there's not enough fails
    stream.MaxReadSize = sizeof(Header) - 1;
    REQUIRE_THROWS(wil::stream_read<Header>(&stream));

    // Reading a POD when there is is OK (and prove that the read happened)
    stream.MaxReadSize = sizeof(Header);
    header = wil::stream_read<Header>(&stream);
    REQUIRE(0UL == header.Flags);
    REQUIRE(0UL == header.Other);
#endif
}

TEST_CASE("StreamTests::Write", "[com][IStream]")
{
    FakeStream stream;
    BYTE buffer[16] = { 8, 6, 7, 5, 3, 0, 9 };

    stream.MaxWriteSize = sizeof(buffer) + 1;
    REQUIRE_SUCCEEDED(wil::stream_write_nothrow(&stream, buffer, sizeof(buffer)));

    stream.MaxWriteSize = sizeof(buffer) - 1;
    REQUIRE_FAILED(wil::stream_write_nothrow(&stream, buffer, sizeof(buffer)));

    struct Header
    {
        ULONG Flags;
        ULONG Other;
    } header = { 1, 2 };

    stream.MaxWriteSize = sizeof(header) + 1;
    REQUIRE_SUCCEEDED(wil::stream_write_nothrow(&stream, header));

    stream.MaxWriteSize = sizeof(header) - 1;
    REQUIRE_FAILED(wil::stream_write_nothrow(&stream, header));

#ifdef WIL_ENABLE_EXCEPTIONS
    stream.MaxWriteSize = sizeof(buffer) + 1;
    REQUIRE_NOTHROW(wil::stream_write(&stream, buffer, sizeof(buffer)));

    stream.MaxWriteSize = sizeof(buffer) - 1;
    REQUIRE_THROWS(wil::stream_write(&stream, buffer, sizeof(buffer)));

    header = { 1, 2 };
    stream.MaxWriteSize = sizeof(header) + 1;
    REQUIRE_NOTHROW(wil::stream_write(&stream, header));

    stream.MaxWriteSize = sizeof(header) - 1;
    REQUIRE_THROWS(wil::stream_write(&stream, header));
#endif
}

TEST_CASE("StreamTests::Size", "[com][IStream]")
{
    FakeStream stream;
    unsigned long long size;

    stream.TotalSize = 150;
    REQUIRE_SUCCEEDED(wil::stream_size_nothrow(&stream, &size));
    REQUIRE(stream.TotalSize == size);

#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(stream.TotalSize == wil::stream_size(&stream));
#endif
}

TEST_CASE("StreamTests::SeekStart", "[com][IStream]")
{
    FakeStream stream;
    unsigned long long landed;

    // Seek within the stream
    stream.SetPosition(100, 1000);
    REQUIRE_SUCCEEDED(wil::stream_set_position_nothrow(&stream, 10));
    REQUIRE(10ULL == stream.Position);

    // Seek and get the landing position
    REQUIRE_SUCCEEDED(wil::stream_set_position_nothrow(&stream, 11, &landed));
    REQUIRE(11ULL == stream.Position);
    REQUIRE(11ULL == landed);

    // Seek past the end
    REQUIRE_SUCCEEDED(wil::stream_set_position_nothrow(&stream, 5000, &landed));
    REQUIRE(stream.PositionMax == landed);

    // Seek to the start
    REQUIRE_SUCCEEDED(wil::stream_reset_nothrow(&stream));
    REQUIRE(0ULL == stream.Position);

#ifdef WIL_ENABLE_EXCEPTIONS
    // Seek within the stream
    stream.SetPosition(100, 1000);
    REQUIRE(10ULL == wil::stream_set_position(&stream, 10));

    // Seek past the end
    REQUIRE(stream.PositionMax == wil::stream_set_position(&stream, 5000));

    // Seek to the start
    REQUIRE_NOTHROW(wil::stream_reset(&stream));
    REQUIRE(0ULL == stream.Position);
#endif
}

TEST_CASE("StreamTests::SeekCur", "[com][IStream]")
{
    FakeStream stream;
    unsigned long long landed;

    stream.SetPosition(100, 5000);
    REQUIRE_SUCCEEDED(wil::stream_seek_from_current_position_nothrow(&stream, 10, &landed));
    REQUIRE(110ULL == landed);

    REQUIRE_SUCCEEDED(wil::stream_seek_from_current_position_nothrow(&stream, -10, &landed));
    REQUIRE(100ULL == landed);

    REQUIRE_SUCCEEDED(wil::stream_seek_from_current_position_nothrow(&stream, -1000, &landed));
    REQUIRE(0ULL == landed);

    REQUIRE_SUCCEEDED(wil::stream_seek_from_current_position_nothrow(&stream, 6000, &landed));
    REQUIRE(5000ULL == landed);

#ifdef WIL_ENABLE_EXCEPTIONS
    stream.SetPosition(100, 5000);

    REQUIRE(110ULL == wil::stream_seek_from_current_position(&stream, 10));

    REQUIRE(100ULL == wil::stream_seek_from_current_position(&stream, -10));

    REQUIRE(0ULL == wil::stream_seek_from_current_position(&stream, -1000));

    REQUIRE(5000ULL == wil::stream_seek_from_current_position(&stream, 6000));
#endif
}

TEST_CASE("StreamTests::GetPosition", "[com][IStream]")
{
    FakeStream stream;
    unsigned long long landed;

    stream.SetPosition(50);
    REQUIRE_SUCCEEDED(wil::stream_get_position_nothrow(&stream, &landed));
    REQUIRE(stream.Position == landed);

#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(stream.Position == wil::stream_get_position(&stream));
#endif
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("StreamTests::Saver", "[com][IStream]")
{
    FakeStream first;
    FakeStream second;

    first.SetPosition(200);
    {
        auto saved = wil::stream_position_saver(&first);
        first.SetPosition(250);
    }
    REQUIRE(200ULL == first.Position);

    first.SetPosition(200);
    {
        auto saved = wil::stream_position_saver(&first);
        first.SetPosition(250);
        saved.reset();
        REQUIRE(200ULL == first.Position);
    }

    first.SetPosition(200);
    {
        auto saved = wil::stream_position_saver(&first);
        first.SetPosition(250);
        saved.dismiss();
    }
    REQUIRE(250ULL == first.Position);

    first.SetPosition(200);
    second.SetPosition(250);
    {
        auto saved = wil::stream_position_saver(&first);
        first.SetPosition(210);
        saved.reset(&second);
        REQUIRE(200ULL == first.Position);

        second.SetPosition(300);
        saved.reset();
        REQUIRE(250ULL == second.Position);
    }
}
#endif
