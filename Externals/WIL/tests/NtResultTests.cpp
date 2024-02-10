
#include <wil/result.h>
#include <wil/nt_result_macros.h>

#include "common.h"

#define STATUS_OBJECT_PATH_NOT_FOUND     ((NTSTATUS)0xC000003AL)
#define STATUS_INTERNAL_ERROR            ((NTSTATUS)0xC00000E5L)
#define STATUS_INVALID_CONNECTION        ((NTSTATUS)0xC0000140L)
#define E_LOAD_NAMESERVICE_FAILED        ((HRESULT)0x80000140L)

TEST_CASE("NtResultTests::NtReturn", "[result]")
{
    auto status = []()
    {
        NT_RETURN_NTSTATUS(STATUS_INVALID_CONNECTION);
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);

    status = []()
    {
        NT_RETURN_NTSTATUS_MSG(STATUS_INVALID_CONNECTION, "Test NT_RETURN_NTSTATUS_MSG");
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);

    status = []()
    {
        NT_RETURN_NTSTATUS_MSG(STATUS_INVALID_CONNECTION, "Test NT_RETURN_NTSTATUS_MSG %s", L"with parameter");
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);

    status = []()
    {
        NT_RETURN_IF_NTSTATUS_FAILED(STATUS_INVALID_CONNECTION);
        return STATUS_SUCCESS;
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);

    status = []()
    {
        NT_RETURN_IF_NTSTATUS_FAILED_MSG(STATUS_INVALID_CONNECTION, "Test NT_RETURN_NTSTATUS_MSG %s", L"with parameter");
        return STATUS_SUCCESS;
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);

    status = []()
    {
        NT_RETURN_IF_NTSTATUS_FAILED(STATUS_SUCCESS);
        return STATUS_INVALID_CONNECTION;
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);
}

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("NtResultTests::NtThrowCatch", "[result]")
{
    // Throw NTSTATUS with immediate conversion to HRESULT. HRESULT would appear in the logs.
    auto hr = []()
    {
        try
        {
            THROW_NTSTATUS(STATUS_INVALID_CONNECTION);
        }
        CATCH_RETURN();
    }();
    // THROW_NTSTATUS converts NTSTATUS to HRESULT through WIN32 error code.
    REQUIRE(hr == wil::details::NtStatusToHr(STATUS_INVALID_CONNECTION));

    // Verify that conversion NTSTATUS -> HRESULT -> NTSTATUS is not 1:1.
    auto status = []()
    {
        try
        {
            THROW_HR(wil::details::NtStatusToHr(STATUS_INVALID_CONNECTION));
        }
        NT_CATCH_RETURN();
    }();
    if (wil::details::g_pfnRtlNtStatusToDosErrorNoTeb)
    {
        REQUIRE(status != STATUS_INVALID_CONNECTION);
    }
    else
    {
        REQUIRE(status == STATUS_INVALID_CONNECTION);
    }

    // Throw HRESULT with conversion to NTSTATUS on a best effort. NTSTATUS would appear in the logs.
    status = []()
    {
        try
        {
            THROW_HR(__HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
        }
        NT_CATCH_RETURN();
    }();
    REQUIRE(status == STATUS_OBJECT_PATH_NOT_FOUND);

    // Throw HRESULT with conversion to NTSTATUS on a best effort that maps to generic error. NTSTATUS would appear in the logs.
    status = []()
    {
        try
        {
            THROW_HR(E_LOAD_NAMESERVICE_FAILED);
        }
        NT_CATCH_RETURN();
    }();
    REQUIRE(status == STATUS_INTERNAL_ERROR);

    // Throw NTSTATUS without conversion. NTSTATUS would appear in the logs.
    status = []()
    {
        try
        {
            THROW_NTSTATUS(STATUS_INVALID_CONNECTION);
        }
        NT_CATCH_RETURN();
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);

    status = []()
    {
        try
        {
            THROW_NTSTATUS_MSG(STATUS_INVALID_CONNECTION, "Throw STATUS_INVALID_CONNECTION as NTSTATUS");
        }
        NT_CATCH_RETURN();
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);

    status = []()
    {
        try
        {
            THROW_NTSTATUS_MSG(STATUS_INVALID_CONNECTION, "Throw STATUS_INVALID_CONNECTION as NTSTATUS with custom catch");
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();

            return wil::StatusFromCaughtException();
        }
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);

    hr = []()
    {
        try
        {
            THROW_NTSTATUS_MSG(STATUS_INVALID_CONNECTION, "Throw STATUS_INVALID_CONNECTION as NTSTATUS");
        }
        CATCH_RETURN();
    }();
    REQUIRE(hr == wil::details::NtStatusToHr(STATUS_INVALID_CONNECTION));

    status = []()
    {
        try
        {
            THROW_NTSTATUS_MSG(STATUS_INVALID_CONNECTION, "Throw STATUS_INVALID_CONNECTION as NTSTATUS");
        }
        NT_CATCH_RETURN_MSG("Catching STATUS_INVALID_CONNECTION thrown by NT_THROW_NTSTATUS_MSG");
    }();
    REQUIRE(status == STATUS_INVALID_CONNECTION);
}
#endif
