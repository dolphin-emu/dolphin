
#include <wil/filesystem.h>
#include <wil/registry.h>
#include <wil/resource.h>

#include <memory> // For shared_event_watcher
#include <wil/resource.h>

#include "common.h"

TEST_CASE("EventWatcherTests::Construction", "[resource][event_watcher]")
{
    SECTION("Create unique_event_watcher_nothrow without event")
    {
        auto watcher = wil::make_event_watcher_nothrow([]{});
        REQUIRE(watcher != nullptr);
    }

    SECTION("Create unique_event_watcher_nothrow with unique_event_nothrow")
    {
        wil::unique_event_nothrow eventToPass;
        FAIL_FAST_IF_FAILED(eventToPass.create(wil::EventOptions::None));
        auto watcher = wil::make_event_watcher_nothrow(wistd::move(eventToPass), []{});
        REQUIRE(watcher != nullptr);
        REQUIRE(eventToPass.get() == nullptr); // move construction must take it
    }

    SECTION("Create unique_event_watcher_nothrow with handle")
    {
        wil::unique_event_nothrow eventToDupe;
        FAIL_FAST_IF_FAILED(eventToDupe.create(wil::EventOptions::None));
        auto watcher = wil::make_event_watcher_nothrow(eventToDupe.get(), []{});
        REQUIRE(watcher != nullptr);
        REQUIRE(eventToDupe.get() != nullptr); // handle duped in this case
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Create unique_event_watcher_nothrow with unique_event")
    {
        wil::unique_event eventToPass(wil::EventOptions::None);
        auto watcher = wil::make_event_watcher_nothrow(wistd::move(eventToPass), []{});
        REQUIRE(watcher != nullptr);
        REQUIRE(eventToPass.get() == nullptr); // move construction must take it
    }

    SECTION("Create unique_event_watcher without event")
    {
        auto watcher = wil::make_event_watcher([]{});
    }

    SECTION("Create unique_event_watcher with unique_event_nothrow")
    {
        wil::unique_event_nothrow eventToPass;
        THROW_IF_FAILED(eventToPass.create(wil::EventOptions::None));
        auto watcher = wil::make_event_watcher(wistd::move(eventToPass), []{});
        REQUIRE(eventToPass.get() == nullptr); // move construction must take it
    }

    SECTION("Create unique_event_watcher with unique_event")
    {
        wil::unique_event eventToPass(wil::EventOptions::None);
        auto watcher = wil::make_event_watcher(wistd::move(eventToPass), []{});
        REQUIRE(eventToPass.get() == nullptr); // move construction must take it
    }

    SECTION("Create unique_event_watcher with handle")
    {
        wil::unique_event eventToDupe(wil::EventOptions::None);
        auto watcher = wil::make_event_watcher(eventToDupe.get(), []{});
        REQUIRE(eventToDupe.get() != nullptr); // handle duped in this case
    }

    SECTION("Create unique_event_watcher shared watcher")
    {
        wil::shared_event_watcher sharedWatcher = wil::make_event_watcher([]{});
    }
#endif
}

static auto make_event(wil::EventOptions options = wil::EventOptions::None)
{
    wil::unique_event_nothrow result;
    FAIL_FAST_IF_FAILED(result.create(options));
    return result;
}

TEST_CASE("EventWatcherTests::VerifyDelivery", "[resource][event_watcher]")
{
    auto notificationReceived = make_event();

    int volatile countObserved = 0;
    auto watcher = wil::make_event_watcher_nothrow([&]
    {
        countObserved = countObserved + 1;
        notificationReceived.SetEvent();
    });
    REQUIRE(watcher != nullptr);

    watcher.SetEvent();
    REQUIRE(notificationReceived.wait(5000)); // 5 second max wait

    watcher.SetEvent();
    REQUIRE(notificationReceived.wait(5000)); // 5 second max wait
    REQUIRE(countObserved == 2);
}

TEST_CASE("EventWatcherTests::VerifyLastChangeObserved", "[resource][event_watcher]")
{
    wil::EventOptions const eventOptions[] =
    {
        wil::EventOptions::None,
        wil::EventOptions::ManualReset,
        wil::EventOptions::Signaled,
        wil::EventOptions::ManualReset | wil::EventOptions::Signaled,
    };

    for (auto const &eventOption : eventOptions)
    {
        auto allChangesMade = make_event(wil::EventOptions::ManualReset); // ManualReset to avoid hang in case where 2 callbacks are generated (a test failure).
        auto processedChange = make_event();

        DWORD volatile stateToObserve = 0;
        DWORD volatile lastObservedState = 0;
        int volatile countObserved = 0;
        auto watcher = wil::make_event_watcher_nothrow(make_event(eventOption), [&]
        {
            allChangesMade.wait();
            countObserved = countObserved + 1;
            lastObservedState = stateToObserve;
            processedChange.SetEvent();
        });
        REQUIRE(watcher != nullptr);

        stateToObserve = 1;
        watcher.SetEvent();
        stateToObserve = 2;
        watcher.SetEvent();

        allChangesMade.SetEvent();
        REQUIRE(processedChange.wait(5000));

        REQUIRE((countObserved == 1 || countObserved == 2)); // ensure the race worked how we wanted it to
        REQUIRE(lastObservedState == stateToObserve);
    }
}

#define ROOT_KEY_PAIR HKEY_CURRENT_USER, L"Software\\Microsoft\\RegistryWatcherTest"

TEST_CASE("RegistryWatcherTests::Construction", "[registry][registry_watcher]")
{
    SECTION("Create unique_registry_watcher_nothrow with string")
    {
        auto watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, true, [&](wil::RegistryChangeKind){});
        REQUIRE(watcher);
    }

    SECTION("Create unique_registry_watcher_nothrow with unique_hkey")
    {
        wil::unique_hkey keyToMove;
        REQUIRE_SUCCEEDED(HRESULT_FROM_WIN32(RegCreateKeyExW(ROOT_KEY_PAIR, 0, nullptr, 0, KEY_NOTIFY, nullptr, &keyToMove, nullptr)));

        auto watcher = wil::make_registry_watcher_nothrow(wistd::move(keyToMove), true, [&](wil::RegistryChangeKind){});
        REQUIRE(watcher);
        REQUIRE(keyToMove.get() == nullptr); // ownership is transferred
    }

    SECTION("Create unique_registry_watcher_nothrow with handle")
    {
        // construct with just an open registry key
        wil::unique_hkey rootKey;
        REQUIRE_SUCCEEDED(HRESULT_FROM_WIN32(RegCreateKeyExW(ROOT_KEY_PAIR, 0, nullptr, 0, KEY_NOTIFY, nullptr, &rootKey, nullptr)));

        auto watcher = wil::make_registry_watcher_nothrow(rootKey.get(), L"", true, [&](wil::RegistryChangeKind){});
        REQUIRE(watcher);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Create unique_registry_watcher with string")
    {
        REQUIRE_NOTHROW(wil::make_registry_watcher(ROOT_KEY_PAIR, true, [&](wil::RegistryChangeKind){}));
    }

    SECTION("Create unique_registry_watcher with unique_hkey")
    {
        wil::unique_hkey keyToMove;
        THROW_IF_FAILED(HRESULT_FROM_WIN32(RegCreateKeyExW(ROOT_KEY_PAIR, 0, nullptr, 0, KEY_NOTIFY, nullptr, &keyToMove, nullptr)));

        REQUIRE_NOTHROW(wil::make_registry_watcher(wistd::move(keyToMove), true, [&](wil::RegistryChangeKind){}));
        REQUIRE(keyToMove.get() == nullptr); // ownership is transferred
    }
#endif
}

void SetRegistryValue(
    _In_ HKEY hKey,
    _In_opt_ LPCWSTR lpSubKey,
    _In_opt_ LPCWSTR lpValueName,
    _In_ DWORD dwType,
    _In_reads_bytes_opt_(cbData) LPCVOID lpData,
    _In_ DWORD cbData)
{
    wil::unique_hkey key;
    REQUIRE(RegOpenKeyExW(hKey, lpSubKey, 0, KEY_WRITE, &key) == ERROR_SUCCESS);
    REQUIRE(RegSetValueExW(key.get(), lpValueName, 0, dwType, static_cast<BYTE const*>(lpData), cbData) == ERROR_SUCCESS);
}

TEST_CASE("RegistryWatcherTests::VerifyDelivery", "[registry][registry_watcher]")
{
    RegDeleteTreeW(ROOT_KEY_PAIR); // So that we get the 'Modify' event
    auto notificationReceived = make_event();

    int volatile countObserved = 0;
    auto volatile observedChangeType = wil::RegistryChangeKind::Delete;
    auto watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, true, [&](wil::RegistryChangeKind changeType)
    {
        countObserved = countObserved + 1;
        observedChangeType = changeType;
        notificationReceived.SetEvent();
    });
    REQUIRE(watcher);

    DWORD value = 1;
    SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));
    REQUIRE(notificationReceived.wait(5000));
    REQUIRE(observedChangeType == wil::RegistryChangeKind::Modify);

    value++;
    SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));
    REQUIRE(notificationReceived.wait(5000));
    REQUIRE(countObserved == 2);
    REQUIRE(observedChangeType == wil::RegistryChangeKind::Modify);
}

TEST_CASE("RegistryWatcherTests::VerifyLastChangeObserved", "[registry][registry_watcher]")
{
    RegDeleteTreeW(ROOT_KEY_PAIR);
    auto allChangesMade = make_event(wil::EventOptions::ManualReset); // ManualReset for the case where both registry operations result in a callback.
    auto processedChange = make_event();

    DWORD volatile stateToObserve = 0;
    DWORD volatile lastObservedState = 0;
    DWORD volatile lastObservedValue = 0;
    int volatile countObserved = 0;
    auto watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, true, [&, called = false](wil::RegistryChangeKind) mutable
    {
        // This callback may be called more than once (since we modify the key twice), but we're holding references to
        // local variables. Therefore, bail out if this is not the first time we're called
        if (called)
        {
            return;
        }
        called = true;

        allChangesMade.wait();
        countObserved = countObserved + 1;
        lastObservedState = stateToObserve;
        DWORD value, cbValue = sizeof(value);
        RegGetValueW(ROOT_KEY_PAIR, L"value", RRF_RT_REG_DWORD, nullptr, &value, &cbValue);
        lastObservedValue = value;
        processedChange.SetEvent();
    });
    REQUIRE(watcher);

    DWORD value;
    // make 2 changes and verify that only the last gets observed
    stateToObserve = 1;
    value = 0;
    SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));

    stateToObserve = 2;
    value = 1;
    SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));

    allChangesMade.SetEvent();
    REQUIRE(processedChange.wait(5000));

    REQUIRE(countObserved >= 1); // Sometimes 2 events are observed, see if this can be eliminated.
    REQUIRE(lastObservedState == stateToObserve);
    REQUIRE(lastObservedValue == 1);
}

TEST_CASE("RegistryWatcherTests::VerifyDeleteBehavior", "[registry][registry_watcher]")
{
    auto notificationReceived = make_event();

    int volatile countObserved = 0;
    auto volatile observedChangeType = wil::RegistryChangeKind::Modify;
    auto watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, true, [&](wil::RegistryChangeKind changeType)
    {
        countObserved = countObserved + 1;
        observedChangeType = changeType;
        notificationReceived.SetEvent();
    });
    REQUIRE(watcher);

    RegDeleteTreeW(ROOT_KEY_PAIR); // delete the key to signal the watcher with the special error case
    REQUIRE(notificationReceived.wait(5000));
    REQUIRE(countObserved == 1);
    REQUIRE(observedChangeType == wil::RegistryChangeKind::Delete);
}

TEST_CASE("RegistryWatcherTests::VerifyResetInCallback", "[registry][registry_watcher]")
{
    auto notificationReceived = make_event();

    wil::unique_registry_watcher_nothrow watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, TRUE, [&](wil::RegistryChangeKind)
    {
        watcher.reset();
        DWORD value = 2;
        SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));
        notificationReceived.SetEvent();
    });
    REQUIRE(watcher);

    DWORD value = 1;
    SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));
    REQUIRE(notificationReceived.wait(5000));
}

// Stress test, disabled by default
TEST_CASE("RegistryWatcherTests::VerifyResetInCallbackStress", "[LocalOnly][registry][registry_watcher][stress]")
{
    for (DWORD value = 0; value < 10000; ++value)
    {
        wil::srwlock lock;
        auto notificationReceived = make_event();

        wil::unique_registry_watcher_nothrow watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, TRUE, [&](wil::RegistryChangeKind)
        {
            {
                auto al = lock.lock_exclusive();
                watcher.reset(); // get m_refCount to 1 to ensure the Release happens on the background thread
            }
            ++value;
            SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));
            notificationReceived.SetEvent();
        });
        REQUIRE(watcher);

        SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));
        notificationReceived.wait();

        {
            auto al = lock.lock_exclusive();
            watcher.reset();
        }
    }
}

TEST_CASE("RegistryWatcherTests::VerifyResetAfterDelete", "[registry][registry_watcher]")
{
    auto notificationReceived = make_event();

    int volatile countObserved = 0;
    auto volatile observedChangeType = wil::RegistryChangeKind::Modify;
    wil::unique_registry_watcher_nothrow watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, true, [&](wil::RegistryChangeKind changeType)
    {
        countObserved = countObserved + 1;
        observedChangeType = changeType;
        notificationReceived.SetEvent();
        watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, true, [&](wil::RegistryChangeKind changeType)
        {
            countObserved = countObserved + 1;
            observedChangeType = changeType;
            notificationReceived.SetEvent();
        });
        REQUIRE(watcher);
    });
    REQUIRE(watcher);

    RegDeleteTreeW(ROOT_KEY_PAIR); // delete the key to signal the watcher with the special error case
    notificationReceived.wait();
    REQUIRE(countObserved == 1);
    REQUIRE(observedChangeType == wil::RegistryChangeKind::Delete);

    // wait for the reset to finish. The constructor creates the registry key
    notificationReceived.wait(300);
    DWORD value = 1;
    SetRegistryValue(ROOT_KEY_PAIR, L"value", REG_DWORD, &value, sizeof(value));

    notificationReceived.wait();
    REQUIRE(countObserved == 2);
    REQUIRE(observedChangeType == wil::RegistryChangeKind::Modify);
}

TEST_CASE("RegistryWatcherTests::VerifyCallbackFinishesBeforeFreed", "[registry][registry_watcher]")
{
    auto notificationReceived = make_event();
    auto deleteNotification = make_event();

    int volatile deleteObserved = 0;
    auto watcher = wil::make_registry_watcher_nothrow(ROOT_KEY_PAIR, true, [&](wil::RegistryChangeKind)
    {
        notificationReceived.SetEvent();
        // ensure that the callback is still being executed while the watcher is reset().
        deleteNotification.wait(200);
        deleteObserved = deleteObserved + 1;
        notificationReceived.SetEvent();
    });

    RegDeleteTreeW(ROOT_KEY_PAIR); // delete the key to signal the watcher with the special error case
    REQUIRE(notificationReceived.wait(5000));

    watcher.reset();
    deleteNotification.SetEvent();
    REQUIRE(notificationReceived.wait(5000));
    REQUIRE(deleteObserved == 1);
}

TEST_CASE("FileSystemWatcherTests::Construction", "[resource][folder_watcher]")
{
    SECTION("Create unique_folder_watcher_nothrow with valid path")
    {
        auto watcher = wil::make_folder_watcher_nothrow(L"C:\\Windows\\System32", true, wil::FolderChangeEvents::All, []{});
        REQUIRE(watcher);
    }

    SECTION("Create unique_folder_watcher_nothrow with invalid path")
    {
        auto watcher = wil::make_folder_watcher_nothrow(L"X:\\invalid path", true, wil::FolderChangeEvents::All, []{});
        REQUIRE(!watcher);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Create unique_folder_watcher with valid path")
    {
        REQUIRE_NOTHROW(wil::make_folder_watcher(L"C:\\Windows\\System32", true, wil::FolderChangeEvents::All, []{}));
    }

    SECTION("Create unique_folder_watcher with invalid path")
    {
        REQUIRE_THROWS(wil::make_folder_watcher(L"X:\\invalid path", true, wil::FolderChangeEvents::All, []{}));
    }
#endif
}

TEST_CASE("FileSystemWatcherTests::VerifyDelivery", "[resource][folder_watcher]")
{
    witest::TestFolder folder;
    REQUIRE(folder);

    auto notificationEvent = make_event();
    int observedCount = 0;
    auto watcher = wil::make_folder_watcher_nothrow(folder.Path(), true, wil::FolderChangeEvents::All, [&]
    {
        ++observedCount;
        notificationEvent.SetEvent();
    });
    REQUIRE(watcher);

    witest::TestFile file(folder.Path(), L"file.txt");
    REQUIRE(file);
    REQUIRE(notificationEvent.wait(5000));
    REQUIRE(observedCount == 1);

    witest::TestFile file2(folder.Path(), L"file2.txt");
    REQUIRE(file2);
    REQUIRE(notificationEvent.wait(5000));
    REQUIRE(observedCount == 2);
}

TEST_CASE("FolderChangeReaderTests::Construction", "[resource][folder_change_reader]")
{
    SECTION("Create folder_change_reader_nothrow with valid path")
    {
        auto reader = wil::make_folder_change_reader_nothrow(L"C:\\Windows\\System32", true, wil::FolderChangeEvents::All, [](auto, auto) {});
        REQUIRE(reader);
    }

    SECTION("Create folder_change_reader_nothrow with invalid path")
    {
        auto reader = wil::make_folder_change_reader_nothrow(L"X:\\invalid path", true, wil::FolderChangeEvents::All, [](auto, auto) {});
        REQUIRE(!reader);
    }

#ifdef WIL_ENABLE_EXCEPTIONS
    SECTION("Create folder_change_reader with valid path")
    {
        REQUIRE_NOTHROW(wil::make_folder_change_reader(L"C:\\Windows\\System32", true, wil::FolderChangeEvents::All, [](auto, auto) {}));
    }

    SECTION("Create folder_change_reader with invalid path")
    {
        REQUIRE_THROWS(wil::make_folder_change_reader(L"X:\\invalid path", true, wil::FolderChangeEvents::All, [](auto, auto) {}));
    }
#endif
}

TEST_CASE("FolderChangeReaderTests::VerifyDelivery", "[resource][folder_change_reader]")
{
    witest::TestFolder folder;
    REQUIRE(folder);

    auto notificationEvent = make_event();
    wil::FolderChangeEvent observedEvent;
    wchar_t observedFileName[MAX_PATH] = L"";
    auto reader = wil::make_folder_change_reader_nothrow(folder.Path(), true, wil::FolderChangeEvents::All,
        [&](wil::FolderChangeEvent event, PCWSTR fileName)
    {
        observedEvent = event;
        StringCchCopyW(observedFileName, ARRAYSIZE(observedFileName), fileName);
        notificationEvent.SetEvent();
    });
    REQUIRE(reader);

    witest::TestFile testFile(folder.Path(), L"file.txt");
    REQUIRE(testFile);
    REQUIRE(notificationEvent.wait(5000));
    REQUIRE(observedEvent == wil::FolderChangeEvent::Added);
    REQUIRE(wcscmp(observedFileName, L"file.txt") == 0);

    witest::TestFile testFile2(folder.Path(), L"file2.txt");
    REQUIRE(testFile2);
    REQUIRE(notificationEvent.wait(5000));
    REQUIRE(observedEvent == wil::FolderChangeEvent::Added);
    REQUIRE(wcscmp(observedFileName, L"file2.txt") == 0);
}
