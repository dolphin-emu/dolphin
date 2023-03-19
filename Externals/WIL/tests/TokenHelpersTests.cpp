
#include <wil/token_helpers.h>

#include "common.h"

TEST_CASE("TokenHelpersTests::VerifyOpenCurrentAccessTokenNoThrow", "[token_helpers]")
{
    // Open current thread/process access token
    wil::unique_handle token;
    REQUIRE_SUCCEEDED(wil::open_current_access_token_nothrow(&token));
    REQUIRE(token != nullptr);

    REQUIRE_SUCCEEDED(wil::open_current_access_token_nothrow(&token, TOKEN_READ));
    REQUIRE(token != nullptr);

    REQUIRE_SUCCEEDED(wil::open_current_access_token_nothrow(&token, TOKEN_READ, wil::OpenThreadTokenAs::Current));
    REQUIRE(token != nullptr);

    REQUIRE_SUCCEEDED(wil::open_current_access_token_nothrow(&token, TOKEN_READ, wil::OpenThreadTokenAs::Self));
    REQUIRE(token != nullptr);
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("TokenHelpersTests::VerifyOpenCurrentAccessToken", "[token_helpers]")
{
    // Open current thread/process access token
    wil::unique_handle token(wil::open_current_access_token());
    REQUIRE(token != nullptr);

    token = wil::open_current_access_token(TOKEN_READ);
    REQUIRE(token != nullptr);

    token = wil::open_current_access_token(TOKEN_READ, wil::OpenThreadTokenAs::Current);
    REQUIRE(token != nullptr);

    token = wil::open_current_access_token(TOKEN_READ, wil::OpenThreadTokenAs::Self);
    REQUIRE(token != nullptr);
}
#endif

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
TEST_CASE("TokenHelpersTests::VerifyGetTokenInformationNoThrow", "[token_helpers]")
{
    SECTION("Passing a null token")
    {
        wistd::unique_ptr<TOKEN_USER> tokenInfo;
        REQUIRE_SUCCEEDED(wil::get_token_information_nothrow(tokenInfo, nullptr));
        REQUIRE(tokenInfo != nullptr);
    }

    SECTION("Passing a non null token, since it a fake token there is no tokenInfo and hence should fail, code path is correct")
    {
        HANDLE faketoken = GetStdHandle(STD_INPUT_HANDLE);
        wistd::unique_ptr<TOKEN_USER> tokenInfo;
        REQUIRE_FAILED(wil::get_token_information_nothrow(tokenInfo, faketoken));
    }
}

// Pseudo tokens can be passed to token APIs and avoid the handle allocations
// making use more efficient.
TEST_CASE("TokenHelpersTests::DemonstrateUseWithPseudoTokens", "[token_helpers]")
{
    wistd::unique_ptr<TOKEN_USER> tokenInfo;
    REQUIRE_SUCCEEDED(wil::get_token_information_nothrow(tokenInfo, GetCurrentProcessToken()));
    REQUIRE(tokenInfo != nullptr);

    REQUIRE_SUCCEEDED(wil::get_token_information_nothrow(tokenInfo, GetCurrentThreadEffectiveToken()));
    REQUIRE(tokenInfo != nullptr);

    // No thread token by default, this should fail
    REQUIRE_FAILED(wil::get_token_information_nothrow(tokenInfo, GetCurrentThreadToken()));
    REQUIRE(tokenInfo == nullptr);
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("TokenHelpersTests::VerifyGetTokenInformation", "[token_helpers]")
{
    // Passing a null token
    wistd::unique_ptr<TOKEN_USER> tokenInfo(wil::get_token_information<TOKEN_USER>(nullptr));
    REQUIRE(tokenInfo != nullptr);
}
#endif

// This fails with 'ERROR_NO_SUCH_LOGON_SESSION' on the CI machines, so disable
#ifndef WIL_FAST_BUILD
TEST_CASE("TokenHelpersTests::VerifyLinkedToken", "[token_helpers]")
{
    wil::unique_token_linked_token theToken;
    REQUIRE_SUCCEEDED(wil::get_token_information_nothrow(theToken, nullptr));

#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE_NOTHROW(wil::get_linked_token_information());
#endif
}
#endif
#endif

bool IsImpersonating()
{
    wil::unique_handle token;
    if (!::OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token))
    {
        WI_ASSERT(::GetLastError() == ERROR_NO_TOKEN);
        return false;
    }

    return true;
}

wil::unique_handle GetTokenToImpersonate()
{
    wil::unique_handle processToken;
    FAIL_FAST_IF_WIN32_BOOL_FALSE(::OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &processToken));

    wil::unique_handle impersonateToken;
    FAIL_FAST_IF_WIN32_BOOL_FALSE(::DuplicateToken(processToken.get(), SecurityImpersonation, &impersonateToken));

    return impersonateToken;
}

TEST_CASE("TokenHelpersTests::VerifyResetThreadTokenNoThrow", "[token_helpers]")
{
    auto impersonateToken = GetTokenToImpersonate();

    // Set the thread into a known state - no token.
    wil::unique_token_reverter clearThreadToken;
    REQUIRE_SUCCEEDED(wil::run_as_self_nothrow(clearThreadToken));
    REQUIRE_FALSE(IsImpersonating());

    // Set a token on the thread - the process token, guaranteed to be friendly
    wil::unique_token_reverter setThreadToken1;
    REQUIRE_SUCCEEDED(wil::impersonate_token_nothrow(impersonateToken.get(), setThreadToken1));
    REQUIRE(IsImpersonating());

    SECTION("Clear the token again, should be not impersonating, explicit reset")
    {
        wil::unique_token_reverter clearThreadAgain;
        REQUIRE_SUCCEEDED(wil::run_as_self_nothrow(clearThreadAgain));
        REQUIRE_FALSE(IsImpersonating());
        clearThreadAgain.reset();
        REQUIRE(IsImpersonating());
    }

    SECTION("Clear the token again, should be not impersonating, dtor reset")
    {
        wil::unique_token_reverter clearThreadAgain;
        REQUIRE_SUCCEEDED(wil::run_as_self_nothrow(clearThreadAgain));
        REQUIRE_FALSE(IsImpersonating());
    }
    REQUIRE(IsImpersonating());

    // Clear what we were impersonating
    setThreadToken1.reset();
    REQUIRE_FALSE(IsImpersonating());
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("TokenHelpersTests::VerifyResetThreadToken", "[token_helpers]")
{
    auto impersonateToken = GetTokenToImpersonate();

    // Set the thread into a known state - no token.
    auto clearThreadToken = wil::run_as_self();
    REQUIRE_FALSE(IsImpersonating());

    // Set a token on the thread - the process token, guaranteed to be friendly
    auto setThreadToken1 = wil::impersonate_token(impersonateToken.get());
    REQUIRE(IsImpersonating());

    SECTION("Clear the token again, should be not impersonating, explicit reset")
    {
        auto clearThreadAgain = wil::run_as_self();
        REQUIRE_FALSE(IsImpersonating());
        clearThreadAgain.reset();
        REQUIRE(IsImpersonating());
    }

    SECTION("Clear the token again, should be not impersonating, dtor reset")
    {
        auto clearThreadAgain = wil::run_as_self();
        REQUIRE_FALSE(IsImpersonating());
    }
    REQUIRE(IsImpersonating());

    // Clear what we were impersonating
    setThreadToken1.reset();
    REQUIRE_FALSE(IsImpersonating());
}
#endif // WIL_ENABLE_EXCEPTIONS

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
template <typename T, wistd::enable_if_t<!wil::details::MapTokenStructToInfoClass<T>::FixedSize>* = nullptr>
void TestGetTokenInfoForCurrentThread()
{
    wistd::unique_ptr<T> tokenInfo;
    const auto hr = wil::get_token_information_nothrow(tokenInfo, nullptr);
    REQUIRE(S_OK == hr);
}

template <typename T, wistd::enable_if_t<wil::details::MapTokenStructToInfoClass<T>::FixedSize>* = nullptr>
void TestGetTokenInfoForCurrentThread()
{
    T tokenInfo{};
    const auto hr = wil::get_token_information_nothrow(&tokenInfo, nullptr);
    REQUIRE(S_OK == hr);
}

TEST_CASE("TokenHelpersTests::VerifyGetTokenInformation2", "[token_helpers]")
{
    // Variable sized cases
    TestGetTokenInfoForCurrentThread<TOKEN_ACCESS_INFORMATION>();
    TestGetTokenInfoForCurrentThread<TOKEN_APPCONTAINER_INFORMATION>();
    TestGetTokenInfoForCurrentThread<TOKEN_DEFAULT_DACL>();
    TestGetTokenInfoForCurrentThread<TOKEN_GROUPS_AND_PRIVILEGES>();
    TestGetTokenInfoForCurrentThread<TOKEN_MANDATORY_LABEL>();
    TestGetTokenInfoForCurrentThread<TOKEN_OWNER>();
    TestGetTokenInfoForCurrentThread<TOKEN_PRIMARY_GROUP>();
    TestGetTokenInfoForCurrentThread<TOKEN_PRIVILEGES>();
    TestGetTokenInfoForCurrentThread<TOKEN_USER>();

    // Fixed size and reports size using ERROR_INSUFFICIENT_BUFFER (perf opportunity, ignore second allocation)
    TestGetTokenInfoForCurrentThread<TOKEN_ELEVATION_TYPE>();
    TestGetTokenInfoForCurrentThread<TOKEN_MANDATORY_POLICY>();
    TestGetTokenInfoForCurrentThread<TOKEN_ORIGIN>();
    TestGetTokenInfoForCurrentThread<TOKEN_SOURCE>();
    TestGetTokenInfoForCurrentThread<TOKEN_STATISTICS>();
    TestGetTokenInfoForCurrentThread<TOKEN_TYPE>();
}

TEST_CASE("TokenHelpersTests::VerifyGetTokenInformationBadLength", "[token_helpers]")
{
    // Fixed size and reports size using ERROR_BAD_LENGTH (bug)
    TestGetTokenInfoForCurrentThread<TOKEN_ELEVATION>();
}

TEST_CASE("TokenHelpersTests::VerifyGetTokenInformationSecurityImpersonationLevelErrorCases", "[token_helpers]")
{
    SECURITY_IMPERSONATION_LEVEL tokenInfo{};

    // SECURITY_IMPERSONATION_LEVEL does not support the effective token when it is implicit.
    // Demonstrate the error return in that case.
    REQUIRE(E_INVALIDARG == wil::get_token_information_nothrow(&tokenInfo, GetCurrentThreadEffectiveToken()));

    // Using an explicit token is supported but returns ERROR_NO_TOKEN when there is no
    // impersonation token be sure to use RETURN_IF_FAILED_EXPECTED() and don't use
    // the exception forms if this case is not expected.
    REQUIRE(HRESULT_FROM_WIN32(ERROR_NO_TOKEN) == wil::get_token_information_nothrow(&tokenInfo, GetCurrentThreadToken()));

    // Setup the impersonation token that SECURITY_IMPERSONATION_LEVEL requires.
    FAIL_FAST_IF_WIN32_BOOL_FALSE(ImpersonateSelf(SecurityIdentification));
    TestGetTokenInfoForCurrentThread<SECURITY_IMPERSONATION_LEVEL>();

    REQUIRE(S_OK == wil::get_token_information_nothrow(&tokenInfo, GetCurrentThreadToken()));

    RevertToSelf();
}
#endif

bool operator==(const SID_IDENTIFIER_AUTHORITY& left, const SID_IDENTIFIER_AUTHORITY& right)
{
    return memcmp(&left, &right, sizeof(left)) == 0;
}

TEST_CASE("TokenHelpersTests::StaticSid", "[token_helpers]")
{
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    auto staticSid = wil::make_static_sid(SECURITY_NT_AUTHORITY, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_GUESTS);
    auto largerSid = wil::make_static_sid(SECURITY_NT_AUTHORITY, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_GUESTS, DOMAIN_ALIAS_RID_BACKUP_OPS);

    largerSid = staticSid;
    largerSid = largerSid;
    // staticSid = largerSid; // Uncommenting this correctly fails to compile.

    REQUIRE(IsValidSid(staticSid.get()));
    REQUIRE(*GetSidSubAuthorityCount(staticSid.get()) == 2);
    REQUIRE(*GetSidIdentifierAuthority(staticSid.get()) == ntAuthority);
    REQUIRE(*GetSidSubAuthority(staticSid.get(), 0) == SECURITY_BUILTIN_DOMAIN_RID);
    REQUIRE(*GetSidSubAuthority(staticSid.get(), 1) == DOMAIN_ALIAS_RID_GUESTS);
}

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
TEST_CASE("TokenHelpersTests::TestMembership", "[token_helpers]")
{
    bool member;
    REQUIRE_SUCCEEDED(wil::test_token_membership_nothrow(
        &member,
        GetCurrentThreadEffectiveToken(),
        SECURITY_NT_AUTHORITY,
        SECURITY_AUTHENTICATED_USER_RID));
}

#ifdef WIL_ENABLE_EXCEPTIONS

TEST_CASE("TokenHelpersTests::VerifyGetTokenInfo", "[token_helpers]")
{
    REQUIRE_NOTHROW(wil::get_token_information<TOKEN_APPCONTAINER_INFORMATION>());
    REQUIRE_NOTHROW(wil::get_token_information<TOKEN_ELEVATION_TYPE>());
    REQUIRE_NOTHROW(wil::get_token_information<TOKEN_MANDATORY_POLICY>());
    REQUIRE_NOTHROW(wil::get_token_information<TOKEN_ORIGIN>());
    REQUIRE_NOTHROW(wil::get_token_information<TOKEN_STATISTICS>());
    REQUIRE_NOTHROW(wil::get_token_information<TOKEN_TYPE>());
    REQUIRE_NOTHROW(wil::get_token_information<TOKEN_ELEVATION>());

    // check a non-pointer size value to make sure the whole struct is returned.
    DWORD resultSize{};
    TOKEN_SOURCE ts{};
    auto tokenSource = wil::get_token_information<TOKEN_SOURCE>();
    GetTokenInformation(GetCurrentThreadEffectiveToken(), TokenSource, &ts, sizeof(ts), &resultSize);
    REQUIRE(memcmp(&ts, &tokenSource, sizeof(ts)) == 0);
}

TEST_CASE("TokenHelpersTests::VerifyGetTokenInfoFailFast", "[token_helpers]")
{
    // fixed size
    REQUIRE_NOTHROW(wil::get_token_information_failfast<TOKEN_APPCONTAINER_INFORMATION>());
    // variable size
    REQUIRE_NOTHROW(wil::get_token_information_failfast<TOKEN_OWNER>());
}

TEST_CASE("TokenHelpersTests::Verify_impersonate_token", "[token_helpers]")
{
    auto impersonationToken = wil::impersonate_token();
    REQUIRE_NOTHROW(wil::get_token_information<TOKEN_TYPE>());
}
#endif // WIL_ENABLE_EXCEPTIONS
#endif
