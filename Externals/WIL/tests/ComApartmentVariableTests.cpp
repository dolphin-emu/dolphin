#include <wil/com_apartment_variable.h>
#include <wil/com.h>
#include <functional>

#include "common.h"

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

template <typename... args_t>
inline void LogOutput(_Printf_format_string_ PCWSTR format, args_t&&... args)
{
    OutputDebugStringW(wil::str_printf_failfast<wil::unique_cotaskmem_string>(format, wistd::forward<args_t>(args)...).get());
}

inline bool IsComInitialized()
{
    APTTYPE type{}; APTTYPEQUALIFIER qualifier{};
    return CoGetApartmentType(&type, &qualifier) == S_OK;
}

inline void WaitForAllComApartmentsToRundown()
{
    while (IsComInitialized())
    {
        Sleep(0);
    }
}

void co_wait(const wil::unique_event& e)
{
    HANDLE raw[] = { e.get() };
    ULONG index{};
    REQUIRE_SUCCEEDED(CoWaitForMultipleHandles(COWAIT_DISPATCH_CALLS, INFINITE, static_cast<ULONG>(std::size(raw)), raw, &index));
}

void RunApartmentVariableTest(void(*test)())
{
    test();
    // Apartment variable rundown is async, wait for the last COM apartment
    // to rundown before proceeding to the next test.
    WaitForAllComApartmentsToRundown();
}

struct mock_platform
{
    static unsigned long long GetApartmentId()
    {
        APTTYPE type; APTTYPEQUALIFIER qualifer;
        REQUIRE_SUCCEEDED(CoGetApartmentType(&type, &qualifer)); // ensure COM is inited

        // Approximate apartment Id
        if (type == APTTYPE_STA)
        {
            REQUIRE_FALSE(GetCurrentThreadId() < APTTYPE_MAINSTA);
            return GetCurrentThreadId();
        }
        else
        {
            // APTTYPE_MTA (1), APTTYPE_NA (2), APTTYPE_MAINSTA (3)
            return type;
        }
    }

    static auto RegisterForApartmentShutdown(IApartmentShutdown* observer)
    {
        const auto id = GetApartmentId();
        auto apt_observers = m_observers.find(id);
        if (apt_observers == m_observers.end())
        {
            m_observers.insert({ id, { observer} });
        }
        else
        {
            apt_observers->second.emplace_back(observer);
        }
        return shutdown_type{ reinterpret_cast<APARTMENT_SHUTDOWN_REGISTRATION_COOKIE>(id) };
    }

    static void UnRegisterForApartmentShutdown(APARTMENT_SHUTDOWN_REGISTRATION_COOKIE cookie)
    {
        auto id = reinterpret_cast<unsigned long long>(cookie);
        m_observers.erase(id);
    }

    using shutdown_type = wil::unique_any<APARTMENT_SHUTDOWN_REGISTRATION_COOKIE, decltype(&UnRegisterForApartmentShutdown), UnRegisterForApartmentShutdown>;

    // This is needed to simulate the platform for unit testing.
    static auto CoInitializeEx(DWORD coinitFlags = 0 /*COINIT_MULTITHREADED*/)
    {
        return wil::scope_exit([aptId = GetCurrentThreadId(), init = wil::CoInitializeEx(coinitFlags)]()
        {
            const auto id = GetApartmentId();
            auto apt_observers = m_observers.find(id);
            if (apt_observers != m_observers.end())
            {
                const auto& observers = apt_observers->second;
                for (auto& observer : observers)
                {
                    observer->OnUninitialize(id);
                }
                m_observers.erase(apt_observers);
            }
        });
    }

    // Enable the test hook to force losing the race
    inline static constexpr unsigned long AsyncRundownDelayForTestingRaces = 1; // enable test hook
    inline static std::unordered_map<unsigned long long, std::vector<wil::com_ptr<IApartmentShutdown>>> m_observers;
};

auto fn() { return 42; };
auto fn2() { return 43; };

wil::apartment_variable<int, wil::apartment_variable_leak_action::ignore, mock_platform> g_v1;
wil::apartment_variable<int, wil::apartment_variable_leak_action::ignore> g_v2;

template <typename platform = wil::apartment_variable_platform>
void TestApartmentVariableAllMethods()
{
    auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

    std::ignore = g_v1.get_or_create(fn);

    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> v1;

    REQUIRE(v1.get_if() == nullptr);
    REQUIRE(v1.get_or_create(fn) == 42);
    int value = 43;
    v1.set(value);
    REQUIRE(v1.get_or_create(fn) == 43);
    REQUIRE(v1.get_existing() == 43);
    v1.clear();
    REQUIRE(v1.get_if() == nullptr);
}

template <typename platform = wil::apartment_variable_platform>
void TestApartmentVariableGetOrCreateForms()
{
    auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> v1;
    REQUIRE(v1.get_or_create(fn) == 42);
    v1.clear();
    REQUIRE(v1.get_or_create([&]
    {
        return 1;
    }) == 1);
    v1.clear();
    REQUIRE(v1.get_or_create() == 0);
}

template <typename platform = wil::apartment_variable_platform>
void TestApartmentVariableLifetimes()
{
    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> av1, av2;

    {
        auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

        auto v1 = av1.get_or_create(fn);
        REQUIRE(av1.storage().size() == 1);
        auto v2 = av1.get_existing();
        REQUIRE(av1.current_apartment_variable_count() == 1);
        REQUIRE(v1 == v2);
    }

    {
        auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);
        auto v1 = av1.get_or_create(fn);
        auto v2 = av2.get_or_create(fn2);
        REQUIRE((av1.current_apartment_variable_count() == 2));
        REQUIRE(v1 != v2);
        REQUIRE(av1.storage().size() == 1);
    }

    REQUIRE(av1.storage().size() == 0);

    {
        auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

        auto v = av1.get_or_create(fn);
        REQUIRE(av1.current_apartment_variable_count() == 1);

        std::thread([&]() // join below makes this ok
        {
            SetThreadDescription(GetCurrentThread(), L"STA");
            auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
            std::ignore = av1.get_or_create(fn);
            REQUIRE(av1.storage().size() == 2);
            REQUIRE(av1.current_apartment_variable_count() == 1);
        }).join();
        REQUIRE(av1.storage().size() == 1);

        av1.get_or_create(fn)++;
        v = av1.get_existing();
        REQUIRE(v == 43);
    }

    {
        auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

        std::ignore = av1.get_or_create(fn);
        REQUIRE(av1.current_apartment_variable_count() == 1);
        int i = 1;
        av1.set(i);
        av1.clear();
        REQUIRE(av1.current_apartment_variable_count() == 0);

        // will fail fast since clear() was called.
        // av1.set(1);
        av1.clear_all_apartments_async().get();
    }

    REQUIRE(av1.storage().size() == 0);
}

template <typename platform = wil::apartment_variable_platform>
void TestMultipleApartments()
{
    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> av1, av2;

    wil::unique_event t1Created{ wil::EventOptions::None }, t2Created{ wil::EventOptions::None };
    wil::unique_event t1Shutdown{ wil::EventOptions::None }, t2Shutdown{ wil::EventOptions::None };

    auto apt1_thread = std::thread([&]() // join below makes this ok
    {
        SetThreadDescription(GetCurrentThread(), L"STA 1");
        auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
        std::ignore = av1.get_or_create(fn);
        std::ignore = av2.get_or_create(fn);
        t1Created.SetEvent();
        co_wait(t1Shutdown);
    });

    auto apt2_thread = std::thread([&]() // join below makes this ok
    {
        SetThreadDescription(GetCurrentThread(), L"STA 2");
        auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
        std::ignore = av1.get_or_create(fn);
        std::ignore = av2.get_or_create(fn);
        t2Created.SetEvent();
        co_wait(t2Shutdown);
    });

    t1Created.wait();
    t2Created.wait();
    av1.clear_all_apartments_async().get();
    av2.clear_all_apartments_async().get();

    t1Shutdown.SetEvent();
    t2Shutdown.SetEvent();

    apt1_thread.join();
    apt2_thread.join();

    REQUIRE((wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform>::storage().size() == 0));
}

template <typename platform = wil::apartment_variable_platform>
void TestWinningApartmentAlreadyRundownRace()
{
    auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> av;

    std::ignore = av.get_or_create(fn);
    const auto& storage = av.storage(); // for viewing the storage in the debugger

    wil::unique_event otherAptVarCreated{ wil::EventOptions::None };
    wil::unique_event startApartmentRundown{ wil::EventOptions::None };
    wil::unique_event comRundownComplete{ wil::EventOptions::None };

    auto apt_thread = std::thread([&]() // join below makes this ok
    {
        SetThreadDescription(GetCurrentThread(), L"STA");
        auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
        std::ignore = av.get_or_create(fn);
        otherAptVarCreated.SetEvent();
        co_wait(startApartmentRundown);
    });

    otherAptVarCreated.wait();
    // we now have av in this apartment and in the STA
    REQUIRE(storage.size() == 2);
    // wait for async clean to complete
    av.clear_all_apartments_async().get();
    startApartmentRundown.SetEvent();

    REQUIRE(av.storage().size() == 0);
    apt_thread.join();
}

template <typename platform = wil::apartment_variable_platform>
void TestLosingApartmentAlreadyRundownRace()
{
    auto coUninit = platform::CoInitializeEx(COINIT_MULTITHREADED);

    wil::apartment_variable<int, wil::apartment_variable_leak_action::fail_fast, platform> av;

    std::ignore = av.get_or_create(fn);
    const auto& storage = av.storage(); // for viewing the storage in the debugger

    wil::unique_event otherAptVarCreated{ wil::EventOptions::None };
    wil::unique_event startApartmentRundown{ wil::EventOptions::None };
    wil::unique_event comRundownComplete{ wil::EventOptions::None };

    auto apt_thread = std::thread([&]() // join below makes this ok
    {
        SetThreadDescription(GetCurrentThread(), L"STA");
        auto coUninit = platform::CoInitializeEx(COINIT_APARTMENTTHREADED);
        std::ignore = av.get_or_create(fn);
        otherAptVarCreated.SetEvent();
        co_wait(startApartmentRundown);
        coUninit.reset();
        comRundownComplete.SetEvent();
    });

    otherAptVarCreated.wait();
    // we now have av in this apartment and in the STA
    REQUIRE(storage.size() == 2);
    auto clearAllOperation = av.clear_all_apartments_async();
    startApartmentRundown.SetEvent();
    comRundownComplete.wait();
    clearAllOperation.get(); // wait for the async rundowns to complete

    REQUIRE(av.storage().size() == 0);
    apt_thread.join();
}

TEST_CASE("ComApartmentVariable::ShutdownRegistration", "[LocalOnly][com][unique_apartment_shutdown_registration]")
{
    {
        wil::unique_apartment_shutdown_registration r;
    }

    {
        auto coUninit = wil::CoInitializeEx(COINIT_MULTITHREADED);

        struct ApartmentObserver : public winrt::implements<ApartmentObserver, IApartmentShutdown>
        {
            void STDMETHODCALLTYPE OnUninitialize(unsigned long long apartmentId) noexcept override
            {
                LogOutput(L"OnUninitialize %ull\n", apartmentId);
            }
        };

        wil::unique_apartment_shutdown_registration apt_shutdown_registration;
        unsigned long long id{};
        REQUIRE_SUCCEEDED(::RoRegisterForApartmentShutdown(winrt::make<ApartmentObserver>().get(), &id, apt_shutdown_registration.put()));
        LogOutput(L"RoRegisterForApartmentShutdown %p\r\n", apt_shutdown_registration.get());
        // don't unregister and let the pending COM apartment rundown invoke the callback.
        apt_shutdown_registration.release();
    }
}

TEST_CASE("ComApartmentVariable::CallAllMethods", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestApartmentVariableAllMethods<mock_platform>);
}

TEST_CASE("ComApartmentVariable::GetOrCreateForms", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestApartmentVariableGetOrCreateForms<mock_platform>);
}

TEST_CASE("ComApartmentVariable::VariableLifetimes", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestApartmentVariableLifetimes<mock_platform>);
}

TEST_CASE("ComApartmentVariable::WinningApartmentAlreadyRundownRace", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestWinningApartmentAlreadyRundownRace<mock_platform>);
}

TEST_CASE("ComApartmentVariable::LosingApartmentAlreadyRundownRace", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestLosingApartmentAlreadyRundownRace<mock_platform>);
}

TEST_CASE("ComApartmentVariable::MultipleApartments", "[com][apartment_variable]")
{
    RunApartmentVariableTest(TestMultipleApartments<mock_platform>);
}

TEST_CASE("ComApartmentVariable::UseRealPlatformRunAllTests", "[com][apartment_variable]")
{
    if (!wil::are_apartment_variables_supported())
    {
        return;
    }

    RunApartmentVariableTest(TestApartmentVariableAllMethods);
    RunApartmentVariableTest(TestApartmentVariableGetOrCreateForms);
    RunApartmentVariableTest(TestApartmentVariableLifetimes);
    RunApartmentVariableTest(TestWinningApartmentAlreadyRundownRace);
    RunApartmentVariableTest(TestLosingApartmentAlreadyRundownRace);
    RunApartmentVariableTest(TestMultipleApartments);
}

#endif
