//
// Copyright (C) Microsoft Corporation
// All rights reserved.
//
// Code in details namespace is for internal usage within the library code
//
#ifndef _WRL_ASYNC_H_
#define _WRL_ASYNC_H_

#ifdef _MSC_VER
#pragma once
#endif  // _MSC_VER

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wignored-qualifiers"
#pragma clang diagnostic ignored "-Wextra-tokens"
#pragma clang diagnostic ignored "-Wreorder"
#endif

#include <asyncinfo.h>
#include <winerror.h>
#include <intrin.h>

#include <windows.foundation.h>
#include <windows.foundation.diagnostics.h>
#if !defined(MIDL_NS_PREFIX) && !defined(____x_ABI_CWindows_CFoundation_CDiagnostics_CITracingStatusChangedEventArgs_FWD_DEFINED__)
namespace ABI {
namespace Windows {
namespace Foundation {
typedef ::Windows::Foundation::IAsyncActionCompletedHandler IAsyncActionCompletedHandler;
namespace Diagnostics {
typedef ::Windows::Foundation::Diagnostics::CausalitySource CausalitySource;
typedef ::Windows::Foundation::Diagnostics::IAsyncCausalityTracerStatics IAsyncCausalityTracerStatics;
typedef ::Windows::Foundation::Diagnostics::TracingStatusChangedEventArgs TracingStatusChangedEventArgs;
typedef ::Windows::Foundation::Diagnostics::ITracingStatusChangedEventArgs ITracingStatusChangedEventArgs;

typedef ::Windows::Foundation::Diagnostics::CausalityTraceLevel CausalityTraceLevel;
const ::Windows::Foundation::Diagnostics::CausalityTraceLevel CausalityTraceLevel_Verbose = ::Windows::Foundation::Diagnostics::CausalityTraceLevel_Verbose;
const ::Windows::Foundation::Diagnostics::CausalityTraceLevel CausalityTraceLevel_Important = ::Windows::Foundation::Diagnostics::CausalityTraceLevel_Important;
const ::Windows::Foundation::Diagnostics::CausalityTraceLevel CausalityTraceLevel_Required = ::Windows::Foundation::Diagnostics::CausalityTraceLevel_Required;

const ::Windows::Foundation::Diagnostics::CausalityRelation CausalityRelation_Join = ::Windows::Foundation::Diagnostics::CausalityRelation_Join;
const ::Windows::Foundation::Diagnostics::CausalityRelation CausalityRelation_Choice = ::Windows::Foundation::Diagnostics::CausalityRelation_Choice;
const ::Windows::Foundation::Diagnostics::CausalityRelation CausalityRelation_Error = ::Windows::Foundation::Diagnostics::CausalityRelation_Error;
const ::Windows::Foundation::Diagnostics::CausalityRelation CausalityRelation_Cancel = ::Windows::Foundation::Diagnostics::CausalityRelation_Cancel;
const ::Windows::Foundation::Diagnostics::CausalityRelation CausalityRelation_AssignDelegate = ::Windows::Foundation::Diagnostics::CausalityRelation_AssignDelegate ;

const ::Windows::Foundation::Diagnostics::CausalitySynchronousWork CausalitySynchronousWork_CompletionNotification = ::Windows::Foundation::Diagnostics::CausalitySynchronousWork_CompletionNotification;
const ::Windows::Foundation::Diagnostics::CausalitySynchronousWork CausalitySynchronousWork_ProgressNotification = ::Windows::Foundation::Diagnostics::CausalitySynchronousWork_ProgressNotification;
const ::Windows::Foundation::Diagnostics::CausalitySynchronousWork CausalitySynchronousWork_Execution = ::Windows::Foundation::Diagnostics::CausalitySynchronousWork_Execution;
}
}
}
}
#endif

#include <wrl\def.h>
#include <wrl\internal.h>
#include <wrl\implements.h>
#include <wrl\event.h>
#include <wrl\wrappers\corewrappers.h>

#include <new.h>

// Set packing
#include <pshpack8.h>

#pragma warning(push)
// nonstandard extension used: override specifier 'override'
#pragma warning(disable: 4481)

// GUID identifying the the Windows Platform for logging purposes
// {C54C95D9-5B6E-41E9-A28D-2DD68F94B500}
extern _declspec(selectany) const GUID GUID_CAUSALITY_WINDOWS_PLATFORM_ID =
{ 0xc54c95d9, 0x5b6e, 0x41e9, { 0xa2, 0x8d, 0x2d, 0xd6, 0x8f, 0x94, 0xb5, 0x0 } };

namespace Microsoft {
namespace WRL {

// Designates the error propagation policy used by FireProgress and FireComplete. If PropagateDelegateError is the mode
// then failures returned from the async completion and progress delegates are propagated.  If IgnoreDelegateError
// is the mode, then failures returned from the async completion and progress delegates are converted to successes and
// the errors are swallowed.
enum ErrorPropagationPolicy
{
    PropagateDelegateError = 1,
    IgnoreDelegateError = 2
};

namespace Details
{
  // contains states indicating existance or lack of options
  struct AsyncOptionsBase
  {
      static const bool hasCausalityOptions = false;
      static const bool hasErrorPropagationPolicy = false;
      static const bool hasCausalityOperationName = false;
      static const bool isCausalityEnabled = true;
  };

  template < PCWSTR OpName >
  struct IsOperationName
  {
      static const bool Value = true;
  };

  template < >
  struct IsOperationName< nullptr >
  {
      static const bool Value = false;
  };
}

// Options for error propagation and the defaults are set here.
#if defined(BUILD_WINDOWS) && (NTDDI_VERSION >= NTDDI_WINBLUE)
template < ErrorPropagationPolicy errorPropagationPolicy = PropagateErrorWithWin8Quirk>
#else
template < ErrorPropagationPolicy errorPropagationPolicy = Microsoft::WRL::ErrorPropagationPolicy::IgnoreDelegateError>
#endif
struct ErrorPropagationOptions : public Microsoft::WRL::Details::AsyncOptionsBase
{
    static const ErrorPropagationPolicy PropagationPolicy = errorPropagationPolicy;
    static const bool hasErrorPropagationPolicy = true;
};

#ifndef _WRL_DISABLE_CAUSALITY_

// Options for causality tracing and the needed defaults are set here. The following class may be used as
// a reference to add more options to AsyncBase
#ifdef BUILD_WINDOWS
#define WRL_DEFAULT_CAUSALITY_GUID GUID_CAUSALITY_WINDOWS_PLATFORM_ID
#define WRL_DEFAULT_CAUSALITY_SOURCE ::ABI::Windows::Foundation::Diagnostics::CausalitySource::CausalitySource_System
#else
#define WRL_DEFAULT_CAUSALITY_GUID GUID_NULL
#define WRL_DEFAULT_CAUSALITY_SOURCE ::ABI::Windows::Foundation::Diagnostics::CausalitySource::CausalitySource_Application
#endif //BUILD_WINDOWS

template <
    PCWSTR OpName = nullptr,
    const GUID &PlatformId = WRL_DEFAULT_CAUSALITY_GUID,
    ::ABI::Windows::Foundation::Diagnostics::CausalitySource CausalitySource = WRL_DEFAULT_CAUSALITY_SOURCE
>
struct AsyncCausalityOptions : public Microsoft::WRL::Details::AsyncOptionsBase
{
    static PCWSTR GetAsyncOperationName()
    {
        return OpName;
    }

    static const GUID GetPlatformId()
    {
        return PlatformId;
    }

    static ::ABI::Windows::Foundation::Diagnostics::CausalitySource GetCausalitySource()
    {
        return CausalitySource;
    }

    static const bool hasCausalityOptions = true;
    static const bool hasCausalityOperationName = Microsoft::WRL::Details::IsOperationName<OpName>::Value;
};

// This option type for causality tracing disables just the tracing part.
extern __declspec(selectany) const WCHAR DisableCausalityAsyncOperationName[] = L"Disabled";
struct DisableCausality : public AsyncCausalityOptions< DisableCausalityAsyncOperationName >
{
    static const bool isCausalityEnabled = false;
};

#endif // _WRL_DISABLE_CAUSALITY_

namespace Details
{
// maps internal definitions for AsyncStatus and defines states that are not client visible
enum AsyncStatusInternal
{
    // non-client visible internal states
    _Undefined = -2,
    _Created = -1,

    // client visible states (must match AsyncStatus exactly)
    _Started = static_cast<int>(::ABI::Windows::Foundation::AsyncStatus::Started),
    _Completed = static_cast<int>(::ABI::Windows::Foundation::AsyncStatus::Completed),
    _Canceled = static_cast<int>(::ABI::Windows::Foundation::AsyncStatus::Canceled),
    _Error = static_cast<int>(::ABI::Windows::Foundation::AsyncStatus::Error),

    // non-client visible internal states
    _Closed
};

template < typename T >
struct DerefHelper;

template < typename T >
struct DerefHelper<T*>
{
    typedef T DerefType;
};

#pragma region AsyncOptionsHelper

#ifndef _WRL_DISABLE_CAUSALITY_

// Provides the name for a async operation/action for logging purposes
template < typename TComplete, bool hasName, typename TOptions >
struct CausalityNameHelper
{
    // Provides a default string for the Async Operation/Action name if a name is not provided
    // for logging purposes
    static PCWSTR GetName()
    {
        return TOptions::GetAsyncOperationName();
    }
};

// Specialization to handle the logging for those classes that implement z_get_rc_name_impl
template < typename TComplete, typename TOptions >
struct CausalityNameHelper< TComplete, false, TOptions >
{
    static PCWSTR GetName()
    {
        return TComplete::z_get_rc_name_impl();
    }
};

// Specialization to handle the logging for IAsyncAction
template <typename TOptions>
struct CausalityNameHelper< ::ABI::Windows::Foundation::IAsyncActionCompletedHandler, false, TOptions >
{
    // IAsyncActionCompletedHandler is not templatized so it does not implement z_get_rc_name_impl
    // hence this specialization
    static PCWSTR GetName()
    {
        return L"Windows.Foundation.IAsyncActionCompletedHandler";
    }
};

#endif // _WRL_DISABLE_CAUSALITY_

// helper class to switch between default or given options for error
// propagation
template < bool hasValue, typename TOptions >
struct ErrorPropagationOptionsHelper;

// provides the given options for error propagation
template < typename TOptions >
struct ErrorPropagationOptionsHelper< true , TOptions >
{
    static const Microsoft::WRL::ErrorPropagationPolicy PropagationPolicy = TOptions::PropagationPolicy;
};

// provides default options for error propagation
template < typename TOptions >
struct ErrorPropagationOptionsHelper< false , TOptions >
{
    static const Microsoft::WRL::ErrorPropagationPolicy PropagationPolicy = Microsoft::WRL::ErrorPropagationOptions<>::PropagationPolicy;
};

#ifndef _WRL_DISABLE_CAUSALITY_

// helper class to switch between default or given options for
// Async Causality Options
template < bool hasCausalityOptions, typename TComplete, typename TOptions >
struct AsyncCausalityOptionsHelper;

// provides the given options for causality tracing
template < typename TComplete, typename TOptions >
struct AsyncCausalityOptionsHelper < true, TComplete, TOptions >
{
#ifdef BUILD_WINDOWS
    static_assert(!(__is_base_of(::ABI::Windows::Foundation::IAsyncActionCompletedHandler, TComplete) && !TOptions::hasCausalityOperationName),"Please add name to Asynchronous Operations for Better Diagnostics: http://winri/BreakingChanges/BreakingChangeForm/Index/1992");
#endif
    static PCWSTR GetAsyncOperationName()
    {
        return CausalityNameHelper< TComplete, TOptions::hasCausalityOperationName, TOptions >::GetName();
    }

    static const GUID GetPlatformId()
    {
        return TOptions::GetPlatformId();
    }

    static const ::ABI::Windows::Foundation::Diagnostics::CausalitySource GetCausalitySource()
    {
        return TOptions::GetCausalitySource();
    }

    static const bool CausalityEnabled = TOptions::isCausalityEnabled;
};

// provides the default options for causality tracing
template < typename TComplete, typename TOptions >
struct AsyncCausalityOptionsHelper < false, TComplete, TOptions >
{
#ifdef BUILD_WINDOWS
    static_assert(!(__is_base_of(::ABI::Windows::Foundation::IAsyncActionCompletedHandler, TComplete) && !TOptions::hasCausalityOperationName),"Please add name to Asynchronous Operations for Better Diagnostics: http://winri/BreakingChanges/BreakingChangeForm/Index/1992");
#endif
    static PCWSTR GetAsyncOperationName()
    {
        return CausalityNameHelper< TComplete, TOptions::hasCausalityOperationName, TOptions >::GetName();
    }

    static const GUID GetPlatformId()
    {
        return Microsoft::WRL::AsyncCausalityOptions<>::GetPlatformId();
    }

    static ::ABI::Windows::Foundation::Diagnostics::CausalitySource GetCausalitySource()
    {
        return Microsoft::WRL::AsyncCausalityOptions<>::GetCausalitySource();
    }

    static const bool CausalityEnabled = TOptions::isCausalityEnabled;
};

#endif // _WRL_DISABLE_CAUSALITY_

// helper calls to accumulate all options
// Future options have to be added here
template < typename TComplete, typename TOptions >
struct AsyncOptionsHelper :
#ifndef _WRL_DISABLE_CAUSALITY_
    public AsyncCausalityOptionsHelper < TOptions::hasCausalityOptions, TComplete, TOptions >,
#endif // _WRL_DISABLE_CAUSALITY_
    public ErrorPropagationOptionsHelper < TOptions::hasErrorPropagationPolicy, TOptions >
{
};

#pragma endregion
// End of AsyncOptionsHelper

} // Details

// designates whether the "GetResults" method returns a single result (after complete fires) or multiple results
// (which are progressively consumable between Start state and before Close is called)
enum AsyncResultType
{
    SingleResult    = 0x0001,
    MultipleResults = 0x0002
};

// indicates how an attempt to transition to a terminal state of Completed or Error should behave with respect to
// the (client-requested) Canceled state.
enum CancelTransitionPolicy
{
    // If the async operation is presently in a (client-requested) Canceled state, this indicates that
    // it will stay in the Canceled state as opposed to transitioning to a terminal Completed or Error
    // state.
    RemainCanceled = 0,

    // If the async operation is presently in a (client-requested) Canceled state, this indicates that
    // state should transition from that Canceled state to the terminal state of Completed or Error as
    // determined by the call utilizing this flag.
    TransitionFromCanceled
};

#pragma region AsyncOptions

template < ErrorPropagationPolicy errorPropagationPolicy >
struct ErrorPropagationPolicyTraits;

// This error propagation policy passes through all errors
template <>
struct ErrorPropagationPolicyTraits< PropagateDelegateError >
{
    static HRESULT FireCompletionErrorPropagationPolicyFilter(HRESULT hrIn, IUnknown *, void * = nullptr)
    {
        // Ignore errors if the error is caused by a disconnected object
        if (hrIn == RPC_E_DISCONNECTED || hrIn == HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE) || hrIn == JSCRIPT_E_CANTEXECUTE)
        {
            ::RoTransformError(hrIn, S_OK, nullptr);
            hrIn = S_OK;
        }
        return hrIn;
    }

    static HRESULT FireProgressErrorPropagationPolicyFilter(HRESULT hrIn, IUnknown *, void * = nullptr)
    {
        // Ignore errors if the error is caused by a disconnected object
        if (hrIn == RPC_E_DISCONNECTED || hrIn == HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE) || hrIn == JSCRIPT_E_CANTEXECUTE)
        {
            ::RoTransformError(hrIn, S_OK, nullptr);
            hrIn = S_OK;
        }
        return hrIn;
    }
};

// This error propagation policy ignores all errors and converts them to S_OK
template <>
struct ErrorPropagationPolicyTraits< IgnoreDelegateError >
{
    static HRESULT FireCompletionErrorPropagationPolicyFilter(HRESULT hrIn, IUnknown *, void * = nullptr)
    {
        if (FAILED(hrIn))
        {
            ::RoTransformError(hrIn, S_OK, nullptr);
            hrIn = S_OK;
        }
        return hrIn;
    }

    static HRESULT FireProgressErrorPropagationPolicyFilter(HRESULT hrIn, IUnknown *, void * = nullptr)
    {
        if (FAILED(hrIn))
        {
            ::RoTransformError(hrIn, S_OK, nullptr);
            hrIn = S_OK;
        }
        return hrIn;
    }
};

// All options for the AsyncBase class are accumulated here. This class may be expanded to include
// new options as needed.
template <
#if defined(BUILD_WINDOWS) && (NTDDI_VERSION >= NTDDI_WINBLUE)
    ErrorPropagationPolicy errorPropagationPolicy = PropagateErrorWithWin8Quirk,
#else
    ErrorPropagationPolicy errorPropagationPolicy = ErrorPropagationPolicy::IgnoreDelegateError,
#endif
    PCWSTR OpName = nullptr,
#ifdef BUILD_WINDOWS
    const GUID &PlatformId = GUID_CAUSALITY_WINDOWS_PLATFORM_ID,
    ::ABI::Windows::Foundation::Diagnostics::CausalitySource CausalitySource = ::ABI::Windows::Foundation::Diagnostics::CausalitySource::CausalitySource_System
#else
    const GUID &PlatformId = GUID_NULL,
    ::ABI::Windows::Foundation::Diagnostics::CausalitySource CausalitySource = ::ABI::Windows::Foundation::Diagnostics::CausalitySource::CausalitySource_Application
#endif //BUILD_WINDOWS
>
struct AsyncOptions :
#ifndef _WRL_DISABLE_CAUSALITY_
    public AsyncCausalityOptions<OpName, PlatformId, CausalitySource>,
#endif // _WRL_DISABLE_CAUSALITY_
    public ErrorPropagationOptions<errorPropagationPolicy>
{
    static const bool hasCausalityOptions  = true;
    static const bool hasErrorPropagationPolicy = true;
    static const bool hasCausalityOperationName = Microsoft::WRL::Details::IsOperationName<OpName>::Value;
    static const bool isCausalityEnabled = true;
};

#pragma endregion
// End of AsyncOptions region

#ifndef _WRL_DISABLE_CAUSALITY_
    _declspec(selectany) INIT_ONCE gCausalityInitOnce = INIT_ONCE_STATIC_INIT;
    _declspec(selectany) ::ABI::Windows::Foundation::Diagnostics::IAsyncCausalityTracerStatics* gCausality;
#endif // _WRL_DISABLE_CAUSALITY_

// AsyncBase - base class that implements the WinRT Async state machine
// this base class is designed to be used with WRL to implement an async worker object
template <
    typename TComplete,
    typename TProgress = Details::Nil,
    AsyncResultType resultType = SingleResult,
    typename TAsyncBaseOptions = AsyncOptions<>
>
class AsyncBase : public AsyncBase< TComplete, Details::Nil, resultType, TAsyncBaseOptions >
{
    typedef typename Details::ArgTraitsHelper< TProgress >::Traits ProgressTraits;
    typedef Microsoft::WRL::Details::AsyncOptionsHelper< TComplete, TAsyncBaseOptions > AllOptions;
    friend class AsyncBase< TComplete, Details::Nil, resultType, TAsyncBaseOptions >;

public:

    // since this is designed to be used inside of an RuntimeClass<> template, we can
    // only have a default constructor
    AsyncBase() :
        progressDelegate_(nullptr),
        progressDelegateBucketAssist_(nullptr)
    {
    }

    // Delegate Helpers
    STDMETHOD(PutOnProgress)(TProgress* progressHandler)
    {
        HRESULT hr = this->CheckValidStateForDelegateCall();
        if (SUCCEEDED(hr))
        {
            progressDelegate_ = progressHandler;

            if (progressDelegate_ != nullptr)
            {
                progressDelegateBucketAssist_ = Microsoft::WRL::Details::GetDelegateBucketAssist(progressDelegate_.Get());
            }

            this->TraceDelegateAssigned();
        }
        return hr;
    }

    STDMETHOD(GetOnProgress)(TProgress** progressHandler)
    {
        *progressHandler = nullptr;
        HRESULT hr = this->CheckValidStateForDelegateCall();
        if (SUCCEEDED(hr))
        {
            progressDelegate_.CopyTo(progressHandler);
        }
        return hr;
    }

    HRESULT FireProgress(const typename ProgressTraits::Arg2Type arg)
    {
        HRESULT hr = S_OK;
        ComPtr< ::ABI::Windows::Foundation::IAsyncInfo > asyncInfo = this;
        ComPtr<typename Details::DerefHelper<typename ProgressTraits::Arg1Type>::DerefType> operationInterface;
        if (progressDelegate_)
        {
            hr = asyncInfo.As(&operationInterface);
            if (SUCCEEDED(hr))
            {
                this->TraceProgressNotificationStart();

                hr = progressDelegate_->Invoke(operationInterface.Get(), arg);

                this->TraceProgressNotificationComplete();
            }
        }

        // filter the errors per the Error Propagation Policy
        hr = ErrorPropagationPolicyTraits< AllOptions::PropagationPolicy >::FireProgressErrorPropagationPolicyFilter(hr, progressDelegate_.Get(), progressDelegateBucketAssist_);

        return hr;
    }

    HRESULT FireCompletion(void) override
    {
        // "this" may be deleted during the completion call. Remove progress prior to firing completion.
        progressDelegate_.Reset();
        return AsyncBase< TComplete, Details::Nil, resultType, TAsyncBaseOptions >::FireCompletion();
    }

private:
    ::Microsoft::WRL::ComPtr<TProgress> progressDelegate_;
    void *progressDelegateBucketAssist_;
};

template < typename TComplete, AsyncResultType resultType, typename TAsyncBaseOptions >
class AsyncBase< TComplete, Details::Nil, resultType, TAsyncBaseOptions > : public ::Microsoft::WRL::Implements< ::ABI::Windows::Foundation::IAsyncInfo >
{
    typedef typename Details::ArgTraitsHelper< TComplete >::Traits CompleteTraits;
    typedef Microsoft::WRL::Details::AsyncOptionsHelper<TComplete, TAsyncBaseOptions> AllOptions;
public:
    // since this is designed to be used inside of a RuntimeClass<> template, we can
    // only have a default constructor
    AsyncBase() :
        currentStatus_(Details::AsyncStatusInternal::_Created),
        id_(1),
        errorCode_(S_OK),
        completeDelegate_(nullptr),
        completeDelegateBucketAssist_(nullptr),
        asyncOperationBucketAssist_(nullptr),
        cCompleteDelegateAssigned_(0),
        cCallbackMade_(0)
    {
    }

    // The TraceCompletion in logged if the FireCompletion occurs and the completion call back is assigned
    // if the callback was not made then the async operation completion is logged here
    virtual ~AsyncBase()
    {
        if (!cCallbackMade_)
        {
            TraceOperationComplete();
        }
    }

    // IAsyncInfo::put_Id
    STDMETHOD(put_Id)(const unsigned int id)
    {
        if (id == 0)
        {
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
            WCHAR const pszParamName[] = L"id";
            ::RoOriginateErrorW(E_INVALIDARG, ARRAYSIZE(pszParamName) - 1, pszParamName);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
            return E_INVALIDARG;
        }
        id_ = id;

        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);

        if (current != Details::_Created)
        {
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
            ::RoOriginateError(E_ILLEGAL_METHOD_CALL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
            return E_ILLEGAL_METHOD_CALL;
        }

        return S_OK;
    }

    // IAsyncInfo::get_Id
    STDMETHOD(get_Id)(unsigned int *id) override
    {
        *id = id_;
        return CheckValidStateForAsyncInfoCall();
    }

    // IAsyncInfo::get_Status
    STDMETHOD(get_Status)(::ABI::Windows::Foundation::AsyncStatus *status) override
    {
        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);
        *status = static_cast< ::ABI::Windows::Foundation::AsyncStatus >(current);
        return CheckValidStateForAsyncInfoCall();
    }

    // IAsyncInfo::get_ErrorCode
    STDMETHOD(get_ErrorCode)(HRESULT* errorCode) override
    {
        HRESULT hr = CheckValidStateForAsyncInfoCall();
        if (SUCCEEDED(hr))
        {
            ErrorCode(errorCode);
        }
        else
        {
            // Do not propagate the error and error info associated with the async error if this call generated
            // a specific error itself.
            *errorCode = hr;
        }
        return hr;
    }

protected:
    // Start - this is not externally visible since async operations "hot start" before returning to the caller
    STDMETHOD(Start)(void)
    {
        HRESULT hr = S_OK;
        if (TransitionToState(Details::_Started))
        {
            hr = OnStart();

#ifndef _WRL_DISABLE_CAUSALITY_
            if (SUCCEEDED(hr) &&
                ::InitOnceExecuteOnce(&gCausalityInitOnce, InitCausality, NULL, NULL))
            {
                TraceOperationStart();
            }
#endif // _WRL_DISABLE_CAUSALITY_
        }
        else
        {
            hr = E_ILLEGAL_STATE_CHANGE;
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
            ::RoOriginateError(E_ILLEGAL_STATE_CHANGE, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
        }
        return hr;
    }

public:
    // IAsyncInfo::Cancel
    STDMETHOD(Cancel)(void)
    {
        if (TransitionToState(Details::_Canceled))
        {
            OnCancel();

            TraceCancellation();
        }
        return S_OK;
    }

    // IAsyncInfo::Close
    STDMETHOD(Close)(void) override
    {
        HRESULT hr = S_OK;
        if (TransitionToState(Details::_Closed))
        {
            OnClose();
        }
        else    // illegal state change
        {
            Details::AsyncStatusInternal current = Details::_Undefined;
            CurrentStatus(&current);

            if (current == Details::_Closed)
            {
                hr = S_OK;          // Closed => Closed transition is just ignored
            }
            else
            {
                hr = E_ILLEGAL_STATE_CHANGE;
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
                ::RoOriginateError(E_ILLEGAL_STATE_CHANGE, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
            }
        }
        return hr;
    }

    // Delegate helpers
    STDMETHOD(PutOnComplete)(TComplete* completeHandler)
    {
        HRESULT hr = CheckValidStateForDelegateCall();
        if (SUCCEEDED(hr))
        {
            // this delegate property is "write once"
            if (InterlockedIncrement(&cCompleteDelegateAssigned_) == 1)
            {
                if (completeHandler != nullptr)
                {
                    completeDelegateBucketAssist_ = Microsoft::WRL::Details::GetDelegateBucketAssist(completeHandler);
                }

                completeDelegate_ = completeHandler;

                // Guarantee that the write of completeDelegate_ is ordered with respect to the read of state below
                // as perceived from FireCompletion on another thread.
                MemoryBarrier();

                this->TraceDelegateAssigned();

                // in the "hot start" case, put_Completed could have been called after the async operation has hit
                // a terminal state.  If so, fire the completion immediately.
                if (IsTerminalState())
                {
                    FireCompletion();
                }
            }
            else
            {
                hr = E_ILLEGAL_DELEGATE_ASSIGNMENT;
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
                ::RoOriginateError(E_ILLEGAL_DELEGATE_ASSIGNMENT, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
            }
        }
        return hr;
    }

    STDMETHOD(GetOnComplete)(TComplete** completeHandler)
    {
        *completeHandler = nullptr;
        HRESULT hr = CheckValidStateForDelegateCall();
        if (SUCCEEDED(hr))
        {
            completeDelegate_.CopyTo(completeHandler);
        }
        return hr;
    }

    virtual HRESULT FireCompletion()
    {
        HRESULT hr = S_OK;
        // must do this *before* the InterlockedIncrement!
        TryTransitionToCompleted();

        __WRL_ASSERT__(IsTerminalState() && "Must only call FireCompletion when operation is in terminal state");

        // we guarantee that completion can only ever be fired once
        if (completeDelegate_ != nullptr && InterlockedIncrement(&cCallbackMade_) == 1)
        {
            ComPtr< ::ABI::Windows::Foundation::IAsyncInfo> asyncInfo = this;
            ComPtr<typename Details::DerefHelper<typename CompleteTraits::Arg1Type>::DerefType> operationInterface;

            TraceOperationComplete();

            if (SUCCEEDED(asyncInfo.As(&operationInterface)))
            {
                Details::AsyncStatusInternal current = Details::_Undefined;
                CurrentStatus(&current);

                TraceCompletionNotificationStart();

                hr = completeDelegate_->Invoke(operationInterface.Get(), static_cast<::ABI::Windows::Foundation::AsyncStatus>(current));
                // Filter the errors as per the Error Propagation Policy
                hr = ErrorPropagationPolicyTraits< AllOptions::PropagationPolicy >::FireCompletionErrorPropagationPolicyFilter(hr, completeDelegate_.Get(), completeDelegateBucketAssist_);
                completeDelegate_ = nullptr;


                TraceCompletionNotificationComplete();
            }
        }

        return hr;
    }

protected:

    inline void CurrentStatus(Details::AsyncStatusInternal *status)
    {
        ::_InterlockedCompareExchange(reinterpret_cast<LONG*>(status), currentStatus_, static_cast<LONG>(*status));
        __WRL_ASSERT__(*status != Details::_Undefined);
    }

    // This method returns the error code stored as a result of a transition into the error state.
    // In addition, if there is any restricted error information associated with the error that was captured at the time
    // of the error transition, it will be associated with the calling thread via a SetRestrictedErrorInfo call.
    inline void ErrorCode(HRESULT *error)
    {
        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);

        // Do not allow visibility of the error until such point as we have had a successful state transition into the error state.
        // The error + information is not a single atomic quantity.  It is not considered "published" until we are actively in the error state.
        if (current != Details::_Error)
        {
            *error = S_OK;
        }
        else
        {
            ::_InterlockedCompareExchange(reinterpret_cast<LONG*>(error), errorCode_, static_cast<LONG>(*error));
            if (errorInfo_ != nullptr)
            {
                SetRestrictedErrorInfo(errorInfo_.Get());
            }
        }
    }

    bool TryTransitionToCompleted(CancelTransitionPolicy cancelBehavior = CancelTransitionPolicy::RemainCanceled)
    {
        bool bTransition = TransitionToState(Details::AsyncStatusInternal::_Completed);
        if (!bTransition && cancelBehavior == CancelTransitionPolicy::TransitionFromCanceled)
        {
            bTransition = TransitionCanceledToCompleted();
        }
        return bTransition;
    }

    bool TryTransitionToError(const HRESULT error, CancelTransitionPolicy cancelBehavior = CancelTransitionPolicy::RemainCanceled, _In_opt_ void * bucketAssist = nullptr)
    {
        // In addition to the result being transitioned to, there might be restricted error information associated with the error.  It
        // is assumed that such is on the calling thread.  If we successfully transition to the error state with "error" as the code,
        // we must also capture the error info and funnel it over to callers of GetResults / ErrorCode.  Our call to
        // GetRestrictedErrorInfo below will capture the error info after which, it is owned by this async operation.
        //
        // Since there are multiple pieces of information and the capturing of these are not atomic, no one from the outside is allowed
        // to view these until the state transition to error is complete.  This happens in two parts:
        //
        // - A successful CAS from S_OK to error (meaning that this error is the one being captures)
        // - A successful state change into the error state (via the Transition* call below)

        if (bucketAssist != nullptr)
        {
            asyncOperationBucketAssist_ = bucketAssist;
        }
        bool bTransition = false;
        if (::_InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&errorCode_), error, S_OK) == S_OK)
        {
            (void)GetRestrictedErrorInfo(&errorInfo_);

            // This thread is the "owner" of the rights to transition to the error state.
            bTransition = TransitionToState(Details::AsyncStatusInternal::_Error);
            if (!bTransition && cancelBehavior == CancelTransitionPolicy::TransitionFromCanceled)
            {
                bTransition = TransitionCanceledToError();
            }
        }

        if (bTransition)
        {
            TraceError();
        }

        // if we return true, then we did a valid state transition
        // queue firing of completed event (cannot be done from this call frame)
        // otherwise we are already in a terminal state: error, canceled, completed, or closed
        // and we ignore the transition request to the Error state
        return bTransition;
    }

    // This method checks to see if the delegate properties can be
    // modified in the current state and generates the appropriate
    // error hr in the case of violation.
    inline HRESULT CheckValidStateForDelegateCall()
    {
        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);
        if (current == Details::_Closed)
        {
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
            ::RoOriginateError(E_ILLEGAL_METHOD_CALL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
            return E_ILLEGAL_METHOD_CALL;
        }
        return S_OK;
    }

    // This method checks to see if results can be collected in the
    // current state and generates the appropriate error hr in
    // the case of a violation.
    inline HRESULT CheckValidStateForResultsCall()
    {
        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);

        if (current == Details::_Error)
        {
            HRESULT hr;

            // Make sure to propagate any restricted error info associated with the asynchronous failure.
            ErrorCode(&hr);
            return hr;
        }
#pragma warning(push)
#pragma warning(disable: 4127) // Conditional expression is constant
        // single result only legal in Completed state
        if (resultType == SingleResult)
#pragma warning(pop)
        {
            if (current != Details::_Completed)
            {
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
                ::RoOriginateError(E_ILLEGAL_METHOD_CALL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
                return E_ILLEGAL_METHOD_CALL;
            }
        }
        // multiple results can be called after async operation is running (started) and before/after Completed
        else if (current != Details::_Started &&
                 current != Details::_Canceled &&
                 current != Details::_Completed)
        {
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
            ::RoOriginateError(E_ILLEGAL_METHOD_CALL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
            return E_ILLEGAL_METHOD_CALL;
        }
        return S_OK;
    }

    // This method can be called by derived classes periodically to determine
    // whether the asynchronous operation should continue processing or should
    // be halted.
    inline bool ContinueAsyncOperation()
    {
        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);
        return (current == Details::_Started);
    }

    // These methods are used to allow the async worker implementation do work on
    // state transitions. No real "work" should be done in these methods. In other words
    // they should not block for a long time on UI timescales.
    virtual HRESULT OnStart(void) = 0;
    virtual void OnClose(void) = 0;
    virtual void OnCancel(void) = 0;

private:
#ifndef _WRL_DISABLE_CAUSALITY_
    // This method is used to initialize the Causality tracking
    static BOOL WINAPI InitCausality(
        _Inout_opt_ PINIT_ONCE InitOnce,
        _Inout_opt_ PVOID Parameter,
        _Out_opt_ PVOID* Context )
    {
        UNREFERENCED_PARAMETER(InitOnce);
        UNREFERENCED_PARAMETER(Parameter);
        UNREFERENCED_PARAMETER(Context);

#ifdef _NO_CAUSALITY_DOWNLEVEL_
        // do not attempt to trace causality on OS versions less than 6.2 (Windows 8)
        OSVERSIONINFOEX osvi;
        DWORDLONG       dwlConditionMask = 0;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        osvi.dwMajorVersion = 6;
        osvi.dwMinorVersion = 2;

        VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
        VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

        // if running on Windows 8 or greater
        if (VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION, dwlConditionMask))
        {
#endif
            Microsoft::WRL::Wrappers::HStringReference hstrCausalityTraceName(RuntimeClass_Windows_Foundation_Diagnostics_AsyncCausalityTracer);
            if (FAILED(::Windows::Foundation::GetActivationFactory(hstrCausalityTraceName.Get(), &gCausality)))
            {
                return FALSE;
            }
            return TRUE;
#ifdef _NO_CAUSALITY_DOWNLEVEL_
        }
        else
        {
            gCausality = nullptr;
            return FALSE;
        }
#endif
    }
#endif _WRL_DISABLE_CAUSALITY_

    // This method is used to check if calls to the AsyncInfo properties
    // (id, status, error code) are legal in the current state. It also
    // generates the appropriate error hr to return in the case of an
    // illegal call.
    inline HRESULT CheckValidStateForAsyncInfoCall()
    {
        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);
        if (current == Details::_Closed)
        {
#if (NTDDI_VERSION >= NTDDI_WINBLUE)
            ::RoOriginateError(E_ILLEGAL_METHOD_CALL, nullptr);
#endif // (NTDDI_VERSION >= NTDDI_WINBLUE)
            return E_ILLEGAL_METHOD_CALL;
        }
        else if (current == Details::_Created)  // error in async ::ABI object - returned to caller not started
        {
            // No RoOriginateError needed since this can hit multiple times in expected scenarios

            return E_ASYNC_OPERATION_NOT_STARTED;
        }

        return S_OK;
    }

    inline bool TransitionToState(const Details::AsyncStatusInternal newState)
    {
        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);

        // This enforces the valid state transitions of the asynchronous worker object
        // state machine.
        switch(newState)
        {
        case Details::_Started:
            if (current != Details::_Created)
            {
                return false;
            }
            break;
        case Details::_Completed:
            if (current != Details::_Started)
            {
                return false;
            }
            break;
        case Details::_Canceled:
            if (current != Details::_Started)
            {
                return false;
            }
            break;
        case Details::_Error:
            if (current != Details::_Started)
            {
                return false;
            }
            break;
        case Details::_Closed:
            if (!IsTerminalState(current))
            {
                return false;
            }
            break;
        default:
            return false;
            break;
        }
        // attempt the transition to the new state
        // Note: if currentStatus_ == current, then there was no intervening write
        // by the async work object and the swap succeeded.
        Details::AsyncStatusInternal retState = static_cast<Details::AsyncStatusInternal>(
                ::_InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&currentStatus_),
                                            newState,
                                            static_cast<LONG>(current)));

        // ICE returns the former state, if the returned state and the
        // state we captured at the beginning of this method are the same,
        // the swap succeeded.
        return (retState == current);
    }

protected:

    // It is legal for an async operation object to transition from (client-requested) Canceled
    // state to Completed if, for example, the operation completed near the time of the cancellation request.
    // An operation which is no longer responsive to client requests to cancel and intends to complete
    // successfully despite any new incoming requests to cancel should call TryTransitionToCompleted and
    // pass TransitionFromCanceled instead of using this method.
    inline bool TransitionCanceledToCompleted()
    {
        // this is somewhat overly pessimistic since the client cannot possibly transition
        // the operation out of the canceled state (only the async operation itself can call
        // this method)
        Details::AsyncStatusInternal retState = static_cast<Details::AsyncStatusInternal>(
                ::_InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&currentStatus_),
                                            Details::AsyncStatusInternal::_Completed,
                                            Details::AsyncStatusInternal::_Canceled));
        return (retState == Details::AsyncStatusInternal::_Canceled);
    }

    // It is legal for an async operation object to transition from (client-requested) Canceled
    // state to the error state if, for example, the operation encountered an error near the time
    // of the cancellation request.  An operation which is no longer responsive to client requests to cancel
    // and intends to complete with an error despite any new incoming requests to cancel should call
    // TryTransitionToError and pass TransitionFromCanceled instead of using this method.
    inline bool TransitionCanceledToError()
    {
        Details::AsyncStatusInternal retState = static_cast<Details::AsyncStatusInternal>(
                ::_InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&currentStatus_),
                                            Details::AsyncStatusInternal::_Error,
                                            Details::AsyncStatusInternal::_Canceled));
        return (retState == Details::AsyncStatusInternal::_Canceled);
    }

    inline bool IsTerminalState()
    {
        Details::AsyncStatusInternal current = Details::_Undefined;
        CurrentStatus(&current);
        return IsTerminalState(current);
    }

    inline bool IsTerminalState(Details::AsyncStatusInternal status)
    {
        return (status == Details::_Error ||
                status == Details::_Canceled ||
                status == Details::_Completed ||
                status == Details::_Closed);
    }

    long                                  cCallbackMade_;
    long                                  cCompleteDelegateAssigned_;

#pragma region TracingMethods

    void TraceOperationStart()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceOperationCreation(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Required,
                AllOptions::GetCausalitySource(),
                AllOptions::GetPlatformId(),
                reinterpret_cast< UINT64 >(this),
                Microsoft::WRL::Wrappers::HStringReference(AllOptions::GetAsyncOperationName()).Get(),
                id_);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceOperationComplete()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            Details::AsyncStatusInternal status;
            CurrentStatus(&status);
            gCausality->TraceOperationCompletion(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Required,
                AllOptions::GetCausalitySource(),
                AllOptions::GetPlatformId(),
                reinterpret_cast< UINT64 >(this),
                static_cast< ::ABI::Windows::Foundation::AsyncStatus >(status));
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceProgressNotificationStart()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceSynchronousWorkStart(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Important,
                AllOptions::GetCausalitySource(),
                AllOptions::GetPlatformId(),
                reinterpret_cast< UINT64 >(this),
                ::ABI::Windows::Foundation::Diagnostics::CausalitySynchronousWork_ProgressNotification);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceProgressNotificationComplete()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceSynchronousWorkCompletion(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Important,
                AllOptions::GetCausalitySource(),
                ::ABI::Windows::Foundation::Diagnostics::CausalitySynchronousWork_ProgressNotification);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceCompletionNotificationStart()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceSynchronousWorkStart(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Required,
                AllOptions::GetCausalitySource(),
                AllOptions::GetPlatformId(),
                reinterpret_cast< UINT64 >(this),
                ::ABI::Windows::Foundation::Diagnostics::CausalitySynchronousWork_CompletionNotification);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceCompletionNotificationComplete()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceSynchronousWorkCompletion(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Required,
                AllOptions::GetCausalitySource(),
                ::ABI::Windows::Foundation::Diagnostics::CausalitySynchronousWork_CompletionNotification);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceExecutionStart(::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel traceLevel)
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceSynchronousWorkStart(
                traceLevel,
                AllOptions::GetCausalitySource(),
                AllOptions::GetPlatformId(),
                reinterpret_cast< UINT64 >(this),
                ::ABI::Windows::Foundation::Diagnostics::CausalitySynchronousWork_Execution);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceExecutionComplete(::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel traceLevel)
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceSynchronousWorkCompletion(
                traceLevel,
                AllOptions::GetCausalitySource(),
                ::ABI::Windows::Foundation::Diagnostics::CausalitySynchronousWork_Execution);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceDelegateAssigned()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceOperationRelation(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Verbose,
                AllOptions::GetCausalitySource(),
                AllOptions::GetPlatformId(),
                reinterpret_cast< UINT64 >(this),
                ::ABI::Windows::Foundation::Diagnostics::CausalityRelation_AssignDelegate);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceError()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceOperationRelation(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Verbose,
                AllOptions::GetCausalitySource(),
                AllOptions::GetPlatformId(),
                reinterpret_cast< UINT64 >(this),
                ::ABI::Windows::Foundation::Diagnostics::CausalityRelation_Error);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

    void TraceCancellation()
    {
#ifndef _WRL_DISABLE_CAUSALITY_
        if (gCausality && AllOptions::CausalityEnabled)
        {
            // Ignoring HRESULT intentionally. Tracking failure should not change the
            // normal flow of AsyncOperations
            gCausality->TraceOperationRelation(
                ::ABI::Windows::Foundation::Diagnostics::CausalityTraceLevel_Important,
                AllOptions::GetCausalitySource(),
                AllOptions::GetPlatformId(),
                reinterpret_cast< UINT64 >(this),
                ::ABI::Windows::Foundation::Diagnostics::CausalityRelation_Cancel);
        }
#endif // _WRL_DISABLE_CAUSALITY_
    }

#pragma endregion

private:
    ::Microsoft::WRL::ComPtr<TComplete>     completeDelegate_;
    void *completeDelegateBucketAssist_;

    ::Microsoft::WRL::ComPtr<IRestrictedErrorInfo> errorInfo_;
    Details::AsyncStatusInternal volatile currentStatus_;
    HRESULT volatile                      errorCode_;
    unsigned int id_;

protected:
    void *asyncOperationBucketAssist_;
};

}} // namespace Microsoft::WRL

#pragma warning(pop)

#ifndef _WRL_DISABLE_CAUSALITY
#define CAUSALITY_OPTIONS(OpName) \
    Microsoft::WRL::AsyncCausalityOptions< OpName >
#define ASYNCBASE_CAUSALITY_OPTIONS(TComplete, OpName) \
    Microsoft::WRL::AsyncBase< TComplete, Microsoft::WRL::Details::Nil, Microsoft::WRL::AsyncResultType::SingleResult, CAUSALITY_OPTIONS(OpName)>
#define ASYNCBASE_WITH_PROGRESS_CAUSALITY_OPTIONS(TComplete, TProgress, OpName) \
    Microsoft::WRL::AsyncBase< TComplete, TProgress, Microsoft::WRL::AsyncResultType::SingleResult, CAUSALITY_OPTIONS(OpName)>
#define ASYNCBASE_DISABLE_CAUSALITY(TComplete) \
    Microsoft::WRL::AsyncBase< TComplete, Microsoft::WRL::Details::Nil, Microsoft::WRL::AsyncResultType::SingleResult, Microsoft::WRL::DisableCausality>
#define ASYNCBASE_WITH_PROGRESS_DISABLE_CAUSALITY(TComplete, TProgress) \
    Microsoft::WRL::AsyncBase< TComplete, TProgress, Microsoft::WRL::AsyncResultType::SingleResult, Microsoft::WRL::DisableCausality>
#endif // _WRL_DISABLE_CAUSALITY

// Restore packing
#include <poppack.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif // _WRL_ASYNC_H_

#ifdef BUILD_WINDOWS
#include <wrl\internalasync.h>
#endif
