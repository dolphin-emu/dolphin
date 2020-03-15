
#include <wil/winrt.h>
#include <wrl/implements.h>

#include "common.h"

using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;

namespace wiltest
{
    class AbiTestEventSender WrlFinal : public RuntimeClass<
        RuntimeClassFlags<RuntimeClassType::WinRtClassicComMix>,
        IClosable,
        IMemoryBufferReference,
        FtmBase>
    {
    public:

        // IMemoryBufferReference
        IFACEMETHODIMP get_Capacity(_Out_ UINT32* value)
        {
            *value = 0;
            return S_OK;
        }

        IFACEMETHODIMP add_Closed(
            _In_ ITypedEventHandler<IMemoryBufferReference*, IInspectable*>* handler,
            _Out_ ::EventRegistrationToken* token)
        {
            return m_closedEvent.Add(handler, token);
        }

        IFACEMETHODIMP remove_Closed(::EventRegistrationToken token)
        {
            return m_closedEvent.Remove(token);
        }

        // IClosable
        IFACEMETHODIMP Close()
        {
            RETURN_IF_FAILED(m_closedEvent.InvokeAll(this, nullptr));
            return S_OK;
        }

    private:
        Microsoft::WRL::EventSource<ITypedEventHandler<IMemoryBufferReference*, IInspectable*>> m_closedEvent;
    };
}

TEST_CASE("UniqueWinRTEventTokenTests::AbiUniqueWinrtEventTokenEventSubscribe", "[winrt][unique_winrt_event_token]")
{
    ComPtr<IMemoryBufferReference> testEventSender;
    REQUIRE_SUCCEEDED(MakeAndInitialize<wiltest::AbiTestEventSender>(&testEventSender));
    ComPtr<IClosable> closable;
    testEventSender.As(&closable);

    int timesInvoked = 0;
    auto handler = Callback<Implements<RuntimeClassFlags<ClassicCom>,
        ITypedEventHandler<IMemoryBufferReference*, IInspectable*>, FtmBase>>
        ([&timesInvoked](IInspectable*, IInspectable*)
    {
        timesInvoked++;
        return S_OK;
    });
    REQUIRE(timesInvoked == 0);

    {
        wil::unique_winrt_event_token<IMemoryBufferReference> token;
        REQUIRE_SUCCEEDED(WI_MakeUniqueWinRtEventTokenNoThrow(Closed, testEventSender.Get(), handler.Get(), &token));
        REQUIRE(static_cast<bool>(token));
        REQUIRE_SUCCEEDED(closable->Close());
        REQUIRE(timesInvoked == 1);
    }

    REQUIRE_SUCCEEDED(closable->Close());
    REQUIRE(timesInvoked == 1);
}

TEST_CASE("UniqueWinRTEventTokenTests::AbiUniqueWinrtEventTokenEarlyReset", "[winrt][unique_winrt_event_token]")
{
    ComPtr<IMemoryBufferReference> testEventSender;
    REQUIRE_SUCCEEDED(MakeAndInitialize<wiltest::AbiTestEventSender>(&testEventSender));
    ComPtr<IClosable> closable;
    testEventSender.As(&closable);

    int timesInvoked = 0;
    auto handler = Callback<Implements<RuntimeClassFlags<ClassicCom>,
        ITypedEventHandler<IMemoryBufferReference*, IInspectable*>, FtmBase>>
        ([&timesInvoked](IInspectable*, IInspectable*)
    {
        timesInvoked++;
        return S_OK;
    });
    REQUIRE(timesInvoked == 0);

    wil::unique_winrt_event_token<IMemoryBufferReference> token;
    REQUIRE_SUCCEEDED(WI_MakeUniqueWinRtEventTokenNoThrow(Closed, testEventSender.Get(), handler.Get(), &token));
    REQUIRE(static_cast<bool>(token));
    REQUIRE_SUCCEEDED(closable->Close());
    REQUIRE(timesInvoked == 1);

    token.reset();

    REQUIRE_SUCCEEDED(closable->Close());
    REQUIRE(timesInvoked == 1);
}

TEST_CASE("UniqueWinRTEventTokenTests::AbiUniqueWinrtEventTokenMoveTokenToDifferentScope", "[winrt][unique_winrt_event_token]")
{
    ComPtr<IMemoryBufferReference> testEventSender;
    REQUIRE_SUCCEEDED(MakeAndInitialize<wiltest::AbiTestEventSender>(&testEventSender));
    ComPtr<IClosable> closable;
    testEventSender.As(&closable);

    int timesInvoked = 0;
    auto handler = Callback<Implements<RuntimeClassFlags<ClassicCom>,
        ITypedEventHandler<IMemoryBufferReference*, IInspectable*>, FtmBase>>
        ([&timesInvoked](IInspectable*, IInspectable*)
    {
        timesInvoked++;
        return S_OK;
    });
    REQUIRE(timesInvoked == 0);

    wil::unique_winrt_event_token<IMemoryBufferReference> outerToken;
    REQUIRE_FALSE(static_cast<bool>(outerToken));
    {
        wil::unique_winrt_event_token<IMemoryBufferReference> token;
        REQUIRE_SUCCEEDED(WI_MakeUniqueWinRtEventTokenNoThrow(Closed, testEventSender.Get(), handler.Get(), &token));
        REQUIRE(static_cast<bool>(token));
        REQUIRE_SUCCEEDED(closable->Close());
        REQUIRE(timesInvoked == 1);

        outerToken = std::move(token);
        REQUIRE_FALSE(static_cast<bool>(token));
        REQUIRE(static_cast<bool>(outerToken));
    }

    REQUIRE_SUCCEEDED(closable->Close());
    REQUIRE(timesInvoked == 2);
}

TEST_CASE("UniqueWinRTEventTokenTests::AbiUniqueWinrtEventTokenMoveConstructor", "[winrt][unique_winrt_event_token]")
{
    ComPtr<IMemoryBufferReference> testEventSender;
    REQUIRE_SUCCEEDED(MakeAndInitialize<wiltest::AbiTestEventSender>(&testEventSender));
    ComPtr<IClosable> closable;
    testEventSender.As(&closable);

    int timesInvoked = 0;
    auto handler = Callback<Implements<RuntimeClassFlags<ClassicCom>,
        ITypedEventHandler<IMemoryBufferReference*, IInspectable*>, FtmBase>>
        ([&timesInvoked](IInspectable*, IInspectable*)
    {
        timesInvoked++;
        return S_OK;
    });
    REQUIRE(timesInvoked == 0);

    wil::unique_winrt_event_token<IMemoryBufferReference> firstToken;
    REQUIRE_SUCCEEDED(WI_MakeUniqueWinRtEventTokenNoThrow(Closed, testEventSender.Get(), handler.Get(), &firstToken));
    REQUIRE(static_cast<bool>(firstToken));
    closable->Close();
    REQUIRE(timesInvoked == 1);

    wil::unique_winrt_event_token<IMemoryBufferReference> secondToken(std::move(firstToken));
    REQUIRE_FALSE(static_cast<bool>(firstToken));
    REQUIRE(static_cast<bool>(secondToken));

    closable->Close();
    REQUIRE(timesInvoked == 2);

    firstToken.reset();
    closable->Close();
    REQUIRE(timesInvoked == 3);

    secondToken.reset();
    closable->Close();
    REQUIRE(timesInvoked == 3);
}

TEST_CASE("UniqueWinRTEventTokenTests::AbiUniqueWinrtEventTokenReleaseAndReattachToNewWrapper", "[winrt][unique_winrt_event_token]")
{
    ComPtr<IMemoryBufferReference> testEventSender;
    REQUIRE_SUCCEEDED(MakeAndInitialize<wiltest::AbiTestEventSender>(&testEventSender));
    ComPtr<IClosable> closable;
    testEventSender.As(&closable);

    int timesInvoked = 0;
    auto handler = Callback<Implements<RuntimeClassFlags<ClassicCom>,
        ITypedEventHandler<IMemoryBufferReference*, IInspectable*>, FtmBase>>
        ([&timesInvoked](IInspectable*, IInspectable*)
    {
        timesInvoked++;
        return S_OK;
    });
    REQUIRE(timesInvoked == 0);

    wil::unique_winrt_event_token<IMemoryBufferReference> firstToken;
    REQUIRE_SUCCEEDED(WI_MakeUniqueWinRtEventTokenNoThrow(Closed, testEventSender.Get(), handler.Get(), &firstToken));
    REQUIRE(static_cast<bool>(firstToken));
    REQUIRE_SUCCEEDED(closable->Close());
    REQUIRE(timesInvoked == 1);

    ::EventRegistrationToken rawToken = firstToken.release();
    REQUIRE_FALSE(static_cast<bool>(firstToken));
    REQUIRE(rawToken.value != 0);

    REQUIRE_SUCCEEDED(closable->Close());
    REQUIRE(timesInvoked == 2);

    wil::unique_winrt_event_token<IMemoryBufferReference> secondToken(
        rawToken, testEventSender.Get(), &IMemoryBufferReference::remove_Closed);

    REQUIRE_SUCCEEDED(closable->Close());
    REQUIRE(timesInvoked == 3);

    secondToken.reset();
    REQUIRE_SUCCEEDED(closable->Close());
    REQUIRE(timesInvoked == 3);
}

TEST_CASE("UniqueWinRTEventTokenTests::AbiUniqueWinrtEventTokenPolicyVariants", "[winrt][unique_winrt_event_token]")
{
    ComPtr<IMemoryBufferReference> testEventSender;
    REQUIRE_SUCCEEDED(MakeAndInitialize<wiltest::AbiTestEventSender>(&testEventSender));
    ComPtr<IClosable> closable;
    testEventSender.As(&closable);

    int timesInvoked = 0;
    auto handler = Callback<Implements<RuntimeClassFlags<ClassicCom>,
        ITypedEventHandler<IMemoryBufferReference*, IInspectable*>, FtmBase>>
        ([&timesInvoked](IInspectable*, IInspectable*)
    {
        timesInvoked++;
        return S_OK;
    });
    REQUIRE(timesInvoked == 0);

    {
#ifdef WIL_ENABLE_EXCEPTIONS
        auto exceptionPolicyToken = WI_MakeUniqueWinRtEventToken(Closed, testEventSender.Get(), handler.Get());
        REQUIRE(static_cast<bool>(exceptionPolicyToken));
#endif

        auto failFastPolicyToken = WI_MakeUniqueWinRtEventTokenFailFast(Closed, testEventSender.Get(), handler.Get());
        REQUIRE(static_cast<bool>(failFastPolicyToken));

        REQUIRE_SUCCEEDED(closable->Close());

#ifdef WIL_ENABLE_EXCEPTIONS
        REQUIRE(timesInvoked == 2);
#else
        REQUIRE(timesInvoked == 1);
#endif
    }

    REQUIRE_SUCCEEDED(closable->Close());

#ifdef WIL_ENABLE_EXCEPTIONS
    REQUIRE(timesInvoked == 2);
#else
    REQUIRE(timesInvoked == 1);
#endif
}
