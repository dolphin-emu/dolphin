
#include <wil/cppwinrt.h>

#include "catch.hpp"

// HRESULT values that C++/WinRT throws as something other than winrt::hresult_error - e.g. a type derived from
// winrt::hresult_error, std::*, etc.
static const HRESULT cppwinrt_mapped_hresults[] =
{
    E_ACCESSDENIED,
    RPC_E_WRONG_THREAD,
    E_NOTIMPL,
    E_INVALIDARG,
    E_BOUNDS,
    E_NOINTERFACE,
    CLASS_E_CLASSNOTAVAILABLE,
    E_CHANGED_STATE,
    E_ILLEGAL_METHOD_CALL,
    E_ILLEGAL_STATE_CHANGE,
    E_ILLEGAL_DELEGATE_ASSIGNMENT,
    HRESULT_FROM_WIN32(ERROR_CANCELLED),
    E_OUTOFMEMORY,
};

TEST_CASE("CppWinRTTests::WilToCppWinRTExceptionTranslationTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr)
    {
        try
        {
            THROW_HR(hr);
        }
        catch (...)
        {
            REQUIRE(hr == winrt::to_hresult());
        }
    };

    for (auto hr : cppwinrt_mapped_hresults)
    {
        test(hr);
    }

    // A non-mapped HRESULT
    test(E_UNEXPECTED);
}

TEST_CASE("CppWinRTTests::CppWinRTToWilExceptionTranslationTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr)
    {
        try
        {
            winrt::check_hresult(hr);
        }
        catch (...)
        {
            REQUIRE(hr == wil::ResultFromCaughtException());
        }
    };

    for (auto hr : cppwinrt_mapped_hresults)
    {
        test(hr);
    }

    // A non-mapped HRESULT
    test(E_UNEXPECTED);
}

TEST_CASE("CppWinRTTests::ResultFromExceptionDebugTest", "[cppwinrt]")
{
    auto test = [](HRESULT hr, wil::SupportedExceptions supportedExceptions)
    {
        auto result = wil::ResultFromExceptionDebug(WI_DIAGNOSTICS_INFO, supportedExceptions, [&]()
        {
            winrt::check_hresult(hr);
        });
        REQUIRE(hr == result);
    };

    for (auto hr : cppwinrt_mapped_hresults)
    {
        test(hr, wil::SupportedExceptions::Known);
        test(hr, wil::SupportedExceptions::All);
    }

    // A non-mapped HRESULT
    test(E_UNEXPECTED, wil::SupportedExceptions::Known);
    test(E_UNEXPECTED, wil::SupportedExceptions::All);

    // Uncomment any of the following to validate SEH failfast
    //test(E_UNEXPECTED, wil::SupportedExceptions::None);
    //test(E_ACCESSDENIED, wil::SupportedExceptions::Thrown);
    //test(E_INVALIDARG, wil::SupportedExceptions::ThrownOrAlloc);
}

TEST_CASE("CppWinRTTests::CppWinRTConsistencyTest", "[cppwinrt]")
{
    // Since setting 'winrt_to_hresult_handler' opts us into _all_ C++/WinRT exception translation handling, we need to
    // make sure that we preserve behavior, at least with 'check_hresult', especially when C++/WinRT maps a particular
    // HRESULT value to a different exception type
    auto test = [](HRESULT hr)
    {
        try
        {
            winrt::check_hresult(hr);
        }
        catch (...)
        {
            REQUIRE(hr == winrt::to_hresult());
        }
    };

    for (auto hr : cppwinrt_mapped_hresults)
    {
        test(hr);
    }

    // A non-mapped HRESULT
    test(E_UNEXPECTED);

    // C++/WinRT also maps a few std::* exceptions to various HRESULTs. We should preserve this behavior
    try
    {
        throw std::out_of_range("oopsie");
    }
    catch (...)
    {
        REQUIRE(winrt::to_hresult() == E_BOUNDS);
    }

    try
    {
        throw std::invalid_argument("daisy");
    }
    catch (...)
    {
        REQUIRE(winrt::to_hresult() == E_INVALIDARG);
    }

    // NOTE: C++/WinRT maps other 'std::exception' derived exceptions to E_FAIL, however we preserve the WIL behavior
    // that such exceptions become HRESULT_FROM_WIN32(ERROR_UNHANDLED_EXCEPTION)
}
