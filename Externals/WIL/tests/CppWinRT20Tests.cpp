
// Prior to C++/WinRT 2.0 this would cause issues since we're not including wil/cppwinrt.h in this translation unit.
// However, since we're going to link into the same executable as 'CppWinRTTests.cpp', the 'winrt_to_hresult_handler'
// global function pointer should be set, so these should all run successfully

#include <inspectable.h> // Must be included before base.h

#include <winrt/base.h>
#include <wil/result.h>

#include "common.h"

TEST_CASE("CppWinRTTests::CppWinRT20Test", "[cppwinrt]")
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

    test(E_OUTOFMEMORY);
    test(E_INVALIDARG);
    test(E_UNEXPECTED);
}
