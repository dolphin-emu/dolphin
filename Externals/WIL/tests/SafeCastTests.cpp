
#include <wil/safecast.h>

#include "common.h"

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("SafeCastTests::SafeCastThrowsTemplateCheck", "[safecast]")
{
    // In all cases, a value of '1' should be cast-able to any signed or unsigned integral type without error
    SECTION("Unqualified char")
    {
        char orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        // wil::safe_cast<unsigned char>     (orig); // No available conversion in intsafe
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        // wil::safe_cast<unsigned short>    (orig); // No available conversion in intsafe
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        // wil::safe_cast<unsigned int>      (orig); // No available conversion in intsafe
        wil::safe_cast<long>              (orig);
        // wil::safe_cast<unsigned long>     (orig); // No available conversion in intsafe
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        // wil::safe_cast<unsigned __int64>  (orig); // No available conversion in intsafe
        wil::safe_cast<__int3264>         (orig);
        // wil::safe_cast<unsigned __int3264>(orig); // No available conversion in intsafe
        // wil::safe_cast<wchar_t>           (orig); // No available conversion in intsafe
    }

    SECTION("Signed char")
    {
        signed char orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unsigned char")
    {
        unsigned char orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unqualified short")
    {
        short orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Signed short")
    {
        signed short orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unsigned short")
    {
        unsigned short orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unqualified int")
    {
        int orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Signed int")
    {
        signed int orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unsigned int")
    {
        unsigned int orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unqualified long")
    {
        long orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unsigned log")
    {
        unsigned long orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unqualified int64")
    {
        __int64 orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Signed int64")
    {
        signed __int64 orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unsigned int64")
    {
        unsigned __int64 orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unqualified int3264")
    {
        __int3264 orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("Unsigned int3264")
    {
        unsigned __int3264 orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }

    SECTION("wchar_t")
    {
        wchar_t orig = 1;
        wil::safe_cast<char>              (orig);
        wil::safe_cast<signed char>       (orig);
        wil::safe_cast<unsigned char>     (orig);
        wil::safe_cast<short>             (orig);
        wil::safe_cast<signed short>      (orig);
        wil::safe_cast<unsigned short>    (orig);
        wil::safe_cast<int>               (orig);
        wil::safe_cast<signed int>        (orig);
        wil::safe_cast<unsigned int>      (orig);
        wil::safe_cast<long>              (orig);
        wil::safe_cast<unsigned long>     (orig);
        wil::safe_cast<__int64>           (orig);
        wil::safe_cast<signed __int64>    (orig);
        wil::safe_cast<unsigned __int64>  (orig);
        wil::safe_cast<__int3264>         (orig);
        wil::safe_cast<unsigned __int3264>(orig);
        wil::safe_cast<wchar_t>           (orig);
    }
}
#endif

TEST_CASE("SafeCastTests::SafeCastFailFastSyntaxCheck", "[safecast]")
{
    SECTION("safe_cast_failfast safe")
    {
        INT i = INT_MAX;
        LONG l = wil::safe_cast_failfast<LONG>(i);
        REQUIRE(l == INT_MAX);
    }

    SECTION("safe_cast_failfast unsafe")
    {
        INT i = 0;
        SHORT s = wil::safe_cast_failfast<SHORT>(i);
        REQUIRE(s == 0);
    }

    SECTION("safe_cast_failfast unsafe to wchar_t")
    {
        INT i = 0;
        wchar_t wc = wil::safe_cast_failfast<wchar_t>(i);
        REQUIRE(wc == 0);
    }

    SECTION("safe_cast_failfast unsafe from wchar_t")
    {
        wchar_t wc = 0;
        unsigned char uc = wil::safe_cast_failfast<unsigned char>(wc);
        REQUIRE(uc == 0);
    }
}

TEST_CASE("SafeCastTests::SafeCastNoThrowSyntaxCheck", "[safecast]")
{
    SECTION("safe_cast_nothrow safe")
    {
        INT i = INT_MAX;
        LONG l = wil::safe_cast_nothrow<LONG>(i);
        REQUIRE(l == INT_MAX);
    }

    // safe_cast_nothrow one parameter unsafe, throws compiler error as expected
    // {
    //     __int64 i64 = 0;
    //     int i = wil::safe_cast_nothrow<int>(i64);
    // }

    SECTION("safe_cast_nothrow two parameter potentially unsafe due to usage of variable sized types")
    {
        SIZE_T st = 0;
        UINT ui;
        auto result = wil::safe_cast_nothrow<UINT>(st, &ui);
        REQUIRE_SUCCEEDED(result);
        REQUIRE(ui == 0);
    }

    // safe_cast_nothrow two parameter known safe, throws compiler error as expected
    // {
    //     unsigned char uc = 0;
    //     unsigned short us;
    //     auto result = wil::safe_cast_nothrow(uc, &us);
    // }

    SECTION("safe_cast_nothrow unsafe")
    {
        INT i = 0;
        SHORT s;
        auto result = wil::safe_cast_nothrow<SHORT>(i, &s);
        REQUIRE_SUCCEEDED(result);
        REQUIRE(s == 0);
    }

    SECTION("safe_cast_nothrow unsafe to wchar_t")
    {
        INT i = 0;
        wchar_t wc;
        auto result = wil::safe_cast_nothrow<wchar_t>(i, &wc);
        REQUIRE_SUCCEEDED(result);
        REQUIRE(wc == 0);
    }

    SECTION("safe_cast_nothrow unsafe from wchar_t")
    {
        wchar_t wc = 0;
        unsigned char uc;
        auto result = wil::safe_cast_nothrow<unsigned char>(wc, &uc);
        REQUIRE_SUCCEEDED(result);
        REQUIRE(uc == 0);
    }
}

TEST_CASE("SafeCastTests::SafeCastNoFailures", "[safecast]")
{
    SECTION("INT -> LONG")
    {
        INT i = INT_MAX;
        LONG l = wil::safe_cast_nothrow<LONG>(i);
        REQUIRE(l == INT_MAX);
    }

    SECTION("LONG -> INT")
    {
        LONG l = LONG_MAX;
        INT i = wil::safe_cast_nothrow<INT>(l);
        REQUIRE(i == LONG_MAX);
    }

    SECTION("INT -> UINT")
    {
        INT i = INT_MAX;
        UINT ui = wil::safe_cast_failfast<UINT>(i);
        REQUIRE(ui == INT_MAX);
    }

    SECTION("SIZE_T -> SIZE_T")
    {
        SIZE_T st = SIZE_T_MAX;
        SIZE_T st2 = wil::safe_cast_failfast<SIZE_T>(st);
        REQUIRE(st2 == SIZE_T_MAX);
    }

    SECTION("wchar_t -> uint")
    {
        wchar_t wc = 0;
        UINT ui = wil::safe_cast_failfast<UINT>(wc);
        REQUIRE(ui == 0);
    }

    SECTION("wchar_t -> unsigned char")
    {
        wchar_t wc = 0;
        unsigned char uc = wil::safe_cast_failfast<unsigned char>(wc);
        REQUIRE(uc == 0);
        auto result = wil::safe_cast_nothrow<unsigned char>(wc, &uc);
        REQUIRE_SUCCEEDED(result);
    }

    SECTION("uint -> wchar_t")
    {
        UINT ui = 0;
        wchar_t wc = wil::safe_cast_failfast<wchar_t>(ui);
        REQUIRE(wc == 0);
        auto result = wil::safe_cast_nothrow<wchar_t>(ui, &wc);
        REQUIRE_SUCCEEDED(result);
    }

#ifndef _WIN64
    SECTION("SIZE_T -> UINT")
    {
        SIZE_T st = SIZE_T_MAX;
        UINT ui = wil::safe_cast_nothrow<UINT>(st);
        REQUIRE(ui == SIZE_T_MAX);
    }
#endif
}

TEST_CASE("SafeCastTests::SafeCastNoThrowFail", "[safecast]")
{
    SECTION("size_t -> short")
    {
        size_t st = SIZE_T_MAX;
        short s;
        REQUIRE_FAILED(wil::safe_cast_nothrow(st, &s));
    }
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("SafeCastTests::SafeCastExpectFailFast", "[safecast]")
{
    // Template for safe_cast fail fast tests, fill out more instances needed
    witest::TestFailureCache failures;

    failures.clear();
    {
        size_t st = SIZE_T_MAX;
        REQUIRE_CRASH(wil::safe_cast_failfast<short>(st));
        REQUIRE(failures.size() == 1);
    }

    failures.clear();
    {
        size_t st = SIZE_T_MAX;
        REQUIRE_THROWS(wil::safe_cast<short>(st));
        REQUIRE(failures.size() == 1);
    }
}
#endif
