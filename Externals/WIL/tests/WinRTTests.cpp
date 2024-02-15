
#include <wil/winrt.h>

#ifdef WIL_ENABLE_EXCEPTIONS
#include <map>
#include <string>
#include <vector>
#endif

// Required for pinterface template specializations that we depend on in this test
#include <Windows.ApplicationModel.Chat.h>
#pragma push_macro("GetCurrentTime")
#undef GetCurrentTime
#include <Windows.UI.Xaml.Data.h>
#pragma pop_macro("GetCurrentTime")

#include "common.h"
#include "FakeWinRTTypes.h"
#include "test_objects.h"

using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Storage;
using namespace ABI::Windows::System;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

TEST_CASE("WinRTTests::VerifyTraitsTypes", "[winrt]")
{
    static_assert(wistd::is_same_v<bool, typename wil::details::LastType<int, bool>::type>, "");
    static_assert(wistd::is_same_v<int, typename wil::details::LastType<int>::type>, "");

    static_assert(wistd::is_same_v<IAsyncAction*, decltype(wil::details::GetReturnParamPointerType(&IFileIOStatics::WriteTextAsync))>, "");
    static_assert(wistd::is_same_v<IAsyncOperation<bool>*, decltype(wil::details::GetReturnParamPointerType(&ILauncherStatics::LaunchUriAsync))>, "");

    static_assert(wistd::is_same_v<void, decltype(wil::details::GetAsyncResultType(static_cast<IAsyncAction*>(nullptr)))>, "");
    static_assert(wistd::is_same_v<boolean, decltype(wil::details::GetAsyncResultType(static_cast<IAsyncOperation<bool>*>(nullptr)))>, "");
    static_assert(wistd::is_same_v<IStorageFile*, decltype(wil::details::GetAsyncResultType(static_cast<IAsyncOperation<StorageFile*>*>(nullptr)))>, "");
}

template <bool InhibitArrayReferences, bool IgnoreCase, typename LhsT, typename RhsT>
void DoHStringComparisonTest(LhsT&& lhs, RhsT&& rhs, int relation)
{
    using compare = wil::details::hstring_compare<InhibitArrayReferences, IgnoreCase>;

    // == and !=
    REQUIRE(compare::equals(lhs, rhs) == (relation == 0));
    REQUIRE(compare::not_equals(lhs, rhs) == (relation != 0));

    REQUIRE(compare::equals(rhs, lhs) == (relation == 0));
    REQUIRE(compare::not_equals(rhs, lhs) == (relation != 0));

    // < and >=
    REQUIRE(compare::less(lhs, rhs) == (relation < 0));
    REQUIRE(compare::greater_equals(lhs, rhs) == (relation >= 0));

    REQUIRE(compare::less(rhs, lhs) == (relation > 0));
    REQUIRE(compare::greater_equals(rhs, lhs) == (relation <= 0));

    // > and <=
    REQUIRE(compare::greater(lhs, rhs) == (relation > 0));
    REQUIRE(compare::less_equals(lhs, rhs) == (relation <= 0));

    REQUIRE(compare::greater(rhs, lhs) == (relation < 0));
    REQUIRE(compare::less_equals(rhs, lhs) == (relation >= 0));

    // We wish to test with both const and non-const values. We can do this for free here so long as the type is
    // not an array since changing the const-ness of an array may change the expected results
#pragma warning(suppress: 4127)
    if (!wistd::is_array<wistd::remove_reference_t<LhsT>>::value &&
        !wistd::is_const<wistd::remove_reference_t<LhsT>>::value)
    {
        const wistd::remove_reference_t<LhsT>& constLhs = lhs;
        DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(constLhs, rhs, relation);
    }

#pragma warning(suppress: 4127)
    if (!wistd::is_array<wistd::remove_reference_t<RhsT>>::value &&
        !wistd::is_const<wistd::remove_reference_t<RhsT>>::value)
    {
        const wistd::remove_reference_t<RhsT>& constRhs = rhs;
        DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, constRhs, relation);
    }
}

// The two string arguments are expected to compare equal to one another using the specified IgnoreCase argument and
// contain at least one embedded null character
template <bool InhibitArrayReferences, bool IgnoreCase, size_t Size>
void DoHStringSameValueComparisonTest(const wchar_t (&lhs)[Size], const wchar_t (&rhs)[Size])
{
    wchar_t lhsNonConstArray[Size + 5];
    wchar_t rhsNonConstArray[Size + 5];
    wcsncpy_s(lhsNonConstArray, lhs, Size);
    wcsncpy_s(rhsNonConstArray, rhs, Size);

    // For non-const arrays, we should never deduce length, so even though we append different values to each string, we
    // do so after the last null character, so they should never be read
    wcsncpy_s(lhsNonConstArray + Size + 1, 4, L"foo", 3);
    wcsncpy_s(rhsNonConstArray + Size + 1, 4, L"bar", 3);

    const wchar_t* lhsCstr = lhs;
    const wchar_t* rhsCstr = rhs;

    HStringReference lhsRef(lhs);
    HStringReference rhsRef(rhs);
    HString lhsStr;
    HString rhsStr;
    REQUIRE_SUCCEEDED(lhsStr.Set(lhs));
    REQUIRE_SUCCEEDED(rhsStr.Set(rhs));
    auto lhsHstr = lhsStr.Get();
    auto rhsHstr = rhsStr.Get();

    wil::unique_hstring lhsUniqueStr;
    wil::unique_hstring rhsUniqueStr;
    REQUIRE_SUCCEEDED(lhsStr.CopyTo(&lhsUniqueStr));
    REQUIRE_SUCCEEDED(rhsStr.CopyTo(&rhsUniqueStr));

    // Const array - embedded nulls are included only if InhibitArrayReferences is false
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhs, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsNonConstArray, InhibitArrayReferences ? 0 : 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsCstr, InhibitArrayReferences ? 0 : 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsRef, InhibitArrayReferences ? -1 : 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsStr, InhibitArrayReferences ? -1 : 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsHstr, InhibitArrayReferences ? -1 : 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsUniqueStr, InhibitArrayReferences ? -1 : 0);

    // Non-const array - *never* deduce length
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsNonConstArray, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsCstr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsRef, -1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsStr, -1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsHstr, -1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsUniqueStr, -1);

    // C string - impossible to deduce length
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsCstr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsRef, -1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsStr, -1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsHstr, -1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsUniqueStr, -1);

    // HStringReference
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsRef, rhsRef, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsRef, rhsStr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsRef, rhsHstr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsRef, rhsUniqueStr, 0);

    // HString
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsStr, rhsStr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsStr, rhsHstr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsStr, rhsUniqueStr, 0);

    // Raw HSTRING
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsHstr, rhsHstr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsHstr, rhsUniqueStr, 0);

    // wil::unique_hstring
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsUniqueStr, rhsUniqueStr, 0);

#ifdef WIL_ENABLE_EXCEPTIONS
    std::wstring lhsWstr(lhs, 7);
    std::wstring rhsWstr(rhs, 7);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsWstr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhs, InhibitArrayReferences ? 1 : 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsNonConstArray, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsCstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsRef, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsStr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsHstr, 0);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsUniqueStr, 0);
#endif
}

// It's expected that the first argument (lhs) compares greater than the second argument (rhs)
template <bool InhibitArrayReferences, bool IgnoreCase, size_t LhsSize, size_t RhsSize>
void DoHStringDifferentValueComparisonTest(const wchar_t (&lhs)[LhsSize], const wchar_t (&rhs)[RhsSize])
{
    wchar_t lhsNonConstArray[LhsSize];
    wchar_t rhsNonConstArray[RhsSize];
    wcsncpy_s(lhsNonConstArray, lhs, LhsSize);
    wcsncpy_s(rhsNonConstArray, rhs, RhsSize);

    const wchar_t* lhsCstr = lhs;
    const wchar_t* rhsCstr = rhs;

    HStringReference lhsRef(lhs);
    HStringReference rhsRef(rhs);
    HString lhsStr;
    HString rhsStr;
    REQUIRE_SUCCEEDED(lhsStr.Set(lhs));
    REQUIRE_SUCCEEDED(rhsStr.Set(rhs));
    auto lhsHstr = lhsStr.Get();
    auto rhsHstr = rhsStr.Get();

    wil::unique_hstring lhsUniqueStr;
    wil::unique_hstring rhsUniqueStr;
    REQUIRE_SUCCEEDED(lhsStr.CopyTo(&lhsUniqueStr));
    REQUIRE_SUCCEEDED(rhsStr.CopyTo(&rhsUniqueStr));

    // Const array
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhs, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsNonConstArray, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsCstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsRef, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsStr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsHstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhs, rhsUniqueStr, 1);

    // Non-const array
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsNonConstArray, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsCstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsRef, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsStr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsHstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsNonConstArray, rhsUniqueStr, 1);

    // C string
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsCstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsRef, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsStr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsHstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsCstr, rhsUniqueStr, 1);

    // HStringReference
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsRef, rhsRef, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsRef, rhsStr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsRef, rhsHstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsRef, rhsUniqueStr, 1);

    // HString
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsStr, rhsStr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsStr, rhsHstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsStr, rhsUniqueStr, 1);

    // Raw HSTRING
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsHstr, rhsHstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsHstr, rhsUniqueStr, 1);

    // wil::unique_hstring
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsUniqueStr, rhsUniqueStr, 1);

#ifdef WIL_ENABLE_EXCEPTIONS
    std::wstring lhsWstr(lhs);
    std::wstring rhsWstr(rhs);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsWstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhs, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsNonConstArray, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsCstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsRef, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsStr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsHstr, 1);
    DoHStringComparisonTest<InhibitArrayReferences, IgnoreCase>(lhsWstr, rhsUniqueStr, 1);
#endif
}

TEST_CASE("WinRTTests::HStringComparison", "[winrt][hstring_compare]")
{
    SECTION("Don't inhibit arrays")
    {
        DoHStringSameValueComparisonTest<false, false>(L"foo\0bar", L"foo\0bar");
        DoHStringDifferentValueComparisonTest<false, false>(L"foo", L"bar");
    }

    SECTION("Inhibit arrays")
    {
        DoHStringSameValueComparisonTest<true, false>(L"foo\0bar", L"foo\0bar");
        DoHStringDifferentValueComparisonTest<true, false>(L"foo", L"bar");
    }

    SECTION("Ignore case")
    {
        DoHStringSameValueComparisonTest<true, true>(L"foo\0bar", L"FoO\0bAR");
        DoHStringDifferentValueComparisonTest<true, true>(L"Foo", L"baR");
    }

    SECTION("Empty string")
    {
        const wchar_t constArray[] = L"";
        wchar_t nonConstArray[] = L"";
        const wchar_t* cstr = constArray;
        const wchar_t* nullCstr = nullptr;

        // str may end up referencing a null HSTRING. That's fine; we'll just test null HSTRING twice
        HString str;
        REQUIRE_SUCCEEDED(str.Set(constArray));
        HSTRING nullHstr = nullptr;

        // Const array - impossible to use null value
        DoHStringComparisonTest<false, false>(constArray, constArray, 0);
        DoHStringComparisonTest<false, false>(constArray, nonConstArray, 0);
        DoHStringComparisonTest<false, false>(constArray, cstr, 0);
        DoHStringComparisonTest<false, false>(constArray, nullCstr, 0);
        DoHStringComparisonTest<false, false>(constArray, str.Get(), 0);
        DoHStringComparisonTest<false, false>(constArray, nullHstr, 0);

        // Non-const array - impossible to use null value
        DoHStringComparisonTest<false, false>(nonConstArray, nonConstArray, 0);
        DoHStringComparisonTest<false, false>(nonConstArray, cstr, 0);
        DoHStringComparisonTest<false, false>(nonConstArray, nullCstr, 0);
        DoHStringComparisonTest<false, false>(nonConstArray, str.Get(), 0);
        DoHStringComparisonTest<false, false>(nonConstArray, nullHstr, 0);

        // Non-null c-string
        DoHStringComparisonTest<false, false>(cstr, cstr, 0);
        DoHStringComparisonTest<false, false>(cstr, nullCstr, 0);
        DoHStringComparisonTest<false, false>(cstr, str.Get(), 0);
        DoHStringComparisonTest<false, false>(cstr, nullHstr, 0);

        // Null c-string
        DoHStringComparisonTest<false, false>(nullCstr, nullCstr, 0);
        DoHStringComparisonTest<false, false>(nullCstr, str.Get(), 0);
        DoHStringComparisonTest<false, false>(nullCstr, nullHstr, 0);

        // (Possibly) non-null HSTRING
        DoHStringComparisonTest<false, false>(str.Get(), str.Get(), 0);
        DoHStringComparisonTest<false, false>(str.Get(), nullHstr, 0);

        // Null HSTRING
        DoHStringComparisonTest<false, false>(nullHstr, nullHstr, 0);

#ifdef WIL_ENABLE_EXCEPTIONS
        std::wstring wstr;
        DoHStringComparisonTest<false, false>(wstr, wstr, 0);
        DoHStringComparisonTest<false, false>(wstr, constArray, 0);
        DoHStringComparisonTest<false, false>(wstr, nonConstArray, 0);
        DoHStringComparisonTest<false, false>(wstr, cstr, 0);
        DoHStringComparisonTest<false, false>(wstr, nullCstr, 0);
        DoHStringComparisonTest<false, false>(wstr, str.Get(), 0);
        DoHStringComparisonTest<false, false>(wstr, nullHstr, 0);
#endif
    }
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("WinRTTests::HStringMapTest", "[winrt][hstring_compare]")
{
    int nextValue = 0;
    std::map<std::wstring, int> wstringMap;
    wstringMap.emplace(L"foo", nextValue++);
    wstringMap.emplace(L"bar", nextValue++);
    wstringMap.emplace(std::wstring(L"foo\0bar", 7), nextValue++);
    wstringMap.emplace(L"adding", nextValue++);
    wstringMap.emplace(L"quite", nextValue++);
    wstringMap.emplace(L"a", nextValue++);
    wstringMap.emplace(L"few", nextValue++);
    wstringMap.emplace(L"more", nextValue++);
    wstringMap.emplace(L"values", nextValue++);
    wstringMap.emplace(L"for", nextValue++);
    wstringMap.emplace(L"testing", nextValue++);
    wstringMap.emplace(L"", nextValue++);

    std::map<HString, int> hstringMap;
    for (auto& pair : wstringMap)
    {
        HString str;
        THROW_IF_FAILED(str.Set(pair.first.c_str(), static_cast<UINT>(pair.first.length())));
        hstringMap.emplace(std::move(str), pair.second);
    }

    // Order should be the same as the map of wstring
    auto itr = hstringMap.begin();
    for (auto& pair : wstringMap)
    {
        REQUIRE(itr != hstringMap.end());
        REQUIRE(itr->first == HStringReference(pair.first.c_str(), static_cast<UINT>(pair.first.length())));

        // Should also be able to find the value
        REQUIRE(hstringMap.find(pair.first) != hstringMap.end());

        ++itr;
    }
    REQUIRE(itr == hstringMap.end());

    const wchar_t constArray[] = L"foo\0bar";
    wchar_t nonConstArray[] = L"foo\0bar";
    const wchar_t* cstr = constArray;

    HString key;
    wil::unique_hstring uniqueHstr;
    THROW_IF_FAILED(key.Set(constArray));
    THROW_IF_FAILED(key.CopyTo(&uniqueHstr));

    HStringReference ref(constArray);
    std::wstring wstr(constArray, 7);

    auto verifyFunc = [&](int expectedValue, auto&& keyValue)
    {
        auto itr = hstringMap.find(std::forward<decltype(keyValue)>(keyValue));
        REQUIRE(itr != hstringMap.end());
        REQUIRE(expectedValue == itr->second);
    };

    // The following should find "foo\0bar"
    auto expectedValue = wstringMap[wstr];
    verifyFunc(expectedValue, uniqueHstr);
    verifyFunc(expectedValue, key);
    verifyFunc(expectedValue, key.Get());
    verifyFunc(expectedValue, ref);
    verifyFunc(expectedValue, wstr);

    // Arrays/strings should not deduce length and should therefore find "foo"
    expectedValue = wstringMap[L"foo"];
    verifyFunc(expectedValue, constArray);
    verifyFunc(expectedValue, nonConstArray);
    verifyFunc(expectedValue, cstr);

    // Should not ignore case
    REQUIRE(hstringMap.find(L"FOO") == hstringMap.end());

    // Should also be able to find empty values
    const wchar_t constEmptyArray[] = L"";
    wchar_t nonConstEmptyArray[] = L"";
    const wchar_t* emptyCstr = constEmptyArray;
    const wchar_t* nullCstr = nullptr;

    HString emptyStr;
    HSTRING nullHstr = nullptr;

    std::wstring emptyWstr;

    expectedValue = wstringMap[L""];
    verifyFunc(expectedValue, constEmptyArray);
    verifyFunc(expectedValue, nonConstEmptyArray);
    verifyFunc(expectedValue, emptyCstr);
    verifyFunc(expectedValue, nullCstr);
    verifyFunc(expectedValue, emptyStr);
    verifyFunc(expectedValue, nullHstr);
    verifyFunc(expectedValue, emptyWstr);
}

TEST_CASE("WinRTTests::HStringCaseInsensitiveMapTest", "[winrt][hstring_compare]")
{
    std::map<HString, int, wil::hstring_insensitive_less> hstringMap;

    auto emplaceFunc = [&](auto&& key, int value)
    {
        HString str;
        THROW_IF_FAILED(str.Set(std::forward<decltype(key)>(key)));
        hstringMap.emplace(std::move(str), value);
    };

    int nextValue = 0;
    int fooValue = nextValue++;
    emplaceFunc(L"foo", fooValue);
    emplaceFunc(L"bar", nextValue++);
    int foobarValue = nextValue++;
    emplaceFunc(L"foo\0bar", foobarValue);
    emplaceFunc(L"foobar", nextValue++);
    emplaceFunc(L"adding", nextValue++);
    emplaceFunc(L"some", nextValue++);
    emplaceFunc(L"more", nextValue++);
    emplaceFunc(L"values", nextValue++);
    emplaceFunc(L"for", nextValue++);
    emplaceFunc(L"testing", nextValue++);
    WI_ASSERT(static_cast<size_t>(nextValue) == hstringMap.size());

    const wchar_t constArray[] = L"FoO\0BAr";
    wchar_t nonConstArray[] = L"fOo\0baR";
    const wchar_t* cstr = constArray;

    HString key;
    wil::unique_hstring uniqueHstr;
    THROW_IF_FAILED(key.Set(constArray));
    THROW_IF_FAILED(key.CopyTo(&uniqueHstr));

    HStringReference ref(constArray);
    std::wstring wstr(constArray, 7);

    auto verifyFunc = [&](int expectedValue, auto&& key)
    {
        auto itr = hstringMap.find(std::forward<decltype(key)>(key));
        REQUIRE(itr != std::end(hstringMap));
        REQUIRE(expectedValue == itr->second);
    };

    // The following should find "foo\0bar"
    verifyFunc(foobarValue, uniqueHstr);
    verifyFunc(foobarValue, key);
    verifyFunc(foobarValue, key.Get());
    verifyFunc(foobarValue, ref);
    verifyFunc(foobarValue, wstr);

    // Arrays/strings should not deduce length and should therefore find "foo"
    verifyFunc(fooValue, constArray);
    verifyFunc(fooValue, nonConstArray);
    verifyFunc(fooValue, cstr);
}
#endif

// This is not a test method, nor should it be called. This is a compilation-only test.
#ifdef WIL_ENABLE_EXCEPTIONS
void RunWhenCompleteCompilationTest()
{
    {
        ComPtr<IAsyncOperation<HSTRING>> stringOp;
        wil::run_when_complete(stringOp.Get(), [](HRESULT /* result */, HSTRING /* value */) {});
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
        auto result = wil::wait_for_completion(stringOp.Get());
#endif
    }

    {
        ComPtr<IAsyncOperationWithProgress<HSTRING, UINT64>> stringOpWithProgress;
        wil::run_when_complete(stringOpWithProgress.Get(), [](HRESULT /* result */, HSTRING /* value */) {});
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
        auto result = wil::wait_for_completion(stringOpWithProgress.Get());
#endif
    }
}
#endif

TEST_CASE("WinRTTests::RunWhenCompleteMoveOnlyTest", "[winrt][run_when_complete]")
{
    auto op = Make<FakeAsyncOperation<int>>();
    REQUIRE(op);

    bool gotEvent = false;
    auto hr = wil::run_when_complete_nothrow(op.Get(), [&gotEvent, enforce = cannot_copy{}](HRESULT hr, int result)
    {
        (void)enforce;
        REQUIRE_SUCCEEDED(hr);
        REQUIRE(result == 42);
        gotEvent = true;
        return S_OK;
    });
    REQUIRE_SUCCEEDED(hr);

    op->Complete(S_OK, 42);
    REQUIRE(gotEvent);
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
TEST_CASE("WinRTTests::WaitForCompletionTimeout", "[winrt][wait_for_completion]")
{
    auto op = Make<FakeAsyncOperation<bool, boolean>>();
    REQUIRE(op);

    // The wait_for_completion* functions don't properly deduce the "decayed" async type, so force it here
    auto asyncOp = static_cast<IAsyncOperation<bool>*>(op.Get());

    bool timedOut = false;
    REQUIRE_SUCCEEDED(wil::wait_for_completion_or_timeout_nothrow(asyncOp, 1, &timedOut));
    REQUIRE(timedOut);
}

// This is not a test method, nor should it be called. This is a compilation-only test.
#pragma warning(push)
#pragma warning(disable: 4702) // Unreachable code
void WaitForCompletionCompilationTest()
{
    // Ensure the wait_for_completion variants compile
    FAIL_FAST_HR_MSG(E_UNEXPECTED, "This is a compilation test, and should not be called");

    // template <typename TAsync = ABI::Windows::Foundation::IAsyncAction>
    // inline HRESULT wait_for_completion_nothrow(_In_ TAsync* operation, COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS, DWORD timeout = INFINITE);
    IAsyncAction* action = nullptr;
    wil::wait_for_completion_nothrow(action);
    wil::wait_for_completion_nothrow(action, COWAIT_DEFAULT);

    // template <typename TResult>
    // HRESULT wait_for_completion_nothrow(_In_ ABI::Windows::Foundation::IAsyncOperation<TResult>* operation,
    //     _Out_ typename wil::details::MapAsyncOpResultType<TResult>::type* result,
    //     COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS, DWORD timeout = INFINITE);
    IAsyncOperation<bool>* operation = nullptr;
    wil::wait_for_completion_nothrow(operation);
    wil::wait_for_completion_nothrow(operation, COWAIT_DEFAULT);

    // template <typename TResult, typename TProgress>
    // HRESULT wait_for_completion_nothrow(_In_ ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress>* operation,
    //     _Out_ typename wil::details::MapAsyncOpProgressResultType<TResult, TProgress>::type* result,
    //     COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS, DWORD timeout = INFINITE);

    ComPtr<IAsyncOperation<bool>> operationWithResult;
    boolean result = false;
    wil::wait_for_completion_nothrow(operationWithResult.Get(), &result);
    wil::wait_for_completion_nothrow(operationWithResult.Get(), &result, COWAIT_DEFAULT);

    DWORD timeoutValue = 1000; // arbitrary
    bool timedOut = false;

    // template <typename TAsync = ABI::Windows::Foundation::IAsyncAction>
    // inline HRESULT wait_for_completion_or_timeout_nothrow(_In_ TAsync* operation,
    //     DWORD timeoutValue, _Out_ bool* timedOut, COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS);
    wil::wait_for_completion_or_timeout_nothrow(action, timeoutValue, &timedOut);
    wil::wait_for_completion_or_timeout_nothrow(action, timeoutValue, &timedOut, COWAIT_DEFAULT);

    // template <typename TResult>
    // HRESULT wait_for_completion_or_timeout_nothrow(_In_ ABI::Windows::Foundation::IAsyncOperation<TResult>* operation,
    //     _Out_ typename wil::details::MapAsyncOpResultType<TResult>::type* result,
    //     DWORD timeoutValue, _Out_ bool* timedOut, COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS);
    wil::wait_for_completion_or_timeout_nothrow(operation, timeoutValue, &timedOut);
    wil::wait_for_completion_or_timeout_nothrow(operation, timeoutValue, &timedOut, COWAIT_DEFAULT);

    // template <typename TResult, typename TProgress>
    // HRESULT wait_for_completion_or_timeout_nothrow(_In_ ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress>* operation,
    //     _Out_ typename wil::details::MapAsyncOpProgressResultType<TResult, TProgress>::type* result,
    //     DWORD timeoutValue, _Out_ bool* timedOut, COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS);
    wil::wait_for_completion_or_timeout_nothrow(operationWithResult.Get(), &result, timeoutValue, &timedOut);
    wil::wait_for_completion_or_timeout_nothrow(operationWithResult.Get(), &result, timeoutValue, &timedOut, COWAIT_DEFAULT);

#ifdef WIL_ENABLE_EXCEPTIONS
    // template <typename TAsync = ABI::Windows::Foundation::IAsyncAction>
    // inline void wait_for_completion(_In_ TAsync* operation, COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS, DWORD timeout = INFINITE);
    wil::wait_for_completion(action);
    wil::wait_for_completion(action, COWAIT_DEFAULT);

    // template <typename TResult, typename TReturn = typename wil::details::MapToSmartType<typename wil::details::MapAsyncOpResultType<TResult>::type>::type>
    // TReturn
    //     wait_for_completion(_In_ ABI::Windows::Foundation::IAsyncOperation<TResult>* operation, COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS, DWORD timeout = INFINITE);
    wil::wait_for_completion(operation);
    wil::wait_for_completion(operation, COWAIT_DEFAULT);

    // template <typename TResult, typename TProgress, typename TReturn = typename wil::details::MapToSmartType<typename wil::details::MapAsyncOpResultType<TResult>::type>::type>
    // TReturn
    //     wait_for_completion(_In_ ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult, TProgress>* operation, COWAIT_FLAGS flags = COWAIT_DISPATCH_CALLS, DWORD timeout = INFINITE);
    result = wil::wait_for_completion(operationWithResult.Get());
    result = wil::wait_for_completion(operationWithResult.Get(), COWAIT_DEFAULT);
#endif
}
#pragma warning(pop)
#endif

TEST_CASE("WinRTTests::TimeTTests", "[winrt][time_t]")
{
    // Verifying that converting DateTime variable set as the date that means 0 as time_t works
    DateTime time1 = { wil::SecondsToStartOf1970 * wil::HundredNanoSecondsInSecond };
    __time64_t time_t1 = wil::DateTime_to_time_t(time1);
    REQUIRE(time_t1 == 0);

    // Verifying that converting back to DateTime would return the same value
    DateTime time2 = wil::time_t_to_DateTime(time_t1);
    REQUIRE(time1.UniversalTime == time2.UniversalTime);

    // Verifying that converting to time_t for non-zero value also works
    time2.UniversalTime += wil::HundredNanoSecondsInSecond * 123;
    __time64_t time_t2 = wil::DateTime_to_time_t(time2);
    REQUIRE(time_t2 - time_t1 == 123);

    // Verifying that converting back to DateTime for non-zero value also works
    time1 = wil::time_t_to_DateTime(time_t2);
    REQUIRE(time1.UniversalTime == time2.UniversalTime);
}

ComPtr<IVector<IInspectable*>> MakeSampleInspectableVector(UINT32 count = 5)
{
    auto result = Make<FakeVector<IInspectable*>>();
    REQUIRE(result);

    ComPtr<IPropertyValueStatics> propStatics;
    REQUIRE_SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Foundation_PropertyValue).Get(), &propStatics));

    for (UINT32 i = 0; i < count; ++i)
    {
        ComPtr<IInspectable> myProp;
        REQUIRE_SUCCEEDED(propStatics->CreateUInt32(i, &myProp));
        REQUIRE_SUCCEEDED(result->Append(myProp.Get()));
    }

    return result;
}

ComPtr<IVector<HSTRING>> MakeSampleStringVector()
{
    auto result = Make<FakeVector<HSTRING>>();
    REQUIRE(result);

    const HStringReference items[] = { HStringReference(L"one"), HStringReference(L"two"), HStringReference(L"three") };
    for (const auto& i : items)
    {
        REQUIRE_SUCCEEDED(result->Append(i.Get()));
    }

    return result;
}

ComPtr<IVector<Point>> MakeSamplePointVector(int count = 5)
{
    auto result = Make<FakeVector<Point>>();
    REQUIRE(result);

    for (int i = 0; i < count; ++i)
    {
        auto value = static_cast<float>(i);
        REQUIRE_SUCCEEDED(result->Append(Point{ value, value }));
    }

    return result;
}

template<typename T> auto cast_to(ComPtr<IInspectable> const& src)
{
    ComPtr<IReference<T>> theRef;
    T value{};
    THROW_IF_FAILED(src.As(&theRef));
    THROW_IF_FAILED(theRef->get_Value(&value));
    return value;
}

TEST_CASE("WinRTTests::VectorToVectorTest", "[winrt][to_vector]")
{
#if defined(WIL_ENABLE_EXCEPTIONS)
    auto uninit = wil::RoInitialize_failfast();
    auto ints = MakeSampleInspectableVector(100);
    auto vec = wil::to_vector(ints.Get());
    UINT32 size;
    THROW_IF_FAILED(ints->get_Size(&size));
    REQUIRE(size == vec.size());
    for (UINT32 i = 0; i < size; ++i)
    {
        ComPtr<IInspectable> oneItem;
        THROW_IF_FAILED(ints->GetAt(i, &oneItem));
        REQUIRE(cast_to<UINT32>(vec[i]) == cast_to<UINT32>(oneItem));
    }
#endif
}

TEST_CASE("WinRTTests::VectorRangeTest", "[winrt][vector_range]")
{
    auto uninit = wil::RoInitialize_failfast();

    auto inspectables = MakeSampleInspectableVector();
    unsigned count = 0;
    REQUIRE_SUCCEEDED(inspectables->get_Size(&count));

    unsigned idx = 0;
    HRESULT success = S_OK;
    for (const auto& i : wil::get_range_nothrow(inspectables.Get(), &success))
    {
        // Duplications are not a typo - they verify the thing is callable twice

        UINT32 value;
        ComPtr<IReference<UINT32>> intRef;
        REQUIRE_SUCCEEDED(i.CopyTo(IID_PPV_ARGS(&intRef)));
        REQUIRE_SUCCEEDED(intRef->get_Value(&value));
        REQUIRE(idx == value);
        REQUIRE_SUCCEEDED(i.CopyTo(IID_PPV_ARGS(&intRef)));
        REQUIRE_SUCCEEDED(intRef->get_Value(&value));
        REQUIRE(idx == value);

        ++idx;

        HString rtc;
        REQUIRE_SUCCEEDED(i->GetRuntimeClassName(rtc.GetAddressOf()));
        REQUIRE_SUCCEEDED(i->GetRuntimeClassName(rtc.GetAddressOf()));
    }
    REQUIRE_SUCCEEDED(success);
    REQUIRE(count == idx);

    auto strings = MakeSampleStringVector();
    for (const auto& i : wil::get_range_nothrow(strings.Get(), &success))
    {
        REQUIRE(i.Get());
        REQUIRE(i.Get());
    }
    REQUIRE_SUCCEEDED(success);

    int index = 0;
    auto points = MakeSamplePointVector();
    for (auto value : wil::get_range_nothrow(points.Get(), &success))
    {
        REQUIRE(index++ == value.Get().X);
    }
    REQUIRE_SUCCEEDED(success);

    // operator-> should not clear out the pointer
    auto inspRange = wil::get_range_nothrow(inspectables.Get());
    for (auto itr = inspRange.begin(); itr != inspRange.end(); ++itr)
    {
        REQUIRE(itr->Get());
    }

    auto strRange = wil::get_range_nothrow(strings.Get());
    for (auto itr = strRange.begin(); itr != strRange.end(); ++itr)
    {
        REQUIRE(itr->Get());
    }

    index = 0;
    auto pointRange = wil::get_range_nothrow(points.Get());
    for (auto itr = pointRange.begin(); itr != pointRange.end(); ++itr)
    {
        REQUIRE(index++ == itr->Get().X);
    }

#if (defined WIL_ENABLE_EXCEPTIONS)
    idx = 0;
    for (const auto& i : wil::get_range(inspectables.Get()))
    {
        // Duplications are not a typo - they verify the thing is callable twice

        UINT32 value;
        ComPtr<IReference<UINT32>> intRef;
        REQUIRE_SUCCEEDED(i.CopyTo(IID_PPV_ARGS(&intRef)));
        REQUIRE_SUCCEEDED(intRef->get_Value(&value));
        REQUIRE(idx == value);
        REQUIRE_SUCCEEDED(i.CopyTo(IID_PPV_ARGS(&intRef)));
        REQUIRE_SUCCEEDED(intRef->get_Value(&value));
        REQUIRE(idx == value);

        ++idx;

        HString rtc;
        REQUIRE_SUCCEEDED(i->GetRuntimeClassName(rtc.GetAddressOf()));
        REQUIRE_SUCCEEDED(i->GetRuntimeClassName(rtc.GetAddressOf()));
    }
    REQUIRE(count == idx);

    for (const auto& i : wil::get_range(strings.Get()))
    {
        REQUIRE(i.Get());
        REQUIRE(i.Get());
    }

    index = 0;
    for (auto value : wil::get_range(points.Get()))
    {
        REQUIRE(index++ == value.Get().X);
    }

    // operator-> should not clear out the pointer
    for (auto itr = inspRange.begin(); itr != inspRange.end(); ++itr)
    {
        REQUIRE(itr->Get());
    }

    for (auto itr = strRange.begin(); itr != strRange.end(); ++itr)
    {
        REQUIRE(itr->Get());
    }

    index = 0;
    for (auto itr = pointRange.begin(); itr != pointRange.end(); ++itr)
    {
        REQUIRE(index++ == itr->Get().X);
    }

    // Iterator self-assignment is a nop.
    {
        auto inspRange2 = wil::get_range(inspectables.Get());
        auto itr = inspRange2.begin();
        REQUIRE(itr != inspRange2.end()); // should have something in it
        auto& ref = *itr;
        auto val = ref;
        itr = itr;
        REQUIRE(val == ref);
        itr = std::move(itr);
        REQUIRE(val == ref);
    }

    {
        auto strRange2 = wil::get_range(strings.Get());
        auto itr = strRange2.begin();
        REQUIRE(itr != strRange2.end()); // should have something in it
        auto& ref = *itr;
        auto val = ref.Get();
        itr = itr;
        REQUIRE(val == ref);
        itr = std::move(itr);
        REQUIRE(val == ref.Get());
    }
#endif
}

unsigned long GetComObjectRefCount(IUnknown* unk) { unk->AddRef(); return unk->Release(); }

TEST_CASE("WinRTTests::VectorRangeLeakTest", "[winrt][vector_range]")
{
    auto uninit = wil::RoInitialize_failfast();

    auto inspectables = MakeSampleInspectableVector();
    ComPtr<IInspectable> verifyNotLeaked;
    HRESULT hr = S_OK;
    for (const auto& ptr : wil::get_range_nothrow(inspectables.Get(), &hr))
    {
        if (!verifyNotLeaked)
        {
            verifyNotLeaked = ptr;
        }
    }
    inspectables = nullptr; // clear all refs to verifyNotLeaked
    REQUIRE_SUCCEEDED(hr);
    REQUIRE(GetComObjectRefCount(verifyNotLeaked.Get()) == 1);

    inspectables = MakeSampleInspectableVector();
    for (const auto& ptr : wil::get_range_failfast(inspectables.Get()))
    {
        if (!verifyNotLeaked)
        {
            verifyNotLeaked = ptr;
        }
    }
    inspectables = nullptr; // clear all refs to verifyNotLeaked
    REQUIRE(GetComObjectRefCount(verifyNotLeaked.Get()) == 1);

#if (defined WIL_ENABLE_EXCEPTIONS)
    inspectables = MakeSampleInspectableVector();
    for (const auto& ptr : wil::get_range(inspectables.Get()))
    {
        if (!verifyNotLeaked)
        {
            verifyNotLeaked = ptr;
        }
    }
    inspectables = nullptr; // clear all refs to verifyNotLeaked
    REQUIRE(GetComObjectRefCount(verifyNotLeaked.Get()) == 1);
#endif
}

TEST_CASE("WinRTTests::TwoPhaseConstructor", "[winrt][hstring]")
{
    const wchar_t left[] = L"left";
    const wchar_t right[] = L"right";
    ULONG needed = ARRAYSIZE(left) + ARRAYSIZE(right) - 1;
    auto maker = wil::TwoPhaseHStringConstructor::Preallocate(needed);
    REQUIRE_SUCCEEDED(StringCbCopyW(maker.Get(), maker.ByteSize(), left));
    REQUIRE_SUCCEEDED(StringCbCatW(maker.Get(), maker.ByteSize(), right));
    REQUIRE_SUCCEEDED(maker.Validate(needed * sizeof(wchar_t)));

    wil::unique_hstring promoted{ maker.Promote() };
    REQUIRE(wcscmp(L"leftright", str_raw_ptr(promoted)) == 0);
}