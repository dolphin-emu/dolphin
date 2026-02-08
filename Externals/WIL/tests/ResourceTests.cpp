
// Included first and then again later to ensure that we're able to "light up" new functionality based off new includes
#include <wil/resource.h>

#include <wil/com.h>
#include <wil/stl.h>

// Headers to "light up" functionality in resource.h
#include <memory>
#include <roapi.h>
#include <winstring.h>
#include <WinUser.h>

#include <wil/resource.h>
#include <wrl/implements.h>

#include "common.h"

TEST_CASE("ResourceTests::TestLastErrorContext", "[resource][last_error_context]")
{
    // Destructing the last_error_context restores the error.
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(0);
    }
    REQUIRE(GetLastError() == 42);

    // The context can be moved.
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(0);
        {
            auto another_error42 = wil::last_error_context(std::move(error42));
            SetLastError(1);
        }
        REQUIRE(GetLastError() == 42);
        SetLastError(0);
        // error42 has been moved-from and should not do anything at destruction.
    }
    REQUIRE(GetLastError() == 0);

    // The context can be self-assigned, which has no effect.
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(0);
        error42 = std::move(error42);
        SetLastError(1);
    }
    REQUIRE(GetLastError() == 42);

    // The context can be dismissed, which cause it to do nothing at destruction.
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(0);
        error42.release();
        SetLastError(1);
    }
    REQUIRE(GetLastError() == 1);

    // The value in the context is unimpacted by other things changing the last error
    {
        SetLastError(42);
        auto error42 = wil::last_error_context();
        SetLastError(1);
        REQUIRE(error42.value() == 42);
    }
}

TEST_CASE("ResourceTests::TestScopeExit", "[resource][scope_exit]")
{
    int count = 0;
    auto validate = [&](int expected) { REQUIRE(count == expected); count = 0; };

    {
        auto foo = wil::scope_exit([&] { count++; });
    }
    validate(1);

    {
        auto foo = wil::scope_exit([&] { count++; });
        foo.release();
        foo.reset();
    }
    validate(0);

    {
        auto foo = wil::scope_exit([&] { count++; });
        foo.reset();
        foo.reset();
        validate(1);
    }
    validate(0);

#ifdef WIL_ENABLE_EXCEPTIONS
    {
        auto foo = wil::scope_exit_log(WI_DIAGNOSTICS_INFO, [&] { count++; THROW_HR(E_FAIL); });
    }
    validate(1);

    {
        auto foo = wil::scope_exit_log(WI_DIAGNOSTICS_INFO, [&] { count++; THROW_HR(E_FAIL); });
        foo.release();
        foo.reset();
    }
    validate(0);

    {
        auto foo = wil::scope_exit_log(WI_DIAGNOSTICS_INFO, [&] { count++; THROW_HR(E_FAIL); });
        foo.reset();
        foo.reset();
        validate(1);
    }
    validate(0);
#endif // WIL_ENABLE_EXCEPTIONS
}

interface __declspec(uuid("ececcc6a-5193-4d14-b38e-ed1460c20b00"))
ITest : public IUnknown
{
   STDMETHOD_(void, Test)() = 0;
};

class PointerTestObject : witest::AllocatedObject,
    public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>, ITest>
{
public:
    STDMETHOD_(void, Test)() {};
};

TEST_CASE("ResourceTests::TestOperationsOnGenericSmartPointerClasses", "[resource]")
{
#ifdef WIL_ENABLE_EXCEPTIONS
    {
        // wil::unique_any_t example
        wil::unique_event ptr2(wil::EventOptions::ManualReset);
        // wil::com_ptr
        wil::com_ptr<PointerTestObject> ptr3 = Microsoft::WRL::Make<PointerTestObject>();
        // wil::shared_any_t example
        wil::shared_event ptr4(wil::EventOptions::ManualReset);
        // wistd::unique_ptr example
        auto ptr5 = wil::make_unique_failfast<POINT>();

        static_assert(wistd::is_same<typename wil::smart_pointer_details<decltype(ptr2)>::pointer, HANDLE>::value, "type-mismatch");
        static_assert(wistd::is_same<typename wil::smart_pointer_details<decltype(ptr3)>::pointer, PointerTestObject*>::value, "type-mismatch");

        auto p2 = wil::detach_from_smart_pointer(ptr2);
        auto p3 = wil::detach_from_smart_pointer(ptr3);
        // auto p4 = wil::detach_from_smart_pointer(ptr4); // wil::shared_any_t and std::shared_ptr do not support release().
        HANDLE p4{};
        auto p5 = wil::detach_from_smart_pointer(ptr5);

        REQUIRE((!ptr2 && !ptr3));
        REQUIRE((p2 && p3));

        wil::attach_to_smart_pointer(ptr2, p2);
        wil::attach_to_smart_pointer(ptr3, p3);
        wil::attach_to_smart_pointer(ptr4, p4);
        wil::attach_to_smart_pointer(ptr5, p5);

        p2 = nullptr;
        p3 = nullptr;
        p4 = nullptr;
        p5 = nullptr;

        wil::detach_to_opt_param(&p2, ptr2);
        wil::detach_to_opt_param(&p3, ptr3);

        REQUIRE((!ptr2 && !ptr3));
        REQUIRE((p2 && p3));

        wil::attach_to_smart_pointer(ptr2, p2);
        wil::attach_to_smart_pointer(ptr3, p3);
        p2 = nullptr;
        p3 = nullptr;

        wil::detach_to_opt_param(&p2, ptr2);
        wil::detach_to_opt_param(&p3, ptr3);
        REQUIRE((!ptr2 && !ptr3));
        REQUIRE((p2 && p3));

        [&](decltype(p2)* ptr) { *ptr = p2; } (wil::out_param(ptr2));
        [&](decltype(p3)* ptr) { *ptr = p3; } (wil::out_param(ptr3));
        [&](decltype(p4)* ptr) { *ptr = p4; } (wil::out_param(ptr4));
        [&](decltype(p5)* ptr) { *ptr = p5; } (wil::out_param(ptr5));

        REQUIRE((ptr2 && ptr3));

        // Validate R-Value compilation
        wil::detach_to_opt_param(&p2, decltype(ptr2){});
        wil::detach_to_opt_param(&p3, decltype(ptr3){});
    }
#endif

    std::unique_ptr<int> ptr1(new int(1));
    Microsoft::WRL::ComPtr<PointerTestObject> ptr4 = Microsoft::WRL::Make<PointerTestObject>();

    static_assert(wistd::is_same<typename wil::smart_pointer_details<decltype(ptr1)>::pointer, int*>::value, "type-mismatch");
    static_assert(wistd::is_same<typename wil::smart_pointer_details<decltype(ptr4)>::pointer, PointerTestObject*>::value, "type-mismatch");

    auto p1 = wil::detach_from_smart_pointer(ptr1);
    auto p4 = wil::detach_from_smart_pointer(ptr4);

    REQUIRE((!ptr1 && !ptr4));
    REQUIRE((p1 && p4));

    wil::attach_to_smart_pointer(ptr1, p1);
    wil::attach_to_smart_pointer(ptr4, p4);

    REQUIRE((ptr1 && ptr4));

    p1 = nullptr;
    p4 = nullptr;

    int** pNull = nullptr;
    wil::detach_to_opt_param(pNull, ptr1);
    REQUIRE(ptr1);

    wil::detach_to_opt_param(&p1, ptr1);
    wil::detach_to_opt_param(&p4, ptr4);

    REQUIRE((!ptr1 && !ptr4));
    REQUIRE((p1 && p4));

    [&](decltype(p1)* ptr) { *ptr = p1; } (wil::out_param(ptr1));
    [&](decltype(p4)* ptr) { *ptr = p4; } (wil::out_param(ptr4));

    REQUIRE((ptr1 && ptr4));

    p1 = wil::detach_from_smart_pointer(ptr1);
    [&](int** ptr) { *ptr = p1; } (wil::out_param_ptr<int **>(ptr1));
    REQUIRE(ptr1);
}

// Compilation only test...
void StlAdlTest()
{
    // This test has exposed some Argument Dependent Lookup issues in wistd / stl.  Primarily we're
    // just looking for clean compilation.

    std::vector<wistd::unique_ptr<int>> v;
    v.emplace_back(new int{ 1 });
    v.emplace_back(new int{ 2 });
    v.emplace_back(new int{ 3 });
    std::rotate(begin(v), begin(v) + 1, end(v));

    REQUIRE(*v[0] == 1);
    REQUIRE(*v[1] == 3);
    REQUIRE(*v[2] == 2);

    decltype(v) v2;
    v2 = std::move(v);
    REQUIRE(*v2[0] == 1);
    REQUIRE(*v2[1] == 3);
    REQUIRE(*v2[2] == 2);

    decltype(v) v3;
    std::swap(v2, v3);
    REQUIRE(*v3[0] == 1);
    REQUIRE(*v3[1] == 3);
    REQUIRE(*v3[2] == 2);
}

// Compilation only test...
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
void UniqueProcessInfo()
{
    wil::unique_process_information process;
    CreateProcessW(nullptr, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, nullptr, &process);
    ResumeThread(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    wil::unique_process_information other(wistd::move(process));
}
#endif

// Compilation only test...
#ifdef WIL_ENABLE_EXCEPTIONS
void NoexceptConstructibleTest()
{
    using BaseStorage = wil::details::unique_storage<wil::details::handle_resource_policy>;

    struct ThrowingConstructor : BaseStorage
    {
        ThrowingConstructor() = default;
        explicit ThrowingConstructor(HANDLE) __WI_NOEXCEPT_(false) {}
    };

    struct ProtectedConstructor : BaseStorage
    {
    protected:
        ProtectedConstructor() = default;
        explicit ProtectedConstructor(HANDLE) WI_NOEXCEPT {}
    };

    // wil::unique_handle is one of the many types which are expected to be noexcept
    // constructible since they don't perform any "advanced" initialization.
    static_assert(wistd::is_nothrow_default_constructible_v<wil::unique_handle>, "wil::unique_any_t should always be nothrow default constructible");
    static_assert(wistd::is_nothrow_constructible_v<wil::unique_handle, HANDLE>, "wil::unique_any_t should be noexcept if the storage is");

    // The inverse: A throwing storage constructor.
    static_assert(wistd::is_nothrow_default_constructible_v<wil::unique_any_t<ThrowingConstructor>>, "wil::unique_any_t should always be nothrow default constructible");
    static_assert(!wistd::is_nothrow_constructible_v<wil::unique_any_t<ThrowingConstructor>, HANDLE>, "wil::unique_any_t shouldn't be noexcept if the storage isn't");

    // With a protected constructor wil::unique_any_t will be unable to correctly
    // "forward" the noexcept attribute, but the code should still compile.
    wil::unique_any_t<ProtectedConstructor> p{ INVALID_HANDLE_VALUE };
}
#endif

struct FakeComInterface
{
    void AddRef()
    {
        refs++;
    }
    void Release()
    {
        refs--;
    }

    HRESULT __stdcall Close()
    {
        closes++;
        return S_OK;
    }

    size_t refs = 0;
    size_t closes = 0;

    bool called()
    {
        auto old = closes;
        closes = 0;
        return (old > 0);
    }

    bool has_ref()
    {
        return (refs > 0);
    }
};

static void __stdcall CloseFakeComInterface(FakeComInterface* fake)
{
    fake->Close();
}

using unique_fakeclose_call = wil::unique_com_call<FakeComInterface, decltype(&CloseFakeComInterface), CloseFakeComInterface>;

TEST_CASE("ResourceTests::VerifyUniqueComCall", "[resource][unique_com_call]")
{
    unique_fakeclose_call call1;
    unique_fakeclose_call call2;

    // intentional compilation errors
    // unique_fakeclose_call call3 = call1;
    // call2 = call1;

    FakeComInterface fake1;
    unique_fakeclose_call call4(&fake1);
    REQUIRE(fake1.has_ref());

    unique_fakeclose_call call5(wistd::move(call4));
    REQUIRE(!call4);
    REQUIRE(call5);
    REQUIRE(fake1.has_ref());

    call4 = wistd::move(call5);
    REQUIRE(call4);
    REQUIRE(!call5);
    REQUIRE(fake1.has_ref());
    REQUIRE(!fake1.called());

    FakeComInterface fake2;
    {
        unique_fakeclose_call scoped(&fake2);
    }
    REQUIRE(!fake2.has_ref());
    REQUIRE(fake2.called());

    call4.reset(&fake2);
    REQUIRE(fake1.called());
    REQUIRE(!fake1.has_ref());
    call4.reset();
    REQUIRE(!fake2.has_ref());
    REQUIRE(fake2.called());

    call1.reset(&fake1);
    call2.swap(call1);
    REQUIRE((call2 && !call1));

    call2.release();
    REQUIRE(!fake1.called());
    REQUIRE(!fake1.has_ref());
    REQUIRE(!call2);

    REQUIRE(*call1.addressof() == nullptr);

    call1.reset(&fake1);
    fake2.closes = 0;
    fake2.refs = 1;
    *(&call1) = &fake2;
    REQUIRE(!fake1.has_ref());
    REQUIRE(fake1.called());
    REQUIRE(fake2.has_ref());

    call1.reset(&fake1);
    fake2.closes = 0;
    fake2.refs = 1;
    *call1.put() = &fake2;
    REQUIRE(!fake1.has_ref());
    REQUIRE(fake1.called());
    REQUIRE(fake2.has_ref());

    call1.reset();
    REQUIRE(!fake2.has_ref());
    REQUIRE(fake2.called());
}

static bool g_called = false;
static bool called()
{
    auto call = g_called;
    g_called = false;
    return (call);
}

static void __stdcall FakeCall()
{
    g_called = true;
}

using unique_fake_call = wil::unique_call<decltype(&FakeCall), FakeCall>;

TEST_CASE("ResourceTests::VerifyUniqueCall", "[resource][unique_call]")
{
    unique_fake_call call1;
    unique_fake_call call2;

    // intentional compilation errors
    // unique_fake_call call3 = call1;
    // call2 = call1;

    unique_fake_call call4;
    REQUIRE(!called());

    unique_fake_call call5(wistd::move(call4));
    REQUIRE(!call4);
    REQUIRE(call5);

    call4 = wistd::move(call5);
    REQUIRE(call4);
    REQUIRE(!call5);
    REQUIRE(!called());

    {
        unique_fake_call scoped;
    }
    REQUIRE(called());

    call4.reset();
    REQUIRE(called());
    call4.reset();
    REQUIRE(!called());

    call1.release();
    REQUIRE((!call1 && call2));
    call2.swap(call1);
    REQUIRE((call1 && !call2));

    call2.release();
    REQUIRE(!called());
    REQUIRE(!call2);

#ifdef __WIL__ROAPI_H_APPEXCEPTIONAL
    {
        auto call = wil::RoInitialize();
    }
#endif
#ifdef __WIL__ROAPI_H_APP
    {
        wil::unique_rouninitialize_call uninit;
        uninit.release();

        auto call = wil::RoInitialize_failfast();
    }
#endif
#ifdef __WIL__COMBASEAPI_H_APPEXCEPTIONAL
    {
        auto call = wil::CoInitializeEx();
    }
#endif
#ifdef __WIL__COMBASEAPI_H_APP
    {
        wil::unique_couninitialize_call uninit;
        uninit.release();

        auto call = wil::CoInitializeEx_failfast();
    }
#endif
}

void UniqueCallCompilationTest()
{
#ifdef __WIL__COMBASEAPI_H_EXCEPTIONAL
    {
        auto call = wil::CoImpersonateClient();
    }
#endif
#ifdef __WIL__COMBASEAPI_H_
    {
        wil::unique_coreverttoself_call uninit;
        uninit.release();

        auto call = wil::CoImpersonateClient_failfast();
    }
#endif
}

template<typename StringType, typename VerifyContents>
static void TestStringMaker(VerifyContents&& verifyContents)
{
    PCWSTR values[] =
    {
        L"",
        L"value",
        // 300 chars
        L"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
        L"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
        L"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"
    };

    for (const auto& value : values)
    {
        auto const valueLength = wcslen(value);

        // Direct construction case.
        wil::details::string_maker<StringType> maker;
        THROW_IF_FAILED(maker.make(value, valueLength));
        auto result = maker.release();
        verifyContents(value, valueLength, result);

        // Two phase construction case.
        THROW_IF_FAILED(maker.make(nullptr, valueLength));
        REQUIRE(maker.buffer() != nullptr);
        // In the case of the wil::unique_hstring and the empty string the buffer is in a read only
        // section and can't be written to, so StringCchCopy(maker.buffer(), valueLength + 1, value) will fault adding the nul terminator.
        // Use memcpy_s specifying exact size that will be zero in this case instead.
        memcpy_s(maker.buffer(), valueLength * sizeof(*value), value, valueLength * sizeof(*value));
        result = maker.release();
        verifyContents(value, valueLength, result);

        {
            // no promote, ensure no leaks (not tested here, inspect in the debugger)
            wil::details::string_maker<StringType> maker2;
            THROW_IF_FAILED(maker2.make(value, valueLength));
        }
    }
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename StringType>
static void VerifyMakeUniqueString(bool nullValueSupported = true)
{
    if (nullValueSupported)
    {
        auto value0 = wil::make_unique_string<StringType>(nullptr, 5);
    }

    struct
    {
        PCWSTR expectedValue;
        PCWSTR testValue;
        // this is an optional parameter
        size_t testLength = static_cast<size_t>(-1);
    }
    const testCaseEntries[] =
    {
        { L"value", L"value", 5 },
        { L"value", L"value" },
        { L"va", L"va\0ue", 5 },
        { L"v", L"value", 1 },
        { L"\0", L"", 5 },
        { L"\0", nullptr, 5 },
    };

    using maker = wil::details::string_maker<StringType>;
    for (auto const &entry : testCaseEntries)
    {
        bool shouldSkipNullString = ((wcscmp(entry.expectedValue, L"\0") == 0) && !nullValueSupported);
        if (!shouldSkipNullString)
        {
            auto desiredValue = wil::make_unique_string<StringType>(entry.expectedValue);
            auto stringValue = wil::make_unique_string<StringType>(entry.testValue, entry.testLength);
            auto stringValueNoThrow = wil::make_unique_string_nothrow<StringType>(entry.testValue, entry.testLength);
            auto stringValueFailFast = wil::make_unique_string_failfast<StringType>(entry.testValue, entry.testLength);
            REQUIRE(wcscmp(maker::get(desiredValue), maker::get(stringValue)) == 0);
            REQUIRE(wcscmp(maker::get(desiredValue), maker::get(stringValueNoThrow)) == 0);
            REQUIRE(wcscmp(maker::get(desiredValue), maker::get(stringValueFailFast)) == 0);
        }
    }
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerCoTaskMem", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_cotaskmem_string>();
    TestStringMaker<wil::unique_cotaskmem_string>(
        [](PCWSTR value, size_t /*valueLength*/, const wil::unique_cotaskmem_string& result)
    {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerLocalAlloc", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_hlocal_string>();
    TestStringMaker<wil::unique_hlocal_string>(
        [](PCWSTR value, size_t /*valueLength*/, const wil::unique_hlocal_string& result)
    {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerGlobalAlloc", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_hglobal_string>();
    TestStringMaker<wil::unique_hglobal_string>(
        [](PCWSTR value, size_t /*valueLength*/, const wil::unique_hglobal_string& result)
    {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerProcessHeap", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_process_heap_string>();
    TestStringMaker<wil::unique_process_heap_string>(
        [](PCWSTR value, size_t /*valueLength*/, const wil::unique_process_heap_string& result)
    {
        REQUIRE(wcscmp(value, result.get()) == 0);
    });
}
#endif

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerMidl", "[resource][string_maker]")
{
    VerifyMakeUniqueString<wil::unique_midl_string>();
    TestStringMaker<wil::unique_midl_string>(
        [](PCWSTR value, size_t /*valueLength*/, const wil::unique_midl_string& result)
        {
            REQUIRE(wcscmp(value, result.get()) == 0);
        });
}

TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerHString", "[resource][string_maker]")
{
    wil::unique_hstring value;
    value.reset(static_cast<HSTRING>(nullptr));

    VerifyMakeUniqueString<wil::unique_hstring>(false);

    TestStringMaker<wil::unique_hstring>(
        [](PCWSTR value, size_t valueLength, const wil::unique_hstring& result)
    {
        UINT32 length;
        REQUIRE(wcscmp(value, WindowsGetStringRawBuffer(result.get(), &length)) == 0);
        REQUIRE(valueLength == length);
    });
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("UniqueStringAndStringMakerTests::VerifyStringMakerStdWString", "[resource][string_maker]")
{
    std::string s;
    wil::details::string_maker<std::wstring> maker;

    TestStringMaker<std::wstring>(
        [](PCWSTR value, size_t valueLength, const std::wstring& result)
    {
        REQUIRE(wcscmp(value, result.c_str()) == 0);
        REQUIRE(result == value);
        REQUIRE(result.size() == valueLength);
    });
}
#endif

TEST_CASE("UniqueStringAndStringMakerTests::VerifyLegacySTringMakers", "[resource][string_maker]")
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    auto l = wil::make_hlocal_string(L"value");
    l = wil::make_hlocal_string_nothrow(L"value");
    l = wil::make_hlocal_string_failfast(L"value");

    auto p = wil::make_process_heap_string(L"value");
    p = wil::make_process_heap_string_nothrow(L"value");
    p = wil::make_process_heap_string_failfast(L"value");
#endif
    auto c = wil::make_cotaskmem_string(L"value");
    c = wil::make_cotaskmem_string_nothrow(L"value");
    c = wil::make_cotaskmem_string_failfast(L"value");
}
#endif

_Use_decl_annotations_ void* __RPC_USER MIDL_user_allocate(size_t size)
{
    return ::HeapAlloc(GetProcessHeap(), 0, size);
}

_Use_decl_annotations_ void __RPC_USER MIDL_user_free(void* p)
{
    ::HeapFree(GetProcessHeap(), 0, p);
}

TEST_CASE("UniqueMidlStringTests", "[resource][rpc]")
{
    wil::unique_midl_ptr<int[]> intArray{ reinterpret_cast<int*>(::MIDL_user_allocate(sizeof(int) * 10)) };
    intArray[2] = 1;

    wil::unique_midl_ptr<int> intSingle{ reinterpret_cast<int*>(::MIDL_user_allocate(sizeof(int) * 1)) };
}

TEST_CASE("UniqueEnvironmentStrings", "[resource][win32]")
{
    wil::unique_environstrings_ptr env{ ::GetEnvironmentStringsW() };
    const wchar_t* nextVar = env.get();
    while (nextVar &&* nextVar)
    {
        // consume 'nextVar'
        nextVar += wcslen(nextVar) + 1;
    }

    wil::unique_environansistrings_ptr envAnsi{ ::GetEnvironmentStringsA() };
    const char* nextVarAnsi = envAnsi.get();
    while (nextVarAnsi && *nextVarAnsi)
    {
        // consume 'nextVar'
        nextVarAnsi += strlen(nextVarAnsi) + 1;
    }
}

TEST_CASE("UniqueVariant", "[resource][com]")
{
    wil::unique_variant var;
    var.vt = VT_BSTR;
    var.bstrVal = ::SysAllocString(L"25");
    REQUIRE(var.bstrVal != nullptr);

    auto call = [](const VARIANT&) {};
    call(var);

    VARIANT weakVar = var;
    (void)weakVar;

    wil::unique_variant var2;
    REQUIRE_SUCCEEDED(VariantChangeType(&var2, &var, 0, VT_UI4));
    REQUIRE(var2.vt == VT_UI4);
    REQUIRE(var2.uiVal == 25);
}

TEST_CASE("DefaultTemplateParamCompiles", "[resource]")
{
    wil::unique_process_heap_ptr<> a;
    wil::unique_virtualalloc_ptr<> b;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    wil::unique_hlocal_ptr<> c;
    wil::unique_hlocal_secure_ptr<> d;
    wil::unique_hglobal_ptr<> e;
    wil::unique_cotaskmem_secure_ptr<> f;
#endif

    wil::unique_midl_ptr<> g;
    wil::unique_cotaskmem_ptr<> h;
    wil::unique_mapview_ptr<> i;
}

TEST_CASE("UniqueInvokeCleanupMembers", "[resource]")
{
    // Case 1 - unique_ptr<> for a T* that has a "destroy" member
    struct ThingWithDestroy
    {
        bool destroyed = false;
        void destroy() { destroyed = true; };
    };
    ThingWithDestroy toDestroy;
    wil::unique_any<ThingWithDestroy*, decltype(&ThingWithDestroy::destroy), &ThingWithDestroy::destroy> p(&toDestroy);
    p.reset();
    REQUIRE(!p);
    REQUIRE(toDestroy.destroyed);

    // Case 2 - unique_struct calling a member, like above
    struct ThingToDestroy2
    {
        bool* destroyed;
        void destroy() { *destroyed = true; };
    };
    bool structDestroyed = false;
    {
        wil::unique_struct<ThingToDestroy2, decltype(&ThingToDestroy2::destroy), &ThingToDestroy2::destroy> other;
        other.destroyed = &structDestroyed;
        REQUIRE(!structDestroyed);
    }
    REQUIRE(structDestroyed);
}

struct ITokenTester : IUnknown
{
    virtual void DirectClose(DWORD_PTR token) = 0;
};

struct TokenTester : ITokenTester
{
    IFACEMETHOD_(ULONG, AddRef)() override { return 2; }
    IFACEMETHOD_(ULONG, Release)() override { return 1; }
    IFACEMETHOD(QueryInterface)(REFIID, void**) { return E_NOINTERFACE; }
    void DirectClose(DWORD_PTR token) override {
        m_closed = (token == m_closeToken);
    }
    bool m_closed = false;
    DWORD_PTR m_closeToken;
};

void MyTokenTesterCloser(ITokenTester* tt, DWORD_PTR token)
{
    tt->DirectClose(token);
}

TEST_CASE("ComTokenCloser", "[resource]")
{
    using token_tester_t = wil::unique_com_token<ITokenTester, DWORD_PTR, decltype(MyTokenTesterCloser), &MyTokenTesterCloser>;

    TokenTester tt;
    tt.m_closeToken = 4;
    {
        token_tester_t tmp{ &tt, 4 };
    }
    REQUIRE(tt.m_closed);
}

TEST_CASE("ComTokenDirectCloser", "[resource]")
{
    using token_tester_t = wil::unique_com_token<ITokenTester, DWORD_PTR, decltype(&ITokenTester::DirectClose), &ITokenTester::DirectClose>;

    TokenTester tt;
    tt.m_closeToken = 4;
    {
        token_tester_t tmp{ &tt, 4 };
    }
    REQUIRE(tt.m_closed);
}

TEST_CASE("UniqueCloseClipboardCall", "[resource]")
{
#if defined(__WIL__WINUSER_) && !defined(NOCLIPBOARD)
    if (auto clip = wil::open_clipboard(nullptr))
    {
        REQUIRE(::EmptyClipboard());
    }
#endif
}
