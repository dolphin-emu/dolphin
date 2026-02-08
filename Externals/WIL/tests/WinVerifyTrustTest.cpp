
#include <Windows.h>
#include <wincrypt.h>
#include <mscat.h>
#include <softpub.h>

#include <memory>
#include <wintrust.h>

#include <wil/resource.h>

#include "common.h"

#pragma comment(lib, "Wintrust.lib")

TEST_CASE("WilWintrustWrapperTest::VerifyWintrustDataAllocateAndFree", "[resource][wintrust]")
{
    wil::unique_wintrust_data uwvtData;
    uwvtData.cbStruct = sizeof(WINTRUST_DATA);
    DWORD zero = 0;
    REQUIRE(sizeof(WINTRUST_DATA) == uwvtData.cbStruct);

    uwvtData.reset();
    REQUIRE(zero == uwvtData.cbStruct);
}

TEST_CASE("WilWintrustWrapperTest::VerifyUniqueHCATADMINAllocateAndFree", "[resource][wintrust]")
{
    wil::unique_hcatadmin hCatAdmin;

    REQUIRE(
        CryptCATAdminAcquireContext2(
        hCatAdmin.addressof(),
        nullptr,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0));

    REQUIRE(hCatAdmin.get() != nullptr);
    hCatAdmin.reset();
    REQUIRE(hCatAdmin.get() == nullptr);
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("WilWintrustWrapperTest::VerifyUnqiueHCATINFOAllocate", "[resource][wintrust]")
{
    wil::shared_hcatadmin hCatAdmin;
    HCATINFO hCatInfo = nullptr;

    REQUIRE(
        CryptCATAdminAcquireContext2(
        hCatAdmin.addressof(),
        nullptr,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0));

    wil::unique_hcatinfo hCatInfoWrapper(hCatInfo, hCatAdmin);
    REQUIRE(hCatInfoWrapper.get() == nullptr);
}
#endif
