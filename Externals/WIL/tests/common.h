#pragma once

#include <windows.h>
#include <PathCch.h>

#include "catch.hpp"

#include <wil/filesystem.h>
#include <wil/result.h>

#define REPORTS_ERROR(expr) witest::ReportsError(wistd::is_same<HRESULT, decltype(expr)>{}, [&]() { return expr; })
#define REQUIRE_ERROR(expr) REQUIRE(REPORTS_ERROR(expr))
#define REQUIRE_NOERROR(expr) REQUIRE_FALSE(REPORTS_ERROR(expr))

#define CRASHES(expr) witest::DoesCodeCrash([&]() { return expr; })
#define REQUIRE_CRASH(expr) REQUIRE(CRASHES(expr))
#define REQUIRE_NOCRASH(expr) REQUIRE_FALSE(CRASHES(expr))

// NOTE: SUCCEEDED/FAILED macros not used here since Catch2 can give us better diagnostics if it knows the HRESULT value
#define REQUIRE_SUCCEEDED(expr) REQUIRE((HRESULT)(expr) >= 0)
#define REQUIRE_FAILED(expr) REQUIRE((HRESULT)(expr) < 0)

// MACRO double evaluation check.
// The following macro illustrates a common problem with writing macros:
//      #define MY_MAX(a, b) (((a) > (b)) ? (a) : (b))
// The issue is that whatever code is being used for both a and b is being executed twice.
// This isn't harmful when thinking of constant numerics, but consider this example:
//      MY_MAX(4, InterlockedIncrement(&cCount))
// This evaluates the (B) parameter twice and results in incrementing the counter twice.
// We use MDEC in unit tests to verify that this kind of pattern is not present.  A test
// of this kind:
//      MY_MAX(MDEC(4), MDEC(InterlockedIncrement(&cCount))
// will verify that the parameters are not evaluated more than once.
#define MDEC(PARAM) (witest::details::MacroDoubleEvaluationCheck(__LINE__, #PARAM), PARAM)

// There's some functionality that we need for testing that's not available for the app partition. Since those tests are
// primarily compilation tests, declare what's needed here
extern "C" {

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)
WINBASEAPI _Ret_maybenull_
PVOID WINAPI AddVectoredExceptionHandler(_In_ ULONG First, _In_ PVECTORED_EXCEPTION_HANDLER Handler);

WINBASEAPI
ULONG WINAPI RemoveVectoredExceptionHandler(_In_ PVOID Handle);
#endif

}

#pragma warning(push)
#pragma warning(disable: 4702) // Unreachable code

namespace witest
{
    namespace details
    {
        inline void MacroDoubleEvaluationCheck(size_t uLine, _In_ const char* pszCode)
        {
            struct SEval
            {
                size_t uLine;
                const char* pszCode;
            };

            static SEval rgEval[15] = {};
            static size_t nOffset = 0;

            for (auto& eval : rgEval)
            {
                if ((eval.uLine == uLine) && (eval.pszCode != nullptr) && (0 == strcmp(pszCode, eval.pszCode)))
                {
                    // This verification indicates that macro-double-evaluation check is firing for a particular usage of MDEC().
                    FAIL("Expression '" << pszCode << "' double evaluated in macro on line " << uLine);
                }
            }

            rgEval[nOffset].uLine = uLine;
            rgEval[nOffset].pszCode = pszCode;
            nOffset = (nOffset + 1) % ARRAYSIZE(rgEval);
        }

        template <typename T>
        class AssignTemporaryValueCleanup
        {
        public:
            AssignTemporaryValueCleanup(_In_ AssignTemporaryValueCleanup const &) = delete;
            AssignTemporaryValueCleanup & operator=(_In_ AssignTemporaryValueCleanup const &) = delete;

            explicit AssignTemporaryValueCleanup(_Inout_ T *pVal, T val) WI_NOEXCEPT :
                m_pVal(pVal),
                m_valOld(*pVal)
            {
                *pVal = val;
            }

            AssignTemporaryValueCleanup(_Inout_ AssignTemporaryValueCleanup && other) WI_NOEXCEPT :
                m_pVal(other.m_pVal),
                m_valOld(other.m_valOld)
            {
                other.m_pVal = nullptr;
            }

            ~AssignTemporaryValueCleanup() WI_NOEXCEPT
            {
                operator()();
            }

            void operator()() WI_NOEXCEPT
            {
                if (m_pVal != nullptr)
                {
                    *m_pVal = m_valOld;
                    m_pVal = nullptr;
                }
            }

            void Dismiss() WI_NOEXCEPT
            {
                m_pVal = nullptr;
            }

        private:
            T *m_pVal;
            T m_valOld;
        };
    }

    // Use the following routine to allow for a variable to be swapped with another and automatically revert the
    // assignment at the end of the scope.
    // Example:
    //      int nFoo = 10
    //      {
    //          auto revert = witest::AssignTemporaryValue(&nFoo, 12);
    //          // nFoo will now be 12 within this scope...
    //      }
    //      // and nFoo is back to 10 within the outer scope
    template <typename T>
    inline witest::details::AssignTemporaryValueCleanup<T> AssignTemporaryValue(_Inout_ T *pVal, T val) WI_NOEXCEPT
    {
        return witest::details::AssignTemporaryValueCleanup<T>(pVal, val);
    }

    //! Global class which tracks objects that derive from @ref AllocatedObject.
    //! Use `witest::g_objectCount.Leaked()` to determine if an object deriving from `AllocatedObject` has been leaked.
    class GlobalCount
    {
    public:
        int m_count = 0;

        //! Returns `true` if there are any objects that derive from @ref AllocatedObject still in memory.
        bool Leaked() const
        {
            return (m_count != 0);
        }

        ~GlobalCount()
        {
            if (Leaked())
            {
                // NOTE: This runs when no test is active, but will still cause an assert failure to notify
                FAIL("GlobalCount is non-zero; there is a leak somewhere");
            }
        }
    };
    __declspec(selectany) GlobalCount g_objectCount;

    //! Derive an allocated test object from witest::AllocatedObject to ensure that those objects aren't leaked in the test.
    //! Note that you can call g_objectCount.Leaked() at any point to determine if a leak has already occurred (assuming that
    //! all objects should have been destroyed at that point.
    class AllocatedObject
    {
    public:
        AllocatedObject()   { g_objectCount.m_count++; }
        ~AllocatedObject()  { g_objectCount.m_count--; }
    };

    template <typename Lambda>
    bool DoesCodeThrow(Lambda&& callOp)
    {
#ifdef WIL_ENABLE_EXCEPTIONS
        try
#endif
        {
            callOp();
        }
#ifdef WIL_ENABLE_EXCEPTIONS
        catch (...)
        {
            return true;
        }
#endif

        return false;
    }

    [[noreturn]]
    inline void __stdcall TranslateFailFastException(PEXCEPTION_RECORD rec, PCONTEXT, DWORD)
    {
        // RaiseFailFastException cannot be continued or handled. By instead calling RaiseException, it allows us to
        // handle exceptions
        ::RaiseException(rec->ExceptionCode, rec->ExceptionFlags, rec->NumberParameters, rec->ExceptionInformation);
#ifdef __clang__
        __builtin_unreachable();
#endif
    }

    [[noreturn]]
    inline void __stdcall FakeFailfastWithContext(const wil::FailureInfo&) noexcept
    {
        ::RaiseException(STATUS_STACK_BUFFER_OVERRUN, 0, 0, nullptr);
#ifdef __clang__
        __builtin_unreachable();
#endif
    }

    constexpr DWORD msvc_exception_code = 0xE06D7363;

    // This is a MAJOR hack. Catch2 registers a vectored exception handler - which gets run before our handler below -
    // that interprets a set of exception codes as fatal. We don't want this behavior since we may be expecting such
    // crashes, so instead translate all exception codes to something not fatal
    inline LONG WINAPI TranslateExceptionCodeHandler(PEXCEPTION_POINTERS info)
    {
        if (info->ExceptionRecord->ExceptionCode != witest::msvc_exception_code)
        {
            info->ExceptionRecord->ExceptionCode = STATUS_STACK_BUFFER_OVERRUN;
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }

    namespace details
    {
        inline bool DoesCodeCrash(wistd::function<void()>& callOp)
        {
            bool result = false;
            __try
            {
                callOp();
            }
            // Let C++ exceptions pass through
            __except ((::GetExceptionCode() != msvc_exception_code) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
            {
                result = true;
            }
            return result;
        }
    }

    inline bool DoesCodeCrash(wistd::function<void()> callOp)
    {
        // See above; we don't want to actually fail fast, so make sure we raise a different exception instead
        auto restoreHandler = AssignTemporaryValue(&wil::details::g_pfnRaiseFailFastException, TranslateFailFastException);
        auto restoreHandler2 = AssignTemporaryValue(&wil::details::g_pfnFailfastWithContextCallback, FakeFailfastWithContext);

        auto handler = AddVectoredExceptionHandler(1, TranslateExceptionCodeHandler);
        auto removeVectoredHandler = wil::scope_exit([&] { RemoveVectoredExceptionHandler(handler); });

        return details::DoesCodeCrash(callOp);
    }

    template <typename Lambda>
    bool ReportsError(wistd::false_type, Lambda&& callOp)
    {
        bool doesThrow = false;
        bool doesCrash = DoesCodeCrash([&]()
        {
            doesThrow = DoesCodeThrow(callOp);
        });

        return doesThrow || doesCrash;
    }

    template <typename Lambda>
    bool ReportsError(wistd::true_type, Lambda&& callOp)
    {
        return FAILED(callOp());
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    class TestFailureCache final :
        public wil::details::IFailureCallback
    {
    public:
        TestFailureCache() :
            m_callbackHolder(this)
        {
        }

        void clear()
        {
            m_failures.clear();
        }

        size_t size() const
        {
            return m_failures.size();
        }

        bool empty() const
        {
            return m_failures.empty();
        }

        const wil::FailureInfo& operator[](size_t pos) const
        {
            return m_failures.at(pos).GetFailureInfo();
        }

        // IFailureCallback
        bool NotifyFailure(wil::FailureInfo const & failure) WI_NOEXCEPT override
        {
            m_failures.emplace_back(failure);
            return false;
        }

    private:
        std::vector<wil::StoredFailureInfo> m_failures;
        wil::details::ThreadFailureCallbackHolder m_callbackHolder;
    };
#endif

    inline HRESULT GetTempFileName(wchar_t (&result)[MAX_PATH])
    {
        wchar_t dir[MAX_PATH];
        RETURN_LAST_ERROR_IF(::GetTempPathW(MAX_PATH, dir) == 0);
        RETURN_LAST_ERROR_IF(::GetTempFileNameW(dir, L"wil", 0, result) == 0);
        return S_OK;
    }

    inline HRESULT CreateUniqueFolderPath(wchar_t (&buffer)[MAX_PATH], PCWSTR root = nullptr)
    {
        if (root)
        {
            RETURN_LAST_ERROR_IF(::GetTempFileNameW(root, L"wil", 0, buffer) == 0);
        }
        else
        {
            wchar_t tempPath[MAX_PATH];
            RETURN_LAST_ERROR_IF(::GetTempPathW(ARRAYSIZE(tempPath), tempPath) == 0);
            RETURN_LAST_ERROR_IF(::GetLongPathNameW(tempPath, tempPath, ARRAYSIZE(tempPath)) == 0);
            RETURN_LAST_ERROR_IF(::GetTempFileNameW(tempPath, L"wil", 0, buffer) == 0);
        }
        RETURN_IF_WIN32_BOOL_FALSE(DeleteFileW(buffer));
        PathCchRemoveExtension(buffer, ARRAYSIZE(buffer));
        return S_OK;
    }

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && (_WIN32_WINNT >= _WIN32_WINNT_WIN7)

    struct TestFolder
    {
        TestFolder()
        {
            if (SUCCEEDED(CreateUniqueFolderPath(m_path)) && SUCCEEDED(wil::CreateDirectoryDeepNoThrow(m_path)))
            {
                m_valid = true;
            }
        }

        TestFolder(PCWSTR path)
        {
            if (SUCCEEDED(StringCchCopyW(m_path, ARRAYSIZE(m_path), path)) && SUCCEEDED(wil::CreateDirectoryDeepNoThrow(m_path)))
            {
                m_valid = true;
            }
        }

        TestFolder(const TestFolder&) = delete;
        TestFolder& operator=(const TestFolder&) = delete;

        TestFolder(TestFolder&& other)
        {
            if (other.m_valid)
            {
                m_valid = true;
                other.m_valid = false;
                wcscpy_s(m_path, other.m_path);
            }
        }

        ~TestFolder()
        {
            if (m_valid)
            {
                wil::RemoveDirectoryRecursiveNoThrow(m_path);
            }
        }

        operator bool() const
        {
            return m_valid;
        }

        operator PCWSTR() const
        {
            return m_path;
        }

        PCWSTR Path() const
        {
            return m_path;
        }

    private:

        bool m_valid = false;
        wchar_t m_path[MAX_PATH] = L"";
    };

    struct TestFile
    {
        TestFile(PCWSTR path)
        {
            if (SUCCEEDED(StringCchCopyW(m_path, ARRAYSIZE(m_path), path)))
            {
                Create();
            }
        }

        TestFile(PCWSTR dirPath, PCWSTR fileName)
        {
            if (SUCCEEDED(StringCchCopyW(m_path, ARRAYSIZE(m_path), dirPath)) && SUCCEEDED(PathCchAppend(m_path, ARRAYSIZE(m_path), fileName)))
            {
                Create();
            }
        }

        TestFile(const TestFile&) = delete;
        TestFile& operator=(const TestFile&) = delete;

        TestFile(TestFile&& other)
        {
            if (other.m_valid)
            {
                m_valid = true;
                m_deleteDir = other.m_deleteDir;
                other.m_valid = other.m_deleteDir = false;
                wcscpy_s(m_path, other.m_path);
            }
        }

        ~TestFile()
        {
            // Best effort on all of these
            if (m_valid)
            {
                ::DeleteFileW(m_path);
            }
            if (m_deleteDir)
            {
                size_t parentLength;
                if (wil::try_get_parent_path_range(m_path, &parentLength))
                {
                    m_path[parentLength] = L'\0';
                    ::RemoveDirectoryW(m_path);
                    m_path[parentLength] = L'\\';
                }
            }
        }

        operator bool() const
        {
            return m_valid;
        }

        operator PCWSTR() const
        {
            return m_path;
        }

        PCWSTR Path() const
        {
            return m_path;
        }

    private:

        HRESULT Create()
        {
            WI_ASSERT(!m_valid && !m_deleteDir);
            wil::unique_hfile fileHandle(::CreateFileW(m_path,
                FILE_WRITE_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!fileHandle)
            {
                auto err = ::GetLastError();
                size_t parentLength;
                if ((err == ERROR_PATH_NOT_FOUND) && wil::try_get_parent_path_range(m_path, &parentLength))
                {
                    m_path[parentLength] = L'\0';
                    RETURN_IF_FAILED(wil::CreateDirectoryDeepNoThrow(m_path));
                    m_deleteDir = true;

                    m_path[parentLength] = L'\\';
                    fileHandle.reset(::CreateFileW(m_path,
                        FILE_WRITE_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
                    RETURN_LAST_ERROR_IF(!fileHandle);
                }
                else
                {
                    RETURN_WIN32(err);
                }
            }

            m_valid = true;
            return S_OK;
        }

        bool m_valid = false;
        bool m_deleteDir = false;
        wchar_t m_path[MAX_PATH] = L"";
    };

#endif
}

#pragma warning(pop)
