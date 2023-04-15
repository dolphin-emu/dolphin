
#include <windows.h>
#include <winstring.h> // For wil::unique_hstring

#include <wil/common.h>
#ifdef WIL_ENABLE_EXCEPTIONS
#include <string>
#endif

// TODO: str_raw_ptr is not two-phase name lookup clean (https://github.com/Microsoft/wil/issues/8)
namespace wil
{
    PCWSTR str_raw_ptr(HSTRING);
#ifdef WIL_ENABLE_EXCEPTIONS
    PCWSTR str_raw_ptr(const std::wstring&);
#endif
}

#include <wil/filesystem.h>

#ifdef WIL_ENABLE_EXCEPTIONS
#include <wil/stl.h> // For std::wstring string_maker
#endif

#include "common.h"

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

bool DirectoryExists(_In_ PCWSTR path)
{
    DWORD dwAttrib = GetFileAttributesW(path);

    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool FileExists(_In_ PCWSTR path)
{
  DWORD dwAttrib = GetFileAttributesW(path);

  return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

TEST_CASE("FileSystemTests::CreateDirectory", "[filesystem]")
{
    wchar_t basePath[MAX_PATH];
    REQUIRE(GetTempPathW(ARRAYSIZE(basePath), basePath));
    REQUIRE_SUCCEEDED(PathCchAppend(basePath, ARRAYSIZE(basePath), L"FileSystemTests"));

    REQUIRE_FALSE(DirectoryExists(basePath));
    REQUIRE(SUCCEEDED(wil::CreateDirectoryDeepNoThrow(basePath)));
    REQUIRE(DirectoryExists(basePath));

    auto scopeGuard = wil::scope_exit([&]
    {
        REQUIRE_SUCCEEDED(wil::RemoveDirectoryRecursiveNoThrow(basePath));
    });

    PCWSTR relativeTestPath = L"folder1\\folder2\\folder3\\folder4\\folder5\\folder6\\folder7\\folder8";
    wchar_t absoluteTestPath[MAX_PATH];
    REQUIRE_SUCCEEDED(StringCchCopyW(absoluteTestPath, ARRAYSIZE(absoluteTestPath), basePath));
    REQUIRE_SUCCEEDED(PathCchAppend(absoluteTestPath, ARRAYSIZE(absoluteTestPath), relativeTestPath));
    REQUIRE_FALSE(DirectoryExists(absoluteTestPath));
    REQUIRE_SUCCEEDED(wil::CreateDirectoryDeepNoThrow(absoluteTestPath));

    PCWSTR invalidCharsPath = L"Bad?Char|";
    wchar_t absoluteInvalidPath[MAX_PATH];
    REQUIRE_SUCCEEDED(StringCchCopyW(absoluteInvalidPath, ARRAYSIZE(absoluteInvalidPath), basePath));
    REQUIRE_SUCCEEDED(PathCchAppend(absoluteInvalidPath, ARRAYSIZE(absoluteInvalidPath), invalidCharsPath));
    REQUIRE_FALSE(DirectoryExists(absoluteInvalidPath));
    REQUIRE_FALSE(SUCCEEDED(wil::CreateDirectoryDeepNoThrow(absoluteInvalidPath)));

    PCWSTR testPath3 = L"folder1\\folder2\\folder3";
    wchar_t absoluteTestPath3[MAX_PATH];
    REQUIRE_SUCCEEDED(StringCchCopyW(absoluteTestPath3, ARRAYSIZE(absoluteTestPath3), basePath));
    REQUIRE_SUCCEEDED(PathCchAppend(absoluteTestPath3, ARRAYSIZE(absoluteTestPath3), testPath3));
    REQUIRE(DirectoryExists(absoluteTestPath3));

    PCWSTR testPath4 = L"folder1\\folder2\\folder3\\folder4";
    wchar_t absoluteTestPath4[MAX_PATH];
    REQUIRE_SUCCEEDED(StringCchCopyW(absoluteTestPath4, ARRAYSIZE(absoluteTestPath4), basePath));
    REQUIRE_SUCCEEDED(PathCchAppend(absoluteTestPath4, ARRAYSIZE(absoluteTestPath4), testPath4));
    REQUIRE(DirectoryExists(absoluteTestPath4));

    REQUIRE_SUCCEEDED(wil::RemoveDirectoryRecursiveNoThrow(absoluteTestPath3, wil::RemoveDirectoryOptions::KeepRootDirectory));
    REQUIRE(DirectoryExists(absoluteTestPath3));
    REQUIRE_FALSE(DirectoryExists(absoluteTestPath4));
}

TEST_CASE("FileSystemTests::VerifyRemoveDirectoryRecursiveDoesNotTraverseWithoutAHandle", "[filesystem]")
{
    auto CreateRelativePath = [](PCWSTR root, PCWSTR name)
    {
        wil::unique_hlocal_string path;
        REQUIRE_SUCCEEDED(PathAllocCombine(root, name, PATHCCH_ALLOW_LONG_PATHS, &path));
        return path;
    };

    wil::unique_cotaskmem_string tempPath;
    REQUIRE_SUCCEEDED(wil::ExpandEnvironmentStringsW(LR"(%TEMP%)", tempPath));
    const auto basePath = CreateRelativePath(tempPath.get(), L"FileSystemTests");
    REQUIRE_SUCCEEDED(wil::CreateDirectoryDeepNoThrow(basePath.get()));

    auto scopeGuard = wil::scope_exit([&]
    {
        wil::RemoveDirectoryRecursiveNoThrow(basePath.get());
    });

    // Try to delete a directory whose handle is already taken.
    const auto folderToRecurse = CreateRelativePath(basePath.get(), L"folderToRecurse");
    REQUIRE(::CreateDirectoryW(folderToRecurse.get(), nullptr));

    const auto subfolderWithHandle = CreateRelativePath(folderToRecurse.get(), L"subfolderWithHandle");
    REQUIRE(::CreateDirectoryW(subfolderWithHandle.get(), nullptr));

    const auto childOfSubfolder = CreateRelativePath(subfolderWithHandle.get(), L"childOfSubfolder");
    REQUIRE(::CreateDirectoryW(childOfSubfolder.get(), nullptr));

    // Passing a 0 in share flags only allows metadata query on this file by other processes.
    // This should fail with a sharing violation error when any other action is taken.
    wil::unique_hfile subFolderHandle(::CreateFileW(subfolderWithHandle.get(), GENERIC_ALL,
        0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    REQUIRE(subFolderHandle);

    REQUIRE(wil::RemoveDirectoryRecursiveNoThrow(folderToRecurse.get()) == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION));

    // Release the handle to allow cleanup.
    subFolderHandle.reset();
}

TEST_CASE("FileSystemTests::VerifyRemoveDirectoryRecursiveCanDeleteReadOnlyFiles", "[filesystem]")
{
    auto CreateRelativePath = [](PCWSTR root, PCWSTR name)
    {
        wil::unique_hlocal_string path;
        REQUIRE_SUCCEEDED(PathAllocCombine(root, name, PATHCCH_ALLOW_LONG_PATHS, &path));
        return path;
    };

    auto CreateReadOnlyFile = [](PCWSTR path)
    {
        wil::unique_hfile fileHandle(CreateFileW(path, 0,
            0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_READONLY, nullptr));
        REQUIRE(fileHandle);
    };

    wil::unique_cotaskmem_string tempPath;
    REQUIRE_SUCCEEDED(wil::ExpandEnvironmentStringsW(LR"(%TEMP%)", tempPath));
    const auto basePath = CreateRelativePath(tempPath.get(), L"FileSystemTests");
    REQUIRE_SUCCEEDED(wil::CreateDirectoryDeepNoThrow(basePath.get()));

    auto scopeGuard = wil::scope_exit([&]
    {
        wil::RemoveDirectoryRecursiveNoThrow(basePath.get(), wil::RemoveDirectoryOptions::RemoveReadOnly);
    });

    // Create a reparse point and a target folder that shouldn't get deleted
    auto folderToDelete = CreateRelativePath(basePath.get(), L"folderToDelete");
    REQUIRE(::CreateDirectoryW(folderToDelete.get(), nullptr));

    auto topLevelReadOnly = CreateRelativePath(folderToDelete.get(), L"topLevelReadOnly.txt");
    CreateReadOnlyFile(topLevelReadOnly.get());

    auto subLevel = CreateRelativePath(folderToDelete.get(), L"subLevel");
    REQUIRE(::CreateDirectoryW(subLevel.get(), nullptr));

    auto subLevelReadOnly = CreateRelativePath(subLevel.get(), L"subLevelReadOnly.txt");
    CreateReadOnlyFile(subLevelReadOnly.get());

    // Delete will fail without the RemoveReadOnlyFlag
    REQUIRE_FAILED(wil::RemoveDirectoryRecursiveNoThrow(folderToDelete.get()));
    REQUIRE_SUCCEEDED(wil::RemoveDirectoryRecursiveNoThrow(folderToDelete.get(), wil::RemoveDirectoryOptions::RemoveReadOnly));

    // Verify all files have been deleted
    REQUIRE_FALSE(FileExists(subLevelReadOnly.get()));
    REQUIRE_FALSE(DirectoryExists(subLevel.get()));

    REQUIRE_FALSE(FileExists(topLevelReadOnly.get()));
    REQUIRE_FALSE(DirectoryExists(folderToDelete.get()));
}

#ifdef WIL_ENABLE_EXCEPTIONS
// Learn about the Win32 API normalization here: https://blogs.msdn.microsoft.com/jeremykuhne/2016/04/21/path-normalization/
// This test verifies the ability of RemoveDirectoryRecursive to be able to delete files
// that are in the non-normalized form.
TEST_CASE("FileSystemTests::VerifyRemoveDirectoryRecursiveCanDeleteFoldersWithNonNormalizedNames", "[filesystem]")
{
    // Extended length paths can access files with non-normalized names.
    // This function creates a path with that ability.
    auto CreatePathThatCanAccessNonNormalizedNames = [](PCWSTR root, PCWSTR name)
    {
        wil::unique_hlocal_string path;
        THROW_IF_FAILED(PathAllocCombine(root, name, PATHCCH_DO_NOT_NORMALIZE_SEGMENTS | PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH, &path));
        REQUIRE(wil::is_extended_length_path(path.get()));
        return path;
    };

    // Regular paths are normalized in the Win32 APIs thus can't address files in the non-normalized form.
    // This function creates a regular path form but preserves the non-normalized parts of the input (for testing)
    auto CreateRegularPath = [](PCWSTR root, PCWSTR name)
    {
        wil::unique_hlocal_string path;
        THROW_IF_FAILED(PathAllocCombine(root, name, PATHCCH_DO_NOT_NORMALIZE_SEGMENTS, &path));
        REQUIRE_FALSE(wil::is_extended_length_path(path.get()));
        return path;
    };

    struct TestCases
    {
        PCWSTR CreateWithName;
        PCWSTR DeleteWithName;
        wil::unique_hlocal_string (*CreatePathFunction)(PCWSTR root, PCWSTR name);
        HRESULT ExpectedResult;
    };

    PCWSTR NormalizedName = L"Foo";
    PCWSTR NonNormalizedName = L"Foo."; // The dot at the end is what makes this non-normalized.
    const auto PathNotFoundError = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);

    TestCases tests[] =
    {
        { NormalizedName,       NormalizedName,     CreateRegularPath, S_OK },
        { NonNormalizedName,    NormalizedName,     CreateRegularPath, PathNotFoundError },
        { NormalizedName,       NonNormalizedName,  CreateRegularPath, S_OK },
        { NonNormalizedName,    NonNormalizedName,  CreateRegularPath, PathNotFoundError },
        { NormalizedName,       NormalizedName,     CreatePathThatCanAccessNonNormalizedNames, S_OK },
        { NonNormalizedName,    NormalizedName,     CreatePathThatCanAccessNonNormalizedNames, PathNotFoundError },
        { NormalizedName,       NonNormalizedName,  CreatePathThatCanAccessNonNormalizedNames, PathNotFoundError },
        { NonNormalizedName,    NonNormalizedName,  CreatePathThatCanAccessNonNormalizedNames, S_OK },
    };

    auto folderRoot = wil::ExpandEnvironmentStringsW(LR"(%TEMP%)");
    REQUIRE_FALSE(wil::is_extended_length_path(folderRoot.get()));

    auto EnsureFolderWithNonCanonicalNameAndContentsExists = [&](const TestCases& test)
    {
        const auto enableNonNormalized = PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH | PATHCCH_DO_NOT_NORMALIZE_SEGMENTS;

        wil::unique_hlocal_string targetFolder;
        // Create a folder for testing using the extended length form to enable
        // access to non-normalized forms of the path
        THROW_IF_FAILED(PathAllocCombine(folderRoot.get(), test.CreateWithName, enableNonNormalized, &targetFolder));

        // This ensures the folder is there and won't fail if it already exists (common when testing).
        wil::CreateDirectoryDeep(targetFolder.get());

        // Create a file in that folder with a non-normalized name (with the dot at the end).
        wil::unique_hlocal_string extendedFilePath;
        THROW_IF_FAILED(PathAllocCombine(targetFolder.get(), L"NonNormalized.", enableNonNormalized, &extendedFilePath));
        wil::unique_hfile fileHandle(CreateFileW(extendedFilePath.get(), FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        THROW_LAST_ERROR_IF(!fileHandle);
    };

    for (auto const& test : tests)
    {
        // remove remnants from previous test that will cause failures
        wil::RemoveDirectoryRecursiveNoThrow(CreatePathThatCanAccessNonNormalizedNames(folderRoot.get(), NormalizedName).get());
        wil::RemoveDirectoryRecursiveNoThrow(CreatePathThatCanAccessNonNormalizedNames(folderRoot.get(), NonNormalizedName).get());

        EnsureFolderWithNonCanonicalNameAndContentsExists(test);
        auto deleteWithPath = test.CreatePathFunction(folderRoot.get(), test.DeleteWithName);

        const auto hr = wil::RemoveDirectoryRecursiveNoThrow(deleteWithPath.get());
        REQUIRE(test.ExpectedResult == hr);
    }
}
#endif

// real paths to test
const wchar_t c_variablePath[] = L"%systemdrive%\\Windows\\System32\\Windows.Storage.dll";
const wchar_t c_expandedPath[] = L"c:\\Windows\\System32\\Windows.Storage.dll";

// // paths that should not exist on the system
const wchar_t c_missingVariable[] = L"%doesnotexist%\\doesnotexist.dll";
const wchar_t c_missingPath[] = L"c:\\Windows\\System32\\doesnotexist.dll";

const int c_stackBufferLimitTest = 5;

#ifdef WIL_ENABLE_EXCEPTIONS
TEST_CASE("FileSystemTests::VerifyGetCurrentDirectory", "[filesystem]")
{
    auto pwd = wil::GetCurrentDirectoryW();
    REQUIRE(*pwd.get() != L'\0');
}

TEST_CASE("FileSystemTests::VerifyGetFullPathName", "[filesystem]")
{
    PCWSTR fileName = L"ReadMe.txt";
    auto result = wil::GetFullPathNameW<wil::unique_cotaskmem_string>(fileName, nullptr);

    PCWSTR fileNameResult;
    result = wil::GetFullPathNameW<wil::unique_cotaskmem_string>(fileName, &fileNameResult);
    REQUIRE(wcscmp(fileName, fileNameResult) == 0);
    auto result2 = wil::GetFullPathNameW<wil::unique_cotaskmem_string, c_stackBufferLimitTest>(fileName, &fileNameResult);
    REQUIRE(wcscmp(fileName, fileNameResult) == 0);
    REQUIRE(wcscmp(result.get(), result2.get()) == 0);

    // The only negative test case I've found is a path > 32k.
    std::wstring big(1024 * 32, L'a');
    wil::unique_hstring output;
    auto hr = wil::GetFullPathNameW(big.c_str(), output, nullptr);
    REQUIRE(hr == HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE));
}

TEST_CASE("FileSystemTests::VerifyGetFinalPathNameByHandle", "[filesystem]")
{
    wil::unique_hfile fileHandle(CreateFileW(c_expandedPath, FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    THROW_LAST_ERROR_IF(!fileHandle);

    auto name = wil::GetFinalPathNameByHandleW(fileHandle.get());
    auto name2 = wil::GetFinalPathNameByHandleW<wil::unique_cotaskmem_string, c_stackBufferLimitTest>(fileHandle.get());
    REQUIRE(wcscmp(name.get(), name2.get()) == 0);

    std::wstring path;
    auto hr = wil::GetFinalPathNameByHandleW(nullptr, path);
    REQUIRE(hr == E_HANDLE); // should be a usage error so be a fail fast.
                             // A more legitimate case is a non file handler like a drive volume.

    wil::unique_hfile volumeHandle(CreateFileW(LR"(\\?\C:)", FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    THROW_LAST_ERROR_IF(!volumeHandle);
    const auto hr2 = wil::GetFinalPathNameByHandleW(volumeHandle.get(), path);
    REQUIRE(hr2 == HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION));
}

TEST_CASE("FileSystemTests::VerifyTrySearchPathW", "[filesystem]")
{
    auto pathToTest = wil::TrySearchPathW(nullptr, c_expandedPath, nullptr);
    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, c_expandedPath, -1, TRUE) == CSTR_EQUAL);

    pathToTest = wil::TrySearchPathW(nullptr, c_missingPath, nullptr);
    REQUIRE(wil::string_get_not_null(pathToTest)[0] == L'\0');
}
#endif

// Simple test to expand an environmental string
TEST_CASE("FileSystemTests::VerifyExpandEnvironmentStringsW", "[filesystem]")
{
    wil::unique_cotaskmem_string pathToTest;
    REQUIRE_SUCCEEDED(wil::ExpandEnvironmentStringsW(c_variablePath, pathToTest));
    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, c_expandedPath, -1, TRUE) == CSTR_EQUAL);

    // This should effectively be a no-op
    REQUIRE_SUCCEEDED(wil::ExpandEnvironmentStringsW(c_expandedPath, pathToTest));
    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, c_expandedPath, -1, TRUE) == CSTR_EQUAL);

    // Environment variable does not exist, but the call should still succeed
    REQUIRE_SUCCEEDED(wil::ExpandEnvironmentStringsW(c_missingVariable, pathToTest));
    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, c_missingVariable, -1, TRUE) == CSTR_EQUAL);
}

TEST_CASE("FileSystemTests::VerifySearchPathW", "[filesystem]")
{
    wil::unique_cotaskmem_string pathToTest;
    REQUIRE_SUCCEEDED(wil::SearchPathW(nullptr, c_expandedPath, nullptr, pathToTest));
    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, c_expandedPath, -1, TRUE) == CSTR_EQUAL);

    REQUIRE(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == wil::SearchPathW(nullptr, c_missingPath, nullptr, pathToTest));
}

TEST_CASE("FileSystemTests::VerifyExpandEnvAndSearchPath", "[filesystem]")
{
    wil::unique_cotaskmem_string pathToTest;
    REQUIRE_SUCCEEDED(wil::ExpandEnvAndSearchPath(c_variablePath, pathToTest));
    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, c_expandedPath, -1, TRUE) == CSTR_EQUAL);

    // This test will exercise the case where AdaptFixedSizeToAllocatedResult will need to
    // reallocate the initial buffer to fit the final string.
    // This test is sufficient to test both wil::ExpandEnvironmentStringsW and wil::SeachPathW
    REQUIRE_SUCCEEDED((wil::ExpandEnvAndSearchPath<wil::unique_cotaskmem_string, c_stackBufferLimitTest>(c_variablePath, pathToTest)));
    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, c_expandedPath, -1, TRUE) == CSTR_EQUAL);

    pathToTest.reset();
    REQUIRE(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == wil::ExpandEnvAndSearchPath(c_missingVariable, pathToTest));
    REQUIRE(pathToTest.get() == nullptr);
}

TEST_CASE("FileSystemTests::VerifyGetSystemDirectoryW", "[filesystem]")
{
    wil::unique_cotaskmem_string pathToTest;
    REQUIRE_SUCCEEDED(wil::GetSystemDirectoryW(pathToTest));

    // allocate based on the string that wil::GetSystemDirectoryW returned
    size_t length = wcslen(pathToTest.get()) + 1;
    auto trueSystemDir = wil::make_cotaskmem_string_nothrow(nullptr, length);
    REQUIRE(GetSystemDirectoryW(trueSystemDir.get(), static_cast<UINT>(length)) > 0);

    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, trueSystemDir.get(), -1, TRUE) == CSTR_EQUAL);

    // Force AdaptFixed* to realloc. Test stack boundary with small initial buffer limit, c_stackBufferLimitTest
    REQUIRE_SUCCEEDED((wil::GetSystemDirectoryW<wil::unique_cotaskmem_string, c_stackBufferLimitTest>(pathToTest)));

    // allocate based on the string that wil::GetSystemDirectoryW returned
    length = wcslen(pathToTest.get()) + 1;
    trueSystemDir = wil::make_cotaskmem_string_nothrow(nullptr, length);
    REQUIRE(GetSystemDirectoryW(trueSystemDir.get(), static_cast<UINT>(length)) > 0);

    REQUIRE(CompareStringOrdinal(pathToTest.get(), -1, trueSystemDir.get(), -1, TRUE) == CSTR_EQUAL);
}

struct has_operator_pcwstr
{
    PCWSTR value;
    operator PCWSTR() const
    {
        return value;
    }
};

struct has_operator_pwstr
{
    PWSTR value;
    operator PWSTR() const
    {
        return value;
    }
};

#ifdef WIL_ENABLE_EXCEPTIONS
struct has_operator_wstr_ref
{
    std::wstring value;
    operator const std::wstring&() const
    {
        return value;
    }
};

// E.g. mimics something like std::filesystem::path
struct has_operator_wstr
{
    std::wstring value;
    operator std::wstring() const
    {
        return value;
    }
};
#endif

TEST_CASE("FileSystemTests::VerifyStrConcat", "[filesystem]")
{
    SECTION("Concat with multiple strings")
    {
        PCWSTR test1 = L"Test1";
#ifdef WIL_ENABLE_EXCEPTIONS
        std::wstring test2 = L"Test2";
#else
        PCWSTR test2 = L"Test2";
#endif
        WCHAR test3[6] = L"Test3";
        wil::unique_cotaskmem_string test4 = wil::make_unique_string_nothrow<wil::unique_cotaskmem_string>(L"test4");
        wil::unique_hstring test5 = wil::make_unique_string_nothrow<wil::unique_hstring>(L"test5");

        has_operator_pcwstr test6{ L"Test6" };
        WCHAR test7Buffer[] = L"Test7";
        has_operator_pwstr test7{ test7Buffer };

#ifdef WIL_ENABLE_EXCEPTIONS
        has_operator_wstr_ref test8{ L"Test8" };
        has_operator_wstr test9{ L"Test9" };
#else
        PCWSTR test8 = L"Test8";
        PCWSTR test9 = L"Test9";
#endif
        PCWSTR expectedStr = L"Test1Test2Test3Test4Test5Test6Test7Test8Test9";

#ifdef WIL_ENABLE_EXCEPTIONS
        auto combinedString = wil::str_concat<wil::unique_cotaskmem_string>(test1, test2, test3, test4, test5, test6, test7, test8, test9);
        REQUIRE(CompareStringOrdinal(combinedString.get(), -1, expectedStr, -1, TRUE) == CSTR_EQUAL);
#endif

        wil::unique_cotaskmem_string combinedStringNT;
        REQUIRE_SUCCEEDED(wil::str_concat_nothrow(combinedStringNT, test1, test2, test3, test4, test5, test6, test7, test8, test9));
        REQUIRE(CompareStringOrdinal(combinedStringNT.get(), -1, expectedStr, -1, TRUE) == CSTR_EQUAL);

        auto combinedStringFF = wil::str_concat_failfast<wil::unique_cotaskmem_string>(test1, test2, test3, test4, test5, test6, test7, test8, test9);
        REQUIRE(CompareStringOrdinal(combinedStringFF.get(), -1, expectedStr, -1, TRUE) == CSTR_EQUAL);
    }

    SECTION("Concat with single string")
    {
        PCWSTR test1 = L"Test1";

#ifdef WIL_ENABLE_EXCEPTIONS
        auto combinedString = wil::str_concat<wil::unique_cotaskmem_string>(test1);
        REQUIRE(CompareStringOrdinal(combinedString.get(), -1, test1, -1, TRUE) == CSTR_EQUAL);
#endif

        wil::unique_cotaskmem_string combinedStringNT;
        REQUIRE_SUCCEEDED(wil::str_concat_nothrow(combinedStringNT, test1));
        REQUIRE(CompareStringOrdinal(combinedStringNT.get(), -1, test1, -1, TRUE) == CSTR_EQUAL);

        auto combinedStringFF = wil::str_concat_failfast<wil::unique_cotaskmem_string>(test1);
        REQUIRE(CompareStringOrdinal(combinedStringFF.get(), -1, test1, -1, TRUE) == CSTR_EQUAL);
    }

    SECTION("Concat with existing string")
    {
        std::wstring test2 = L"Test2";
        WCHAR test3[6] = L"Test3";
        PCWSTR expectedStr = L"Test1Test2Test3";

        wil::unique_cotaskmem_string combinedStringNT = wil::make_unique_string_nothrow<wil::unique_cotaskmem_string>(L"Test1");
        REQUIRE_SUCCEEDED(wil::str_concat_nothrow(combinedStringNT, test2.c_str(), test3));
        REQUIRE(CompareStringOrdinal(combinedStringNT.get(), -1, expectedStr, -1, TRUE) == CSTR_EQUAL);
    }
}

TEST_CASE("FileSystemTests::VerifyStrPrintf", "[filesystem]")
{
#ifdef WIL_ENABLE_EXCEPTIONS
    auto formattedString = wil::str_printf<wil::unique_cotaskmem_string>(L"Test %s %c %d %4.2f", L"String", L'c', 42, 6.28);
    REQUIRE(CompareStringOrdinal(formattedString.get(), -1, L"Test String c 42 6.28", -1, TRUE) == CSTR_EQUAL);
#endif

    wil::unique_cotaskmem_string formattedStringNT;
    REQUIRE_SUCCEEDED(wil::str_printf_nothrow(formattedStringNT, L"Test %s %c %d %4.2f", L"String", L'c', 42, 6.28));
    REQUIRE(CompareStringOrdinal(formattedStringNT.get(), -1, L"Test String c 42 6.28", -1, TRUE) == CSTR_EQUAL);

    auto formattedStringFF = wil::str_printf_failfast<wil::unique_cotaskmem_string>(L"Test %s %c %d %4.2f", L"String", L'c', 42, 6.28);
    REQUIRE(CompareStringOrdinal(formattedStringFF.get(), -1, L"Test String c 42 6.28", -1, TRUE) == CSTR_EQUAL);
}

TEST_CASE("FileSystemTests::VerifyGetModuleFileNameW", "[filesystem]")
{
    wil::unique_cotaskmem_string path;
    REQUIRE_SUCCEEDED(wil::GetModuleFileNameW(nullptr, path));
    auto len = wcslen(path.get());
    REQUIRE(((len >= 4) && (wcscmp(path.get() + len - 4, L".exe") == 0)));

    // Call again, but force multiple retries through a small initial buffer
    wil::unique_cotaskmem_string path2;
    REQUIRE_SUCCEEDED((wil::GetModuleFileNameW<wil::unique_cotaskmem_string, 4>(nullptr, path2)));
    REQUIRE(wcscmp(path.get(), path2.get()) == 0);

    REQUIRE_FAILED(wil::GetModuleFileNameW((HMODULE)INVALID_HANDLE_VALUE, path));

#ifdef WIL_ENABLE_EXCEPTIONS
    auto wstringPath = wil::GetModuleFileNameW<std::wstring, 15>(nullptr);
    REQUIRE(wstringPath.length() == ::wcslen(wstringPath.c_str()));
#endif
}

#ifdef WIL_ENABLE_EXCEPTIONS
wil::unique_cotaskmem_string NativeGetModuleFileNameWrap(HANDLE processHandle, HMODULE moduleHandle)
{
    DWORD size = MAX_PATH * 4;
    auto path = wil::make_cotaskmem_string_nothrow(nullptr, size);

    DWORD copied = processHandle ?
        ::GetModuleFileNameExW(processHandle, moduleHandle, path.get(), size) :
        ::GetModuleFileNameW(moduleHandle, path.get(), size);
    REQUIRE(copied < size);

    return path;
}
#endif

TEST_CASE("FileSystemTests::VerifyGetModuleFileNameExW", "[filesystem]")
{
    wil::unique_cotaskmem_string path;
    REQUIRE_SUCCEEDED(wil::GetModuleFileNameExW(nullptr, nullptr, path));
    auto len = wcslen(path.get());
    REQUIRE(((len >= 4) && (wcscmp(path.get() + len - 4, L".exe") == 0)));

    // Call again, but force multiple retries through a small initial buffer
    wil::unique_cotaskmem_string path2;
    REQUIRE_SUCCEEDED((wil::GetModuleFileNameExW<wil::unique_cotaskmem_string, 4>(nullptr, nullptr, path2)));
    REQUIRE(wcscmp(path.get(), path2.get()) == 0);

    REQUIRE_FAILED(wil::GetModuleFileNameExW(nullptr, (HMODULE)INVALID_HANDLE_VALUE, path));

#ifdef WIL_ENABLE_EXCEPTIONS
    auto wstringPath = wil::GetModuleFileNameExW<std::wstring, 15>(nullptr, nullptr);
    REQUIRE(wstringPath.length() == ::wcslen(wstringPath.c_str()));
    REQUIRE(wstringPath == NativeGetModuleFileNameWrap(nullptr, nullptr).get());

    wstringPath = wil::GetModuleFileNameExW<std::wstring, 15>(GetCurrentProcess(), nullptr);
    REQUIRE(wstringPath.length() == ::wcslen(wstringPath.c_str()));
    REQUIRE(wstringPath == NativeGetModuleFileNameWrap(GetCurrentProcess(), nullptr).get());

    wstringPath = wil::GetModuleFileNameW<std::wstring, 15>(nullptr);
    REQUIRE(wstringPath.length() == ::wcslen(wstringPath.c_str()));
    REQUIRE(wstringPath == NativeGetModuleFileNameWrap(nullptr, nullptr).get());

    HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");

    wstringPath = wil::GetModuleFileNameExW<std::wstring, 15>(nullptr, kernel32);
    REQUIRE(wstringPath.length() == ::wcslen(wstringPath.c_str()));
    REQUIRE(wstringPath == NativeGetModuleFileNameWrap(nullptr, kernel32).get());

    wstringPath = wil::GetModuleFileNameExW<std::wstring, 15>(GetCurrentProcess(), kernel32);
    REQUIRE(wstringPath.length() == ::wcslen(wstringPath.c_str()));
    REQUIRE(wstringPath == NativeGetModuleFileNameWrap(GetCurrentProcess(), kernel32).get());

    wstringPath = wil::GetModuleFileNameW<std::wstring, 15>(kernel32);
    REQUIRE(wstringPath.length() == ::wcslen(wstringPath.c_str()));
    REQUIRE(wstringPath == NativeGetModuleFileNameWrap(nullptr, kernel32).get());
#endif
}

TEST_CASE("FileSystemTests::QueryFullProcessImageNameW and GetModuleFileNameW", "[filesystem]")
{
#ifdef WIL_ENABLE_EXCEPTIONS
    auto procName = wil::QueryFullProcessImageNameW<std::wstring>();
    auto moduleName = wil::GetModuleFileNameW<std::wstring>();
    REQUIRE(procName == moduleName);
#endif
}

TEST_CASE("FileSystemTests::QueryFullProcessImageNameW", "[filesystem]")
{
    WCHAR fullName[MAX_PATH * 4];
    DWORD fullNameSize = ARRAYSIZE(fullName);
    REQUIRE(::QueryFullProcessImageNameW(::GetCurrentProcess(), 0, fullName, &fullNameSize));

    wil::unique_cotaskmem_string path;
    REQUIRE_SUCCEEDED(wil::QueryFullProcessImageNameW(::GetCurrentProcess(), 0, path));
    REQUIRE(wcscmp(fullName, path.get()) == 0);

    wil::unique_cotaskmem nativePath;
    REQUIRE_SUCCEEDED((wil::QueryFullProcessImageNameW<wil::unique_cotaskmem_string, 15>(::GetCurrentProcess(), 0, path)));
}


#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
