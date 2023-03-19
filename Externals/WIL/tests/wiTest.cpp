
#include <wil/result.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>
#include <wil/filesystem.h>
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
#include <wil/wrl.h>
#endif
#include <wil/com.h>

#ifdef WIL_ENABLE_EXCEPTIONS
#include <memory>
#include <set>
#include <thread>
#include <unordered_set>
#endif

// Do not include most headers until after the WIL headers to ensure that we're not inadvertently adding any unnecessary
// dependencies to STL, WRL, or indirectly retrieved headers

#ifndef __cplusplus_winrt
#include <windows.foundation.collections.h>
#include <windows.foundation.h>
#endif

// Include Resource.h a second time after including other headers
#include <wil/resource.h>

#include "common.h"
#include "MallocSpy.h"
#include "test_objects.h"

#pragma warning(push)
#pragma warning(disable: 4702) // Unreachable code

TEST_CASE("WindowsInternalTests::CommonHelpers", "[resource]")
{
    {
        wil::unique_handle spHandle;
        REQUIRE(spHandle == nullptr);
        REQUIRE(nullptr == spHandle);
        REQUIRE_FALSE(spHandle != nullptr);
        REQUIRE_FALSE(nullptr != spHandle);

        //equivalence check will static_assert because spMutex does not allow pointer access
        wil::mutex_release_scope_exit spMutex;
        //REQUIRE(spMutex == nullptr);
        //REQUIRE(nullptr == spMutex);

        //equivalence check will static_assert because spFile does not use nullptr_t as a invalid value
        wil::unique_hfile spFile;
        //REQUIRE(spFile == nullptr);
    }
#ifdef __WIL_WINBASE_STL
    {
        wil::shared_handle spHandle;
        REQUIRE(spHandle == nullptr);
        REQUIRE(nullptr == spHandle);
        REQUIRE_FALSE(spHandle != nullptr);
        REQUIRE_FALSE(nullptr != spHandle);
    }
#endif
}

TEST_CASE("WindowsInternalTests::AssertMacros", "[result_macros]")
{
    //WI_ASSERT macros are all no-ops if in retail
#ifndef RESULT_DEBUG
    WI_ASSERT(false);
    WI_ASSERT_MSG(false, "WI_ASSERT_MSG");
    WI_ASSERT_NOASSUME(false);
    WI_ASSERT_MSG_NOASSUME(false, "WI_ASSERT_MSG_NOASSUME");
    WI_VERIFY(false);
    WI_VERIFY_MSG(false, "WI_VERIFY_MSG");
#endif

    WI_ASSERT(true);
    WI_ASSERT_MSG(true, "WI_ASSERT_MSG");
    WI_ASSERT_NOASSUME(true);
    WI_ASSERT_MSG_NOASSUME(true, "WI_ASSERT_MSG_NOASSUME");
    WI_VERIFY(true);
    WI_VERIFY_MSG(true, "WI_VERIFY_MSG");
}

void __stdcall EmptyResultMacrosLoggingCallback(wil::FailureInfo*, PWSTR, size_t) WI_NOEXCEPT
{
}

#ifdef WIL_ENABLE_EXCEPTIONS
// Test Result Macros
void TestErrorCallbacks()
{
    {
        size_t callbackCount = 0;
        auto monitor = wil::ThreadFailureCallback([&](wil::FailureInfo const &failure) -> bool
        {
            REQUIRE(failure.hr == E_ACCESSDENIED);
            callbackCount++;
            return false;
        });

        constexpr size_t depthCount = 10;
        for (size_t index = 0; index < depthCount; index++)
        {
            LOG_HR(E_ACCESSDENIED);
        }
        REQUIRE(callbackCount == depthCount);
    }
    {
        wil::ThreadFailureCache cache;

        LOG_HR(E_ACCESSDENIED);
        REQUIRE(cache.GetFailure() != nullptr);
        REQUIRE(cache.GetFailure()->hr == E_ACCESSDENIED);

        wil::ThreadFailureCache cacheNested;

        LOG_HR(E_FAIL); unsigned long errorLine = __LINE__;
        LOG_HR(E_FAIL);
        LOG_HR(E_FAIL);
        REQUIRE(cache.GetFailure()->hr == E_FAIL);
        REQUIRE(cache.GetFailure()->uLineNumber == errorLine);
        REQUIRE(cacheNested.GetFailure()->hr == E_FAIL);
        REQUIRE(cacheNested.GetFailure()->uLineNumber == errorLine);
    }
}

DWORD WINAPI ErrorCallbackThreadTest(_In_ LPVOID lpParameter)
{
    try
    {
        HANDLE hEvent = reinterpret_cast<HANDLE>(lpParameter);

        for (size_t stress = 0; stress < 200; stress++)
        {
            Sleep(1); // allow the threadpool to saturate the thread count...
            TestErrorCallbacks();
        }
        THROW_IF_WIN32_BOOL_FALSE(::SetEvent(hEvent));
    }
    catch (...)
    {
        FAIL();
    }
    return 1;
}

void StressErrorCallbacks()
{
    auto restore = witest::AssignTemporaryValue(&wil::g_fResultOutputDebugString, false);

    constexpr size_t threadCount = 20;
    wil::unique_event eventArray[threadCount];

    for (size_t index = 0; index < threadCount; index++)
    {
        eventArray[index].create();
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        THROW_IF_WIN32_BOOL_FALSE(::QueueUserWorkItem(ErrorCallbackThreadTest, eventArray[index].get(), 0));
#else
        ErrorCallbackThreadTest(eventArray[index].get());
#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */
    }
    for (size_t index = 0; index < threadCount; index++)
    {
        eventArray[index].wait();
    }
}

TEST_CASE("WindowsInternalTests::ResultMacrosStress", "[LocalOnly][result_macros][stress]")
{
    auto restore = witest::AssignTemporaryValue(&wil::g_pfnResultLoggingCallback, EmptyResultMacrosLoggingCallback);
    StressErrorCallbacks();
}
#endif

#define E_AD HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)
void SetAD()
{
    ::SetLastError(ERROR_ACCESS_DENIED);
}

class AlternateAccessDeniedException
{
};

#ifdef WIL_ENABLE_EXCEPTIONS
class DerivedAccessDeniedException : public wil::ResultException
{
public:
    DerivedAccessDeniedException() : ResultException(E_AD) {}
};

HRESULT __stdcall TestResultCaughtFromException() WI_NOEXCEPT
{
    try
    {
        throw;
    }
    catch (AlternateAccessDeniedException)
    {
        return E_AD;
    }
    catch (...)
    {
    }
    return S_OK;
}
#endif

HANDLE hValid = reinterpret_cast<HANDLE>(1);
HANDLE& hValidRef() { return hValid; }
HANDLE hNull = NULL;
HANDLE hInvalid = INVALID_HANDLE_VALUE;
void* pValid = reinterpret_cast<void *>(1);
void*& pValidRef() { return pValid; }
void* pNull = nullptr;
void*& pNullRef() { return pNull; }
bool fTrue = true;
bool& fTrueRef() { return fTrue; }
bool fFalse = false;
bool& fFalseRef() { return fFalse; }
BOOL fTRUE = TRUE;
BOOL& fTRUERef() { return fTRUE; }
BOOL fFALSE = FALSE;
DWORD errSuccess = ERROR_SUCCESS;
DWORD& errSuccessRef() { return errSuccess; }
HRESULT hrOK = S_OK;
HRESULT& hrOKRef() { return hrOK; }
HRESULT hrFAIL = E_FAIL;
HRESULT& hrFAILRef() { return hrFAIL; }
const HRESULT E_hrOutOfPaper = HRESULT_FROM_WIN32(ERROR_OUT_OF_PAPER);
NTSTATUS ntOK = STATUS_SUCCESS;
NTSTATUS& ntOKRef() { return ntOK; }
NTSTATUS ntFAIL = STATUS_NO_MEMORY;
NTSTATUS& ntFAILRef() { return ntFAIL; }
const HRESULT S_hrNtOkay = wil::details::NtStatusToHr(STATUS_SUCCESS);
const HRESULT E_hrNtAssertionFailure = wil::details::NtStatusToHr(STATUS_ASSERTION_FAILURE);

wil::StoredFailureInfo g_log;

void __stdcall ResultMacrosLoggingCallback(wil::FailureInfo *pFailure, PWSTR, size_t) WI_NOEXCEPT
{
    g_log = *pFailure;
}

enum class EType
{
    None = 0x00,
    Expected = 0x02,
    Msg = 0x04,
    FailFast = 0x08,        // overall fail fast (throw exception on successful result code, for example)
    FailFastMacro = 0x10,   // explicit use of fast fail fast (FAIL_FAST_IF...)
    NoContext = 0x20        // file and line info can be wrong (throw does not happen in context to code)
};
DEFINE_ENUM_FLAG_OPERATORS(EType);

template <typename TLambda>
bool VerifyResult(unsigned int lineNumber, EType type, HRESULT hr, TLambda&& lambda)
{
    bool succeeded = true;
#ifdef WIL_ENABLE_EXCEPTIONS
    try
    {
#endif
        HRESULT lambdaResult = E_FAIL;
        bool didFailFast = true;
        {
            didFailFast = witest::DoesCodeCrash([&]()
            {
                lambdaResult = lambda();
            });
        }
        if (WI_IsFlagSet(type, EType::FailFast))
        {
            REQUIRE(didFailFast);
        }
        else
        {
            if (WI_IsFlagClear(type, EType::Expected))
            {
                if (SUCCEEDED(hr))
                {
                    REQUIRE(hr == lambdaResult);
                    REQUIRE(lineNumber != g_log.GetFailureInfo().uLineNumber);
                    REQUIRE(!didFailFast);
                }
                else
                {
                    REQUIRE((WI_IsFlagSet(type, EType::NoContext) || (g_log.GetFailureInfo().uLineNumber == lineNumber)));
                    REQUIRE(g_log.GetFailureInfo().hr == hr);
                    REQUIRE((WI_IsFlagClear(type, EType::Msg) || (nullptr != wcsstr(g_log.GetFailureInfo().pszMessage, L"msg"))));
                    REQUIRE((WI_IsFlagClear(type, EType::FailFastMacro) || (didFailFast)));
                    REQUIRE((WI_IsFlagSet(type, EType::FailFastMacro) || (!didFailFast)));
                }
            }
        }
#ifdef WIL_ENABLE_EXCEPTIONS
    }
    catch (...)
    {
        succeeded = false;
    }
#endif

    // Ensure we come out clean...
    ::SetLastError(ERROR_SUCCESS);
    return succeeded;
}

#ifdef WIL_ENABLE_EXCEPTIONS
template <typename TLambda>
HRESULT TranslateException(TLambda&& lambda)
{
    try
    {
        lambda();
    }
    catch (wil::ResultException &re)
    {
        return re.GetErrorCode();
    }
#ifdef __cplusplus_winrt
    catch (Platform::Exception ^pe)
    {
        return wil::details::GetErrorCode(pe);
    }
#endif
    catch (...)
    {
        FAIL();
    }
    return S_OK;
}
#endif

#define REQUIRE_RETURNS(hr, lambda)             REQUIRE(VerifyResult(__LINE__, EType::None, hr, lambda))
#define REQUIRE_RETURNS_MSG(hr, lambda)         REQUIRE(VerifyResult(__LINE__, EType::Msg, hr, lambda))
#define REQUIRE_RETURNS_EXPECTED(hr, lambda)    REQUIRE(VerifyResult(__LINE__, EType::Expected, hr, lambda))

#ifdef WIL_ENABLE_EXCEPTIONS
#define REQUIRE_THROWS_RESULT(hr, lambda)       REQUIRE(VerifyResult(__LINE__, EType::None, hr, [&] { return TranslateException(lambda); }))
#define REQUIRE_THROWS_MSG(hr, lambda)          REQUIRE(VerifyResult(__LINE__, EType::Msg, hr, [&] { return TranslateException(lambda); }))
#else
#define REQUIRE_THROWS_RESULT(hr, lambda)
#define REQUIRE_THROWS_MSG(hr, lambda)
#endif

#define REQUIRE_LOG(hr, lambda)                 REQUIRE(VerifyResult(__LINE__, EType::None, hr, [&] { auto fn = (lambda); fn(); return hr; }))
#define REQUIRE_LOG_MSG(hr, lambda)             REQUIRE(VerifyResult(__LINE__, EType::Msg, hr, [&] { auto fn = (lambda); fn(); return hr; }))

#define REQUIRE_FAILFAST(hr, lambda)            REQUIRE(VerifyResult(__LINE__, EType::FailFastMacro, hr, [&] { auto fn = (lambda); fn(); return hr; }))
#define REQUIRE_FAILFAST_MSG(hr, lambda)        REQUIRE(VerifyResult(__LINE__, EType::FailFastMacro | EType::Msg, hr, [&] { auto fn = (lambda); fn(); return hr; }))
#define REQUIRE_FAILFAST_UNSPECIFIED(lambda)    REQUIRE(VerifyResult(__LINE__, EType::FailFast, S_OK, [&] { auto fn = (lambda); fn(); return S_OK; }))

TEST_CASE("WindowsInternalTests::ResultMacros", "[result_macros]")
{
    auto restoreLoggingCallback = witest::AssignTemporaryValue(&wil::g_pfnResultLoggingCallback, ResultMacrosLoggingCallback);
#ifdef WIL_ENABLE_EXCEPTIONS
    auto restoreExceptionCallback = witest::AssignTemporaryValue(&wil::g_pfnResultFromCaughtException, TestResultCaughtFromException);
#endif

    REQUIRE_RETURNS(S_OK, [] { RETURN_HR(MDEC(hrOKRef())); });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_HR(MDEC(hrOKRef())); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_HR_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__); });

    REQUIRE_RETURNS(E_FAIL, [] { RETURN_HR(E_FAIL); });
    REQUIRE_RETURNS_MSG(E_FAIL, [] { RETURN_HR_MSG(E_FAIL, "msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(E_FAIL, [] { THROW_HR(E_FAIL); });
    REQUIRE_THROWS_MSG(E_FAIL, [] { THROW_HR_MSG(E_FAIL, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_FAIL, [] { LOG_HR(E_FAIL); });
    REQUIRE_LOG_MSG(E_FAIL, [] { LOG_HR_MSG(E_FAIL, "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(E_FAIL, [] { FAIL_FAST_HR(E_FAIL); });
    REQUIRE_FAILFAST_MSG(E_FAIL, [] { FAIL_FAST_HR_MSG(E_FAIL, "msg: %d", __LINE__); });

    REQUIRE_FAILFAST_UNSPECIFIED([] { ::SetLastError(0); FAIL_FAST_LAST_ERROR(); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { ::SetLastError(0); FAIL_FAST_LAST_ERROR_MSG("msg: %d", __LINE__); });

    REQUIRE_RETURNS(E_AD, [] { SetAD(); RETURN_LAST_ERROR(); });
    REQUIRE_RETURNS_MSG(E_AD, [] { SetAD(); RETURN_LAST_ERROR_MSG("msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(E_AD, [] { SetAD(); THROW_LAST_ERROR(); });
    REQUIRE_THROWS_MSG(E_AD, [] { SetAD(); THROW_LAST_ERROR_MSG("msg: %d", __LINE__); });
    REQUIRE_LOG(E_AD, [] { SetAD(); LOG_LAST_ERROR(); });
    REQUIRE_LOG_MSG(E_AD, [] { SetAD(); LOG_LAST_ERROR_MSG("msg: %d", __LINE__); });
    REQUIRE_FAILFAST(E_AD, [] { SetAD(); FAIL_FAST_LAST_ERROR(); });
    REQUIRE_FAILFAST_MSG(E_AD, [] { SetAD(); FAIL_FAST_LAST_ERROR_MSG("msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_WIN32(MDEC(errSuccessRef())); });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_WIN32_MSG(MDEC(errSuccessRef()), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_WIN32(MDEC(errSuccessRef())); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_WIN32_MSG(MDEC(errSuccessRef()), "msg: %d", __LINE__); });

    REQUIRE_RETURNS(E_AD, [] { RETURN_WIN32(ERROR_ACCESS_DENIED); });
    REQUIRE_RETURNS_MSG(E_AD, [] { RETURN_WIN32_MSG(ERROR_ACCESS_DENIED, "msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(E_AD, [] { THROW_WIN32(ERROR_ACCESS_DENIED); });
    REQUIRE_THROWS_MSG(E_AD, [] { THROW_WIN32_MSG(ERROR_ACCESS_DENIED, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_AD, [] { LOG_WIN32(ERROR_ACCESS_DENIED); });
    REQUIRE_LOG_MSG(E_AD, [] { LOG_WIN32_MSG(ERROR_ACCESS_DENIED, "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(E_AD, [] { FAIL_FAST_WIN32(ERROR_ACCESS_DENIED); });
    REQUIRE_FAILFAST_MSG(E_AD, [] { FAIL_FAST_WIN32_MSG(ERROR_ACCESS_DENIED, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_IF_FAILED(MDEC(hrOKRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_IF_FAILED_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_IF_FAILED_EXPECTED(MDEC(hrOKRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(S_OK == THROW_IF_FAILED(MDEC(hrOKRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(S_OK == THROW_IF_FAILED_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(S_OK == LOG_IF_FAILED(MDEC(hrOKRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(S_OK == LOG_IF_FAILED_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(S_OK == FAIL_FAST_IF_FAILED(MDEC(hrOKRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(S_OK == FAIL_FAST_IF_FAILED_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__)); });

    REQUIRE_RETURNS(E_FAIL, [] { RETURN_IF_FAILED(E_FAIL); return S_OK; });
    REQUIRE_RETURNS_MSG(E_FAIL, [] { RETURN_IF_FAILED_MSG(E_FAIL, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_FAIL, [] { RETURN_IF_FAILED_EXPECTED(E_FAIL); return S_OK; });
    REQUIRE_THROWS_RESULT(E_FAIL, [] { THROW_IF_FAILED(E_FAIL); });
    REQUIRE_THROWS_MSG(E_FAIL, [] { THROW_IF_FAILED_MSG(E_FAIL, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_FAIL, [] { REQUIRE(E_FAIL == LOG_IF_FAILED(E_FAIL)); });
    REQUIRE_LOG_MSG(E_FAIL, [] { REQUIRE(E_FAIL == LOG_IF_FAILED_MSG(E_FAIL, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_FAIL, [] { FAIL_FAST_IF_FAILED(E_FAIL); });
    REQUIRE_FAILFAST_MSG(E_FAIL, [] { FAIL_FAST_IF_FAILED_MSG(E_FAIL, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_IF_WIN32_BOOL_FALSE(MDEC(fTRUERef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_IF_WIN32_BOOL_FALSE_MSG(MDEC(fTRUERef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_IF_WIN32_BOOL_FALSE_EXPECTED(MDEC(fTRUERef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(fTRUE == THROW_IF_WIN32_BOOL_FALSE(MDEC(fTRUERef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(fTRUE == THROW_IF_WIN32_BOOL_FALSE_MSG(MDEC(fTRUERef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(fTRUE == LOG_IF_WIN32_BOOL_FALSE(MDEC(fTRUERef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(fTRUE == LOG_IF_WIN32_BOOL_FALSE_MSG(MDEC(fTRUERef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(fTRUE == FAIL_FAST_IF_WIN32_BOOL_FALSE(MDEC(fTRUERef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(fTRUE == FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(MDEC(fTRUERef()), "msg: %d", __LINE__)); });

    REQUIRE_RETURNS(E_AD, [] { SetAD(); RETURN_IF_WIN32_BOOL_FALSE(fFALSE); return S_OK; });
    REQUIRE_RETURNS_MSG(E_AD, [] { SetAD(); RETURN_IF_WIN32_BOOL_FALSE_MSG(fFALSE, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_AD, [] { SetAD(); RETURN_IF_WIN32_BOOL_FALSE_EXPECTED(fFALSE); return S_OK; });
    REQUIRE_THROWS_RESULT(E_AD, [] { SetAD(); THROW_IF_WIN32_BOOL_FALSE(fFALSE); });
    REQUIRE_THROWS_MSG(E_AD, [] { SetAD(); THROW_IF_WIN32_BOOL_FALSE_MSG(fFALSE, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_AD, [] { SetAD(); REQUIRE(fFALSE == LOG_IF_WIN32_BOOL_FALSE(fFALSE)); });
    REQUIRE_LOG_MSG(E_AD, [] { SetAD(); REQUIRE(fFALSE == LOG_IF_WIN32_BOOL_FALSE_MSG(fFALSE, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_AD, [] { SetAD(); FAIL_FAST_IF_WIN32_BOOL_FALSE(fFALSE); });
    REQUIRE_FAILFAST_MSG(E_AD, [] { SetAD(); FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(fFALSE, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_IF_WIN32_ERROR(MDEC(hrOKRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_IF_WIN32_ERROR_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_IF_WIN32_ERROR_EXPECTED(MDEC(hrOKRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(S_OK == THROW_IF_WIN32_ERROR(MDEC(hrOKRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(S_OK == THROW_IF_WIN32_ERROR_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(S_OK == LOG_IF_WIN32_ERROR(MDEC(hrOKRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(S_OK == LOG_IF_WIN32_ERROR_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(S_OK == FAIL_FAST_IF_WIN32_ERROR(MDEC(hrOKRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(S_OK == FAIL_FAST_IF_WIN32_ERROR_MSG(MDEC(hrOKRef()), "msg: %d", __LINE__)); });

    REQUIRE_RETURNS(E_hrOutOfPaper, [] { RETURN_IF_WIN32_ERROR(ERROR_OUT_OF_PAPER); return S_OK; });
    REQUIRE_RETURNS_MSG(E_hrOutOfPaper, [] { RETURN_IF_WIN32_ERROR_MSG(ERROR_OUT_OF_PAPER, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_hrOutOfPaper, [] { RETURN_IF_WIN32_ERROR_EXPECTED(ERROR_OUT_OF_PAPER); return S_OK; });
    REQUIRE_THROWS_RESULT(E_hrOutOfPaper, [] { THROW_IF_WIN32_ERROR(ERROR_OUT_OF_PAPER); });
    REQUIRE_THROWS_MSG(E_hrOutOfPaper, [] { THROW_IF_WIN32_ERROR_MSG(ERROR_OUT_OF_PAPER, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_hrOutOfPaper, [] { REQUIRE(ERROR_OUT_OF_PAPER == LOG_IF_WIN32_ERROR(ERROR_OUT_OF_PAPER)); });
    REQUIRE_LOG_MSG(E_hrOutOfPaper, [] { REQUIRE(ERROR_OUT_OF_PAPER == LOG_IF_WIN32_ERROR_MSG(ERROR_OUT_OF_PAPER, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_hrOutOfPaper, [] { FAIL_FAST_IF_WIN32_ERROR(ERROR_OUT_OF_PAPER); });
    REQUIRE_FAILFAST_MSG(E_hrOutOfPaper, [] { FAIL_FAST_IF_WIN32_ERROR_MSG(ERROR_OUT_OF_PAPER, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_hrNtOkay, [] { RETURN_NTSTATUS(MDEC(ntOKRef())); });
    REQUIRE_RETURNS_MSG(S_hrNtOkay, [] { RETURN_NTSTATUS_MSG(MDEC(ntOKRef()), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_NTSTATUS(MDEC(ntOKRef())); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_NTSTATUS_MSG(MDEC(ntOKRef()), "msg: %d", __LINE__); });

    REQUIRE_RETURNS(E_hrNtAssertionFailure, [] { RETURN_NTSTATUS(STATUS_ASSERTION_FAILURE); });
    REQUIRE_RETURNS_MSG(E_hrNtAssertionFailure, [] { RETURN_NTSTATUS_MSG(STATUS_ASSERTION_FAILURE, "msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(E_hrNtAssertionFailure, [] { THROW_NTSTATUS(STATUS_ASSERTION_FAILURE); });
    REQUIRE_THROWS_MSG(E_hrNtAssertionFailure, [] { THROW_NTSTATUS_MSG(STATUS_ASSERTION_FAILURE, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_hrNtAssertionFailure, [] { LOG_NTSTATUS(STATUS_ASSERTION_FAILURE); });
    REQUIRE_LOG_MSG(E_hrNtAssertionFailure, [] { LOG_NTSTATUS_MSG(STATUS_ASSERTION_FAILURE, "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(E_hrNtAssertionFailure, [] { FAIL_FAST_NTSTATUS(STATUS_ASSERTION_FAILURE); });
    REQUIRE_FAILFAST_MSG(E_hrNtAssertionFailure, [] { FAIL_FAST_NTSTATUS_MSG(STATUS_ASSERTION_FAILURE, "msg: %d", __LINE__); });

    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_IF_NTSTATUS_FAILED_MSG(MDEC(ntOKRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_IF_NTSTATUS_FAILED_EXPECTED(MDEC(ntOKRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(STATUS_WAIT_0 == THROW_IF_NTSTATUS_FAILED(MDEC(ntOKRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(STATUS_WAIT_0 == THROW_IF_NTSTATUS_FAILED_MSG(MDEC(ntOKRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(STATUS_WAIT_0 == LOG_IF_NTSTATUS_FAILED(MDEC(ntOKRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(STATUS_WAIT_0 == LOG_IF_NTSTATUS_FAILED_MSG(MDEC(ntOKRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(STATUS_WAIT_0 == FAIL_FAST_IF_NTSTATUS_FAILED(MDEC(ntOKRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(STATUS_WAIT_0 == FAIL_FAST_IF_NTSTATUS_FAILED_MSG(MDEC(ntOKRef()), "msg: %d", __LINE__)); });

    REQUIRE_RETURNS(E_hrNtAssertionFailure, [] { RETURN_IF_NTSTATUS_FAILED(STATUS_ASSERTION_FAILURE); return S_OK; });
    REQUIRE_RETURNS_MSG(E_hrNtAssertionFailure, [] { RETURN_IF_NTSTATUS_FAILED_MSG(STATUS_ASSERTION_FAILURE, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_hrNtAssertionFailure, [] { RETURN_IF_NTSTATUS_FAILED_EXPECTED(STATUS_ASSERTION_FAILURE); return S_OK; });
    REQUIRE_THROWS_RESULT(E_hrNtAssertionFailure, [] { THROW_IF_NTSTATUS_FAILED(STATUS_ASSERTION_FAILURE); });
    REQUIRE_THROWS_MSG(E_hrNtAssertionFailure, [] { THROW_IF_NTSTATUS_FAILED_MSG(STATUS_ASSERTION_FAILURE, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_hrNtAssertionFailure, [] { REQUIRE(STATUS_ASSERTION_FAILURE == static_cast<DWORD>(LOG_IF_NTSTATUS_FAILED(STATUS_ASSERTION_FAILURE))); });
    REQUIRE_LOG_MSG(E_hrNtAssertionFailure, [] { REQUIRE(STATUS_ASSERTION_FAILURE == static_cast<DWORD>(LOG_IF_NTSTATUS_FAILED_MSG(STATUS_ASSERTION_FAILURE, "msg: %d", __LINE__))); });
    REQUIRE_FAILFAST(E_hrNtAssertionFailure, [] { FAIL_FAST_IF_NTSTATUS_FAILED(STATUS_ASSERTION_FAILURE); });
    REQUIRE_FAILFAST_MSG(E_hrNtAssertionFailure, [] { FAIL_FAST_IF_NTSTATUS_FAILED_MSG(STATUS_ASSERTION_FAILURE, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(E_OUTOFMEMORY, [] { RETURN_IF_NTSTATUS_FAILED(STATUS_NO_MEMORY); return S_OK; });
    REQUIRE_RETURNS_MSG(E_OUTOFMEMORY, [] { RETURN_IF_NTSTATUS_FAILED_MSG(STATUS_NO_MEMORY, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_OUTOFMEMORY, [] { RETURN_IF_NTSTATUS_FAILED_EXPECTED(STATUS_NO_MEMORY); return S_OK; });
    REQUIRE_THROWS_RESULT(E_OUTOFMEMORY, [] { THROW_IF_NTSTATUS_FAILED(STATUS_NO_MEMORY); });
    REQUIRE_THROWS_MSG(E_OUTOFMEMORY, [] { THROW_IF_NTSTATUS_FAILED_MSG(STATUS_NO_MEMORY, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_OUTOFMEMORY, [] { REQUIRE(STATUS_NO_MEMORY == static_cast<DWORD>(LOG_IF_NTSTATUS_FAILED(STATUS_NO_MEMORY))); });
    REQUIRE_LOG_MSG(E_OUTOFMEMORY, [] { REQUIRE(STATUS_NO_MEMORY == static_cast<DWORD>(LOG_IF_NTSTATUS_FAILED_MSG(STATUS_NO_MEMORY, "msg: %d", __LINE__))); });
    REQUIRE_FAILFAST(E_OUTOFMEMORY, [] { FAIL_FAST_IF_NTSTATUS_FAILED(STATUS_NO_MEMORY); });
    REQUIRE_FAILFAST_MSG(E_OUTOFMEMORY, [] { FAIL_FAST_IF_NTSTATUS_FAILED_MSG(STATUS_NO_MEMORY, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_IF_NULL_ALLOC(MDEC(pValidRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_IF_NULL_ALLOC_MSG(MDEC(pValidRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_IF_NULL_ALLOC_EXPECTED(MDEC(pValidRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(pValid == THROW_IF_NULL_ALLOC(MDEC(pValidRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(pValid == THROW_IF_NULL_ALLOC_MSG(MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(pValid == LOG_IF_NULL_ALLOC(MDEC(pValidRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(pValid == LOG_IF_NULL_ALLOC_MSG(MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(pValid == FAIL_FAST_IF_NULL_ALLOC(MDEC(pValidRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(pValid == FAIL_FAST_IF_NULL_ALLOC_MSG(MDEC(pValidRef()), "msg: %d", __LINE__)); });

    REQUIRE_RETURNS(E_OUTOFMEMORY, [] { RETURN_IF_NULL_ALLOC(pNull); return S_OK; });
    REQUIRE_RETURNS_MSG(E_OUTOFMEMORY, [] { RETURN_IF_NULL_ALLOC_MSG(pNull, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_OUTOFMEMORY, [] { RETURN_IF_NULL_ALLOC_EXPECTED(pNull); return S_OK; });
    REQUIRE_THROWS_RESULT(E_OUTOFMEMORY, [] { THROW_IF_NULL_ALLOC(pNull); });
    REQUIRE_THROWS_MSG(E_OUTOFMEMORY, [] { THROW_IF_NULL_ALLOC_MSG(pNull, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_OUTOFMEMORY, [] { REQUIRE(pNull == LOG_IF_NULL_ALLOC(pNull)); });
    REQUIRE_LOG_MSG(E_OUTOFMEMORY, [] { REQUIRE(pNull == LOG_IF_NULL_ALLOC_MSG(pNull, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_OUTOFMEMORY, [] { FAIL_FAST_IF_NULL_ALLOC(pNull); });
    REQUIRE_FAILFAST_MSG(E_OUTOFMEMORY, [] { FAIL_FAST_IF_NULL_ALLOC_MSG(pNull, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF(MDEC(S_OK), MDEC(fTrueRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_MSG(MDEC(S_OK), MDEC(fTrueRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_EXPECTED(MDEC(S_OK), MDEC(fTrueRef())); return S_OK; });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_HR_IF(MDEC(S_OK), MDEC(fTrueRef())); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_HR_IF_MSG(MDEC(S_OK), MDEC(fTrueRef()), "msg: %d", __LINE__); });
    REQUIRE_RETURNS(E_FAIL, [] { RETURN_HR_IF(E_FAIL, fTrue); return S_OK; });
    REQUIRE_RETURNS_MSG(E_FAIL, [] { RETURN_HR_IF_MSG(E_FAIL, fTrue, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_FAIL, [] { RETURN_HR_IF_EXPECTED(E_FAIL, fTrue); return S_OK; });
    REQUIRE_THROWS_RESULT(E_FAIL, [] { THROW_HR_IF(E_FAIL, fTrue); });
    REQUIRE_THROWS_MSG(E_FAIL, [] { THROW_HR_IF_MSG(E_FAIL, fTrue, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_FAIL, [] { REQUIRE(fTrue == LOG_HR_IF(E_FAIL, fTrue)); });
    REQUIRE_LOG_MSG(E_FAIL, [] { REQUIRE(fTrue == LOG_HR_IF_MSG(E_FAIL, fTrue, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_FAIL, [] { FAIL_FAST_HR_IF(E_FAIL, fTrue); });
    REQUIRE_FAILFAST_MSG(E_FAIL, [] { FAIL_FAST_HR_IF_MSG(E_FAIL, fTrue, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF(MDEC(S_OK), MDEC(fTrueRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_MSG(MDEC(S_OK), MDEC(fTrueRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_EXPECTED(MDEC(S_OK), MDEC(fTrueRef())); return S_OK; });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_HR_IF(MDEC(S_OK), MDEC(fTrueRef())); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_HR_IF_MSG(MDEC(S_OK), MDEC(fTrueRef()), "msg: %d", __LINE__); });
    REQUIRE_RETURNS(E_FAIL, [] { RETURN_HR_IF(E_FAIL, fTrue); return S_OK; });
    REQUIRE_RETURNS_MSG(E_FAIL, [] { RETURN_HR_IF_MSG(E_FAIL, fTrue, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_FAIL, [] { RETURN_HR_IF_EXPECTED(E_FAIL, fTrue); return S_OK; });
    REQUIRE_THROWS_RESULT(E_FAIL, [] { THROW_HR_IF(E_FAIL, fTrue); });
    REQUIRE_THROWS_MSG(E_FAIL, [] { THROW_HR_IF_MSG(E_FAIL, fTrue, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_FAIL, [] { REQUIRE(fTrue == LOG_HR_IF(E_FAIL, fTrue)); });
    REQUIRE_LOG_MSG(E_FAIL, [] { REQUIRE(fTrue == LOG_HR_IF_MSG(E_FAIL, fTrue, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_FAIL, [] { FAIL_FAST_HR_IF(E_FAIL, fTrue); });
    REQUIRE_FAILFAST_MSG(E_FAIL, [] { FAIL_FAST_HR_IF_MSG(E_FAIL, fTrue, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF(MDEC(S_OK), MDEC(fFalseRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_MSG(MDEC(S_OK), MDEC(fFalseRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_EXPECTED(MDEC(S_OK), MDEC(fFalseRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(fFalse == THROW_HR_IF(MDEC(S_OK), MDEC(fFalseRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(fFalse == THROW_HR_IF_MSG(MDEC(S_OK), MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(fFalse == LOG_HR_IF(MDEC(S_OK), MDEC(fFalseRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(fFalse == LOG_HR_IF_MSG(MDEC(S_OK), MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_HR_IF(MDEC(S_OK), MDEC(fFalseRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_HR_IF_MSG(MDEC(S_OK), MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF(E_FAIL, MDEC(fFalseRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_MSG(E_FAIL, MDEC(fFalseRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_EXPECTED(E_FAIL, MDEC(fFalseRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(fFalse == THROW_HR_IF(E_FAIL, MDEC(fFalseRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(fFalse == THROW_HR_IF_MSG(E_FAIL, MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(fFalse == LOG_HR_IF(E_FAIL, MDEC(fFalseRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(fFalse == LOG_HR_IF_MSG(E_FAIL, MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_HR_IF(E_FAIL, MDEC(fFalseRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_HR_IF_MSG(E_FAIL, MDEC(fFalseRef()), "msg: %d", __LINE__)); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF(MDEC(S_OK), MDEC(fFalseRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_MSG(MDEC(S_OK), MDEC(fFalseRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_EXPECTED(MDEC(S_OK), MDEC(fFalseRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(fFalse == THROW_HR_IF(MDEC(S_OK), MDEC(fFalseRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(fFalse == THROW_HR_IF_MSG(MDEC(S_OK), MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(fFalse == LOG_HR_IF(MDEC(S_OK), MDEC(fFalseRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(fFalse == LOG_HR_IF_MSG(MDEC(S_OK), MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_HR_IF(MDEC(S_OK), MDEC(fFalseRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_HR_IF_MSG(MDEC(S_OK), MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF(E_FAIL, MDEC(fFalseRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_MSG(E_FAIL, MDEC(fFalseRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_EXPECTED(E_FAIL, MDEC(fFalseRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(fFalse == THROW_HR_IF(E_FAIL, MDEC(fFalseRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(fFalse == THROW_HR_IF_MSG(E_FAIL, MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(fFalse == LOG_HR_IF(E_FAIL, MDEC(fFalseRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(fFalse == LOG_HR_IF_MSG(E_FAIL, MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_HR_IF(E_FAIL, MDEC(fFalseRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_HR_IF_MSG(E_FAIL, MDEC(fFalseRef()), "msg: %d", __LINE__)); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF_NULL(S_OK, pNull); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_NULL_MSG(S_OK, pNull, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_NULL_EXPECTED(S_OK, pNull); return S_OK; });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_HR_IF_NULL(S_OK, pNull); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_HR_IF_NULL_MSG(S_OK, pNull, "msg: %d", __LINE__); });
    REQUIRE_RETURNS(E_FAIL, [] { RETURN_HR_IF_NULL(E_FAIL, pNull); return S_OK; });
    REQUIRE_RETURNS_MSG(E_FAIL, [] { RETURN_HR_IF_NULL_MSG(E_FAIL, pNull, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_FAIL, [] { RETURN_HR_IF_NULL_EXPECTED(E_FAIL, pNull); return S_OK; });
    REQUIRE_THROWS_RESULT(E_FAIL, [] { THROW_HR_IF_NULL(E_FAIL, pNull); });
    REQUIRE_THROWS_MSG(E_FAIL, [] { THROW_HR_IF_NULL_MSG(E_FAIL, pNull, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_FAIL, [] { REQUIRE(pNull == LOG_HR_IF_NULL(E_FAIL, pNull)); });
    REQUIRE_LOG_MSG(E_FAIL, [] { REQUIRE(pNull == LOG_HR_IF_NULL_MSG(E_FAIL, pNull, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_FAIL, [] { FAIL_FAST_HR_IF_NULL(E_FAIL, pNull); });
    REQUIRE_FAILFAST_MSG(E_FAIL, [] { FAIL_FAST_HR_IF_NULL_MSG(E_FAIL, pNull, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF_NULL(MDEC(S_OK), MDEC(pValidRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_NULL_MSG(MDEC(S_OK), MDEC(pValidRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_NULL_EXPECTED(MDEC(S_OK), MDEC(pValidRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(pValid == THROW_HR_IF_NULL(MDEC(S_OK), MDEC(pValidRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(pValid == THROW_HR_IF_NULL_MSG(MDEC(S_OK), MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(pValid == LOG_HR_IF_NULL(MDEC(S_OK), MDEC(pValidRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(pValid == LOG_HR_IF_NULL_MSG(MDEC(S_OK), MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(pValid == FAIL_FAST_HR_IF_NULL(MDEC(S_OK), MDEC(pValidRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(pValid == FAIL_FAST_HR_IF_NULL_MSG(MDEC(S_OK), MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_RETURNS(S_OK, [] { RETURN_HR_IF_NULL(E_FAIL, MDEC(pValidRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_HR_IF_NULL_MSG(E_FAIL, MDEC(pValidRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_HR_IF_NULL_EXPECTED(E_FAIL, MDEC(pValidRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(pValid == THROW_HR_IF_NULL(E_FAIL, MDEC(pValidRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(pValid == THROW_HR_IF_NULL_MSG(E_FAIL, MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(pValid == LOG_HR_IF_NULL(E_FAIL, MDEC(pValidRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(pValid == LOG_HR_IF_NULL_MSG(E_FAIL, MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(pValid == FAIL_FAST_HR_IF_NULL(E_FAIL, MDEC(pValidRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(pValid == FAIL_FAST_HR_IF_NULL_MSG(E_FAIL, MDEC(pValidRef()), "msg: %d", __LINE__)); });

    REQUIRE_FAILFAST_UNSPECIFIED([] { ::SetLastError(0); FAIL_FAST_LAST_ERROR_IF(fTrue); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { ::SetLastError(0); FAIL_FAST_LAST_ERROR_IF_MSG(fTrue, "msg: %d", __LINE__); });
    REQUIRE_RETURNS(E_AD, [] { SetAD(); RETURN_LAST_ERROR_IF(fTrue); return S_OK; });
    REQUIRE_RETURNS_MSG(E_AD, [] { SetAD(); RETURN_LAST_ERROR_IF_MSG(fTrue, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_AD, [] { SetAD(); RETURN_LAST_ERROR_IF_EXPECTED(fTrue); return S_OK; });
    REQUIRE_THROWS_RESULT(E_AD, [] { SetAD(); THROW_LAST_ERROR_IF(fTrue); });
    REQUIRE_THROWS_MSG(E_AD, [] { SetAD(); THROW_LAST_ERROR_IF_MSG(fTrue, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_AD, [] { SetAD(); REQUIRE(fTrue == LOG_LAST_ERROR_IF(fTrue)); });
    REQUIRE_LOG_MSG(E_AD, [] { SetAD(); REQUIRE(fTrue == LOG_LAST_ERROR_IF_MSG(fTrue, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_AD, [] { SetAD(); FAIL_FAST_LAST_ERROR_IF(fTrue); });
    REQUIRE_FAILFAST_MSG(E_AD, [] { SetAD(); FAIL_FAST_LAST_ERROR_IF_MSG(fTrue, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_LAST_ERROR_IF(MDEC(fFalseRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_LAST_ERROR_IF_MSG(MDEC(fFalseRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_LAST_ERROR_IF_EXPECTED(MDEC(fFalseRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(fFalse == THROW_LAST_ERROR_IF(MDEC(fFalseRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(fFalse == THROW_LAST_ERROR_IF_MSG(MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(fFalse == LOG_LAST_ERROR_IF(MDEC(fFalseRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(fFalse == LOG_LAST_ERROR_IF_MSG(MDEC(fFalseRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_LAST_ERROR_IF(MDEC(fFalseRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_LAST_ERROR_IF_MSG(MDEC(fFalseRef()), "msg: %d", __LINE__)); });

    REQUIRE_FAILFAST_UNSPECIFIED([] { ::SetLastError(0); FAIL_FAST_LAST_ERROR_IF_NULL(pNull); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { ::SetLastError(0); FAIL_FAST_LAST_ERROR_IF_NULL_MSG(pNull, "msg: %d", __LINE__); });
    REQUIRE_RETURNS(E_AD, [] { SetAD(); RETURN_LAST_ERROR_IF_NULL(pNull); return S_OK; });
    REQUIRE_RETURNS_MSG(E_AD, [] { SetAD(); RETURN_LAST_ERROR_IF_NULL_MSG(pNull, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_AD, [] { SetAD(); RETURN_LAST_ERROR_IF_NULL_EXPECTED(pNull); return S_OK; });
    REQUIRE_THROWS_RESULT(E_AD, [] { SetAD(); THROW_LAST_ERROR_IF_NULL(pNull); });
    REQUIRE_THROWS_MSG(E_AD, [] { SetAD(); THROW_LAST_ERROR_IF_NULL_MSG(pNull, "msg: %d", __LINE__); });
    REQUIRE_LOG(E_AD, [] { SetAD(); REQUIRE(pNull == LOG_LAST_ERROR_IF_NULL(pNull)); });
    REQUIRE_LOG_MSG(E_AD, [] { SetAD(); REQUIRE(pNull == LOG_LAST_ERROR_IF_NULL_MSG(pNull, "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(E_AD, [] { SetAD(); FAIL_FAST_LAST_ERROR_IF_NULL(pNull); });
    REQUIRE_FAILFAST_MSG(E_AD, [] { SetAD(); FAIL_FAST_LAST_ERROR_IF_NULL_MSG(pNull, "msg: %d", __LINE__); });

    REQUIRE_RETURNS(S_OK, [] { RETURN_LAST_ERROR_IF_NULL(MDEC(pValidRef())); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { RETURN_LAST_ERROR_IF_NULL_MSG(MDEC(pValidRef()), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_LAST_ERROR_IF_NULL_EXPECTED(MDEC(pValidRef())); return S_OK; });
    REQUIRE_THROWS_RESULT(S_OK, [] { REQUIRE(pNull != THROW_LAST_ERROR_IF_NULL(MDEC(pValidRef()))); });
    REQUIRE_THROWS_MSG(S_OK, [] { REQUIRE(pNull != THROW_LAST_ERROR_IF_NULL_MSG(MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(pNull != LOG_LAST_ERROR_IF_NULL(MDEC(pValidRef()))); });
    REQUIRE_LOG_MSG(S_OK, [] { REQUIRE(pNull != LOG_LAST_ERROR_IF_NULL_MSG(MDEC(pValidRef()), "msg: %d", __LINE__)); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(pNull != FAIL_FAST_LAST_ERROR_IF_NULL(MDEC(pValidRef()))); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { REQUIRE(pNull != FAIL_FAST_LAST_ERROR_IF_NULL_MSG(MDEC(pValidRef()), "msg: %d", __LINE__)); });

    REQUIRE_LOG(S_OK, [] { REQUIRE(true == SUCCEEDED_LOG(MDEC(S_OK))); });
    REQUIRE_LOG(E_FAIL, [] { REQUIRE(false == SUCCEEDED_LOG(E_FAIL)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(false == FAILED_LOG(MDEC(S_OK))); });
    REQUIRE_LOG(E_FAIL, [] { REQUIRE(true == FAILED_LOG(E_FAIL)); });

    REQUIRE_LOG(ERROR_SUCCESS, [] { REQUIRE(true == SUCCEEDED_WIN32_LOG(MDEC(ERROR_SUCCESS))); });
    REQUIRE_LOG(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), [] { REQUIRE(false == SUCCEEDED_WIN32_LOG(ERROR_ACCESS_DENIED)); });
    REQUIRE_LOG(ERROR_SUCCESS, [] { REQUIRE(false == FAILED_WIN32_LOG(MDEC(ERROR_SUCCESS))); });
    REQUIRE_LOG(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), [] { REQUIRE(true == FAILED_WIN32_LOG(ERROR_ACCESS_DENIED)); });

    REQUIRE_LOG(ntOK, [] { REQUIRE(true == SUCCEEDED_NTSTATUS_LOG(MDEC(ntOK))); });
    REQUIRE_LOG(wil::details::NtStatusToHr(ntFAIL), [] { REQUIRE(false == SUCCEEDED_NTSTATUS_LOG(ntFAIL)); });
    REQUIRE_LOG(ntOK, [] { REQUIRE(false == FAILED_NTSTATUS_LOG(MDEC(ntOK))); });
    REQUIRE_LOG(wil::details::NtStatusToHr(ntFAIL), [] { REQUIRE(true == FAILED_NTSTATUS_LOG(ntFAIL)); });

    // FAIL_FAST_IMMEDIATE* directly invokes __fastfail, which we can't catch, so disabled for now
    // REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_IMMEDIATE(); });
    // REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_IMMEDIATE_IF_FAILED(E_FAIL); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(S_OK == FAIL_FAST_IMMEDIATE_IF_FAILED(MDEC(S_OK))); });
    // REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_IMMEDIATE_IF(fTrue); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(fFalse == FAIL_FAST_IMMEDIATE_IF(MDEC(fFalseRef()))); });
    // REQUIRE_FAILFAST_UNSPECIFIED([] { FAIL_FAST_IMMEDIATE_IF_NULL(pNull); });
    REQUIRE_FAILFAST(S_OK, [] { REQUIRE(pValid == FAIL_FAST_IMMEDIATE_IF_NULL(MDEC(pValidRef()))); });

#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE_RETURNS(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_RETURN(); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_RETURN_MSG("msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_RETURN_EXPECTED(); return S_OK; });
    REQUIRE_LOG(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_LOG(); });
    REQUIRE_LOG_MSG(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_LOG_MSG("msg: %d", __LINE__); });
    REQUIRE_FAILFAST(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_FAIL_FAST(); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_FAIL_FAST_MSG("msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_THROW_NORMALIZED(); });
    REQUIRE_THROWS_MSG(S_OK, [] { try { THROW_IF_FAILED(hrOK); } CATCH_THROW_NORMALIZED_MSG("msg: %d", __LINE__); });

    REQUIRE_RETURNS(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_RETURN(); return S_OK; });
    REQUIRE_RETURNS_MSG(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_RETURN_MSG("msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_RETURN_EXPECTED(); return S_OK; });
    REQUIRE_LOG(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_LOG(); });
    REQUIRE_LOG_MSG(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_LOG_MSG("msg: %d", __LINE__); });
    REQUIRE_FAILFAST(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_FAIL_FAST(); });
    REQUIRE_FAILFAST_MSG(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_FAIL_FAST_MSG("msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_THROW_NORMALIZED(); });
    REQUIRE_THROWS_MSG(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_THROW_NORMALIZED_MSG("msg: %d", __LINE__); });

    REQUIRE_FAILFAST_UNSPECIFIED([] { try { if (FAILED(hrFAIL)) { throw E_FAIL; } } CATCH_FAIL_FAST(); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { try { if (FAILED(hrFAIL)) { throw E_FAIL; } } CATCH_FAIL_FAST_MSG("msg: %d", __LINE__); });

    REQUIRE_THROWS_RESULT(E_AD, [] { THROW_EXCEPTION(MDEC(DerivedAccessDeniedException())); });
    REQUIRE_THROWS_MSG(E_AD, [] { THROW_EXCEPTION_MSG(MDEC(DerivedAccessDeniedException()), "msg: %d", __LINE__); });

    REQUIRE_LOG(E_AD, [] { try { throw AlternateAccessDeniedException(); } CATCH_LOG(); });
    REQUIRE_THROWS_RESULT(E_AD, [] { try { throw AlternateAccessDeniedException(); } CATCH_THROW_NORMALIZED(); });

    REQUIRE_RETURNS(S_OK, [] { return wil::ResultFromException([] { THROW_IF_FAILED(hrOK); }); });
    REQUIRE_RETURNS(E_FAIL, [] { return wil::ResultFromException([] { THROW_IF_FAILED(hrFAIL); }); });
    REQUIRE(E_AD == wil::ResultFromException([] { throw AlternateAccessDeniedException(); }));

    try { THROW_HR(E_FAIL); }
    catch (...) { REQUIRE(E_FAIL == wil::ResultFromCaughtException()); };
#endif

#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE_LOG(E_FAIL, [] { try { THROW_IF_FAILED(hrFAIL); } CATCH_LOG(); });
#endif

    REQUIRE_RETURNS(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; RETURN_IF_NULL_ALLOC(MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS_MSG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; RETURN_IF_NULL_ALLOC_MSG(MDEC(pInt), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; RETURN_IF_NULL_ALLOC_EXPECTED(MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); RETURN_IF_NULL_ALLOC(MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); RETURN_IF_NULL_ALLOC_MSG(MDEC(pInt), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); RETURN_IF_NULL_ALLOC_EXPECTED(MDEC(pInt)); return S_OK; });

    REQUIRE_RETURNS(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; RETURN_HR_IF_NULL(E_OUTOFMEMORY, MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS_MSG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; RETURN_HR_IF_NULL_MSG(E_OUTOFMEMORY, pInt, "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; RETURN_HR_IF_NULL_EXPECTED(E_OUTOFMEMORY, MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); RETURN_HR_IF_NULL(E_OUTOFMEMORY, MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); RETURN_HR_IF_NULL_MSG(E_OUTOFMEMORY, MDEC(pInt), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); RETURN_HR_IF_NULL_EXPECTED(E_OUTOFMEMORY, MDEC(pInt)); return S_OK; });

    REQUIRE_RETURNS(E_AD, [] { std::unique_ptr<int> pInt; SetAD(); RETURN_LAST_ERROR_IF_NULL(MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS_MSG(E_AD, [] { std::unique_ptr<int> pInt; SetAD(); RETURN_LAST_ERROR_IF_NULL_MSG(MDEC(pInt), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_AD, [] { std::unique_ptr<int> pInt; SetAD(); RETURN_LAST_ERROR_IF_NULL_EXPECTED(MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); SetAD(); RETURN_LAST_ERROR_IF_NULL(MDEC(pInt)); return S_OK; });
    REQUIRE_RETURNS_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); SetAD(); RETURN_LAST_ERROR_IF_NULL_MSG(MDEC(pInt), "msg: %d", __LINE__); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); SetAD(); RETURN_LAST_ERROR_IF_NULL_EXPECTED(MDEC(pInt)); return S_OK; });

    REQUIRE_THROWS_RESULT(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; THROW_IF_NULL_ALLOC(MDEC(pInt)); });
    REQUIRE_THROWS_MSG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; THROW_IF_NULL_ALLOC_MSG(MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_LOG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; LOG_IF_NULL_ALLOC(MDEC(pInt)); });
    REQUIRE_LOG_MSG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; LOG_IF_NULL_ALLOC_MSG(MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; FAIL_FAST_IF_NULL_ALLOC(MDEC(pInt)); });
    REQUIRE_FAILFAST_MSG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; FAIL_FAST_IF_NULL_ALLOC_MSG(MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); THROW_IF_NULL_ALLOC(MDEC(pInt)); });
    REQUIRE_THROWS_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); THROW_IF_NULL_ALLOC_MSG(MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_LOG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); LOG_IF_NULL_ALLOC(MDEC(pInt)); });
    REQUIRE_LOG_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); LOG_IF_NULL_ALLOC_MSG(MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); FAIL_FAST_IF_NULL_ALLOC(MDEC(pInt)); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); FAIL_FAST_IF_NULL_ALLOC_MSG(MDEC(pInt), "msg: %d", __LINE__); });

    REQUIRE_LOG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; LOG_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(pInt)); });
    REQUIRE_LOG_MSG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; LOG_HR_IF_NULL_MSG(MDEC(E_OUTOFMEMORY), MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(E_FAIL, [] { std::unique_ptr<int> pInt; FAIL_FAST_HR_IF_NULL(MDEC(E_FAIL), MDEC(pInt)); });
    REQUIRE_FAILFAST_MSG(E_FAIL, [] { std::unique_ptr<int> pInt; FAIL_FAST_HR_IF_NULL_MSG(MDEC(E_FAIL), MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; THROW_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(pInt)); });
    REQUIRE_THROWS_MSG(E_OUTOFMEMORY, [] { std::unique_ptr<int> pInt; THROW_HR_IF_NULL_MSG(MDEC(E_OUTOFMEMORY), MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_LOG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); LOG_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(pInt)); });
    REQUIRE_LOG_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); LOG_HR_IF_NULL_MSG(MDEC(E_OUTOFMEMORY), MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); FAIL_FAST_HR_IF_NULL(MDEC(E_FAIL), MDEC(pInt)); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); FAIL_FAST_HR_IF_NULL_MSG(MDEC(E_FAIL), MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); THROW_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(pInt)); });
    REQUIRE_THROWS_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); THROW_HR_IF_NULL_MSG(MDEC(E_OUTOFMEMORY), MDEC(pInt), "msg: %d", __LINE__); });

    REQUIRE_LOG(E_AD, [] {  std::unique_ptr<int> pInt; SetAD(); LOG_LAST_ERROR_IF_NULL(MDEC(pInt)); });
    REQUIRE_LOG_MSG(E_AD, [] { std::unique_ptr<int> pInt; SetAD(); LOG_LAST_ERROR_IF_NULL_MSG(MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(E_AD, [] { std::unique_ptr<int> pInt; SetAD(); FAIL_FAST_LAST_ERROR_IF_NULL(pInt); });
    REQUIRE_FAILFAST_MSG(E_AD, [] { std::unique_ptr<int> pInt; SetAD(); FAIL_FAST_LAST_ERROR_IF_NULL_MSG(pInt, "msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(E_AD, [] { std::unique_ptr<int> pInt; SetAD(); THROW_LAST_ERROR_IF_NULL(MDEC(pInt)); });
    REQUIRE_THROWS_MSG(E_AD, [] { std::unique_ptr<int> pInt; SetAD(); THROW_LAST_ERROR_IF_NULL_MSG(MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_LOG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); LOG_LAST_ERROR_IF_NULL(MDEC(pInt)); });
    REQUIRE_LOG_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); LOG_LAST_ERROR_IF_NULL_MSG(MDEC(pInt), "msg: %d", __LINE__); });
    REQUIRE_FAILFAST(S_OK, [] { std::unique_ptr<int> pInt(new int(5));  FAIL_FAST_LAST_ERROR_IF_NULL(pInt); });
    REQUIRE_FAILFAST_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5));  FAIL_FAST_LAST_ERROR_IF_NULL_MSG(pInt, "msg: %d", __LINE__); });
    REQUIRE_THROWS_RESULT(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); THROW_LAST_ERROR_IF_NULL(MDEC(pInt)); });
    REQUIRE_THROWS_MSG(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); THROW_LAST_ERROR_IF_NULL_MSG(MDEC(pInt), "msg: %d", __LINE__); });

    // REQUIRE_FAILFAST_UNSPECIFIED([] { std::unique_ptr<int> pInt; FAIL_FAST_IMMEDIATE_IF_NULL(pNull); });
    REQUIRE_FAILFAST(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); FAIL_FAST_IMMEDIATE_IF_NULL(MDEC(pValidRef())); });
    REQUIRE_FAILFAST_UNSPECIFIED([] { std::unique_ptr<int> pInt; FAIL_FAST_IF_NULL(pNull); });
    REQUIRE_FAILFAST(S_OK, [] { std::unique_ptr<int> pInt(new int(5)); FAIL_FAST_IF_NULL(MDEC(pInt)); });

    REQUIRE_RETURNS(E_OUTOFMEMORY, [] { Microsoft::WRL::ComPtr<IUnknown> ptr; RETURN_IF_NULL_ALLOC(MDEC(ptr)); return S_OK; });
    REQUIRE_LOG(E_OUTOFMEMORY, [] { Microsoft::WRL::ComPtr<IUnknown> ptr; LOG_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(ptr)); });

    REQUIRE_RETURNS(E_OUTOFMEMORY, [] { std::shared_ptr<int> ptr; RETURN_IF_NULL_ALLOC(MDEC(ptr)); return S_OK; });
    REQUIRE_LOG(E_OUTOFMEMORY, [] { std::shared_ptr<int> ptr; LOG_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(ptr)); });
    REQUIRE_RETURNS(S_OK, [] { std::shared_ptr<int> ptr(new int(5)); RETURN_IF_NULL_ALLOC(MDEC(ptr)); return S_OK; });
    REQUIRE_LOG(S_OK, [] { std::shared_ptr<int> ptr(new int(5)); LOG_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(ptr)); });

#ifdef __cplusplus_winrt
    REQUIRE_RETURNS(E_OUTOFMEMORY, [] { Platform::String^ str(nullptr); RETURN_IF_NULL_ALLOC(MDEC(str)); return S_OK; });
    REQUIRE_LOG(E_OUTOFMEMORY, [] { Platform::String^ str(nullptr); LOG_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(str)); });
    REQUIRE_RETURNS(S_OK, [] { Platform::String^ str(L"a"); RETURN_IF_NULL_ALLOC(MDEC(str)); return S_OK; });
    REQUIRE_LOG(S_OK, [] { Platform::String^ str(L"a"); LOG_HR_IF_NULL(MDEC(E_OUTOFMEMORY), MDEC(str)); });
#endif
}

#define WRAP_LAMBDA(code)           [&] {code;};

//these macros should all have compile errors due to use of an invalid type
void InvalidTypeChecks()
{
    std::unique_ptr<int> boolCastClass;
    std::vector<int> noBoolCastClass;

    //WRAP_LAMBDA(RETURN_IF_FAILED(fTrue));
    //WRAP_LAMBDA(RETURN_IF_FAILED(fTRUE));
    //WRAP_LAMBDA(RETURN_IF_FAILED(boolCastClass));
    //WRAP_LAMBDA(RETURN_IF_FAILED(noBoolCastClass));
    //WRAP_LAMBDA(RETURN_IF_FAILED(errSuccess));

    //WRAP_LAMBDA(RETURN_IF_WIN32_BOOL_FALSE(fTrue));
    //WRAP_LAMBDA(RETURN_IF_WIN32_BOOL_FALSE(noBoolCastClass));
    //WRAP_LAMBDA(RETURN_IF_WIN32_BOOL_FALSE(hrOK));
    //WRAP_LAMBDA(RETURN_IF_WIN32_BOOL_FALSE(errSuccess));

    //WRAP_LAMBDA(RETURN_HR_IF(errSuccess, false));
    //WRAP_LAMBDA(RETURN_HR_IF(errSuccess, true));
    //WRAP_LAMBDA(RETURN_HR_IF(hrOK, noBoolCastClass));
    //WRAP_LAMBDA(RETURN_HR_IF(hrOK, hrOK));
    //WRAP_LAMBDA(RETURN_HR_IF(hrOK, errSuccess));

    //WRAP_LAMBDA(RETURN_HR_IF_NULL(errSuccess, nullptr));
    //WRAP_LAMBDA(RETURN_HR_IF_NULL(errSuccess, pValid));

    //WRAP_LAMBDA(RETURN_LAST_ERROR_IF(noBoolCastClass));
    //WRAP_LAMBDA(RETURN_LAST_ERROR_IF(errSuccess));
    //WRAP_LAMBDA(RETURN_LAST_ERROR_IF(hrOK));

    //WRAP_LAMBDA(RETURN_IF_FAILED_EXPECTED(fTrue));
    //WRAP_LAMBDA(RETURN_IF_FAILED_EXPECTED(fTRUE));
    //WRAP_LAMBDA(RETURN_IF_FAILED_EXPECTED(boolCastClass));
    //WRAP_LAMBDA(RETURN_IF_FAILED_EXPECTED(noBoolCastClass));
    //WRAP_LAMBDA(RETURN_IF_FAILED_EXPECTED(errSuccess));

    //WRAP_LAMBDA(RETURN_IF_WIN32_BOOL_FALSE_EXPECTED(fTrue));
    //WRAP_LAMBDA(RETURN_IF_WIN32_BOOL_FALSE_EXPECTED(noBoolCastClass));
    //WRAP_LAMBDA(RETURN_IF_WIN32_BOOL_FALSE_EXPECTED(hrOK));
    //WRAP_LAMBDA(RETURN_IF_WIN32_BOOL_FALSE_EXPECTED(errSuccess));

    //LOG_IF_FAILED(fTrue);
    //LOG_IF_FAILED(fTRUE);
    //LOG_IF_FAILED(boolCastClass);
    //LOG_IF_FAILED(noBoolCastClass);
    //LOG_IF_FAILED(errSuccess);

    //LOG_IF_WIN32_BOOL_FALSE(fTrue);
    //LOG_IF_WIN32_BOOL_FALSE(noBoolCastClass);
    //LOG_IF_WIN32_BOOL_FALSE(hrOK);
    //LOG_IF_WIN32_BOOL_FALSE(errSuccess);

    //LOG_HR_IF(errSuccess, false);
    //LOG_HR_IF(errSuccess, true);
    //LOG_HR_IF(hrOK, noBoolCastClass);
    //LOG_HR_IF(hrOK, hrOK);
    //LOG_HR_IF(hrOK, errSuccess);

    //FAIL_FAST_IF_FAILED(fTrue);
    //FAIL_FAST_IF_FAILED(fTRUE);
    //FAIL_FAST_IF_FAILED(boolCastClass);
    //FAIL_FAST_IF_FAILED(noBoolCastClass);
    //FAIL_FAST_IF_FAILED(errSuccess);

    //FAIL_FAST_IF_WIN32_BOOL_FALSE(fTrue);
    //FAIL_FAST_IF_WIN32_BOOL_FALSE(noBoolCastClass);
    //FAIL_FAST_IF_WIN32_BOOL_FALSE(hrOK);
    //FAIL_FAST_IF_WIN32_BOOL_FALSE(errSuccess);

    //FAIL_FAST_HR_IF(errSuccess, false);
    //FAIL_FAST_HR_IF(errSuccess, true);
    //FAIL_FAST_HR_IF(hrOK, noBoolCastClass);
    //FAIL_FAST_HR_IF(hrOK, hrOK);
    //FAIL_FAST_HR_IF(hrOK, errSuccess);

    //THROW_IF_FAILED(fTrue);
    //THROW_IF_FAILED(fTRUE);
    //THROW_IF_FAILED(boolCastClass);
    //THROW_IF_FAILED(noBoolCastClass);
    //THROW_IF_FAILED(errSuccess);

    //THROW_IF_WIN32_BOOL_FALSE(fTrue);
    //THROW_IF_WIN32_BOOL_FALSE(noBoolCastClass);
    //THROW_IF_WIN32_BOOL_FALSE(hrOK);
    //THROW_IF_WIN32_BOOL_FALSE(errSuccess);

    //THROW_HR_IF(errSuccess, false);
    //THROW_HR_IF(errSuccess, true);
    //THROW_HR_IF(hrOK, noBoolCastClass);
    //THROW_HR_IF(hrOK, hrOK);
    //THROW_HR_IF(hrOK, errSuccess);

    //FAIL_FAST_IF(noBoolCastClass);
    //FAIL_FAST_IF(hrOK);
    //FAIL_FAST_IF(errSuccess);

    //FAIL_FAST_IMMEDIATE_IF_FAILED(fTrue);
    //FAIL_FAST_IMMEDIATE_IF_FAILED(fTRUE);
    //FAIL_FAST_IMMEDIATE_IF_FAILED(boolCastClass);
    //FAIL_FAST_IMMEDIATE_IF_FAILED(noBoolCastClass);
    //FAIL_FAST_IMMEDIATE_IF_FAILED(errSuccess);

    //FAIL_FAST_IMMEDIATE_IF(noBoolCastClass);
    //FAIL_FAST_IMMEDIATE_IF(hrOK);
    //FAIL_FAST_IMMEDIATE_IF(errSuccess);
}

TEST_CASE("WindowsInternalTests::UniqueHandle", "[resource][unique_any]")
{
    {
        // default construction test
        wil::unique_handle spHandle;
        REQUIRE(spHandle.get() == nullptr);

        // null ptr assignment creation
        wil::unique_handle spNullHandle = nullptr;
        REQUIRE(spNullHandle.get() == nullptr);

        // explicit construction from the invalid value
        wil::unique_handle spInvalidHandle(nullptr);
        REQUIRE(spInvalidHandle.get() == nullptr);

        // valid handle creation
        wil::unique_handle spValidHandle(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0));
        REQUIRE(spValidHandle.get() != nullptr);
        auto const handleValue = spValidHandle.get();

        // r-value construction
        wil::unique_handle spMoveHandle = wistd::move(spValidHandle);
        REQUIRE(spValidHandle.get() == nullptr);
        REQUIRE(spMoveHandle.get() == handleValue);

        // nullptr-assignment
        spNullHandle = nullptr;
        REQUIRE(spNullHandle.get() == nullptr);

        // r-value assignment
        spValidHandle = wistd::move(spMoveHandle);
        REQUIRE(spValidHandle.get() == handleValue);
        REQUIRE(spMoveHandle.get() == nullptr);

        // swap
        spValidHandle.swap(spMoveHandle);
        REQUIRE(spValidHandle.get() == nullptr);
        REQUIRE(spMoveHandle.get() == handleValue);

        // operator bool
        REQUIRE_FALSE(spValidHandle);
        REQUIRE(spMoveHandle);

        // release
        auto ptrValidHandle = spValidHandle.release();
        auto ptrMoveHandle = spMoveHandle.release();
        REQUIRE(ptrValidHandle == nullptr);
        REQUIRE(ptrMoveHandle == handleValue);
        REQUIRE(spValidHandle.get() == nullptr);
        REQUIRE(spMoveHandle.get() == nullptr);

        // reset
        spValidHandle.reset();
        spMoveHandle.reset();
        REQUIRE(spValidHandle.get() == nullptr);
        REQUIRE(spMoveHandle.get() == nullptr);
        spValidHandle.reset(ptrValidHandle);
        spMoveHandle.reset(ptrMoveHandle);
        REQUIRE(spValidHandle.get() == nullptr);
        REQUIRE(spMoveHandle.get() == handleValue);
        spNullHandle.reset(nullptr);
        REQUIRE(spNullHandle.get() == nullptr);

        // address
        REQUIRE(*spMoveHandle.addressof() == handleValue);
        REQUIRE(*spMoveHandle.put() == nullptr);
        *spMoveHandle.put() = ::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0);
        REQUIRE(spMoveHandle);
        REQUIRE(*(&spMoveHandle) == nullptr);
        *(&spMoveHandle) = ::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0);
        REQUIRE(spMoveHandle);
    }
    {
        // default construction test
        wil::unique_hfile spHandle;
        REQUIRE(spHandle.get() == INVALID_HANDLE_VALUE);

        // implicit construction from the invalid value
        wil::unique_hfile spNullHandle; // = nullptr;       // method explicitly disabled as nullptr isn't the invalid value
        REQUIRE(spNullHandle.get() == INVALID_HANDLE_VALUE);

        // assignment from the invalid value
        // spNullHandle = nullptr;                          // method explicitly disabled as nullptr isn't the invalid value
        REQUIRE(spNullHandle.get() == INVALID_HANDLE_VALUE);

        // explicit construction from the invalid value
        wil::unique_hfile spInvalidHandle(INVALID_HANDLE_VALUE);
        REQUIRE(spInvalidHandle.get() == INVALID_HANDLE_VALUE);

        // valid handle creation
        wchar_t tempFileName[MAX_PATH];
        REQUIRE_SUCCEEDED(witest::GetTempFileName(tempFileName));

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
        CREATEFILE2_EXTENDED_PARAMETERS params = { sizeof(params) };
        params.dwFileAttributes = FILE_ATTRIBUTE_TEMPORARY;
        wil::unique_hfile spValidHandle(::CreateFile2(tempFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, CREATE_ALWAYS, &params));
#else
        wil::unique_hfile spValidHandle(::CreateFileW(tempFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr));
#endif

        ::DeleteFileW(tempFileName);
        REQUIRE(spValidHandle.get() != INVALID_HANDLE_VALUE);
        auto const handleValue = spValidHandle.get();

        // r-value construction
        wil::unique_hfile spMoveHandle = wistd::move(spValidHandle);
        REQUIRE(spValidHandle.get() == INVALID_HANDLE_VALUE);
        REQUIRE(spMoveHandle.get() == handleValue);

        // nullptr-assignment -- uncomment to check intentional compilation error
        // spNullHandle = nullptr;

        // r-value assignment
        spValidHandle = wistd::move(spMoveHandle);
        REQUIRE(spValidHandle.get() == handleValue);
        REQUIRE(spMoveHandle.get() == INVALID_HANDLE_VALUE);

        // swap
        spValidHandle.swap(spMoveHandle);
        REQUIRE(spValidHandle.get() == INVALID_HANDLE_VALUE);
        REQUIRE(spMoveHandle.get() == handleValue);

        // operator bool
        REQUIRE_FALSE(spValidHandle);
        REQUIRE(spMoveHandle);

        // release
        auto ptrValidHandle = spValidHandle.release();
        auto ptrMoveHandle = spMoveHandle.release();
        REQUIRE(ptrValidHandle == INVALID_HANDLE_VALUE);
        REQUIRE(ptrMoveHandle == handleValue);
        REQUIRE(spValidHandle.get() == INVALID_HANDLE_VALUE);
        REQUIRE(spMoveHandle.get() == INVALID_HANDLE_VALUE);

        // reset
        spValidHandle.reset();
        spMoveHandle.reset();
        REQUIRE(spValidHandle.get() == INVALID_HANDLE_VALUE);
        REQUIRE(spMoveHandle.get() == INVALID_HANDLE_VALUE);
        spValidHandle.reset(ptrValidHandle);
        spMoveHandle.reset(ptrMoveHandle);
        REQUIRE(spValidHandle.get() == INVALID_HANDLE_VALUE);
        REQUIRE(spMoveHandle.get() == handleValue);
        // uncomment to test intentional compilation error due to conflict with INVALID_HANDLE_VALUE
        // spNullHandle.reset(nullptr);

        // address
        REQUIRE(*spMoveHandle.addressof() == handleValue);
        REQUIRE(*(&spMoveHandle) == INVALID_HANDLE_VALUE);

        wchar_t tempFileName2[MAX_PATH];
        REQUIRE_SUCCEEDED(witest::GetTempFileName(tempFileName2));

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
        CREATEFILE2_EXTENDED_PARAMETERS params2 = { sizeof(params2) };
        params2.dwFileAttributes = FILE_ATTRIBUTE_TEMPORARY;
        *(&spMoveHandle) = ::CreateFile2(tempFileName2, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, CREATE_ALWAYS, &params2);
#else
        *(&spMoveHandle) = ::CreateFileW(tempFileName2, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
#endif

        ::DeleteFileW(tempFileName2);
        REQUIRE(spMoveHandle);

        // ensure that mistaken nullptr usage is not valid...
        spMoveHandle.reset();
        *(&spMoveHandle) = nullptr;
        REQUIRE_FALSE(spMoveHandle);
    }

    auto hFirst = ::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0);
    auto hSecond= ::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0);

    wil::unique_handle spLeft(hFirst);
    wil::unique_handle spRight(hSecond);

    REQUIRE(spRight.get() == hSecond);
    REQUIRE(spLeft.get() == hFirst);
    swap(spLeft, spRight);
    REQUIRE(spLeft.get() == hSecond);
    REQUIRE(spRight.get() == hFirst);
    swap(spLeft, spRight);

    REQUIRE((spLeft.get() == spRight.get()) == (spLeft == spRight));
    REQUIRE((spLeft.get() != spRight.get()) == (spLeft != spRight));
    REQUIRE((spLeft.get() < spRight.get()) == (spLeft < spRight));
    REQUIRE((spLeft.get() <= spRight.get()) == (spLeft <= spRight));
    REQUIRE((spLeft.get() >= spRight.get()) == (spLeft >= spRight));
    REQUIRE((spLeft.get() > spRight.get()) == (spLeft > spRight));

    // test stl container use (hash & std::less)
#ifdef WIL_ENABLE_EXCEPTIONS
    std::unordered_set<wil::unique_handle> hashSet;
    hashSet.insert(std::move(spLeft));
    hashSet.insert(std::move(spRight));
    std::multiset<wil::unique_handle> set;
    set.insert(std::move(spLeft));
    set.insert(std::move(spRight));
#endif
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("WindowsInternalTests::SharedHandle", "[resource][shared_any]")
{
    // default construction
    wil::shared_handle spHandle;
    REQUIRE(spHandle.get() == nullptr);

    // pointer construction
    wil::shared_handle spValid(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0));
    auto ptr = spValid.get();
    REQUIRE(spValid.get() != nullptr);

    // null construction
    wil::shared_handle spNull = nullptr;
    REQUIRE(spNull.get() == nullptr);

    // Present to verify that it doesn't compile (disabled)
    // wil::shared_hfile spFile = nullptr;

    // copy construction
    wil::shared_handle spCopy = spValid;
    REQUIRE(spCopy.get() == ptr);

    // r-value construction
    wil::shared_handle spMove = wistd::move(spCopy);
    REQUIRE(spMove.get() == ptr);
    REQUIRE(spCopy.get() == nullptr);

    // unique handle construction
    wil::shared_handle spFromUnique = wil::unique_handle(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0));
    REQUIRE(spFromUnique.get() != nullptr);

    // direct assignment
    wil::shared_handle spAssign;
    spAssign = spValid;
    REQUIRE(spAssign.get() == ptr);

    // empty reset
    spFromUnique.reset();
    REQUIRE(spFromUnique.get() == nullptr);

    // reset against unique ptr
    spFromUnique.reset(wil::unique_handle(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0)));
    REQUIRE(spFromUnique.get() != nullptr);

    // reset against raw pointer
    spAssign.reset(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0));
    REQUIRE(spAssign.get() != nullptr);
    REQUIRE(spAssign.get() != ptr);

    // ref-count checks
    REQUIRE(spAssign.use_count() == 1);

    // bool operator
    REQUIRE(spAssign);
    spAssign.reset();
    REQUIRE_FALSE(spAssign);

    // swap and compare
    wil::shared_handle sp1(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0));
    wil::shared_handle sp2(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0));
    auto ptr1 = sp1.get();
    auto ptr2 = sp2.get();
    sp1.swap(sp2);
    REQUIRE(sp1.get() == ptr2);
    REQUIRE(sp2.get() == ptr1);
    swap(sp1, sp2);
    REQUIRE(sp1.get() == ptr1);
    REQUIRE(sp2.get() == ptr2);
    REQUIRE((ptr1 == ptr2) == (sp1 == sp2));
    REQUIRE((ptr1 != ptr2) == (sp1 != sp2));
    REQUIRE((ptr1 < ptr2) == (sp1 < sp2));
    REQUIRE((ptr1 <= ptr2) == (sp1 <= sp2));
    REQUIRE((ptr1 > ptr2) == (sp1 > sp2));
    REQUIRE((ptr1 >= ptr2) == (sp1 >= sp2));

    // construction
    wil::weak_handle wh;
    REQUIRE_FALSE(wh.lock());
    wil::weak_handle wh1 = sp1;
    REQUIRE(wh1.lock());
    REQUIRE(wh1.lock().get() == ptr1);
    wil::weak_handle wh1copy = wh1;
    REQUIRE(wh1copy.lock());

    // assignment
    wh = wh1;
    REQUIRE(wh.lock().get() == ptr1);
    wh = sp2;
    REQUIRE(wh.lock().get() == ptr2);

    // reset
    wh.reset();
    REQUIRE_FALSE(wh.lock());

    // expiration
    wh = sp1;
    sp1.reset();
    REQUIRE(wh.expired());
    REQUIRE_FALSE(wh.lock());

    // swap
    wh1 = sp1;
    wil::weak_handle wh2 = sp2;
    ptr1 = sp1.get();
    ptr2 = sp2.get();
    REQUIRE(wh1.lock().get() == ptr1);
    REQUIRE(wh2.lock().get() == ptr2);
    wh1.swap(wh2);
    REQUIRE(wh1.lock().get() == ptr2);
    REQUIRE(wh2.lock().get() == ptr1);
    swap(wh1, wh2);
    REQUIRE(wh1.lock().get() == ptr1);
    REQUIRE(wh2.lock().get() == ptr2);

    // put
    sp1.reset(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0));
    REQUIRE(sp1);
    sp1.put();   // frees the pointer...
    REQUIRE_FALSE(sp1);
    sp2 = sp1;
    REQUIRE_FALSE(sp2);
    *sp1.put() = ::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0);
    REQUIRE(sp1);
    REQUIRE_FALSE(sp2);

    // address
    sp1.reset(::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0));
    REQUIRE(sp1);
    &sp1;   // frees the pointer...
    REQUIRE_FALSE(sp1);
    sp2 = sp1;
    REQUIRE_FALSE(sp2);
    *(&sp1) = ::CreateEventEx(nullptr, nullptr, CREATE_EVENT_INITIAL_SET, 0);
    REQUIRE(sp1);
    REQUIRE_FALSE(sp2);

    // test stl container use (hash & std::less)
    std::unordered_set<wil::shared_handle> hashSet;
    hashSet.insert(sp1);
    hashSet.insert(sp2);
    std::set<wil::shared_handle> set;
    set.insert(sp1);
    set.insert(sp2);
}
#endif

template <typename event_t>
void EventTestCommon()
{
    // Constructor tests...
    event_t e1;
    REQUIRE_FALSE(e1);
    event_t e2(::CreateEventEx(nullptr, nullptr, 0, 0));
    REQUIRE(e2);
    wil::unique_handle h1(::CreateEventEx(nullptr, nullptr, 0, 0));
    REQUIRE(h1);
    event_t e3(h1.release());
    REQUIRE(e3);
    REQUIRE_FALSE(h1);
    event_t e4(std::move(e2));
    REQUIRE(e4);
    REQUIRE_FALSE(e2);

    // inherited address tests...
    REQUIRE(e4);
    &e4;
    REQUIRE_FALSE(e4);
    auto hFill = ::CreateEventEx(nullptr, nullptr, 0, 0);
    *(&e4) = hFill;
    REQUIRE(e4);
    REQUIRE(*e4.addressof() == hFill);
    REQUIRE(e4);

    // assignment...
    event_t e5;
    e5 = std::move(e4);
    REQUIRE(e5);
    REQUIRE_FALSE(e4);

    // various event-based tests
    event_t eManual;
    eManual.create(wil::EventOptions::ManualReset);
    REQUIRE_FALSE(eManual.is_signaled());
    eManual.SetEvent();
    REQUIRE(eManual.is_signaled());
    eManual.ResetEvent();
    REQUIRE_FALSE(eManual.is_signaled());
    {
        auto exit = eManual.SetEvent_scope_exit();
        REQUIRE_FALSE(eManual.is_signaled());
    }
    REQUIRE(eManual.is_signaled());
    {
        auto exit = eManual.ResetEvent_scope_exit();
        REQUIRE(eManual.is_signaled());
    }
    REQUIRE_FALSE(eManual.is_signaled());
    REQUIRE_FALSE(eManual.wait(50));
    REQUIRE_FALSE(wil::handle_wait(eManual.get(), 50));
    eManual.SetEvent();
    REQUIRE(eManual.wait(50));
    REQUIRE(wil::handle_wait(eManual.get(), 50));

    REQUIRE(eManual.wait(50));

    REQUIRE(eManual.try_create(wil::EventOptions::ManualReset, L"IExist"));
    REQUIRE_FALSE(eManual.try_open(L"IDontExist"));
}

template <typename mutex_t>
void MutexTestCommon()
{
    // Constructor tests...
    mutex_t m1;
    REQUIRE_FALSE(m1);
    mutex_t m2(::CreateMutexEx(nullptr, nullptr, 0, 0));
    REQUIRE(m2);
    wil::unique_handle h1(::CreateMutexEx(nullptr, nullptr, 0, 0));
    REQUIRE(h1);
    mutex_t m3(h1.release());
    REQUIRE(m3);
    REQUIRE_FALSE(h1);
    mutex_t m4(std::move(m2));
    REQUIRE(m4);
    REQUIRE_FALSE(m2);

    // inherited address tests...
    REQUIRE(m4);
    &m4;
    REQUIRE_FALSE(m4);
    auto hFill = ::CreateMutexEx(nullptr, nullptr, 0, 0);
    *(&m4) = hFill;
    REQUIRE(m4);
    REQUIRE(*m4.addressof() == hFill);
    REQUIRE(m4);

    // assignment...
    mutex_t m5;
    m5 = std::move(m4);
    REQUIRE(m5);
    REQUIRE_FALSE(m4);

    // various mutex-based tests
    mutex_t eManual;
    eManual.create(nullptr, CREATE_MUTEX_INITIAL_OWNER);
    eManual.ReleaseMutex();
    eManual.create(nullptr, CREATE_MUTEX_INITIAL_OWNER);
    {
        auto release = eManual.ReleaseMutex_scope_exit();
    }
    {
        DWORD dwStatus;
        auto release = eManual.acquire(&dwStatus);
        REQUIRE(release);
        REQUIRE(dwStatus == WAIT_OBJECT_0);
    }

    // pass-through methods -- test compilation;
    REQUIRE(eManual.try_create(L"FOO-TEST"));
    REQUIRE(eManual.try_open(L"FOO-TEST"));
}

template <typename semaphore_t>
void SemaphoreTestCommon()
{
    // Constructor tests...
    semaphore_t m1;
    REQUIRE_FALSE(m1);
    semaphore_t m2(::CreateSemaphoreEx(nullptr, 1, 1, nullptr, 0, 0));
    REQUIRE(m2);
    wil::unique_handle h1(::CreateSemaphoreEx(nullptr, 1, 1, nullptr, 0, 0));
    REQUIRE(h1);
    semaphore_t m3(h1.release());
    REQUIRE(m3);
    REQUIRE_FALSE(h1);
    semaphore_t m4(std::move(m2));
    REQUIRE(m4);
    REQUIRE_FALSE(m2);

    // inherited address tests...
    REQUIRE(m4);
    &m4;
    REQUIRE_FALSE(m4);
    auto hFill = ::CreateSemaphoreEx(nullptr, 1, 1, nullptr, 0, 0);
    *(&m4) = hFill;
    REQUIRE(m4);
    REQUIRE(*m4.addressof() == hFill);
    REQUIRE(m4);

    // assignment...
    semaphore_t m5;
    m5 = std::move(m4);
    REQUIRE(m5);
    REQUIRE_FALSE(m4);

    // various semaphore-based tests
    semaphore_t eManual;
    eManual.create(1, 1);
    WaitForSingleObjectEx(eManual.get(), INFINITE, true);
    eManual.ReleaseSemaphore();
    eManual.create(1, 1);
    WaitForSingleObjectEx(eManual.get(), INFINITE, true);
    {
        auto release = eManual.ReleaseSemaphore_scope_exit();
    }
    {
        DWORD dwStatus;
        auto release = eManual.acquire(&dwStatus);
        REQUIRE(release);
        REQUIRE(dwStatus == WAIT_OBJECT_0);
    }

    // pass-through methods -- test compilation;
    REQUIRE(eManual.try_create(1, 1, L"BAR-TEST"));
    REQUIRE(eManual.try_open(L"BAR-TEST"));
}

template <typename test_t>
void MutexRaiiTests()
{
    test_t var1;
    var1.create();

    {
        REQUIRE(var1.acquire());
    }

    // try_create
    bool exists = false;
    REQUIRE(var1.try_create(L"wiltestmutex", 0, MUTEX_ALL_ACCESS, nullptr, &exists));
    REQUIRE_FALSE(exists);
    test_t var2;
    REQUIRE(var2.try_create(L"wiltestmutex", 0, MUTEX_ALL_ACCESS, nullptr, &exists));
    REQUIRE(exists);
    test_t var3;
    REQUIRE_FALSE(var3.try_create(L"\\illegal\\chars\\too\\\\many\\\\namespaces", 0, MUTEX_ALL_ACCESS, nullptr, &exists));
    REQUIRE(::GetLastError() != ERROR_SUCCESS);

    // try_open
    test_t var4;
    REQUIRE_FALSE(var4.try_open(L"\\illegal\\chars\\too\\\\many\\\\namespaces"));
    REQUIRE(::GetLastError() != ERROR_SUCCESS);
    REQUIRE(var4.try_open(L"wiltestmutex"));
}

template <typename test_t>
void SemaphoreRaiiTests()
{
    test_t var1;
    var1.create(1, 1);

    {
        REQUIRE(var1.acquire());
    }

    // try_create
    bool exists = false;
    REQUIRE(var1.try_create(1, 1, L"wiltestsemaphore", MUTEX_ALL_ACCESS, nullptr, &exists));
    REQUIRE_FALSE(exists);
    test_t var2;
    REQUIRE(var2.try_create(1, 1, L"wiltestsemaphore", MUTEX_ALL_ACCESS, nullptr, &exists));
    REQUIRE(exists);
    test_t var3;
    REQUIRE_FALSE(var3.try_create(1, 1, L"\\illegal\\chars\\too\\\\many\\\\namespaces", MUTEX_ALL_ACCESS, nullptr, &exists));
    REQUIRE(::GetLastError() != ERROR_SUCCESS);

    // try_open
    test_t var4;
    REQUIRE_FALSE(var4.try_open(L"\\illegal\\chars\\too\\\\many\\\\namespaces"));
    REQUIRE(::GetLastError() != ERROR_SUCCESS);
    REQUIRE(var4.try_open(L"wiltestsemaphore"));
}

TEST_CASE("WindowsInternalTests::HandleWrappers", "[resource][unique_any]")
{
    EventTestCommon<wil::unique_event_nothrow>();
    EventTestCommon<wil::unique_event_failfast>();

    // intentionally disabled in the non-exception version...
    // wil::unique_event_nothrow testEvent2(wil::EventOptions::ManualReset);
    wil::unique_event_failfast testEvent3(wil::EventOptions::ManualReset);
#ifdef WIL_ENABLE_EXCEPTIONS
    EventTestCommon<wil::unique_event>();

    wil::unique_event testEvent(wil::EventOptions::ManualReset);
    {
        REQUIRE_FALSE(wil::event_is_signaled(testEvent.get()));
        auto eventSet = wil::SetEvent_scope_exit(testEvent.get());
        REQUIRE_FALSE(wil::event_is_signaled(testEvent.get()));
    }
    {
        REQUIRE(wil::event_is_signaled(testEvent.get()));
        auto eventSet = wil::ResetEvent_scope_exit(testEvent.get());
        REQUIRE(wil::event_is_signaled(testEvent.get()));
    }
    REQUIRE_FALSE(wil::event_is_signaled(testEvent.get()));
    REQUIRE_FALSE(wil::handle_wait(testEvent.get(), 0));

    // Exception-based - no return
    testEvent.create(wil::EventOptions::ManualReset);
#endif

    // Error-code based -- returns HR
    wil::unique_event_nothrow testEventNoExcept;
    REQUIRE(SUCCEEDED(testEventNoExcept.create(wil::EventOptions::ManualReset)));

    MutexTestCommon<wil::unique_mutex_nothrow>();
    MutexTestCommon<wil::unique_mutex_failfast>();
    MutexRaiiTests<wil::unique_mutex_nothrow>();
    MutexRaiiTests<wil::unique_mutex_failfast>();

    // intentionally disabled in the non-exception version...
    // wil::unique_mutex_nothrow testMutex2(L"FOO-TEST-2");
    wil::unique_mutex_failfast testMutex3(L"FOO-TEST-3");
#ifdef WIL_ENABLE_EXCEPTIONS
    MutexTestCommon<wil::unique_mutex>();
    MutexRaiiTests<wil::unique_mutex>();

    wil::unique_mutex testMutex(L"FOO-TEST");
    WaitForSingleObjectEx(testMutex.get(), INFINITE, TRUE);
    {
        auto release = wil::ReleaseMutex_scope_exit(testMutex.get());
    }

    // Exception-based - no return
    testMutex.create(nullptr);
#endif

    // Error-code based -- returns HR
    wil::unique_mutex_nothrow testMutexNoExcept;
    REQUIRE(SUCCEEDED(testMutexNoExcept.create(nullptr)));


    SemaphoreTestCommon<wil::unique_semaphore_nothrow>();
    SemaphoreTestCommon<wil::unique_semaphore_failfast>();
    SemaphoreRaiiTests<wil::unique_semaphore_nothrow>();
    SemaphoreRaiiTests<wil::unique_semaphore_failfast>();

    // intentionally disabled in the non-exception version...
    // wil::unique_semaphore_nothrow testSemaphore2(1, 1);
    wil::unique_semaphore_failfast testSemaphore3(1, 1);
#ifdef WIL_ENABLE_EXCEPTIONS
    SemaphoreTestCommon<wil::unique_semaphore>();
    SemaphoreRaiiTests<wil::unique_semaphore>();

    wil::unique_semaphore testSemaphore(1, 1);
    WaitForSingleObjectEx(testSemaphore.get(), INFINITE, true);
    {
        auto release = wil::ReleaseSemaphore_scope_exit(testSemaphore.get());
    }

    // Exception-based - no return
    testSemaphore.create(1, 1);
#endif

    // Error-code based -- returns HR
    wil::unique_semaphore_nothrow testSemaphoreNoExcept;
    REQUIRE(SUCCEEDED(testSemaphoreNoExcept.create(1, 1)));

    auto unique_cotaskmem_string_failfast1 = wil::make_cotaskmem_string_failfast(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_cotaskmem_string_failfast1.get()) == 0);

    auto unique_cotaskmem_string_nothrow1 = wil::make_cotaskmem_string_nothrow(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_cotaskmem_string_nothrow1.get()) == 0);

    auto unique_cotaskmem_string_nothrow2 = wil::make_cotaskmem_string_nothrow(L"");
    REQUIRE(wcscmp(L"", unique_cotaskmem_string_nothrow2.get()) == 0);

#ifdef WIL_ENABLE_EXCEPTIONS
    auto unique_cotaskmem_string_te1 = wil::make_cotaskmem_string(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_cotaskmem_string_te1.get()) == 0);

    auto unique_cotaskmem_string_te2 = wil::make_cotaskmem_string(L"");
    REQUIRE(wcscmp(L"", unique_cotaskmem_string_te2.get()) == 0);

    auto unique_cotaskmem_string_range1 = wil::make_cotaskmem_string(L"Foo", 2);
    REQUIRE(wcscmp(L"Fo", unique_cotaskmem_string_range1.get()) == 0);

    auto unique_cotaskmem_string_range2 = wil::make_cotaskmem_string(nullptr, 2);
    unique_cotaskmem_string_range2.get()[0] = L'F';
    unique_cotaskmem_string_range2.get()[1] = L'o';
    REQUIRE(wcscmp(L"Fo", unique_cotaskmem_string_range2.get()) == 0);

    auto unique_cotaskmem_string_range3 = wil::make_cotaskmem_string(nullptr, 0);
    REQUIRE(wcscmp(L"", unique_cotaskmem_string_range3.get()) == 0);
#endif

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    {
        auto verify = MakeSecureDeleterMallocSpy();
        REQUIRE_SUCCEEDED(::CoRegisterMallocSpy(verify.Get()));
        auto removeSpy = wil::scope_exit([&] { ::CoRevokeMallocSpy(); });

        auto unique_cotaskmem_string_secure_failfast1 = wil::make_cotaskmem_string_secure_failfast(L"Foo");
        REQUIRE(wcscmp(L"Foo", unique_cotaskmem_string_secure_failfast1.get()) == 0);

        auto unique_cotaskmem_string_secure_nothrow1 = wil::make_cotaskmem_string_secure_nothrow(L"Foo");
        REQUIRE(wcscmp(L"Foo", unique_cotaskmem_string_secure_nothrow1.get()) == 0);

        auto unique_cotaskmem_string_secure_nothrow2 = wil::make_cotaskmem_string_secure_nothrow(L"");
        REQUIRE(wcscmp(L"", unique_cotaskmem_string_secure_nothrow2.get()) == 0);

#ifdef WIL_ENABLE_EXCEPTIONS
        auto unique_cotaskmem_string_secure_te1 = wil::make_cotaskmem_string_secure(L"Foo");
        REQUIRE(wcscmp(L"Foo", unique_cotaskmem_string_secure_te1.get()) == 0);

        auto unique_cotaskmem_string_secure_te2 = wil::make_cotaskmem_string_secure(L"");
        REQUIRE(wcscmp(L"", unique_cotaskmem_string_secure_te2.get()) == 0);
#endif
    }

    auto unique_hlocal_string_failfast1 = wil::make_hlocal_string_failfast(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_hlocal_string_failfast1.get()) == 0);

    auto unique_hlocal_string_nothrow1 = wil::make_hlocal_string_nothrow(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_hlocal_string_nothrow1.get()) == 0);

    auto unique_hlocal_string_nothrow2 = wil::make_hlocal_string_nothrow(L"");
    REQUIRE(wcscmp(L"", unique_hlocal_string_nothrow2.get()) == 0);

    auto unique_hlocal_ansistring_failfast1 = wil::make_hlocal_ansistring_failfast("Foo");
    REQUIRE(strcmp("Foo", unique_hlocal_ansistring_failfast1.get()) == 0);

    auto unique_hlocal_ansistring_nothrow1 = wil::make_hlocal_ansistring_nothrow("Foo");
    REQUIRE(strcmp("Foo", unique_hlocal_ansistring_nothrow1.get()) == 0);

    auto unique_hlocal_ansistring_nothrow2 = wil::make_hlocal_ansistring_nothrow("");
    REQUIRE(strcmp("", unique_hlocal_ansistring_nothrow2.get()) == 0);

#ifdef WIL_ENABLE_EXCEPTIONS
    auto unique_hlocal_string_te1 = wil::make_hlocal_string(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_hlocal_string_te1.get()) == 0);

    auto unique_hlocal_string_te2 = wil::make_hlocal_string(L"");
    REQUIRE(wcscmp(L"", unique_hlocal_string_te2.get()) == 0);

    auto unique_hlocal_string_range1 = wil::make_hlocal_string(L"Foo", 2);
    REQUIRE(wcscmp(L"Fo", unique_hlocal_string_range1.get()) == 0);

    auto unique_hlocal_string_range2 = wil::make_hlocal_string(nullptr, 2);
    unique_hlocal_string_range2.get()[0] = L'F';
    unique_hlocal_string_range2.get()[1] = L'o';
    REQUIRE(wcscmp(L"Fo", unique_hlocal_string_range2.get()) == 0);

    auto unique_hlocal_string_range3 = wil::make_hlocal_string(nullptr, 0);
    REQUIRE(wcscmp(L"", unique_hlocal_string_range3.get()) == 0);

    auto unique_hlocal_ansistring_te1 = wil::make_hlocal_ansistring("Foo");
    REQUIRE(strcmp("Foo", unique_hlocal_ansistring_te1.get()) == 0);

    auto unique_hlocal_ansistring_te2 = wil::make_hlocal_ansistring("");
    REQUIRE(strcmp("", unique_hlocal_ansistring_te2.get()) == 0);

    auto unique_hlocal_ansistring_range1 = wil::make_hlocal_ansistring("Foo", 2);
    REQUIRE(strcmp("Fo", unique_hlocal_ansistring_range1.get()) == 0);

    auto unique_hlocal_ansistring_range2 = wil::make_hlocal_ansistring(nullptr, 2);
    unique_hlocal_ansistring_range2.get()[0] = L'F';
    unique_hlocal_ansistring_range2.get()[1] = L'o';
    REQUIRE(strcmp("Fo", unique_hlocal_ansistring_range2.get()) == 0);

    auto unique_hlocal_ansistring_range3 = wil::make_hlocal_ansistring(nullptr, 0);
    REQUIRE(strcmp("", unique_hlocal_ansistring_range3.get()) == 0);
#endif

    {
        auto verify = MakeSecureDeleterMallocSpy();
        REQUIRE_SUCCEEDED(::CoRegisterMallocSpy(verify.Get()));
        auto removeSpy = wil::scope_exit([&] { ::CoRevokeMallocSpy(); });

        auto unique_hlocal_string_secure_failfast1 = wil::make_hlocal_string_secure_failfast(L"Foo");
        REQUIRE(wcscmp(L"Foo", unique_hlocal_string_secure_failfast1.get()) == 0);

        auto unique_hlocal_string_secure_nothrow1 = wil::make_hlocal_string_secure_nothrow(L"Foo");
        REQUIRE(wcscmp(L"Foo", unique_hlocal_string_secure_nothrow1.get()) == 0);

        auto unique_hlocal_string_secure_nothrow2 = wil::make_hlocal_string_secure_nothrow(L"");
        REQUIRE(wcscmp(L"", unique_hlocal_string_secure_nothrow2.get()) == 0);

#ifdef WIL_ENABLE_EXCEPTIONS
        auto unique_hlocal_string_secure_te1 = wil::make_hlocal_string_secure(L"Foo");
        REQUIRE(wcscmp(L"Foo", unique_hlocal_string_secure_te1.get()) == 0);

        auto unique_hlocal_string_secure_te2 = wil::make_hlocal_string_secure(L"");
        REQUIRE(wcscmp(L"", unique_hlocal_string_secure_te2.get()) == 0);
#endif
    }

    auto unique_process_heap_string_failfast1 = wil::make_process_heap_string_failfast(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_process_heap_string_failfast1.get()) == 0);

    auto unique_process_heap_string_nothrow1 = wil::make_process_heap_string_nothrow(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_process_heap_string_nothrow1.get()) == 0);

    auto unique_process_heap_string_nothrow2 = wil::make_process_heap_string_nothrow(L"");
    REQUIRE(wcscmp(L"", unique_process_heap_string_nothrow2.get()) == 0);

#ifdef WIL_ENABLE_EXCEPTIONS
    auto unique_process_heap_string_te1 = wil::make_process_heap_string(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_process_heap_string_te1.get()) == 0);

    auto unique_process_heap_string_te2 = wil::make_process_heap_string(L"");
    REQUIRE(wcscmp(L"", unique_process_heap_string_te2.get()) == 0);

    auto unique_process_heap_string_range1 = wil::make_process_heap_string(L"Foo", 2);
    REQUIRE(wcscmp(L"Fo", unique_process_heap_string_range1.get()) == 0);

    auto unique_process_heap_string_range2 = wil::make_process_heap_string(nullptr, 2);
    unique_process_heap_string_range2.get()[0] = L'F';
    unique_process_heap_string_range2.get()[1] = L'o';
    REQUIRE(wcscmp(L"Fo", unique_process_heap_string_range2.get()) == 0);

    auto unique_process_heap_string_range3 = wil::make_process_heap_string(nullptr, 0);
    REQUIRE(wcscmp(L"", unique_process_heap_string_range3.get()) == 0);
#endif

#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */

    auto unique_bstr_failfast1 = wil::make_bstr_failfast(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_bstr_failfast1.get()) == 0);

    auto unique_bstr_nothrow1 = wil::make_bstr_nothrow(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_bstr_nothrow1.get()) == 0);

    auto unique_bstr_nothrow2 = wil::make_bstr_nothrow(L"");
    REQUIRE(wcscmp(L"", unique_bstr_nothrow2.get()) == 0);

    auto unique_variant_bstr_failfast1 = wil::make_variant_bstr_failfast(L"Foo");
    REQUIRE(wcscmp(L"Foo", V_UNION(unique_variant_bstr_failfast1.addressof(), bstrVal)) == 0);

    auto unique_variant_bstr_nothrow1 = wil::make_variant_bstr_nothrow(L"Foo");
    REQUIRE(wcscmp(L"Foo", V_UNION(unique_variant_bstr_nothrow1.addressof(), bstrVal)) == 0);

    auto unique_variant_bstr_nothrow2 = wil::make_variant_bstr_nothrow(L"");
    REQUIRE(wcscmp(L"", V_UNION(unique_variant_bstr_nothrow2.addressof(), bstrVal)) == 0);

#ifdef WIL_ENABLE_EXCEPTIONS
    auto unique_bstr_te1 = wil::make_bstr(L"Foo");
    REQUIRE(wcscmp(L"Foo", unique_bstr_te1.get()) == 0);

    auto unique_bstr_te2 = wil::make_bstr(L"");
    REQUIRE(wcscmp(L"", unique_bstr_te2.get()) == 0);

    auto unique_variant_bstr_te1 = wil::make_variant_bstr(L"Foo");
    REQUIRE(wcscmp(L"Foo", V_UNION(unique_variant_bstr_te1.addressof(), bstrVal)) == 0);

    auto unique_variant_bstr_te2 = wil::make_variant_bstr(L"");
    REQUIRE(wcscmp(L"", V_UNION(unique_variant_bstr_te2.addressof(), bstrVal)) == 0);

    auto testString = wil::make_cotaskmem_string(L"Foo");
    {
        auto cleanupMemory = wil::SecureZeroMemory_scope_exit(testString.get());
    }
    REQUIRE(0 == testString.get()[0]);

    auto testString2 = wil::make_cotaskmem_string(L"Bar");
    {
        auto cleanupMemory = wil::SecureZeroMemory_scope_exit(testString2.get(), wcslen(testString2.get()) * sizeof(testString2.get()[0]));
    }
    REQUIRE(0 == testString2.get()[0]);
#endif
}

TEST_CASE("WindowsInternalTests::Locking", "[resource]")
{
    {
        SRWLOCK rwlock = SRWLOCK_INIT;
        {
            auto lock = wil::AcquireSRWLockExclusive(&rwlock);
            REQUIRE(lock);

            auto lockRecursive = wil::TryAcquireSRWLockExclusive(&rwlock);
            REQUIRE_FALSE(lockRecursive);

            auto lockRecursiveShared = wil::TryAcquireSRWLockShared(&rwlock);
            REQUIRE_FALSE(lockRecursiveShared);
        }
        {
            auto lock = wil::AcquireSRWLockShared(&rwlock);
            REQUIRE(lock);

            auto lockRecursive = wil::TryAcquireSRWLockShared(&rwlock);
            REQUIRE(lockRecursive);

            auto lockRecursiveExclusive = wil::TryAcquireSRWLockExclusive(&rwlock);
            REQUIRE_FALSE(lockRecursiveExclusive);
        }
        {
            auto lock = wil::TryAcquireSRWLockExclusive(&rwlock);
            REQUIRE(lock);
        }
        {
            auto lock = wil::TryAcquireSRWLockShared(&rwlock);
            REQUIRE(lock);
        }
    }

    {
        wil::srwlock rwlock;
        {
            auto lock = rwlock.lock_exclusive();
            REQUIRE(lock);

            auto lockRecursive = rwlock.try_lock_exclusive();
            REQUIRE_FALSE(lockRecursive);

            auto lockRecursiveShared = rwlock.try_lock_shared();
            REQUIRE_FALSE(lockRecursiveShared);
        }
        {
            auto lock = rwlock.lock_shared();
            REQUIRE(lock);

            auto lockRecursive = rwlock.try_lock_shared();
            REQUIRE(lockRecursive);

            auto lockRecursiveExclusive = rwlock.try_lock_exclusive();
            REQUIRE_FALSE(lockRecursiveExclusive);
        }
        {
            auto lock = rwlock.try_lock_exclusive();
            REQUIRE(lock);
        }
        {
            auto lock = rwlock.try_lock_shared();
            REQUIRE(lock);
        }
    }

    {
        CRITICAL_SECTION cs;
        ::InitializeCriticalSectionEx(&cs, 0, 0);
        {
            auto lock = wil::EnterCriticalSection(&cs);
            REQUIRE(lock);
            auto tryLock = wil::TryEnterCriticalSection(&cs);
            REQUIRE(tryLock);
        }
        ::DeleteCriticalSection(&cs);
    }
    {
        wil::critical_section cs;
        auto lock = cs.lock();
        REQUIRE(lock);
        auto tryLock = cs.try_lock();
        REQUIRE(tryLock);
    }
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
TEST_CASE("WindowsInternalTests::GDIWrappers", "[resource]")
{
    {
        auto dc = wil::GetDC(::GetDesktopWindow());
    }
    {
        auto dc = wil::GetWindowDC(::GetDesktopWindow());
    }
    {
        auto dc = wil::BeginPaint(::GetDesktopWindow());
        wil::unique_hbrush brush(::CreateSolidBrush(0xffffff));
        auto select = wil::SelectObject(dc.get(), brush.get());
    }
}
#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */

void TestOutHandle(_Out_ HANDLE *pHandle)
{
    *pHandle = nullptr;
}

void TestOutAlloc(_Out_ int **ppInt)
{
    *ppInt = new int(5);
}

void TestCoTask(_Outptr_result_buffer_(*charCount) PWSTR *ppsz, size_t *charCount)
{
    *charCount = 0;
    PWSTR psz = static_cast<PWSTR>(::CoTaskMemAlloc(10));
    if (psz != nullptr)
    {
        *charCount = 5;
        *psz = L'\0';
    }
    *ppsz = psz;
}

void TestVoid(_Out_ void **ppv)
{
    *ppv = nullptr;
}

void TestByte(_Out_ BYTE **ppByte)
{
    *ppByte = nullptr;
}

struct my_deleter
{
    template <typename T>
    void operator()(T* p) const
    {
        delete p;
    }
};

TEST_CASE("WindowsInternalTests::WistdTests", "[resource][wistd]")
{
    wil::unique_handle spHandle;
    TestOutHandle(wil::out_param(spHandle));

    wistd::unique_ptr<int> spInt;
    TestOutAlloc(wil::out_param(spInt));

    std::unique_ptr<int> spIntStd;
    TestOutAlloc(wil::out_param(spIntStd));

    wil::unique_cotaskmem_string spsz0;
    size_t count;
    TestCoTask(wil::out_param(spsz0), &count);

    std::unique_ptr<wchar_t[], wil::cotaskmem_deleter> spsz1;
    TestCoTask(wil::out_param(spsz1), &count);

    wistd::unique_ptr<wchar_t[], wil::cotaskmem_deleter> spsz2;
    TestCoTask(wil::out_param(spsz2), &count);

    wil::unique_cotaskmem_ptr<wchar_t[]> spsz3;
    TestCoTask(wil::out_param(spsz3), &count);

    wil::unique_cotaskmem_ptr<void> spv;
    TestVoid(wil::out_param(spv));

    std::unique_ptr<int> spIntStd2;
    TestByte(wil::out_param_ptr<BYTE**>(spIntStd2));

    struct Nothing
    {
        int n;
        Nothing(int param) : n(param) {}
        void Method() {}
    };

    auto spff = wil::make_unique_failfast<Nothing>(3);
    auto sp = wil::make_unique_nothrow<Nothing>(3);
    REQUIRE(sp);
#ifdef WIL_ENABLE_EXCEPTIONS
    THROW_IF_NULL_ALLOC(sp.get());
    THROW_IF_NULL_ALLOC(sp);
#endif
    sp->Method();
    decltype(sp) sp2;
    sp2 = wistd::move(sp);
    sp2.get();

    wistd::unique_ptr<int> spConstruct;
    wistd::unique_ptr<int> spConstruct2 = nullptr;
    spConstruct = nullptr;
    wistd::unique_ptr<int> spConstruct3(new int(3));
    my_deleter d;
    wistd::unique_ptr<int, my_deleter> spConstruct4(new int(4), d);
    wistd::unique_ptr<int, my_deleter> spConstruct5(new int(5), my_deleter());
    wistd::unique_ptr<int> spConstruct6(wistd::unique_ptr<int>(new int(6)));
    spConstruct = std::move(spConstruct2);
    spConstruct.swap(spConstruct2);
    REQUIRE(*spConstruct4 == 4);
    spConstruct4.get();
    if (spConstruct4)
    {
    }
    spConstruct.reset();
    spConstruct.release();

    auto spTooBig = wil::make_unique_nothrow<int[]>(static_cast<size_t>(-1));
    REQUIRE_FALSE(spTooBig);
    // REQUIRE_FAILFAST_UNSPECIFIED([]{ auto spTooBigFF = wil::make_unique_failfast<int[]>(static_cast<size_t>(-1)); });

    object_counter_state state;
    count = 0;
    {
        object_counter c{ state };
        REQUIRE(state.instance_count() == 1);

        wistd::function<void(int)> fn = [&count, c](int param)
        {
            count += param;
        };
        REQUIRE(state.instance_count() == 2);

        fn(3);
        REQUIRE(count == 3);
    }
    REQUIRE(state.instance_count() == 0);

    count = 0;
    {
        wistd::function<void(int)> fn;
        {
            object_counter c{ state };
            REQUIRE(state.instance_count() == 1);
            fn = [&count, c](int param)
            {
                count += param;
            };
            REQUIRE(state.instance_count() == 2);
        }
        REQUIRE(state.instance_count() == 1);
        fn(3);
        REQUIRE(count == 3);
    }

    {
        // Size Check -- the current implementation allows for 10 pointers to be passed through the lambda
        int a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12;
        (void)a11; (void)a12;

        wistd::function<void()> fn = [&a1, &a2, &a3, &a4, &a5, &a6, &a7, &a8, &a9, &a10]()
        {
            (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10;
        };
        auto fnCopy = fn;

        // Uncomment to double-check static assert.  Reports:
        // "The sizeof(wistd::function) has grown too large for the reserved buffer (10 pointers).  Refactor to reduce size of the capture."
        // wistd::function<void()> fn2 = [&a1, &a2, &a3, &a4, &a5, &a6, &a7, &a8, &a9, &a10, &a11]()
        // {
        //     a1; a2; a3; a4; a5; a6; a7; a8; a9; a10; a11;
        // };
    }
}

template <typename test_t, typename lambda_t>
void NullptrRaiiTests(lambda_t const &fnCreate)
{
    // nullptr_t construct
    test_t var1 = nullptr;      // implicit
    REQUIRE_FALSE(var1);
    test_t var2(nullptr);       // explicit
    REQUIRE_FALSE(var2);

    // nullptr_t assingment
    var1.reset(fnCreate());
    REQUIRE(var1);
    var1 = nullptr;
    REQUIRE_FALSE(var1);

    // nullptr_t reset
    var1.reset(fnCreate());
    REQUIRE(var1);
    var1.reset(nullptr);
    REQUIRE_FALSE(var1);
}

template <typename test_t, typename lambda_t>
void ReleaseRaiiTests(lambda_t const &fnCreate)
{
    test_t var1(fnCreate());
    REQUIRE(var1);
    auto ptr = var1.release();
    REQUIRE_FALSE(var1);
    REQUIRE(ptr != test_t::policy::invalid_value());
    REQUIRE(var1.get() == test_t::policy::invalid_value());

    var1.reset(ptr);
}

template <typename test_t, typename lambda_t>
void GetRaiiTests(lambda_t const &fnCreate)
{
    test_t var1;
    REQUIRE_FALSE(var1);
    REQUIRE(var1.get() == test_t::policy::invalid_value());

    var1.reset(fnCreate());
    REQUIRE(var1);
    REQUIRE(var1.get() != test_t::policy::invalid_value());
}

template <typename test_t, typename lambda_t>
void SharedRaiiTests(lambda_t const &fnCreate)
{
    // copy construction
    test_t var1(fnCreate());
    REQUIRE(var1);
    test_t var2 = var1;     // implicit
    REQUIRE(var1);
    REQUIRE(var2);
    test_t var3(var1);      // explicit

    // copy assignment
    test_t var4(fnCreate());
    test_t var5;
    var5 = var4;
    REQUIRE(var5);
    REQUIRE(var4);

    // r-value construction from unique_ptr
    typename test_t::unique_t unique1(fnCreate());
    test_t var7(std::move(unique1));    // explicit
    REQUIRE(var7);
    REQUIRE_FALSE(unique1);
    typename test_t::unique_t unique2(fnCreate());
    test_t var8 = std::move(unique2);   // implicit
    REQUIRE(var8);
    REQUIRE_FALSE(unique2);

    // r-value assignment from unique_ptr
    var8.reset();
    REQUIRE_FALSE(var8);
    unique2.reset(fnCreate());
    var8 = std::move(unique2);
    REQUIRE(var8);
    REQUIRE_FALSE(unique2);

    // use_count()
    REQUIRE(var8.use_count() == 1);
    auto var9 = var8;
    REQUIRE(var8.use_count() == 2);
}

template <typename test_t, typename lambda_t>
void WeakRaiiTests(lambda_t const &fnCreate)
{
    typedef typename test_t::shared_t shared_type;

    // base constructor
    test_t weak1;

    // construct from shared
    shared_type shared1(fnCreate());
    test_t weak2 = shared1;             // implicit
    test_t weak3(shared1);              // explicit

    // construct from weak
    test_t weak4 = weak2;               // implicit
    test_t weak5(weak2);                // explicit

    // assign from weak
    weak2 = weak5;

    // assign from shared
    weak2 = shared1;

    // reset
    weak2.reset();
    REQUIRE_FALSE(weak2.lock());

    // swap
    test_t swap1 = shared1;
    test_t swap2;
    REQUIRE(swap1.lock());
    REQUIRE_FALSE(swap2.lock());
    swap1.swap(swap2);
    REQUIRE_FALSE(swap1.lock());
    REQUIRE(swap2.lock());

    // expired
    REQUIRE_FALSE(swap2.expired());
    shared1.reset();
    REQUIRE(swap2.expired());

    // lock
    shared1.reset(fnCreate());
    weak1 = shared1;
    auto shared2 = weak1.lock();
    REQUIRE(shared2);
    shared2.reset();
    REQUIRE(weak1.lock());
    shared1.reset();
    shared2 = weak1.lock();
    REQUIRE_FALSE(shared2);
}

template <typename test_t, typename lambda_t>
void AddressRaiiTests(lambda_t const &fnCreate)
{
    test_t var1(fnCreate());
    REQUIRE(var1);

    &var1;
    REQUIRE_FALSE(var1);                              // the address operator does an auto-release

    *(&var1) = fnCreate();
    REQUIRE(var1);

    var1.put();
    REQUIRE_FALSE(var1);                              // verify that 'put()' does an auto-release

    *var1.put() = fnCreate();
    REQUIRE(var1);

    REQUIRE(var1.addressof() != nullptr);
    REQUIRE(var1);                               // verify that 'addressof()' does not auto-release
}

template <typename test_t, typename lambda_t>
void BasicRaiiTests(lambda_t const &fnCreate)
{
    auto invalidHandle = test_t::policy::invalid_value();

    // no-constructor construction
    test_t var1;
    REQUIRE_FALSE(var1);

    // construct from a given resource
    test_t var2(fnCreate());    // r-value
    REQUIRE(var2);
    test_t var3(invalidHandle);      // l-value
    REQUIRE_FALSE(var3);

    // r-value construct from the same type
    test_t var4(std::move(var2));                   // explicit
    REQUIRE(var4);
    REQUIRE_FALSE(var2);
    test_t varMove(fnCreate());
    test_t var4implicit = std::move(varMove);       // implicit
    REQUIRE(var4implicit);

    // move assignment
    var2 = std::move(var4);
    REQUIRE(var2);
    REQUIRE_FALSE(var4);

    // swap
    var2.swap(var4);
    REQUIRE(var4);
    REQUIRE_FALSE(var2);

    // explicit bool cast
    REQUIRE(static_cast<bool>(var4));
    REQUIRE_FALSE(static_cast<bool>(var2));

    // reset
    var4.reset();
    REQUIRE_FALSE(var4);
    var4.reset(fnCreate());     // r-value
    REQUIRE(var4);
    var4.reset(invalidHandle);       // l-value
    REQUIRE_FALSE(var4);
}

template <typename test_t>
void EventRaiiTests()
{
    test_t var1;
    var1.create(wil::EventOptions::ManualReset);
    REQUIRE_FALSE(wil::event_is_signaled(var1.get()));

    // SetEvent/ResetEvent
    var1.SetEvent();
    REQUIRE(wil::event_is_signaled(var1.get()));
    var1.ResetEvent();
    REQUIRE_FALSE(wil::event_is_signaled(var1.get()));

    // SetEvent/ResetEvent scope_exit
    {
        auto exit = var1.SetEvent_scope_exit();
        REQUIRE_FALSE(wil::event_is_signaled(var1.get()));
    }
    REQUIRE(wil::event_is_signaled(var1.get()));
    {
        auto exit = var1.ResetEvent_scope_exit();
        REQUIRE(wil::event_is_signaled(var1.get()));
    }
    REQUIRE_FALSE(wil::event_is_signaled(var1.get()));

    // is_signaled
    REQUIRE_FALSE(var1.is_signaled());

    // wait
    REQUIRE_FALSE(var1.wait(50));

    // try_create
    bool exists = false;
    REQUIRE(var1.try_create(wil::EventOptions::ManualReset, L"wiltestevent", nullptr, &exists));
    REQUIRE_FALSE(exists);
    test_t var2;
    REQUIRE(var2.try_create(wil::EventOptions::ManualReset, L"wiltestevent", nullptr, &exists));
    REQUIRE(exists);
    test_t var3;
    REQUIRE_FALSE(var3.try_create(wil::EventOptions::ManualReset, L"\\illegal\\chars\\too\\\\many\\\\namespaces", nullptr, &exists));
    REQUIRE(::GetLastError() != ERROR_SUCCESS);

    // try_open
    test_t var4;
    REQUIRE_FALSE(var4.try_open(L"\\illegal\\chars\\too\\\\many\\\\namespaces"));
    REQUIRE(::GetLastError() != ERROR_SUCCESS);
    REQUIRE(var4.try_open(L"wiltestevent"));
}

void EventTests()
{
    static_assert(sizeof(wil::unique_event_nothrow) == sizeof(HANDLE), "event_t should be sizeof(HANDLE) to allow for raw array utilization");

    auto fnCreate = []() { return CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, 0); };

    BasicRaiiTests<wil::unique_event_nothrow>(fnCreate);
    NullptrRaiiTests<wil::unique_event_nothrow>(fnCreate);
    GetRaiiTests<wil::unique_event_nothrow>(fnCreate);
    ReleaseRaiiTests<wil::unique_event_nothrow>(fnCreate);
    AddressRaiiTests<wil::unique_event_nothrow>(fnCreate);
    EventRaiiTests<wil::unique_event_nothrow>();

    BasicRaiiTests<wil::unique_event_failfast>(fnCreate);
    NullptrRaiiTests<wil::unique_event_failfast>(fnCreate);
    GetRaiiTests<wil::unique_event_failfast>(fnCreate);
    ReleaseRaiiTests<wil::unique_event_failfast>(fnCreate);
    AddressRaiiTests<wil::unique_event_failfast>(fnCreate);
    EventRaiiTests<wil::unique_event_failfast>();

    wil::unique_event_nothrow event4;
    REQUIRE(S_OK == event4.create(wil::EventOptions::ManualReset));
    REQUIRE(FAILED(event4.create(wil::EventOptions::ManualReset, L"\\illegal\\chars\\too\\\\many\\\\namespaces")));

#ifdef WIL_ENABLE_EXCEPTIONS
    static_assert(sizeof(wil::unique_event) == sizeof(HANDLE), "event_t should be sizeof(HANDLE) to allow for raw array utilization");

    BasicRaiiTests<wil::unique_event>(fnCreate);
    NullptrRaiiTests<wil::unique_event>(fnCreate);
    GetRaiiTests<wil::unique_event>(fnCreate);
    ReleaseRaiiTests<wil::unique_event>(fnCreate);
    AddressRaiiTests<wil::unique_event>(fnCreate);
    EventRaiiTests<wil::unique_event>();

    BasicRaiiTests<wil::shared_event>(fnCreate);
    NullptrRaiiTests<wil::shared_event>(fnCreate);
    GetRaiiTests<wil::shared_event>(fnCreate);
    AddressRaiiTests<wil::shared_event>(fnCreate);
    SharedRaiiTests<wil::shared_event>(fnCreate);
    EventRaiiTests<wil::shared_event>();

    WeakRaiiTests<wil::weak_event>(fnCreate);

    // explicitly disabled
    // wil::unique_event_nothrow event1(wil::EventOptions::ManualReset);
    wil::unique_event event2(wil::EventOptions::ManualReset);
    wil::shared_event event3(wil::EventOptions::ManualReset);

    event2.create(wil::EventOptions::ManualReset);
    REQUIRE(event2);
    event3.create(wil::EventOptions::ManualReset);
    REQUIRE(event3);
    REQUIRE_THROWS(event2.create(wil::EventOptions::ManualReset, L"\\illegal\\chars\\too\\\\many\\\\namespaces") );
    REQUIRE_THROWS(event3.create(wil::EventOptions::ManualReset, L"\\illegal\\chars\\too\\\\many\\\\namespaces") );

    wil::unique_event var1(wil::EventOptions::ManualReset);
    REQUIRE_FALSE(wil::event_is_signaled(var1.get()));
    {
        auto autoset = wil::SetEvent_scope_exit(var1.get());
        REQUIRE_FALSE(wil::event_is_signaled(var1.get()));
        REQUIRE(autoset.get() == var1.get());
        // &autoset;                // verified disabled
        // autoset.addressof();     // verified disabled
    }
    REQUIRE(wil::event_is_signaled(var1.get()));
    {
        auto autoreset = wil::ResetEvent_scope_exit(var1.get());
        REQUIRE(wil::event_is_signaled(var1.get()));
        autoreset.reset();
        REQUIRE_FALSE(wil::event_is_signaled(var1.get()));
    }
    {
        auto autoset = wil::SetEvent_scope_exit(var1.get());
        REQUIRE_FALSE(wil::event_is_signaled(var1.get()));
        autoset.release();
        REQUIRE_FALSE(wil::event_is_signaled(var1.get()));
    }
    REQUIRE_FALSE(wil::event_is_signaled(var1.get()));
#endif
}

typedef wil::unique_struct<PROPVARIANT, decltype(&::PropVariantClear), ::PropVariantClear> unique_prop_variant_no_init;

void SetPropVariantValue(_In_ int intVal, _Out_ PROPVARIANT* ppropvar)
{
    ppropvar->intVal = intVal;
    ppropvar->vt = VT_INT;
}

template<typename T>
void TestUniquePropVariant()
{
    {
        wil::unique_prop_variant spPropVariant;
        REQUIRE(spPropVariant.vt == VT_EMPTY);
    }

    // constructor test
    {
        PROPVARIANT propVariant;
        SetPropVariantValue(12, &propVariant);
        T spPropVariant(propVariant);
        REQUIRE(((spPropVariant.intVal == 12) && (spPropVariant.vt == VT_INT)));

        T spPropVariant2(wistd::move(propVariant));
        REQUIRE(((spPropVariant2.intVal == 12) && (spPropVariant2.vt == VT_INT)));

        //spPropVariant = propVariant;    // deleted function
        //spPropVariant = wistd::move(propVariant);  // deleted function
        //spPropVariant.swap(propVariant);  //deleted function
    }

    // move constructor
    {
        T spPropVariant;
        SetPropVariantValue(12, &spPropVariant);
        REQUIRE(((spPropVariant.intVal == 12) && (spPropVariant.vt == VT_INT)));

        T spPropVariant2(wistd::move(spPropVariant));
        REQUIRE(spPropVariant.vt == VT_EMPTY);
        REQUIRE(((spPropVariant2.intVal == 12) && (spPropVariant2.vt == VT_INT)));

        //T spPropVariant3(spPropVariant);     // deleted function
        //spPropVariant2 = spPropVariant;     // deleted function
    }

    // move operator
    {
        T spPropVariant;
        SetPropVariantValue(12, &spPropVariant);
        T spPropVariant2 = wistd::move(spPropVariant);
        REQUIRE(spPropVariant.vt == VT_EMPTY);
        REQUIRE(((spPropVariant2.intVal == 12) && (spPropVariant2.vt == VT_INT)));
    }

    // reset
    {
        PROPVARIANT propVariant;
        SetPropVariantValue(22, &propVariant);
        T spPropVariant;
        SetPropVariantValue(12, &spPropVariant);
        T spPropVariant2;

        //spPropVariant2.reset(spPropVariant);    // deleted function
        spPropVariant.reset(propVariant);
        REQUIRE(spPropVariant.intVal == 22);
        REQUIRE(propVariant.intVal == 22);

        spPropVariant.reset();
        REQUIRE(spPropVariant.vt == VT_EMPTY);
    }

    // swap
    {
        T spPropVariant;
        SetPropVariantValue(12, &spPropVariant);
        T spPropVariant2;
        SetPropVariantValue(22, &spPropVariant2);

        spPropVariant.swap(spPropVariant2);
        REQUIRE(spPropVariant.intVal == 22);
        REQUIRE(spPropVariant2.intVal == 12);
    }

    // release, addressof, reset_and_addressof
    {
        T spPropVariant;
        SetPropVariantValue(12, &spPropVariant);

        [](PROPVARIANT* propVariant)
        {
            REQUIRE(propVariant->vt == VT_EMPTY);
        }(spPropVariant.reset_and_addressof());

        SetPropVariantValue(12, &spPropVariant);
        PROPVARIANT* pPropVariant = spPropVariant.addressof();
        REQUIRE(pPropVariant->intVal == 12);
        REQUIRE(spPropVariant.intVal == 12);

        PROPVARIANT propVariant = spPropVariant.release();
        REQUIRE(propVariant.intVal == 12);
        REQUIRE(spPropVariant.vt == VT_EMPTY);
    }
}

TEST_CASE("WindowsInternalTests::ResourceTemplateTests", "[resource]")
{
    EventTests();
    TestUniquePropVariant<wil::unique_prop_variant>();
    TestUniquePropVariant<unique_prop_variant_no_init>();
}

inline unsigned long long ToInt64(const FILETIME &ft)
{
    return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) + ft.dwLowDateTime;
}

inline FILETIME FromInt64(unsigned long long i64)
{
    FILETIME ft = { static_cast<DWORD>(i64), static_cast<DWORD>(i64 >> 32) };
    return ft;
}

TEST_CASE("WindowsInternalTests::Win32HelperTests", "[win32_helpers]")
{
    auto systemTime = wil::filetime::get_system_time();
    REQUIRE(ToInt64(systemTime) == wil::filetime::to_int64(systemTime));
    auto systemTime64 = wil::filetime::to_int64(systemTime);
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    auto ft1 = FromInt64(systemTime64);
    auto ft2 = wil::filetime::from_int64(systemTime64);
    REQUIRE(CompareFileTime(&ft1, &ft2) == 0);
#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) */

    REQUIRE(systemTime64 == wil::filetime::to_int64(wil::filetime::from_int64(systemTime64)));
    REQUIRE((systemTime64 + wil::filetime_duration::one_hour) == (systemTime64 + (wil::filetime_duration::one_minute * 60)));
    auto systemTimePlusOneHour = wil::filetime::add(systemTime, wil::filetime_duration::one_hour);
    auto systemTimePlusOneHour64 = wil::filetime::to_int64(systemTimePlusOneHour);
    REQUIRE(systemTimePlusOneHour64 == (systemTime64 + wil::filetime_duration::one_hour));
}

TEST_CASE("WindowsInternalTests::RectHelperTests", "[win32_helpers]")
{
    RECT rect{ 50, 100, 200, 300 };
    POINT leftEdgePoint{ 50, 150 };
    POINT topEdgePoint{ 100, 100 };
    POINT rightEdgePoint{ 200, 150 };
    POINT bottomEdgePoint{ 100, 300 };
    POINT insidePoint{ 150, 150};

    RECT emptyRectAtOrigin{};
    RECT emptyRectNotAtOrigin{ 50, 50, 50, 50 };
    RECT nonNormalizedRect{ 300, 300, 0, 0 };

    REQUIRE(wil::rect_width(rect) == 150);
    REQUIRE(wil::rect_height(rect) == 200);

    // rect_is_empty should work like user32's IsRectEmpty
    REQUIRE_FALSE(wil::rect_is_empty(rect));
    REQUIRE(wil::rect_is_empty(emptyRectAtOrigin));
    REQUIRE(wil::rect_is_empty(emptyRectNotAtOrigin));
    REQUIRE(wil::rect_is_empty(nonNormalizedRect));

    // rect_contains_point should work like user32's PtInRect
    REQUIRE(wil::rect_contains_point(rect, insidePoint));
    REQUIRE(wil::rect_contains_point(rect, leftEdgePoint));
    REQUIRE(wil::rect_contains_point(rect, topEdgePoint));
    REQUIRE_FALSE(wil::rect_contains_point(rect, rightEdgePoint));
    REQUIRE_FALSE(wil::rect_contains_point(rect, bottomEdgePoint));
    REQUIRE_FALSE(wil::rect_contains_point(nonNormalizedRect, insidePoint));

    auto rectFromSize = wil::rect_from_size<RECT>(50, 100, 150, 200);
    REQUIRE(rectFromSize.left == rect.left);
    REQUIRE(rectFromSize.top == rect.top);
    REQUIRE(rectFromSize.right == rect.right);
    REQUIRE(rectFromSize.bottom == rect.bottom);
}

TEST_CASE("WindowsInternalTests::InitOnceNonTests")
{
    bool called = false;
    bool winner = false;
    INIT_ONCE init{};
    REQUIRE_FALSE(wil::init_once_initialized(init));

    // Call, but fail. Should transport the HRESULT back, but mark us as not the winner
    called = false;
    winner = false;
    REQUIRE(E_FAIL == wil::init_once_nothrow(init, [&] { called = true; return E_FAIL; }, &winner));
    REQUIRE_FALSE(wil::init_once_initialized(init));
    REQUIRE(called);
    REQUIRE_FALSE(winner);

    // Call, succeed. Should mark us as the winner.
    called = false;
    winner = false;
    REQUIRE_SUCCEEDED(wil::init_once_nothrow(init, [&] { called = true; return S_OK; }, &winner));
    REQUIRE(wil::init_once_initialized(init));
    REQUIRE(called);
    REQUIRE(winner);

    // Call again. Should not actually be invoked and should not be the winner
    called = false;
    winner = false;
    REQUIRE_SUCCEEDED(wil::init_once_nothrow(init, [&] { called = false; return S_OK; }, &winner));
    REQUIRE(wil::init_once_initialized(init));
    REQUIRE_FALSE(called);
    REQUIRE_FALSE(winner);

    // Call again. Still not invoked, but we don't care if we're the winner
    called = false;
    REQUIRE_SUCCEEDED(wil::init_once_nothrow(init, [&] { called = false; return S_OK; }));
    REQUIRE(wil::init_once_initialized(init));
    REQUIRE_FALSE(called);

#ifdef WIL_ENABLE_EXCEPTIONS
    called = false;
    winner = false;
    init = {};

    // A thrown exception leaves the object un-initialized
    static volatile bool always_true = true; // So that the compiler can't determine that we unconditionally throw below (warning C4702)
    REQUIRE_THROWS_AS(winner = wil::init_once(init, [&] { called = true; THROW_HR_IF(E_FAIL, always_true); }), wil::ResultException);
    REQUIRE_FALSE(wil::init_once_initialized(init));
    REQUIRE(called);
    REQUIRE_FALSE(winner);

    // Success!
    called = false;
    winner = false;
    REQUIRE_NOTHROW(winner = wil::init_once(init, [&] { called = true; }));
    REQUIRE(wil::init_once_initialized(init));
    REQUIRE(called);
    REQUIRE(winner);

    // No-op success!
    called = false;
    winner = false;
    REQUIRE_NOTHROW(winner = wil::init_once(init, [&] { called = true; }));
    REQUIRE(wil::init_once_initialized(init));
    REQUIRE_FALSE(called);
    REQUIRE_FALSE(winner);
#endif // WIL_ENABLE_EXCEPTIONS
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
TEST_CASE("WindowsInternalTests::TestUniquePointerCases", "[resource][unique_any]")
{
    // wil::unique_process_heap_ptr tests
    {
        wil::unique_process_heap_ptr<void> empty; // null case
    }
    {
        wil::unique_process_heap_ptr<void> heapMemory(::HeapAlloc(::GetProcessHeap(), 0, 100));
        REQUIRE(static_cast<bool>(heapMemory));
    }

    // wil::unique_cotaskmem_ptr tests
    {
        wil::unique_cotaskmem_ptr<void> empty; // null case
    }
    {
        wil::unique_cotaskmem_ptr<void> cotaskmemMemory(CoTaskMemAlloc(100));
        REQUIRE(static_cast<bool>(cotaskmemMemory));
    }
    {
        auto cotaskmemMemory = wil::make_unique_cotaskmem_nothrow<DWORD>(42);
        REQUIRE(static_cast<bool>(cotaskmemMemory));
        REQUIRE(*cotaskmemMemory == static_cast<DWORD>(42));
    }
    {
        struct S { size_t s; S() : s(42) {} };
        auto cotaskmemMemory = wil::make_unique_cotaskmem_nothrow<S>();
        REQUIRE(static_cast<bool>(cotaskmemMemory));
        REQUIRE(cotaskmemMemory->s == static_cast<size_t>(42));
    }
    {
        auto cotaskmemArrayMemory = wil::make_unique_cotaskmem_nothrow<BYTE[]>(12);
        REQUIRE(static_cast<bool>(cotaskmemArrayMemory));
    }
    {
        struct S { size_t s; S() : s(42) {} };
        const size_t size = 12;
        auto cotaskmemArrayMemory = wil::make_unique_cotaskmem_nothrow<S[]>(size);
        REQUIRE(static_cast<bool>(cotaskmemArrayMemory));
        bool verified = true;
        for (auto& elem : wil::make_range(cotaskmemArrayMemory.get(), size)) if (elem.s != 42) verified = false;
        REQUIRE(verified);
    }

    // wil::unique_cotaskmem_secure_ptr tests
    {
        wil::unique_cotaskmem_secure_ptr<void> empty; // null case
    }
    {
        wil::unique_cotaskmem_secure_ptr<void> cotaskmemMemory(CoTaskMemAlloc(100));
        REQUIRE(static_cast<bool>(cotaskmemMemory));
    }
    {
        auto cotaskmemMemory = wil::make_unique_cotaskmem_secure_nothrow<DWORD>(42);
        REQUIRE(static_cast<bool>(cotaskmemMemory));
        REQUIRE(*cotaskmemMemory == static_cast<DWORD>(42));
    }
    {
        struct S { size_t s; S() : s(42) {} };
        auto cotaskmemMemory = wil::make_unique_cotaskmem_secure_nothrow<S>();
        REQUIRE(static_cast<bool>(cotaskmemMemory));
        REQUIRE(cotaskmemMemory->s == static_cast<size_t>(42));
    }
    {
        auto cotaskmemArrayMemory = wil::make_unique_cotaskmem_secure_nothrow<BYTE[]>(12);
        REQUIRE(static_cast<bool>(cotaskmemArrayMemory));
    }
    {
        struct S { size_t s; S() : s(42) {} };
        const size_t size = 12;
        auto cotaskmemArrayMemory = wil::make_unique_cotaskmem_secure_nothrow<S[]>(size);
        REQUIRE(static_cast<bool>(cotaskmemArrayMemory));
        bool verified = true;
        for (auto& elem : wil::make_range(cotaskmemArrayMemory.get(), size)) if (elem.s != 42) verified = false;
        REQUIRE(verified);
    }

    // wil::unique_hlocal_ptr tests
    {
        wil::unique_hlocal_ptr<void> empty; // null case
    }
    {
        wil::unique_hlocal_ptr<void> localMemory(LocalAlloc(LPTR, 100));
        REQUIRE(static_cast<bool>(localMemory));
    }
    {
        auto localMemory = wil::make_unique_hlocal_nothrow<DWORD>(42);
        REQUIRE(static_cast<bool>(localMemory));
        REQUIRE(*localMemory == static_cast<DWORD>(42));
    }
    {
        struct S { size_t s; S() : s(42) {} };
        auto localMemory = wil::make_unique_hlocal_nothrow<S>();
        REQUIRE(static_cast<bool>(localMemory));
        REQUIRE(localMemory->s == static_cast<size_t>(42));
    }
    {
        auto localArrayMemory = wil::make_unique_hlocal_nothrow<BYTE[]>(12);
        REQUIRE(static_cast<bool>(localArrayMemory));
    }
    {
        struct S { size_t s; S() : s(42) {} };
        const size_t size = 12;
        auto localArrayMemory = wil::make_unique_hlocal_nothrow<S[]>(size);
        REQUIRE(static_cast<bool>(localArrayMemory));
        bool verified = true;
        for (auto& elem : wil::make_range(localArrayMemory.get(), size)) if (elem.s != 42) verified = false;
        REQUIRE(verified);
    }

    // wil::unique_hlocal_secure_ptr tests
    {
        wil::unique_hlocal_secure_ptr<void> empty; // null case
    }
    {
        wil::unique_hlocal_secure_ptr<void> localMemory(LocalAlloc(LPTR, 100));
        REQUIRE(static_cast<bool>(localMemory));
    }
    {
        auto localMemory = wil::make_unique_hlocal_secure_nothrow<DWORD>(42);
        REQUIRE(static_cast<bool>(localMemory));
        REQUIRE(*localMemory == static_cast<DWORD>(42));
    }
    {
        struct S { size_t s; S() : s(42) {} };
        auto localMemory = wil::make_unique_hlocal_secure_nothrow<S>();
        REQUIRE(static_cast<bool>(localMemory));
        REQUIRE(localMemory->s == static_cast<size_t>(42));
    }
    {
        auto localArrayMemory = wil::make_unique_hlocal_secure_nothrow<BYTE[]>(12);
        REQUIRE(static_cast<bool>(localArrayMemory));
    }
    {
        struct S { size_t s; S() : s(42) {} };
        const size_t size = 12;
        auto localArrayMemory = wil::make_unique_hlocal_secure_nothrow<S[]>(size);
        REQUIRE(static_cast<bool>(localArrayMemory));
        bool verified = true;
        for (auto& elem : wil::make_range(localArrayMemory.get(), size)) if (elem.s != 42) verified = false;
        REQUIRE(verified);
    }

    // wil::unique_hglobal_ptr tests
    {
        wil::unique_hglobal_ptr<void> empty; // null case
    }
    {
        wil::unique_hglobal_ptr<void> globalMemory(GlobalAlloc(GPTR, 100));
        REQUIRE(static_cast<bool>(globalMemory));
    }

    {
        // The following uses are blocked due to a static assert failure

        //struct S { ~S() {} };

        //auto cotaskmemMemory = wil::make_unique_cotaskmem_nothrow<S>();
        //auto cotaskmemArrayMemory = wil::make_unique_cotaskmem_nothrow<S[]>(1);
        //auto cotaskmemMemory2 = wil::make_unique_cotaskmem_secure_nothrow<S>();
        //auto cotaskmemArrayMemory2 = wil::make_unique_cotaskmem_secure_nothrow<S[]>(1);

        //auto localMemory = wil::make_unique_hlocal_nothrow<S>();
        //auto localArrayMemory = wil::make_unique_hlocal_nothrow<S[]>(1);
        //auto localMemory2 = wil::make_unique_hlocal_secure_nothrow<S>();
        //auto localArrayMemory2 = wil::make_unique_hlocal_secure_nothrow<S[]>(1);
    }
}
#endif

void GetDWORDArray(_Out_ size_t* count, _Outptr_result_buffer_(*count) DWORD** numbers)
{
    const size_t size = 5;
    auto ptr = static_cast<DWORD*>(::CoTaskMemAlloc(sizeof(DWORD) * size));
    REQUIRE(ptr);
    ::ZeroMemory(ptr, sizeof(DWORD) * size);
    *numbers = ptr;
    *count = size;
}

void GetHSTRINGArray(_Out_ ULONG* count, _Outptr_result_buffer_(*count) HSTRING** strings)
{
    const size_t size = 5;
    auto ptr = static_cast<HSTRING*>(::CoTaskMemAlloc(sizeof(HSTRING) * size));
    REQUIRE(ptr);
    for (UINT i = 0; i < size; ++i)
    {
        REQUIRE_SUCCEEDED(WindowsCreateString(L"test", static_cast<UINT32>(wcslen(L"test")), &ptr[i]));
    }
    *strings = ptr;
    *count = static_cast<ULONG>(size);
}

void GetPOINTArray(_Out_ UINT32* count, _Outptr_result_buffer_(*count) POINT** points)
{
    const size_t size = 5;
    auto ptr = static_cast<POINT*>(::CoTaskMemAlloc(sizeof(POINT) * size));
    REQUIRE(ptr);
    for (UINT i = 0; i < size; ++i)
    {
        ptr[i].x = ptr[i].y = i;
    }
    *points = ptr;
    *count = static_cast<UINT32>(size);
}

#ifdef WIL_ENABLE_EXCEPTIONS
void GetHANDLEArray(_Out_ size_t* count, _Outptr_result_buffer_(*count) HANDLE** events)
{
    const size_t size = 5;
    HANDLE* ptr = reinterpret_cast<HANDLE*>(::CoTaskMemAlloc(sizeof(HANDLE) * size));
    for (auto& val : wil::make_range(ptr, size))
    {
        val = wil::unique_event(wil::EventOptions::ManualReset).release();
    }
    *events = ptr;
    *count = size;
}
#endif

interface __declspec(uuid("EDCA4ADC-DF46-442A-A69D-FDFD8BC37B31")) IFakeObject : public IUnknown
{
   STDMETHOD_(void, DoStuff)() = 0;
};

class ArrayTestObject : witest::AllocatedObject,
    public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>, IFakeObject>
{
public:
    HRESULT RuntimeClassInitialize(UINT n) { m_number = n; return S_OK; };
    STDMETHOD_(void, DoStuff)() {}
private:
    UINT m_number{};
};

void GetUnknownArray(_Out_ size_t* count, _Outptr_result_buffer_(*count) IFakeObject*** objects)
{
    const size_t size = 5;
    auto ptr = reinterpret_cast<IFakeObject**>(::CoTaskMemAlloc(sizeof(IFakeObject*) * size));
    REQUIRE(ptr);
    for (UINT i = 0; i < size; ++i)
    {
        Microsoft::WRL::ComPtr<IFakeObject> obj;
        REQUIRE_SUCCEEDED(Microsoft::WRL::MakeAndInitialize<ArrayTestObject>(&obj, i));
        ptr[i] = obj.Detach();
    }
    *objects = ptr;
    *count = size;
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
TEST_CASE("WindowsInternalTests::TestUniqueArrayCases", "[resource]")
{
    // wil::unique_cotaskmem_array_ptr tests
    {
        wil::unique_cotaskmem_array_ptr<DWORD> values;
        GetDWORDArray(values.size_address(), &values);
    }
    {
        wil::unique_cotaskmem_array_ptr<wil::unique_hstring> strings;
        GetHSTRINGArray(strings.size_address<ULONG>(), &strings);
        for (ULONG i = 0; i < strings.size(); ++i)
        {
            REQUIRE(WindowsGetStringLen(strings[i]) == wcslen(L"test"));
        }
    }
    {
        wil::unique_cotaskmem_array_ptr<POINT> points;
        GetPOINTArray(points.size_address<UINT32>(), &points);
        for (ULONG i = 0; i < points.size(); ++i)
        {
            REQUIRE((ULONG)points[i].x == i);
        }
    }
#ifdef WIL_ENABLE_EXCEPTIONS
    {
        wil::unique_cotaskmem_array_ptr<wil::unique_event> events;
        GetHANDLEArray(events.size_address(), &events);
    }
    {
        wil::unique_cotaskmem_array_ptr<wil::com_ptr<IFakeObject>> objects;
        GetUnknownArray(objects.size_address(), &objects);
        for (ULONG i = 0; i < objects.size(); ++i)
        {
            objects[i]->DoStuff();
        }
    }
#endif
    {
        wil::unique_cotaskmem_array_ptr<DWORD> values = nullptr;
        REQUIRE(!values);
        REQUIRE(values.size() == 0);

        // move onto self
        values = wistd::move(values);
        REQUIRE(!values);

        // fetch
        GetDWORDArray(values.size_address(), &values);
        REQUIRE(!!values);
        REQUIRE(values.size() > 0);
        REQUIRE(!values.empty());

        // move onto self
        values = wistd::move(values);
        REQUIRE(!!values);

        decltype(values) values2(wistd::move(values));
        REQUIRE(!values);
        REQUIRE(!!values2);
        REQUIRE(values2.size() > 0);

        values = wistd::move(values2);
        REQUIRE(!!values);
        REQUIRE(!values2);

        values = nullptr;
        REQUIRE(!values);
        GetDWORDArray(values.size_address(), values.put());
        REQUIRE(!!values);

        values = nullptr;
        REQUIRE(!values);
        GetDWORDArray(values.size_address(), &values);
        REQUIRE(!!values);

        auto size = values.size();
        auto ptr = values.release();

        REQUIRE(!values);
        REQUIRE(values.empty());

        decltype(values) values3(ptr, size);
        REQUIRE(!!values3);
        REQUIRE(values3.size() == size);

        values3.swap(values);
        REQUIRE(!!values);
        REQUIRE(!values.empty());
        REQUIRE(!values3);
        REQUIRE(values3.empty());

        REQUIRE(!values.empty());
        size_t count = 0;
        for (auto it = values.begin(); it != values.end(); ++it)
        {
            ++count;
        }
        REQUIRE(count == values.size());

        count = 0;
        for (auto it = values.cbegin(); it != values.cend(); ++it)
        {
            ++count;
        }
        REQUIRE(count == values.size());

        for (size_t index = 0; index < values.size(); index++)
        {
            auto& val = values[index];
            REQUIRE(val == 0);
        }

        auto& front = values.front();
        REQUIRE(front == 0);
        auto& back = values.back();
        REQUIRE(back == 0);

        [](const wil::unique_cotaskmem_array_ptr<DWORD>& cvalues)
        {
            size_t count = 0;
            for (auto it = cvalues.begin(); it != cvalues.end(); ++it)
            {
                ++count;
            }
            REQUIRE(count == cvalues.size());
            for (size_t index = 0; index < cvalues.size(); index++)
            {
                auto& val = cvalues[index];
                REQUIRE(val == 0);
            }

            auto& front = cvalues.front();
            REQUIRE(front == 0);
            auto& back = cvalues.back();
            REQUIRE(back == 0);

            REQUIRE(cvalues.data() != nullptr);
        }(values);

        auto data1 = values.data();
        auto data2 = values.get();
        REQUIRE((data1 && (data1 == data2)));

        values.reset();
        REQUIRE(!values);
        REQUIRE(values.empty());

        GetDWORDArray(values2.size_address(), &values2);
        size = values2.size();
        ptr = values2.release();

        values.reset(ptr, size);
        REQUIRE(!!values);
        REQUIRE(!values.empty());

        REQUIRE(values2.put() == values2.addressof());
        REQUIRE(&values2 == values2.addressof());
    }
}
#endif

#if !defined(__cplusplus_winrt) && (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
TEST_CASE("WindowsInternalTests::VerifyMakeAgileCallback", "[wrl]")
{
    using namespace ABI::Windows::Foundation;

    class CallbackClient
    {
    public:
        HRESULT On(IMemoryBufferReference*, IInspectable*)
        {
            return S_OK;
        }
    };
    CallbackClient callbackClient;

#ifdef WIL_ENABLE_EXCEPTIONS
    auto cbAgile = wil::MakeAgileCallback<ITypedEventHandler<IMemoryBufferReference*, IInspectable*>>([](IMemoryBufferReference*, IInspectable*) -> HRESULT
    {
        return S_OK;
    });
    REQUIRE(wil::is_agile(cbAgile));

    auto cbAgileWithMember = wil::MakeAgileCallback<ITypedEventHandler<IMemoryBufferReference*, IInspectable*>>(&callbackClient, &CallbackClient::On);
    REQUIRE(wil::is_agile(cbAgileWithMember));
#endif
    auto cbAgileNoThrow = wil::MakeAgileCallbackNoThrow<ITypedEventHandler<IMemoryBufferReference*, IInspectable*>>([](IMemoryBufferReference*, IInspectable*) -> HRESULT
    {
        return S_OK;
    });
    REQUIRE(wil::is_agile(cbAgileNoThrow));

    auto cbAgileWithMemberNoThrow = wil::MakeAgileCallbackNoThrow<ITypedEventHandler<IMemoryBufferReference*, IInspectable*>>(&callbackClient, &CallbackClient::On);
    REQUIRE(wil::is_agile(cbAgileWithMemberNoThrow));
}
#endif

TEST_CASE("WindowsInternalTests::Ranges", "[common]")
{
    {
        int things[10]{};
        unsigned int count = 0;
        for (auto& m : wil::make_range(things, ARRAYSIZE(things)))
        {
            ++count;
            m = 1;
        }
        REQUIRE(ARRAYSIZE(things) == count);
        REQUIRE(1 == things[1]);
    }

    {
        int things[10]{};
        unsigned int count = 0;
        for (auto m : wil::make_range(things, ARRAYSIZE(things)))
        {
            ++count;
            m = 1;
            (void)m;
        }
        REQUIRE(ARRAYSIZE(things) == count);
        REQUIRE(0 == things[0]);
    }

    {
        int things[10]{};
        unsigned int count = 0;
        auto range = wil::make_range(things, ARRAYSIZE(things));
        for (auto m : range)
        {
            (void)m;
            ++count;
        }
        REQUIRE(ARRAYSIZE(things) == count);
    }

    {
        int things[10]{};
        unsigned int count = 0;
        const auto range = wil::make_range(things, ARRAYSIZE(things));
        for (auto m : range)
        {
            (void)m;
            ++count;
        }
        REQUIRE(ARRAYSIZE(things) == count);
    }
}

TEST_CASE("WindowsInternalTests::HStringTests", "[resource][unique_any]")
{
    const wchar_t kittens[] = L"kittens";

    {
        wchar_t* bufferStorage = nullptr;
        wil::unique_hstring_buffer theBuffer;
        REQUIRE_SUCCEEDED(::WindowsPreallocateStringBuffer(ARRAYSIZE(kittens), &bufferStorage, &theBuffer));
        REQUIRE_SUCCEEDED(StringCchCopyW(bufferStorage, ARRAYSIZE(kittens), kittens));

        // Promote sets the promoted-to value but resets theBuffer
        wil::unique_hstring promoted;
        REQUIRE_SUCCEEDED(wil::make_hstring_from_buffer_nothrow(wistd::move(theBuffer), &promoted));
        REQUIRE(static_cast<bool>(promoted));
        REQUIRE_FALSE(static_cast<bool>(theBuffer));
    }

    {
        wchar_t* bufferStorage = nullptr;
        wil::unique_hstring_buffer theBuffer;
        REQUIRE_SUCCEEDED(::WindowsPreallocateStringBuffer(ARRAYSIZE(kittens), &bufferStorage, &theBuffer));
        REQUIRE_SUCCEEDED(StringCchCopyW(bufferStorage, ARRAYSIZE(kittens), kittens));

        // Failure to promote retains the buffer state
        REQUIRE_FAILED(wil::make_hstring_from_buffer_nothrow(wistd::move(theBuffer), nullptr));
        REQUIRE(static_cast<bool>(theBuffer));
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    {
        wchar_t* bufferStorage = nullptr;
        wil::unique_hstring_buffer theBuffer;
        THROW_IF_FAILED(::WindowsPreallocateStringBuffer(ARRAYSIZE(kittens), &bufferStorage, &theBuffer));
        THROW_IF_FAILED(StringCchCopyW(bufferStorage, ARRAYSIZE(kittens), kittens));

        wil::unique_hstring promoted;
        REQUIRE_NOTHROW(promoted = wil::make_hstring_from_buffer(wistd::move(theBuffer)));
        REQUIRE(static_cast<bool>(promoted));
        REQUIRE_FALSE(static_cast<bool>(theBuffer));
    }
#endif
}

struct ThreadPoolWaitTestContext
{
    volatile LONG Counter = 0;
    wil::unique_event_nothrow Event;
};

static void __stdcall ThreadPoolWaitTestCallback(
    _Inout_ PTP_CALLBACK_INSTANCE /*instance*/,
    _Inout_opt_ void* context,
    _Inout_ PTP_WAIT wait,
    _In_ TP_WAIT_RESULT /*waitResult*/)
{
    ThreadPoolWaitTestContext& myContext = *reinterpret_cast<ThreadPoolWaitTestContext*>(context);
    SetThreadpoolWait(wait, myContext.Event.get(), nullptr);
    ::InterlockedIncrement(&myContext.Counter);
}

template <typename WaitResourceT>
void ThreadPoolWaitTestHelper(bool requireExactCallbackCount)
{
    ThreadPoolWaitTestContext myContext;
    REQUIRE_SUCCEEDED(myContext.Event.create());

    WaitResourceT wait;
    wait.reset(CreateThreadpoolWait(ThreadPoolWaitTestCallback, &myContext, nullptr));
    REQUIRE(wait);

    SetThreadpoolWait(wait.get(), myContext.Event.get(), nullptr);

    constexpr int loopCount = 5;
    for (int currCallbackCount = 0; currCallbackCount != loopCount; ++currCallbackCount)
    {
        // Signal event.
        myContext.Event.SetEvent();

        // Wait until 'myContext.Counter' increments by 1.
        for (int itr = 0; itr != 50 && currCallbackCount == myContext.Counter; ++itr)
        {
            Sleep(10);
        }

        // Ensure we didn't timeout
        REQUIRE(currCallbackCount + 1 == myContext.Counter);
    }

    // Signal one last event.
    myContext.Event.SetEvent();

    // Close thread-pool wait.
    wait.reset();
    myContext.Event.reset();

    // Verify counter.
    if (requireExactCallbackCount)
    {
        REQUIRE(loopCount + 1 == myContext.Counter);
    }
    else
    {
        REQUIRE((loopCount + 1 == myContext.Counter || loopCount == myContext.Counter));
    }
}

TEST_CASE("WindowsInternalTests::ThreadPoolWaitTest", "[resource][unique_threadpool_wait]")
{
    ThreadPoolWaitTestHelper<wil::unique_threadpool_wait>(false);
    ThreadPoolWaitTestHelper<wil::unique_threadpool_wait_nocancel>(true);
}

struct ThreadPoolWaitWorkContext
{
    volatile LONG Counter = 0;
};

static void __stdcall ThreadPoolWaitWorkCallback(
    _Inout_ PTP_CALLBACK_INSTANCE /*instance*/,
    _Inout_opt_ void* context,
    _Inout_ PTP_WORK /*work*/)
{
    ThreadPoolWaitWorkContext& myContext = *reinterpret_cast<ThreadPoolWaitWorkContext*>(context);
    ::InterlockedIncrement(&myContext.Counter);
}

template <typename WaitResourceT>
void ThreadPoolWaitWorkHelper(bool requireExactCallbackCount)
{
    ThreadPoolWaitWorkContext myContext;

    WaitResourceT work;
    work.reset(CreateThreadpoolWork(ThreadPoolWaitWorkCallback, &myContext, nullptr));
    REQUIRE(work);

    constexpr int loopCount = 5;
    for (int itr = 0; itr != loopCount; ++itr)
    {
        SubmitThreadpoolWork(work.get());
    }

    work.reset();

    if (requireExactCallbackCount)
    {
        REQUIRE(loopCount == myContext.Counter);
    }
    else
    {
        REQUIRE(loopCount >= myContext.Counter);
    }
}

TEST_CASE("WindowsInternalTests::ThreadPoolWorkTest", "[resource][unique_threadpool_work]")
{
    ThreadPoolWaitWorkHelper<wil::unique_threadpool_work>(false);
    ThreadPoolWaitWorkHelper<wil::unique_threadpool_work_nocancel>(true);
}

struct ThreadPoolTimerWorkContext
{
    volatile LONG Counter = 0;
    wil::unique_event_nothrow Event;
};

static void __stdcall ThreadPoolTimerWorkCallback(
    _Inout_ PTP_CALLBACK_INSTANCE /*instance*/,
    _Inout_opt_ void* context,
    _Inout_ PTP_TIMER /*timer*/)
{
    ThreadPoolTimerWorkContext& myContext = *reinterpret_cast<ThreadPoolTimerWorkContext*>(context);
    myContext.Event.SetEvent();
    ::InterlockedIncrement(&myContext.Counter);
}

template <typename TimerResourceT, typename DueTimeT, typename SetThreadpoolTimerT>
void ThreadPoolTimerWorkHelper(SetThreadpoolTimerT const &setThreadpoolTimerFn, bool requireExactCallbackCount)
{
    ThreadPoolTimerWorkContext myContext;
    REQUIRE_SUCCEEDED(myContext.Event.create());

    TimerResourceT timer;
    timer.reset(CreateThreadpoolTimer(ThreadPoolTimerWorkCallback, &myContext, nullptr));
    REQUIRE(timer);

    constexpr int loopCount = 5;
    for (int currCallbackCount = 0; currCallbackCount != loopCount; ++currCallbackCount)
    {
        // Schedule timer
        myContext.Event.ResetEvent();
        const auto allowedWindow = 0;
        LONGLONG dueTime = -5 * 10000I64; // 5ms
        setThreadpoolTimerFn(timer.get(), reinterpret_cast<DueTimeT *>(&dueTime), 0, allowedWindow);

        // Wait until 'myContext.Counter' increments by 1.
        REQUIRE(myContext.Event.wait(500));
        for (int itr = 0; itr != 50 && currCallbackCount == myContext.Counter; ++itr)
        {
            Sleep(10);
        }

        // Ensure we didn't timeout
        REQUIRE(currCallbackCount + 1 == myContext.Counter);
    }

    // Schedule one last timer.
    myContext.Event.ResetEvent();
    const auto allowedWindow = 0;
    LONGLONG dueTime = -5 * 10000I64; // 5ms
    setThreadpoolTimerFn(timer.get(), reinterpret_cast<DueTimeT *>(&dueTime), 0, allowedWindow);

    if (requireExactCallbackCount)
    {
        // Wait for the event to be set
        REQUIRE(myContext.Event.wait(500));
    }

    // Close timer.
    timer.reset();
    myContext.Event.reset();

    // Verify counter.
    if (requireExactCallbackCount)
    {
        REQUIRE(loopCount + 1 == myContext.Counter);
    }
    else
    {
        REQUIRE((loopCount + 1 == myContext.Counter || loopCount == myContext.Counter));
    }
}

TEST_CASE("WindowsInternalTests::ThreadPoolTimerTest", "[resource][unique_threadpool_timer]")
{
    static_assert(sizeof(FILETIME) == sizeof(LONGLONG), "FILETIME and LONGLONG must be same size");
    ThreadPoolTimerWorkHelper<wil::unique_threadpool_timer, FILETIME>(SetThreadpoolTimer, false);
    ThreadPoolTimerWorkHelper<wil::unique_threadpool_timer_nocancel, FILETIME>(SetThreadpoolTimer, true);
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
static void __stdcall SlimEventTrollCallback(
    _Inout_ PTP_CALLBACK_INSTANCE /*instance*/,
    _Inout_opt_ void* context,
    _Inout_ PTP_TIMER /*timer*/)
{
    auto event = reinterpret_cast<wil::slim_event*>(context);

    // Wake up the thread without setting the event.
    // Note: This relies on the fact that the 'wil::slim_event' class only has a single member variable.
    WakeByAddressAll(event);
}

static void __stdcall SlimEventFriendlyCallback(
    _Inout_ PTP_CALLBACK_INSTANCE /*instance*/,
    _Inout_opt_ void* context,
    _Inout_ PTP_TIMER /*timer*/)
{
    auto event = reinterpret_cast<wil::slim_event*>(context);
    event->SetEvent();
}

TEST_CASE("WindowsInternalTests::SlimEventTests", "[resource][slim_event]")
{
    {
        wil::slim_event event;

        // Verify simple timeouts work on an auto-reset event.
        REQUIRE_FALSE(event.wait(/*timeout(ms)*/ 0));
        REQUIRE_FALSE(event.wait(/*timeout(ms)*/ 10));

        wil::unique_threadpool_timer trollTimer(CreateThreadpoolTimer(SlimEventTrollCallback, &event, nullptr));
        REQUIRE(trollTimer);

        FILETIME trollDueTime = wil::filetime::from_int64(0);
        SetThreadpoolTimer(trollTimer.get(), &trollDueTime, /*period(ms)*/ 5, /*window(ms)*/ 0);

        // Ensure we timeout in spite of being constantly woken up unnecessarily.
        REQUIRE_FALSE(event.wait(/*timeout(ms)*/ 100));

        wil::unique_threadpool_timer friendlyTimer(CreateThreadpoolTimer(SlimEventFriendlyCallback, &event, nullptr));
        REQUIRE(friendlyTimer);

        FILETIME friendlyDueTime = wil::filetime::from_int64(UINT64(-100 * wil::filetime_duration::one_millisecond)); // 100ms (relative to now)
        SetThreadpoolTimer(friendlyTimer.get(), &friendlyDueTime, /*period(ms)*/ 0, /*window(ms)*/ 0);

        // Now that the 'friendlyTimer' is queued, we should succeed.
        REQUIRE(event.wait(INFINITE));

        // Ensure event is auto-reset.
        REQUIRE_FALSE(event.wait(/*timeout(ms)*/ 100));
    }

    {
        wil::slim_event_manual_reset manualResetEvent;

        // Verify simple timeouts work on a manual-reset event.
        REQUIRE_FALSE(manualResetEvent.wait(/*timeout(ms)*/ 0));
        REQUIRE_FALSE(manualResetEvent.wait(/*timeout(ms)*/ 10));

        // Ensure multiple waits can occur on a manual-reset event.
        manualResetEvent.SetEvent();
        REQUIRE(manualResetEvent.wait());
        REQUIRE(manualResetEvent.wait(/*timeout(ms)*/ 100));
        REQUIRE(manualResetEvent.wait(INFINITE));

        // Verify 'ResetEvent' works.
        manualResetEvent.ResetEvent();
        REQUIRE_FALSE(manualResetEvent.wait(/*timeout(ms)*/ 10));
    }

}
#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && (_WIN32_WINNT >= _WIN32_WINNT_WIN8)

struct ConditionVariableCSCallbackContext
{
    wil::condition_variable event;
    wil::critical_section lock;
    auto acquire() { return lock.lock(); }
};

struct ConditionVariableSRWCallbackContext
{
    wil::condition_variable event;
    wil::srwlock lock;
    auto acquire() { return lock.lock_exclusive(); }
};

template <typename T>
static void __stdcall ConditionVariableCallback(
    _Inout_ PTP_CALLBACK_INSTANCE /*Instance*/,
    _In_ void* Context)
{
    auto callbackContext = reinterpret_cast<T*>(Context);

    // Acquire the lock to ensure we don't notify the condition variable before the other thread has
    // gone to sleep.
    auto gate = callbackContext->acquire();

    // Signal the condition variable.
    callbackContext->event.notify_all();
}

// A quick sanity check of the 'wil::condition_variable' type.
TEST_CASE("WindowsInternalTests::ConditionVariableTests", "[resource][condition_variable]")
{
    SECTION("Test 'wil::condition_variable' with 'wil::critical_section'")
    {
        ConditionVariableCSCallbackContext callbackContext;
        auto gate = callbackContext.lock.lock();

        // Schedule the thread that will wake up this thread.
        REQUIRE(TrySubmitThreadpoolCallback(ConditionVariableCallback<ConditionVariableCSCallbackContext>, &callbackContext, nullptr));

        // Wait on the condition variable.
        REQUIRE(callbackContext.event.wait_for(gate, /*timeout(ms)*/ 500));
    }

    SECTION("Test 'wil::condition_variable' with 'wil::srwlock'")
    {
        ConditionVariableSRWCallbackContext callbackContext;

        // Test exclusive lock.
        {
            auto gate = callbackContext.lock.lock_exclusive();

            // Schedule the thread that will wake up this thread.
            REQUIRE(TrySubmitThreadpoolCallback(ConditionVariableCallback<ConditionVariableSRWCallbackContext>, &callbackContext, nullptr));

            // Wait on the condition variable.
            REQUIRE(callbackContext.event.wait_for(gate, /*timeout(ms)*/ 500));
        }

        // Test shared lock.
        {
            auto gate = callbackContext.lock.lock_shared();

            // Schedule the thread that will wake up this thread.
            REQUIRE(TrySubmitThreadpoolCallback(ConditionVariableCallback<ConditionVariableSRWCallbackContext>, &callbackContext, nullptr));

            // Wait on the condition variable.
            REQUIRE(callbackContext.event.wait_for(gate, /*timeout(ms)*/ 500));
        }
    }
}

TEST_CASE("WindowsInternalTests::ReturnWithExpectedTests", "[result_macros]")
{
    wil::g_pfnResultLoggingCallback = ResultMacrosLoggingCallback;

    // Succeeded
    REQUIRE_RETURNS_EXPECTED(S_OK, [] { RETURN_IF_FAILED_WITH_EXPECTED(MDEC(hrOKRef()), E_UNEXPECTED); return S_OK; });

    // Expected
    REQUIRE_RETURNS_EXPECTED(E_FAIL, [] { RETURN_IF_FAILED_WITH_EXPECTED(E_FAIL, E_FAIL); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_UNEXPECTED, [] { RETURN_IF_FAILED_WITH_EXPECTED(E_UNEXPECTED, E_FAIL, E_UNEXPECTED, E_POINTER, E_INVALIDARG); return S_OK; });

    // Unexpected
    REQUIRE_RETURNS_EXPECTED(E_FAIL, [] { RETURN_IF_FAILED_WITH_EXPECTED(E_FAIL, E_UNEXPECTED); return S_OK; });
    REQUIRE_RETURNS_EXPECTED(E_FAIL, [] { RETURN_IF_FAILED_WITH_EXPECTED(E_FAIL, E_UNEXPECTED, E_POINTER, E_INVALIDARG); return S_OK; });
}

TEST_CASE("WindowsInternalTests::LogWithExpectedTests", "[result_macros]")
{
    wil::g_pfnResultLoggingCallback = ResultMacrosLoggingCallback;

    // Succeeded
    REQUIRE_LOG(S_OK, [] { REQUIRE(S_OK == LOG_IF_FAILED_WITH_EXPECTED(MDEC(hrOKRef()), E_FAIL, E_INVALIDARG)); });

    // Expected
    REQUIRE_LOG(S_OK, [] { REQUIRE(E_UNEXPECTED == LOG_IF_FAILED_WITH_EXPECTED(E_UNEXPECTED, E_UNEXPECTED, E_INVALIDARG)); });
    REQUIRE_LOG(S_OK, [] { REQUIRE(E_UNEXPECTED == LOG_IF_FAILED_WITH_EXPECTED(E_UNEXPECTED, E_FAIL, E_UNEXPECTED, E_POINTER, E_INVALIDARG)); });

    // Unexpected
    REQUIRE_LOG(E_FAIL, [] { REQUIRE(E_FAIL == LOG_IF_FAILED_WITH_EXPECTED(E_FAIL, E_UNEXPECTED)); });
    REQUIRE_LOG(E_FAIL, [] { REQUIRE(E_FAIL == LOG_IF_FAILED_WITH_EXPECTED(E_FAIL, E_UNEXPECTED, E_POINTER, E_INVALIDARG)); });
}

// Verifies that the shutdown-aware objects respect the alignment
// of the wrapped object.
template<template<typename> class Wrapper>
void VerifyAlignment()
{
    // Some of the wrappers require a method called ProcessShutdown(), so we'll give it one.
    struct alignment_sensitive_struct
    {
        // Use SLIST_HEADER as our poster child alignment-sensitive data type.
        SLIST_HEADER value;
        void ProcessShutdown() { }
    };
    static_assert(alignof(alignment_sensitive_struct) != alignof(char), "Need to choose a better alignment-sensitive type");

    // Create a custom structure that tries to force misalignment.
    struct attempted_misalignment
    {
        char c;
        Wrapper<alignment_sensitive_struct> wrapper;
    } possibly_misaligned{};

    static_assert(alignof(attempted_misalignment) == alignof(alignment_sensitive_struct), "Wrapper type does not respect alignment");

    // Verify that the wrapper type placed the inner object at proper alignment.
    // Note: use std::addressof in case the alignment_sensitive_struct overrides the & operator.
    REQUIRE(reinterpret_cast<uintptr_t>(std::addressof(possibly_misaligned.wrapper.get())) % alignof(alignment_sensitive_struct) == 0);
}

TEST_CASE("WindowsInternalTests::ShutdownAwareObjectAlignmentTests", "[result_macros]")
{
    VerifyAlignment<wil::manually_managed_shutdown_aware_object>();
    VerifyAlignment<wil::shutdown_aware_object>();
    VerifyAlignment<wil::object_without_destructor_on_shutdown>();
}

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
TEST_CASE("WindowsInternalTests::ModuleReference", "[wrl]")
{
    REQUIRE(::Microsoft::WRL::GetModuleBase() == nullptr);
    // Executables don't have a ModuleBase, so we need to create one.
    struct FakeModuleBase : Microsoft::WRL::Details::ModuleBase
    {
        unsigned long count = 42;
        STDMETHOD_(unsigned long, IncrementObjectCount)()
        {
            return InterlockedIncrement(&count);
        }
        STDMETHOD_(unsigned long, DecrementObjectCount)()
        {
            return InterlockedDecrement(&count);
        }
        STDMETHOD_(unsigned long, GetObjectCount)() const
        {
            return count;
        }
        // Dummy implementations of everything else (never called).
        STDMETHOD_(const Microsoft::WRL::Details::CreatorMap**, GetFirstEntryPointer)() const { return nullptr; }
        STDMETHOD_(const Microsoft::WRL::Details::CreatorMap**, GetMidEntryPointer)() const { return nullptr; }
        STDMETHOD_(const Microsoft::WRL::Details::CreatorMap**, GetLastEntryPointer)() const { return nullptr; }
        STDMETHOD_(SRWLOCK*, GetLock)() const { return nullptr; }
        STDMETHOD(RegisterWinRTObject)(const wchar_t*, const wchar_t**, _Inout_ RO_REGISTRATION_COOKIE*, unsigned int) { return E_NOTIMPL; }
        STDMETHOD(UnregisterWinRTObject)(const wchar_t*, _In_ RO_REGISTRATION_COOKIE) { return E_NOTIMPL; }
        STDMETHOD(RegisterCOMObject)(const wchar_t*, _In_ IID*, _In_ IClassFactory**, _Inout_ DWORD*, unsigned int) { return E_NOTIMPL; }
        STDMETHOD(UnregisterCOMObject)(const wchar_t*, _Inout_ DWORD*, unsigned int) { return E_NOTIMPL; }

    };
    FakeModuleBase fake;

    auto peek_module_ref_count = []()
    {
        return ::Microsoft::WRL::GetModuleBase()->GetObjectCount();
    };

    auto initial = peek_module_ref_count();

    // Basic test: Construct and destruct.
    {
        auto module_ref = wil::wrl_module_reference();
        REQUIRE(peek_module_ref_count() == initial + 1);
    }
    REQUIRE(peek_module_ref_count() == initial);

    // Fancy test: Copy object with embedded reference.
    {
        struct object_with_ref
        {
            wil::wrl_module_reference ref;
        };
        object_with_ref o1;
        REQUIRE(peek_module_ref_count() == initial + 1);
        auto o2 = o1;
        REQUIRE(peek_module_ref_count() == initial + 2);
        o1 = o2;
        REQUIRE(peek_module_ref_count() == initial + 2);
        o2 = std::move(o1);
        REQUIRE(peek_module_ref_count() == initial + 2);
    }
    REQUIRE(peek_module_ref_count() == initial);
}
#endif

#if defined(WIL_ENABLE_EXCEPTIONS) && (defined(NTDDI_WIN10_CO) ? \
    WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES) : \
    WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES))
TEST_CASE("WindowsInternalTests::VerifyModuleReferencesForThread", "[win32_helpers]")
{
    bool success = true;
    std::thread([&]
    {
        auto moduleRef = wil::get_module_reference_for_thread();
        moduleRef.reset(); // results in exiting the thread
        // should never get here
        success = false;
        FAIL();
    }).join();
    REQUIRE(success);
}
#endif

#pragma warning(pop)
