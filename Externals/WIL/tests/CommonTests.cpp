
#include <wil/common.h>
#include <wil/resource.h>
#include <wrl/client.h>

#include "common.h"

TEST_CASE("CommonTests::OutParamHelpers", "[common]")
{
    int i = 2;
    int *pOutTest = &i;
    int *pNullTest = nullptr;

    SECTION("Value type")
    {
        wil::assign_to_opt_param(pNullTest, 3);
        wil::assign_to_opt_param(pOutTest, 3);
        REQUIRE(*pOutTest == 3);
    }

    SECTION("Pointer to value type")
    {
        int **ppOutTest = &pOutTest;
        int **ppNullTest = nullptr;
        wil::assign_null_to_opt_param(ppNullTest);
        wil::assign_null_to_opt_param(ppOutTest);
        REQUIRE(*ppOutTest == nullptr);
    }

    SECTION("COM out pointer")
    {
        Microsoft::WRL::ComPtr<IUnknown> spUnk;
        IUnknown **ppunkNull = nullptr;
        IUnknown *pUnk = reinterpret_cast<IUnknown *>(1);
        IUnknown **ppUnkValid = &pUnk;

        wil::detach_to_opt_param(ppunkNull, spUnk);
        wil::detach_to_opt_param(ppUnkValid, spUnk);
        REQUIRE(*ppUnkValid == nullptr);
    }
}

TEST_CASE("CommonTests::TypeValidation", "[common]")
{
    std::unique_ptr<BYTE> boolCastClass;
    std::vector<int> noBoolCastClass;
    HRESULT hr = S_OK;
    BOOL bigBool = true;
    bool smallBool = true;
    DWORD dword = 1;
    Microsoft::WRL::ComPtr<IUnknown> comPtr;
    (void)dword;

    // NOTE: The commented out verify* calls should give compilation errors
    SECTION("verify_bool")
    {
        REQUIRE(wil::verify_bool(smallBool));
        REQUIRE(wil::verify_bool(bigBool));
        REQUIRE_FALSE(wil::verify_bool(boolCastClass));
        REQUIRE_FALSE(wil::verify_bool(comPtr));
        //wil::verify_bool(noBoolCastClass);
        //wil::verify_bool(dword);
        //wil::verify_bool(hr);
    }

    SECTION("verify_hresult")
    {
        //wil::verify_hresult(smallBool);
        //wil::verify_hresult(bigBool);
        //wil::verify_hresult(boolCastClass);
        //wil::verify_hresult(noBoolCastClass);
        //wil::verify_hresult(dword);
        //wil::verify_hresult(comPtr);
        REQUIRE(wil::verify_hresult(hr) == S_OK);
    }

    SECTION("verify_BOOL")
    {
        //wil::verify_BOOL(smallBool);
        REQUIRE(wil::verify_BOOL(bigBool));
        //wil::verify_BOOL(boolCastClass);
        //wil::verify_BOOL(noBoolCastClass);
        //wil::verify_BOOL(dword);
        //wil::verify_BOOL(comPtr);
        //wil::verify_BOOL(hr);
    }
}

template <typename T>
static void FlagsMacrosNonStatic(T none, T one, T two, T three, T four)
{
    T eval = one | four;

    REQUIRE(WI_AreAllFlagsSet(MDEC(eval), MDEC(one | four)));
    REQUIRE_FALSE(WI_AreAllFlagsSet(eval, one | three));
    REQUIRE_FALSE(WI_AreAllFlagsSet(eval, three | two));
    REQUIRE(WI_AreAllFlagsSet(eval, none));

    REQUIRE(WI_IsAnyFlagSet(MDEC(eval), MDEC(one)));
    REQUIRE(WI_IsAnyFlagSet(eval, one | four | three));
    REQUIRE_FALSE(WI_IsAnyFlagSet(eval, two));

    REQUIRE(WI_AreAllFlagsClear(MDEC(eval), MDEC(three)));
    REQUIRE(WI_AreAllFlagsClear(eval, three | two));
    REQUIRE_FALSE(WI_AreAllFlagsClear(eval, one | four));
    REQUIRE_FALSE(WI_AreAllFlagsClear(eval, one | three));

    REQUIRE(WI_IsAnyFlagClear(MDEC(eval), MDEC(three)));
    REQUIRE(WI_IsAnyFlagClear(eval, three | two));
    REQUIRE(WI_IsAnyFlagClear(eval, four | three));
    REQUIRE_FALSE(WI_IsAnyFlagClear(eval, one));
    REQUIRE_FALSE(WI_IsAnyFlagClear(eval, one | four));

    REQUIRE_FALSE(WI_IsSingleFlagSet(MDEC(eval)));
    REQUIRE(WI_IsSingleFlagSet(eval & one));

    REQUIRE(WI_IsSingleFlagSetInMask(MDEC(eval), MDEC(one)));
    REQUIRE(WI_IsSingleFlagSetInMask(eval, one | three));
    REQUIRE_FALSE(WI_IsSingleFlagSetInMask(eval, three));
    REQUIRE_FALSE(WI_IsSingleFlagSetInMask(eval, one | four));

    REQUIRE_FALSE(WI_IsClearOrSingleFlagSet(MDEC(eval)));
    REQUIRE(WI_IsClearOrSingleFlagSet(eval & one));
    REQUIRE(WI_IsClearOrSingleFlagSet(none));

    REQUIRE(WI_IsClearOrSingleFlagSetInMask(MDEC(eval), MDEC(one)));
    REQUIRE(WI_IsClearOrSingleFlagSetInMask(eval, one | three));
    REQUIRE(WI_IsClearOrSingleFlagSetInMask(eval, three));
    REQUIRE_FALSE(WI_IsClearOrSingleFlagSetInMask(eval, one | four));

    eval = none;
    WI_SetAllFlags(MDEC(eval), MDEC(one));
    REQUIRE(eval == one);
    WI_SetAllFlags(eval, one | two);
    REQUIRE(eval == (one | two));

    eval = one | two;
    WI_ClearAllFlags(MDEC(eval), one);
    REQUIRE(eval == two);
    WI_ClearAllFlags(eval, two);
    REQUIRE(eval == none);

    eval = one | two;
    WI_UpdateFlagsInMask(MDEC(eval), MDEC(two | three), MDEC(three | four));
    REQUIRE(eval == (one | three));

    eval = one;
    WI_ToggleAllFlags(MDEC(eval), MDEC(one | two));
    REQUIRE(eval == two);
}

enum class EClassTest
{
    None = 0x0,
    One = 0x1,
    Two = 0x2,
    Three = 0x4,
    Four = 0x8,
};
DEFINE_ENUM_FLAG_OPERATORS(EClassTest);

enum ERawTest : unsigned int
{
    ER_None = 0x0,
    ER_One = 0x1,
    ER_Two = 0x2,
    ER_Three = 0x4,
    ER_Four = 0x8,
};
DEFINE_ENUM_FLAG_OPERATORS(ERawTest);

TEST_CASE("CommonTests::FlagsMacros", "[common]")
{
    SECTION("Integral types")
    {
        FlagsMacrosNonStatic<char>(static_cast<char>(0), static_cast<char>(0x1), static_cast<char>(0x2), static_cast<char>(0x4), static_cast<char>(0x40));
        FlagsMacrosNonStatic<unsigned char>(0, 0x1, 0x2, 0x4, 0x80u);
        FlagsMacrosNonStatic<short>(0, 0x1, 0x2, 0x4, 0x4000);
        FlagsMacrosNonStatic<unsigned short>(0, 0x1, 0x2, 0x4, 0x8000u);
        FlagsMacrosNonStatic<long>(0, 0x1, 0x2, 0x4, 0x80000000ul);
        FlagsMacrosNonStatic<unsigned long>(0, 0x1, 0x2, 0x4, 0x80000000ul);
        FlagsMacrosNonStatic<long long>(0, 0x1, 0x2, 0x4, 0x8000000000000000ull);
        FlagsMacrosNonStatic<unsigned long long>(0, 0x1, 0x2, 0x4, 0x8000000000000000ull);
    }

    SECTION("Raw enum")
    {
        FlagsMacrosNonStatic<ERawTest>(ER_None, ER_One, ER_Two, ER_Three, ER_Four);
    }

    SECTION("Enum class")
    {
        FlagsMacrosNonStatic<EClassTest>(EClassTest::None, EClassTest::One, EClassTest::Two, EClassTest::Three, EClassTest::Four);

        EClassTest eclass = EClassTest::One | EClassTest::Two;
        REQUIRE(WI_IsFlagSet(MDEC(eclass), EClassTest::One));
        REQUIRE(WI_IsFlagSet(eclass, EClassTest::Two));
        REQUIRE_FALSE(WI_IsFlagSet(eclass, EClassTest::Three));

        REQUIRE(WI_IsFlagClear(MDEC(eclass), EClassTest::Three));
        REQUIRE_FALSE(WI_IsFlagClear(eclass, EClassTest::One));

        REQUIRE_FALSE(WI_IsSingleFlagSet(MDEC(eclass)));
        REQUIRE(WI_IsSingleFlagSet(eclass & EClassTest::One));

        eclass = EClassTest::None;
        WI_SetFlag(MDEC(eclass), EClassTest::One);
        REQUIRE(eclass == EClassTest::One);

        eclass = EClassTest::None;
        WI_SetFlagIf(eclass, EClassTest::One, false);
        REQUIRE(eclass == EClassTest::None);
        WI_SetFlagIf(eclass, EClassTest::One, true);
        REQUIRE(eclass == EClassTest::One);

        eclass = EClassTest::None;
        WI_SetFlagIf(eclass, EClassTest::One, false);
        REQUIRE(eclass == EClassTest::None);
        WI_SetFlagIf(eclass, EClassTest::One, true);
        REQUIRE(eclass == EClassTest::One);

        eclass = EClassTest::One | EClassTest::Two;
        WI_ClearFlag(eclass, EClassTest::Two);
        REQUIRE(eclass == EClassTest::One);

        eclass = EClassTest::One | EClassTest::Two;
        WI_ClearFlagIf(eclass, EClassTest::One, false);
        REQUIRE(eclass == (EClassTest::One | EClassTest::Two));
        WI_ClearFlagIf(eclass, EClassTest::One, true);
        REQUIRE(eclass == EClassTest::Two);

        eclass = EClassTest::None;
        WI_UpdateFlag(eclass, EClassTest::One, true);
        REQUIRE(eclass == EClassTest::One);
        WI_UpdateFlag(eclass, EClassTest::One, false);
        REQUIRE(eclass == EClassTest::None);

        eclass = EClassTest::One;
        WI_ToggleFlag(eclass, EClassTest::One);
        WI_ToggleFlag(eclass, EClassTest::Two);
        REQUIRE(eclass == EClassTest::Two);
    }
}
