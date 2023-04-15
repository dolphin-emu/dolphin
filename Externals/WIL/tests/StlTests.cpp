
#include <wil/stl.h>

#include "common.h"

#ifndef WIL_ENABLE_EXCEPTIONS
#error STL tests require exceptions
#endif

struct dummy
{
    char value;
};

// Specialize std::allocator<> so that we don't actually allocate/deallocate memory
dummy g_memoryBuffer[256];
namespace std
{
    template <>
    struct allocator<dummy>
    {
        using value_type = dummy;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        dummy* allocate(std::size_t count)
        {
            REQUIRE(count <= std::size(g_memoryBuffer));
            return g_memoryBuffer;
        }

        void deallocate(dummy* ptr, std::size_t count)
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                REQUIRE(ptr[i].value == 0);
            }
        }
    };
}

TEST_CASE("StlTests::TestSecureAllocator", "[stl][secure_allocator]")
{
    {
        wil::secure_vector<dummy> sensitiveBytes(32, dummy{ 'a' });
    }
}

#if __WI_LIBCPP_STD_VER >= 17

struct CustomNoncopyableString
{
    CustomNoncopyableString() = default;
    CustomNoncopyableString(const CustomNoncopyableString&) = delete;
    void operator=(const CustomNoncopyableString&) = delete;

    constexpr operator PCSTR() const { return "hello"; }
    constexpr operator PCWSTR() const { return L"w-hello"; }
};

TEST_CASE("StlTests::TestZStringView", "[stl][zstring_view]")
{
    // Test empty cases
    REQUIRE(wil::zstring_view{}.length() == (size_t)0u);
    REQUIRE(wil::zstring_view{}.data() == nullptr);
    REQUIRE(wil::zstring_view{}.c_str() == nullptr);

    // Test empty string cases
    REQUIRE(wil::zstring_view{""}[0] == '\0');
    REQUIRE(wil::zstring_view{""}.c_str()[0] == '\0');
    REQUIRE(wil::zstring_view{""}.length() == 0);

    // Test different constructor equality
    constexpr wil::zstring_view fromLiteral = "abc";
    REQUIRE(fromLiteral.length() == strlen("abc"));

    std::string stlString = "abc";
    wil::zstring_view fromString(stlString);
    wil::zstring_view fromPtr(stlString.data());

    static constexpr char charArray[] = "abc";
    constexpr wil::zstring_view fromArray(charArray);

    static constexpr char extendedCharArray[] = "abc\0\0\0\0\0";
    constexpr wil::zstring_view fromExtendedArray(extendedCharArray);

    wil::zstring_view copy = fromLiteral;

    REQUIRE(fromLiteral == stlString);
    REQUIRE(fromLiteral == fromString);
    REQUIRE(fromLiteral == fromArray);
    REQUIRE(fromLiteral == fromExtendedArray);
    REQUIRE(fromLiteral == copy);

    // Test decay to std::string_view
    std::string_view sv = fromLiteral;
    REQUIRE(sv == fromLiteral);

    // Test operator[]
    REQUIRE(fromLiteral[0] == 'a');
    REQUIRE(fromLiteral[1] == 'b');
    REQUIRE(fromLiteral[2] == 'c');
    REQUIRE(fromLiteral[3] == '\0');

    // Test constructing with no NULL in range
    static constexpr char badCharArray[2][3] = {{'a', 'b', 'c' }, {'a', 'b', 'c' }};
    REQUIRE_CRASH((wil::zstring_view{ &badCharArray[0][0], _countof(badCharArray[0]) }));
    REQUIRE_CRASH((wil::zstring_view{ badCharArray[0] }));

    // Test constructing with a NULL one character past the valid range, guarding against off-by-one errors
    // Overloads taking an explicit length trust the user that they ensure valid memory follows the buffer
    static constexpr char badCharArrayOffByOne[2][3] = {{'a', 'b', 'c' }, {}};
    const wil::zstring_view fromTerminatedCharArray(&badCharArrayOffByOne[0][0], _countof(badCharArrayOffByOne[0]));
    REQUIRE(fromLiteral == fromTerminatedCharArray);
    REQUIRE_CRASH((wil::zstring_view{ badCharArrayOffByOne[0] }));

    // Test constructing from custom string type
    CustomNoncopyableString customString;
    wil::zstring_view fromCustomString(customString);
    REQUIRE(fromCustomString == (PCSTR)customString);
}

TEST_CASE("StlTests::TestZWStringView", "[stl][zstring_view]")
{
    // Test empty cases
    REQUIRE(wil::zwstring_view{}.length() == (size_t)0u);
    REQUIRE(wil::zwstring_view{}.data() == nullptr);
    REQUIRE(wil::zwstring_view{}.c_str() == nullptr);

    // Test empty string cases
    REQUIRE(wil::zwstring_view{L""}[0] == L'\0');
    REQUIRE(wil::zwstring_view{L""}.c_str()[0] == L'\0');
    REQUIRE(wil::zwstring_view{L""}.length() == 0);

    // Test different constructor equality
    constexpr wil::zwstring_view fromLiteral = L"abc";
    REQUIRE(fromLiteral.length() == wcslen(L"abc"));

    std::wstring stlString = L"abc";
    wil::zwstring_view fromString(stlString);
    wil::zwstring_view fromPtr(stlString.data());

    static constexpr wchar_t charArray[] = L"abc";
    constexpr wil::zwstring_view fromArray(charArray);

    static constexpr wchar_t extendedCharArray[] = L"abc\0\0\0\0\0";
    constexpr wil::zwstring_view fromExtendedArray(extendedCharArray);

    wil::zwstring_view copy = fromLiteral;

    REQUIRE(fromLiteral == stlString);
    REQUIRE(fromLiteral == fromString);
    REQUIRE(fromLiteral == fromArray);
    REQUIRE(fromLiteral == fromExtendedArray);
    REQUIRE(fromLiteral == copy);

    // Test decay to std::wstring_view
    std::wstring_view sv = fromLiteral;
    REQUIRE(sv == fromLiteral);

    // Test operator[]
    REQUIRE(fromLiteral[0] == L'a');
    REQUIRE(fromLiteral[1] == L'b');
    REQUIRE(fromLiteral[2] == L'c');
    REQUIRE(fromLiteral[3] == L'\0');

    // Test constructing with no NULL in range
    static constexpr wchar_t badCharArray[2][3] = {{ L'a', L'b', L'c' }, { L'a', L'b', L'c' } };
    REQUIRE_CRASH((wil::zwstring_view{ &badCharArray[0][0], _countof(badCharArray[0]) }));
    REQUIRE_CRASH((wil::zwstring_view{ badCharArray[0] }));

    // Test constructing with a NULL one character past the valid range, guarding against off-by-one errors
    // Overloads taking an explicit length trust the user that they ensure valid memory follows the buffer
    static constexpr wchar_t badCharArrayOffByOne[2][3] = {{ L'a', L'b', L'c' }, {}};
    const wil::zwstring_view fromTerminatedCharArray(&badCharArrayOffByOne[0][0], _countof(badCharArrayOffByOne[0]));
    REQUIRE(fromLiteral == fromTerminatedCharArray);
    REQUIRE_CRASH((wil::zwstring_view{ badCharArrayOffByOne[0] }));

    // Test constructing from custom string type
    CustomNoncopyableString customString;
    wil::zwstring_view fromCustomString(customString);
    REQUIRE(fromCustomString == (PCWSTR)customString);
}
#endif
