// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

/*
 * The OS X Force Feedback API is very similar to the DirectInput API,
 * but it is no longer object-oriented and all prefixes have been changed.
 *
 * Our implementation uses the Windows API names so we need to adapt
 * for these differences on OS X.
 */

#pragma once

#include <atomic>

typedef LONG* LPLONG;  // Missing type for ForceFeedback.h
#include <CoreFoundation/CoreFoundation.h>
#include <ForceFeedback/ForceFeedback.h>
#include "Common/CommonTypes.h"    // for LONG
#include "DirectInputConstants.h"  // Not stricty necessary

namespace ciface::ForceFeedback
{
// Prototypes
class IUnknownImpl;
class FFEffectAdapter;
class FFDeviceAdapter;

// Structs
typedef FFCAPABILITIES DICAPABILITIES;
typedef FFCONDITION DICONDITION;
typedef FFCONSTANTFORCE DICONSTANTFORCE;
typedef FFCUSTOMFORCE DICUSTOMFORCE;
typedef FFEFFECT DIEFFECT;
typedef FFEFFESCAPE DIEFFESCAPE;
typedef FFENVELOPE DIENVELOPE;
typedef FFPERIODIC DIPERIODIC;
typedef FFRAMPFORCE DIRAMPFORCE;

// Other types
typedef CFUUIDRef GUID;
typedef FFDeviceAdapter* FFDeviceAdapterReference;
typedef FFEffectAdapter* FFEffectAdapterReference;
typedef FFDeviceAdapterReference LPDIRECTINPUTDEVICE8;
typedef FFEffectAdapterReference LPDIRECTINPUTEFFECT;

// Property structures
#define DIPH_DEVICE 0

typedef struct DIPROPHEADER
{
  DWORD dwSize;
  DWORD dwHeaderSize;
  DWORD dwObj;
  DWORD dwHow;
} DIPROPHEADER, *LPDIPROPHEADER;

typedef struct DIPROPDWORD
{
  DIPROPHEADER diph;
  DWORD dwData;
} DIPROPDWORD, *LPDIPROPDWORD;

class IUnknownImpl : public IUnknown
{
private:
  std::atomic<ULONG> m_cRef;

public:
  IUnknownImpl() : m_cRef(1) {}
  virtual ~IUnknownImpl() {}
  HRESULT QueryInterface(REFIID iid, LPVOID* ppv)
  {
    *ppv = nullptr;

    if (CFEqual(&iid, IUnknownUUID))
      *ppv = this;
    if (nullptr == *ppv)
      return E_NOINTERFACE;

    ((IUnknown*)*ppv)->AddRef();

    return S_OK;
  }

  ULONG AddRef() { return ++m_cRef; }
  ULONG Release()
  {
    if (--m_cRef == 0)
      delete this;

    return m_cRef;
  }
};

class FFEffectAdapter : public IUnknownImpl
{
private:
  // Only used for destruction
  FFDeviceObjectReference m_device;

public:
  FFEffectObjectReference m_effect;

  FFEffectAdapter(FFDeviceObjectReference device, FFEffectObjectReference effect)
      : m_device(device), m_effect(effect)
  {
  }
  ~FFEffectAdapter() { FFDeviceReleaseEffect(m_device, m_effect); }
  HRESULT Download() { return FFEffectDownload(m_effect); }
  HRESULT Escape(FFEFFESCAPE* pFFEffectEscape) { return FFEffectEscape(m_effect, pFFEffectEscape); }
  HRESULT GetEffectStatus(FFEffectStatusFlag* pFlags)
  {
    return FFEffectGetEffectStatus(m_effect, pFlags);
  }

  HRESULT GetParameters(FFEFFECT* pFFEffect, FFEffectParameterFlag flags)
  {
    return FFEffectGetParameters(m_effect, pFFEffect, flags);
  }

  HRESULT SetParameters(FFEFFECT* pFFEffect, FFEffectParameterFlag flags)
  {
    return FFEffectSetParameters(m_effect, pFFEffect, flags);
  }

  HRESULT Start(UInt32 iterations, FFEffectStartFlag flags)
  {
    return FFEffectStart(m_effect, iterations, flags);
  }

  HRESULT Stop() { return FFEffectStop(m_effect); }
  HRESULT Unload() { return FFEffectUnload(m_effect); }
};

class FFDeviceAdapter : public IUnknownImpl
{
public:
  FFDeviceObjectReference m_device;

  FFDeviceAdapter(FFDeviceObjectReference device) : m_device(device) {}
  ~FFDeviceAdapter() { FFReleaseDevice(m_device); }
  static HRESULT Create(io_service_t hidDevice, FFDeviceAdapterReference* pDeviceReference)
  {
    FFDeviceObjectReference ref;

    HRESULT hr = FFCreateDevice(hidDevice, &ref);
    if (SUCCEEDED(hr))
      *pDeviceReference = new FFDeviceAdapter(ref);

    return hr;
  }

  HRESULT CreateEffect(CFUUIDRef uuidRef, FFEFFECT* pEffectDefinition,
                       FFEffectAdapterReference* pEffectReference, IUnknown* punkOuter)
  {
    FFEffectObjectReference ref;

    HRESULT hr = FFDeviceCreateEffect(m_device, uuidRef, pEffectDefinition, &ref);
    if (SUCCEEDED(hr))
      *pEffectReference = new FFEffectAdapter(m_device, ref);

    return hr;
  }

  HRESULT Escape(FFEFFESCAPE* pFFEffectEscape) { return FFDeviceEscape(m_device, pFFEffectEscape); }
  HRESULT GetForceFeedbackState(FFState* pFFState)
  {
    return FFDeviceGetForceFeedbackState(m_device, pFFState);
  }

  HRESULT SendForceFeedbackCommand(FFCommandFlag flags)
  {
    return FFDeviceSendForceFeedbackCommand(m_device, flags);
  }

  HRESULT SetCooperativeLevel(void* taskIdentifier, FFCooperativeLevelFlag flags)
  {
    return FFDeviceSetCooperativeLevel(m_device, taskIdentifier, flags);
  }

  HRESULT SetProperty(FFProperty property, const LPDIPROPHEADER pdiph)
  {
    // There are only two properties supported
    if (property != DIPROP_FFGAIN && property != DIPROP_AUTOCENTER)
      return DIERR_UNSUPPORTED;

    // And they are both device properties
    if (pdiph->dwHow != DIPH_DEVICE)
      return DIERR_INVALIDPARAM;

    UInt32 value = ((const LPDIPROPDWORD)pdiph)->dwData;
    return FFDeviceSetForceFeedbackProperty(m_device, property, &value);
  }
};
}  // namespace ciface::ForceFeedback
