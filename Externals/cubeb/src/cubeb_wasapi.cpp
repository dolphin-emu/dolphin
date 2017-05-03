/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#define _WIN32_WINNT 0x0600
#define NOMINMAX

#include <initguid.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <windef.h>
#include <audioclient.h>
#include <devicetopology.h>
#include <process.h>
#include <avrt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <cmath>
#include <algorithm>
#include <memory>
#include <limits>
#include <atomic>
#include <vector>

#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include "cubeb_mixer.h"
#include "cubeb_resampler.h"
#include "cubeb_utils.h"

#ifndef PKEY_Device_FriendlyName
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName,    0xa45c254e, 0xdf1c, 0x4efd, 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, 14);    // DEVPROP_TYPE_STRING
#endif
#ifndef PKEY_Device_InstanceId
DEFINE_PROPERTYKEY(PKEY_Device_InstanceId,      0x78c34fc8, 0x104a, 0x4aca, 0x9e, 0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57, 0x00000100); //    VT_LPWSTR
#endif

namespace {
struct com_heap_ptr_deleter {
  void operator()(void * ptr) const noexcept {
    CoTaskMemFree(ptr);
  }
};

template <typename T>
using com_heap_ptr = std::unique_ptr<T, com_heap_ptr_deleter>;

template<typename T, size_t N>
constexpr size_t
ARRAY_LENGTH(T(&)[N])
{
  return N;
}

template <typename T>
class no_addref_release : public T {
  ULONG STDMETHODCALLTYPE AddRef() = 0;
  ULONG STDMETHODCALLTYPE Release() = 0;
};

template <typename T>
class com_ptr {
public:
  com_ptr() noexcept = default;

  com_ptr(com_ptr const & other) noexcept = delete;
  com_ptr & operator=(com_ptr const & other) noexcept = delete;
  T ** operator&() const noexcept = delete;

  ~com_ptr() noexcept {
    release();
  }

  com_ptr(com_ptr && other) noexcept
    : ptr(other.ptr)
  {
    other.ptr = nullptr;
  }

  com_ptr & operator=(com_ptr && other) noexcept {
    if (ptr != other.ptr) {
      release();
      ptr = other.ptr;
      other.ptr = nullptr;
    }
    return *this;
  }

  explicit operator bool() const noexcept {
    return nullptr != ptr;
  }

  no_addref_release<T> * operator->() const noexcept {
    return static_cast<no_addref_release<T> *>(ptr);
  }

  T * get() const noexcept {
    return ptr;
  }

  T ** receive() noexcept {
    XASSERT(ptr == nullptr);
    return &ptr;
  }

  void ** receive_vpp() noexcept {
    return reinterpret_cast<void **>(receive());
  }

  com_ptr & operator=(std::nullptr_t) noexcept {
    release();
    return *this;
  }

  void reset(T * p = nullptr) noexcept {
    release();
    ptr = p;
  }

private:
  void release() noexcept {
    T * temp = ptr;

    if (temp) {
      ptr = nullptr;
      temp->Release();
    }
  }

  T * ptr = nullptr;
};

struct auto_com {
  auto_com() {
    result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  }
  ~auto_com() {
    if (result == RPC_E_CHANGED_MODE) {
      // This is not an error, COM was not initialized by this function, so it is
      // not necessary to uninit it.
      LOG("COM was already initialized in STA.");
    } else if (result == S_FALSE) {
      // This is not an error. We are allowed to call CoInitializeEx more than
      // once, as long as it is matches by an CoUninitialize call.
      // We do that in the dtor which is guaranteed to be called.
      LOG("COM was already initialized in MTA");
    }
    if (SUCCEEDED(result)) {
      CoUninitialize();
    }
  }
  bool ok() {
    return result == RPC_E_CHANGED_MODE || SUCCEEDED(result);
  }
private:
  HRESULT result;
};

struct mixing_wrapper {
  virtual void mix(void * const input_buffer, long input_frames, void * output_buffer,
                   cubeb_stream_params const * stream_params,
                   cubeb_stream_params const * mixer_params) = 0;
  virtual ~mixing_wrapper() {};
};

template <typename T>
struct mixing_impl : public mixing_wrapper {

  typedef void (*downmix_func)(T * const, long, T *,
                             unsigned int, unsigned int,
                             cubeb_channel_layout, cubeb_channel_layout);
  typedef void (*upmix_func)(T * const, long, T *,
                             unsigned int, unsigned int);

  mixing_impl(downmix_func dmfunc, upmix_func umfunc) {
    downmix_wrapper = dmfunc;
    upmix_wrapper = umfunc;
  }

  ~mixing_impl() {}

  void mix(void * const input_buffer, long input_frames, void * output_buffer,
               cubeb_stream_params const * stream_params,
               cubeb_stream_params const * mixer_params) override {
    T * const in = static_cast<T *>(input_buffer);
    T * out = static_cast<T *>(output_buffer);
    if (cubeb_should_upmix(stream_params, mixer_params)) {
      upmix_wrapper(in, input_frames, out, stream_params->channels, mixer_params->channels);
    } else if (cubeb_should_downmix(stream_params, mixer_params)) {
      downmix_wrapper(in, input_frames, out, stream_params->channels, mixer_params->channels, stream_params->layout, mixer_params->layout);
    }
  }

  downmix_func downmix_wrapper;
  upmix_func upmix_wrapper;
};

extern cubeb_ops const wasapi_ops;

int wasapi_stream_stop(cubeb_stream * stm);
int wasapi_stream_start(cubeb_stream * stm);
void close_wasapi_stream(cubeb_stream * stm);
int setup_wasapi_stream(cubeb_stream * stm);
static char const * wstr_to_utf8(wchar_t const * str);
static std::unique_ptr<wchar_t const []> utf8_to_wstr(char const * str);

}

struct cubeb {
  cubeb_ops const * ops = &wasapi_ops;
};

class wasapi_endpoint_notification_client;

/* We have three possible callbacks we can use with a stream:
 * - input only
 * - output only
 * - synchronized input and output
 *
 * Returns true when we should continue to play, false otherwise.
 */
typedef bool (*wasapi_refill_callback)(cubeb_stream * stm);

struct cubeb_stream {
  cubeb * context = nullptr;
  /* Mixer pameters. We need to convert the input stream to this
     samplerate/channel layout, as WASAPI does not resample nor upmix
     itself. */
  cubeb_stream_params input_mix_params = { CUBEB_SAMPLE_FLOAT32NE, 0, 0, CUBEB_LAYOUT_UNDEFINED };
  cubeb_stream_params output_mix_params = { CUBEB_SAMPLE_FLOAT32NE, 0, 0, CUBEB_LAYOUT_UNDEFINED };
  /* Stream parameters. This is what the client requested,
   * and what will be presented in the callback. */
  cubeb_stream_params input_stream_params = { CUBEB_SAMPLE_FLOAT32NE, 0, 0, CUBEB_LAYOUT_UNDEFINED };
  cubeb_stream_params output_stream_params = { CUBEB_SAMPLE_FLOAT32NE, 0, 0, CUBEB_LAYOUT_UNDEFINED };
  /* The input and output device, or NULL for default. */
  std::unique_ptr<const wchar_t[]> input_device;
  std::unique_ptr<const wchar_t[]> output_device;
  /* The latency initially requested for this stream, in frames. */
  unsigned latency = 0;
  cubeb_state_callback state_callback = nullptr;
  cubeb_data_callback data_callback = nullptr;
  wasapi_refill_callback refill_callback = nullptr;
  void * user_ptr = nullptr;
  /* Lifetime considerations:
     - client, render_client, audio_clock and audio_stream_volume are interface
       pointer to the IAudioClient.
     - The lifetime for device_enumerator and notification_client, resampler,
       mix_buffer are the same as the cubeb_stream instance. */

  /* Main handle on the WASAPI stream. */
  com_ptr<IAudioClient> output_client;
  /* Interface pointer to use the event-driven interface. */
  com_ptr<IAudioRenderClient> render_client;
  /* Interface pointer to use the volume facilities. */
  com_ptr<IAudioStreamVolume> audio_stream_volume;
  /* Interface pointer to use the stream audio clock. */
  com_ptr<IAudioClock> audio_clock;
  /* Frames written to the stream since it was opened. Reset on device
     change. Uses mix_params.rate. */
  UINT64 frames_written = 0;
  /* Frames written to the (logical) stream since it was first
     created. Updated on device change. Uses stream_params.rate. */
  UINT64 total_frames_written = 0;
  /* Last valid reported stream position.  Used to ensure the position
     reported by stream_get_position increases monotonically. */
  UINT64 prev_position = 0;
  /* Device enumerator to be able to be notified when the default
     device change. */
  com_ptr<IMMDeviceEnumerator> device_enumerator;
  /* Device notification client, to be able to be notified when the default
     audio device changes and route the audio to the new default audio output
     device */
  com_ptr<wasapi_endpoint_notification_client> notification_client;
  /* Main andle to the WASAPI capture stream. */
  com_ptr<IAudioClient> input_client;
  /* Interface to use the event driven capture interface */
  com_ptr<IAudioCaptureClient> capture_client;
  /* This event is set by the stream_stop and stream_destroy
     function, so the render loop can exit properly. */
  HANDLE shutdown_event = 0;
  /* Set by OnDefaultDeviceChanged when a stream reconfiguration is required.
     The reconfiguration is handled by the render loop thread. */
  HANDLE reconfigure_event = 0;
  /* This is set by WASAPI when we should refill the stream. */
  HANDLE refill_event = 0;
  /* This is set by WASAPI when we should read from the input stream. In
   * practice, we read from the input stream in the output callback, so
   * this is not used, but it is necessary to start getting input data. */
  HANDLE input_available_event = 0;
  /* Each cubeb_stream has its own thread. */
  HANDLE thread = 0;
  /* The lock protects all members that are touched by the render thread or
     change during a device reset, including: audio_clock, audio_stream_volume,
     client, frames_written, mix_params, total_frames_written, prev_position. */
  owned_critical_section stream_reset_lock;
  /* Maximum number of frames that can be passed down in a callback. */
  uint32_t input_buffer_frame_count = 0;
  /* Maximum number of frames that can be requested in a callback. */
  uint32_t output_buffer_frame_count = 0;
  /* Resampler instance. Resampling will only happen if necessary. */
  std::unique_ptr<cubeb_resampler, decltype(&cubeb_resampler_destroy)> resampler = { nullptr, cubeb_resampler_destroy };
  /* Mixing interface */
  std::unique_ptr<mixing_wrapper> mixing;
  /* A buffer for up/down mixing multi-channel audio. */
  std::vector<BYTE> mix_buffer;
  /* WASAPI input works in "packets". We re-linearize the audio packets
   * into this buffer before handing it to the resampler. */
  std::unique_ptr<auto_array_wrapper> linear_input_buffer;
  /* Bytes per sample. This multiplied by the number of channels is the number
   * of bytes per frame. */
  size_t bytes_per_sample = 0;
  /* WAVEFORMATEXTENSIBLE sub-format: either PCM or float. */
  GUID waveformatextensible_sub_format = GUID_NULL;
  /* Stream volume.  Set via stream_set_volume and used to reset volume on
     device changes. */
  float volume = 1.0;
  /* True if the stream is draining. */
  bool draining = false;
  /* True when we've destroyed the stream. This pointer is leaked on stream
   * destruction if we could not join the thread. */
  std::atomic<std::atomic<bool>*> emergency_bailout;
};

class wasapi_endpoint_notification_client : public IMMNotificationClient
{
public:
  /* The implementation of MSCOM was copied from MSDN. */
  ULONG STDMETHODCALLTYPE
  AddRef()
  {
    return InterlockedIncrement(&ref_count);
  }

  ULONG STDMETHODCALLTYPE
  Release()
  {
    ULONG ulRef = InterlockedDecrement(&ref_count);
    if (0 == ulRef) {
      delete this;
    }
    return ulRef;
  }

  HRESULT STDMETHODCALLTYPE
  QueryInterface(REFIID riid, VOID **ppvInterface)
  {
    if (__uuidof(IUnknown) == riid) {
      AddRef();
      *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
      AddRef();
      *ppvInterface = (IMMNotificationClient*)this;
    } else {
      *ppvInterface = NULL;
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  wasapi_endpoint_notification_client(HANDLE event)
    : ref_count(1)
    , reconfigure_event(event)
  { }

  virtual ~wasapi_endpoint_notification_client()
  { }

  HRESULT STDMETHODCALLTYPE
  OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR device_id)
  {
    LOG("Audio device default changed.");

    /* we only support a single stream type for now. */
    if (flow != eRender && role != eConsole) {
      return S_OK;
    }

    BOOL ok = SetEvent(reconfigure_event);
    if (!ok) {
      LOG("SetEvent on reconfigure_event failed: %lx", GetLastError());
    }

    return S_OK;
  }

  /* The remaining methods are not implemented, they simply log when called (if
     log is enabled), for debugging. */
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR device_id)
  {
    LOG("Audio device added.");
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR device_id)
  {
    LOG("Audio device removed.");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnDeviceStateChanged(LPCWSTR device_id, DWORD new_state)
  {
    LOG("Audio device state changed.");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  OnPropertyValueChanged(LPCWSTR device_id, const PROPERTYKEY key)
  {
    LOG("Audio device property value changed.");
    return S_OK;
  }
private:
  /* refcount for this instance, necessary to implement MSCOM semantics. */
  LONG ref_count;
  HANDLE reconfigure_event;
};

namespace {
bool has_input(cubeb_stream * stm)
{
  return stm->input_stream_params.rate != 0;
}

bool has_output(cubeb_stream * stm)
{
  return stm->output_stream_params.rate != 0;
}

double stream_to_mix_samplerate_ratio(cubeb_stream_params & stream, cubeb_stream_params & mixer)
{
  return double(stream.rate) / mixer.rate;
}

/* Convert the channel layout into the corresponding KSAUDIO_CHANNEL_CONFIG.
   See more: https://msdn.microsoft.com/en-us/library/windows/hardware/ff537083(v=vs.85).aspx */
#define MASK_DUAL_MONO      (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
#define MASK_DUAL_MONO_LFE  (MASK_DUAL_MONO | SPEAKER_LOW_FREQUENCY)
#define MASK_MONO           (KSAUDIO_SPEAKER_MONO)
#define MASK_MONO_LFE       (MASK_MONO | SPEAKER_LOW_FREQUENCY)
#define MASK_STEREO         (KSAUDIO_SPEAKER_STEREO)
#define MASK_STEREO_LFE     (MASK_STEREO | SPEAKER_LOW_FREQUENCY)
#define MASK_3F             (MASK_STEREO | SPEAKER_FRONT_CENTER)
#define MASK_3F_LFE         (MASK_3F | SPEAKER_LOW_FREQUENCY)
#define MASK_2F1            (MASK_STEREO | SPEAKER_BACK_CENTER)
#define MASK_2F1_LFE        (MASK_2F1 | SPEAKER_LOW_FREQUENCY)
#define MASK_3F1            (KSAUDIO_SPEAKER_SURROUND)
#define MASK_3F1_LFE        (MASK_3F1 | SPEAKER_LOW_FREQUENCY)
#define MASK_2F2            (MASK_STEREO | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)
#define MASK_2F2_LFE        (MASK_2F2 | SPEAKER_LOW_FREQUENCY)
#define MASK_3F2            (MASK_3F | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)
#define MASK_3F2_LFE        (KSAUDIO_SPEAKER_5POINT1_SURROUND)
#define MASK_3F3R_LFE       (MASK_3F2_LFE | SPEAKER_BACK_CENTER)
#define MASK_3F4_LFE        (KSAUDIO_SPEAKER_7POINT1_SURROUND)

static DWORD
channel_layout_to_mask(cubeb_channel_layout layout)
{
  XASSERT(layout > CUBEB_LAYOUT_UNDEFINED && layout < CUBEB_LAYOUT_MAX &&
          "This mask conversion is not allowed.");

  // This variable may be used for multiple times, so we should avoid to
  // allocate it in stack, or it will be created and removed repeatedly.
  // Use static to allocate this local variable in data space instead of stack.
  static DWORD map[CUBEB_LAYOUT_MAX] = {
    0,                    // CUBEB_LAYOUT_UNDEFINED (this won't be used.)
    MASK_DUAL_MONO,       // CUBEB_LAYOUT_DUAL_MONO
    MASK_DUAL_MONO_LFE,   // CUBEB_LAYOUT_DUAL_MONO_LFE
    MASK_MONO,            // CUBEB_LAYOUT_MONO
    MASK_MONO_LFE,        // CUBEB_LAYOUT_MONO_LFE
    MASK_STEREO,          // CUBEB_LAYOUT_STEREO
    MASK_STEREO_LFE,      // CUBEB_LAYOUT_STEREO_LFE
    MASK_3F,              // CUBEB_LAYOUT_3F
    MASK_3F_LFE,          // CUBEB_LAYOUT_3F_LFE
    MASK_2F1,             // CUBEB_LAYOUT_2F1
    MASK_2F1_LFE,         // CUBEB_LAYOUT_2F1_LFE
    MASK_3F1,             // CUBEB_LAYOUT_3F1
    MASK_3F1_LFE,         // CUBEB_LAYOUT_3F1_LFE
    MASK_2F2,             // CUBEB_LAYOUT_2F2
    MASK_2F2_LFE,         // CUBEB_LAYOUT_2F2_LFE
    MASK_3F2,             // CUBEB_LAYOUT_3F2
    MASK_3F2_LFE,         // CUBEB_LAYOUT_3F2_LFE
    MASK_3F3R_LFE,        // CUBEB_LAYOUT_3F3R_LFE
    MASK_3F4_LFE,         // CUBEB_LAYOUT_3F4_LFE
  };
  return map[layout];
}

cubeb_channel_layout
mask_to_channel_layout(DWORD mask)
{
  switch (mask) {
    // MASK_DUAL_MONO(_LFE) is same as STEREO(_LFE), so we skip it.
    case MASK_MONO: return CUBEB_LAYOUT_MONO;
    case MASK_MONO_LFE: return CUBEB_LAYOUT_MONO_LFE;
    case MASK_STEREO: return CUBEB_LAYOUT_STEREO;
    case MASK_STEREO_LFE: return CUBEB_LAYOUT_STEREO_LFE;
    case MASK_3F: return CUBEB_LAYOUT_3F;
    case MASK_3F_LFE: return CUBEB_LAYOUT_3F_LFE;
    case MASK_2F1: return CUBEB_LAYOUT_2F1;
    case MASK_2F1_LFE: return CUBEB_LAYOUT_2F1_LFE;
    case MASK_3F1: return CUBEB_LAYOUT_3F1;
    case MASK_3F1_LFE: return CUBEB_LAYOUT_3F1_LFE;
    case MASK_2F2: return CUBEB_LAYOUT_2F2;
    case MASK_2F2_LFE: return CUBEB_LAYOUT_2F2_LFE;
    case MASK_3F2: return CUBEB_LAYOUT_3F2;
    case MASK_3F2_LFE: return CUBEB_LAYOUT_3F2_LFE;
    case MASK_3F3R_LFE: return CUBEB_LAYOUT_3F3R_LFE;
    case MASK_3F4_LFE: return CUBEB_LAYOUT_3F4_LFE;
    default: return CUBEB_LAYOUT_UNDEFINED;
  }
}

uint32_t
get_rate(cubeb_stream * stm)
{
  return has_input(stm) ? stm->input_stream_params.rate
                        : stm->output_stream_params.rate;
}

uint32_t
hns_to_ms(REFERENCE_TIME hns)
{
  return static_cast<uint32_t>(hns / 10000);
}

uint32_t
hns_to_frames(cubeb_stream * stm, REFERENCE_TIME hns)
{
  return hns_to_ms(hns * get_rate(stm)) / 1000;
}

uint32_t
hns_to_frames(uint32_t rate, REFERENCE_TIME hns)
{
  return hns_to_ms(hns * rate) / 1000;
}

REFERENCE_TIME
frames_to_hns(cubeb_stream * stm, uint32_t frames)
{
   return frames * 1000 / get_rate(stm);
}

/* This returns the size of a frame in the stream, before the eventual upmix
   occurs. */
static size_t
frames_to_bytes_before_mix(cubeb_stream * stm, size_t frames)
{
  // This is called only when we has a output client.
  XASSERT(has_output(stm));
  return stm->output_stream_params.channels * stm->bytes_per_sample * frames;
}

/* This function handles the processing of the input and output audio,
 * converting it to rate and channel layout specified at initialization.
 * It then calls the data callback, via the resampler. */
long
refill(cubeb_stream * stm, void * input_buffer, long input_frames_count,
       void * output_buffer, long output_frames_needed)
{
  /* If we need to upmix after resampling, resample into the mix buffer to
     avoid a copy. */
  void * dest = nullptr;
  if (has_output(stm)) {
    if (cubeb_should_mix(&stm->output_stream_params, &stm->output_mix_params)) {
      dest = stm->mix_buffer.data();
    } else {
      dest = output_buffer;
    }
  }

  long out_frames = cubeb_resampler_fill(stm->resampler.get(),
                                         input_buffer,
                                         &input_frames_count,
                                         dest,
                                         output_frames_needed);
  /* TODO: Report out_frames < 0 as an error via the API. */
  XASSERT(out_frames >= 0);

  {
    auto_lock lock(stm->stream_reset_lock);
    stm->frames_written += out_frames;
  }

  /* Go in draining mode if we got fewer frames than requested. */
  if (out_frames < output_frames_needed) {
    LOG("start draining.");
    stm->draining = true;
  }

  /* If this is not true, there will be glitches.
     It is alright to have produced less frames if we are draining, though. */
  XASSERT(out_frames == output_frames_needed || stm->draining || !has_output(stm));

  if (has_output(stm) && cubeb_should_mix(&stm->output_stream_params, &stm->output_mix_params)) {
    stm->mixing->mix(dest, out_frames, output_buffer, &stm->output_stream_params, &stm->output_mix_params);
  }

  return out_frames;
}

/* This helper grabs all the frames available from a capture client, put them in
 * linear_input_buffer. linear_input_buffer should be cleared before the
 * callback exits. */
bool get_input_buffer(cubeb_stream * stm)
{
  HRESULT hr;
  UINT32 padding_in;

  XASSERT(has_input(stm));

  hr = stm->input_client->GetCurrentPadding(&padding_in);
  if (FAILED(hr)) {
    LOG("Failed to get padding");
    return false;
  }
  XASSERT(padding_in <= stm->input_buffer_frame_count);
  UINT32 total_available_input = padding_in;

  BYTE * input_packet = NULL;
  DWORD flags;
  UINT64 dev_pos;
  UINT32 next;
  /* Get input packets until we have captured enough frames, and put them in a
   * contiguous buffer. */
  uint32_t offset = 0;
  while (offset != total_available_input) {
    hr = stm->capture_client->GetNextPacketSize(&next);
    if (FAILED(hr)) {
      LOG("cannot get next packet size: %lx", hr);
      return false;
    }
    /* This can happen if the capture stream has stopped. Just return in this
     * case. */
    if (!next) {
      break;
    }

    UINT32 packet_size;
    hr = stm->capture_client->GetBuffer(&input_packet,
                                        &packet_size,
                                        &flags,
                                        &dev_pos,
                                        NULL);
    if (FAILED(hr)) {
      LOG("GetBuffer failed for capture: %lx", hr);
      return false;
    }
    XASSERT(packet_size == next);
    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
      LOG("insert silence: ps=%u", packet_size);
      stm->linear_input_buffer->push_silence(packet_size * stm->input_stream_params.channels);
    } else {
      if (cubeb_should_mix(&stm->input_mix_params, &stm->input_stream_params)) {
        bool ok = stm->linear_input_buffer->reserve(stm->linear_input_buffer->length() +
                                                   packet_size * stm->input_stream_params.channels);
        XASSERT(ok);
        stm->mixing->mix(input_packet, packet_size,
                         stm->linear_input_buffer->end(),
                         &stm->input_mix_params,
                         &stm->input_stream_params);
        stm->linear_input_buffer->set_length(stm->linear_input_buffer->length() + packet_size * stm->input_stream_params.channels);
      } else {
        stm->linear_input_buffer->push(input_packet,
                                      packet_size * stm->input_stream_params.channels);
      }
    }
    hr = stm->capture_client->ReleaseBuffer(packet_size);
    if (FAILED(hr)) {
      LOG("FAILED to release intput buffer");
      return false;
    }
    offset += packet_size;
  }

  XASSERT(stm->linear_input_buffer->length() >= total_available_input &&
          offset == total_available_input);

  return true;
}

/* Get an output buffer from the render_client. It has to be released before
 * exiting the callback. */
bool get_output_buffer(cubeb_stream * stm, void *& buffer, size_t & frame_count)
{
  UINT32 padding_out;
  HRESULT hr;

  XASSERT(has_output(stm));

  hr = stm->output_client->GetCurrentPadding(&padding_out);
  if (FAILED(hr)) {
    LOG("Failed to get padding: %lx", hr);
    return false;
  }
  XASSERT(padding_out <= stm->output_buffer_frame_count);

  if (stm->draining) {
    if (padding_out == 0) {
      LOG("Draining finished.");
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      return false;
    }
    LOG("Draining.");
    return true;
  }

  frame_count = stm->output_buffer_frame_count - padding_out;
  BYTE * output_buffer;

  hr = stm->render_client->GetBuffer(frame_count, &output_buffer);
  if (FAILED(hr)) {
    LOG("cannot get render buffer");
    return false;
  }

  buffer = output_buffer;

  return true;
}

/**
 * This function gets input data from a input device, and pass it along with an
 * output buffer to the resamplers.  */
bool
refill_callback_duplex(cubeb_stream * stm)
{
  HRESULT hr;
  void * output_buffer = nullptr;
  size_t output_frames = 0;
  size_t input_frames;
  bool rv;

  XASSERT(has_input(stm) && has_output(stm));

  rv = get_input_buffer(stm);
  if (!rv) {
    return rv;
  }

  input_frames = stm->linear_input_buffer->length() / stm->input_stream_params.channels;
  if (!input_frames) {
    return true;
  }

  rv = get_output_buffer(stm, output_buffer, output_frames);
  if (!rv) {
    hr = stm->render_client->ReleaseBuffer(output_frames, 0);
    return rv;
  }

  /* This can only happen when debugging, and having breakpoints set in the
   * callback in a way that it makes the stream underrun. */
  if (output_frames == 0) {
    return true;
  }


  ALOGV("Duplex callback: input frames: %Iu, output frames: %Iu",
        stm->linear_input_buffer->length(), output_frames);

  refill(stm,
         stm->linear_input_buffer->data(),
         stm->linear_input_buffer->length(),
         output_buffer,
         output_frames);

  stm->linear_input_buffer->clear();

  hr = stm->render_client->ReleaseBuffer(output_frames, 0);
  if (FAILED(hr)) {
    LOG("failed to release buffer: %lx", hr);
    return false;
  }
  return true;
}

bool
refill_callback_input(cubeb_stream * stm)
{
  bool rv;

  XASSERT(has_input(stm) && !has_output(stm));

  rv = get_input_buffer(stm);
  if (!rv) {
    return rv;
  }

  // This can happen at the very beginning of the stream.
  if (!stm->linear_input_buffer->length()) {
    return true;
  }

  ALOGV("Input callback: input frames: %Iu", stm->linear_input_buffer->length());

  long read = refill(stm,
                     stm->linear_input_buffer->data(),
                     stm->linear_input_buffer->length(),
                     nullptr,
                     0);

  XASSERT(read >= 0);

  stm->linear_input_buffer->clear();

  return !stm->draining;
}

bool
refill_callback_output(cubeb_stream * stm)
{
  bool rv;
  HRESULT hr;
  void * output_buffer = nullptr;
  size_t output_frames = 0;

  XASSERT(!has_input(stm) && has_output(stm));

  rv = get_output_buffer(stm, output_buffer, output_frames);
  if (!rv) {
    return rv;
  }

  if (stm->draining || output_frames == 0) {
    return true;
  }

  long got = refill(stm,
                    nullptr,
                    0,
                    output_buffer,
                    output_frames);

  ALOGV("Output callback: output frames requested: %Iu, got %ld",
        output_frames, got);

  XASSERT(got >= 0);
  XASSERT((unsigned long) got == output_frames || stm->draining);

  hr = stm->render_client->ReleaseBuffer(got, 0);
  if (FAILED(hr)) {
    LOG("failed to release buffer: %lx", hr);
    return false;
  }

  return (unsigned long) got == output_frames || stm->draining;
}

static unsigned int __stdcall
wasapi_stream_render_loop(LPVOID stream)
{
  cubeb_stream * stm = static_cast<cubeb_stream *>(stream);
  std::atomic<bool> * emergency_bailout = stm->emergency_bailout;

  bool is_playing = true;
  HANDLE wait_array[4] = {
    stm->shutdown_event,
    stm->reconfigure_event,
    stm->refill_event,
    stm->input_available_event
  };
  HANDLE mmcss_handle = NULL;
  HRESULT hr = 0;
  DWORD mmcss_task_index = 0;
  auto_com com;
  if (!com.ok()) {
    LOG("COM initialization failed on render_loop thread.");
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
    return 0;
  }

  /* We could consider using "Pro Audio" here for WebAudio and
     maybe WebRTC. */
  mmcss_handle = AvSetMmThreadCharacteristicsA("Audio", &mmcss_task_index);
  if (!mmcss_handle) {
    /* This is not fatal, but we might glitch under heavy load. */
    LOG("Unable to use mmcss to bump the render thread priority: %lx", GetLastError());
  }

  // This has already been nulled out, simply exit.
  if (!emergency_bailout) {
    is_playing = false;
  }

  /* WaitForMultipleObjects timeout can trigger in cases where we don't want to
     treat it as a timeout, such as across a system sleep/wake cycle.  Trigger
     the timeout error handling only when the timeout_limit is reached, which is
     reset on each successful loop. */
  unsigned timeout_count = 0;
  const unsigned timeout_limit = 5;
  while (is_playing) {
    // We want to check the emergency bailout variable before a
    // and after the WaitForMultipleObject, because the handles WaitForMultipleObjects
    // is going to wait on might have been closed already.
    if (*emergency_bailout) {
      delete emergency_bailout;
      return 0;
    }
    DWORD waitResult = WaitForMultipleObjects(ARRAY_LENGTH(wait_array),
                                              wait_array,
                                              FALSE,
                                              1000);
    if (*emergency_bailout) {
      delete emergency_bailout;
      return 0;
    }
    if (waitResult != WAIT_TIMEOUT) {
      timeout_count = 0;
    }
    switch (waitResult) {
    case WAIT_OBJECT_0: { /* shutdown */
      is_playing = false;
      /* We don't check if the drain is actually finished here, we just want to
         shutdown. */
      if (stm->draining) {
        stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      }
      continue;
    }
    case WAIT_OBJECT_0 + 1: { /* reconfigure */
      XASSERT(stm->output_client || stm->input_client);
      LOG("Reconfiguring the stream");
      /* Close the stream */
      if (stm->output_client) {
        stm->output_client->Stop();
        LOG("Output stopped.");
      }
      if (stm->input_client) {
        stm->input_client->Stop();
        LOG("Input stopped.");
      }
      {
        auto_lock lock(stm->stream_reset_lock);
        close_wasapi_stream(stm);
        LOG("Stream closed.");
        /* Reopen a stream and start it immediately. This will automatically pick the
           new default device for this role. */
        int r = setup_wasapi_stream(stm);
        if (r != CUBEB_OK) {
          LOG("Error setting up the stream during reconfigure.");
          /* Don't destroy the stream here, since we expect the caller to do
             so after the error has propagated via the state callback. */
          is_playing = false;
          hr = E_FAIL;
          continue;
        }
        LOG("Stream setup successfuly.");
      }
      XASSERT(stm->output_client || stm->input_client);
      if (stm->output_client) {
        stm->output_client->Start();
        LOG("Output started after reconfigure.");
      }
      if (stm->input_client) {
        stm->input_client->Start();
        LOG("Input started after reconfigure.");
      }
      break;
    }
    case WAIT_OBJECT_0 + 2:  /* refill */
      XASSERT((has_input(stm) && has_output(stm)) ||
              (!has_input(stm) && has_output(stm)));
      is_playing = stm->refill_callback(stm);
      break;
    case WAIT_OBJECT_0 + 3: /* input available */
      if (has_input(stm) && has_output(stm)) { continue; }
      is_playing = stm->refill_callback(stm);
      break;
    case WAIT_TIMEOUT:
      XASSERT(stm->shutdown_event == wait_array[0]);
      if (++timeout_count >= timeout_limit) {
        LOG("Render loop reached the timeout limit.");
        is_playing = false;
        hr = E_FAIL;
      }
      break;
    default:
      LOG("case %lu not handled in render loop.", waitResult);
      abort();
    }
  }

  if (FAILED(hr)) {
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
  }

  if (mmcss_handle) {
    AvRevertMmThreadCharacteristics(mmcss_handle);
  }

  return 0;
}

void wasapi_destroy(cubeb * context);

HRESULT register_notification_client(cubeb_stream * stm)
{
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(stm->device_enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return hr;
  }

  stm->notification_client.reset(new wasapi_endpoint_notification_client(stm->reconfigure_event));

  hr = stm->device_enumerator->RegisterEndpointNotificationCallback(stm->notification_client.get());
  if (FAILED(hr)) {
    LOG("Could not register endpoint notification callback: %lx", hr);
    stm->notification_client = nullptr;
    stm->device_enumerator = nullptr;
  }

  return hr;
}

HRESULT unregister_notification_client(cubeb_stream * stm)
{
  XASSERT(stm);
  HRESULT hr;

  if (!stm->device_enumerator) {
    return S_OK;
  }

  hr = stm->device_enumerator->UnregisterEndpointNotificationCallback(stm->notification_client.get());
  if (FAILED(hr)) {
    // We can't really do anything here, we'll probably leak the
    // notification client, but we can at least release the enumerator.
    stm->device_enumerator = nullptr;
    return S_OK;
  }

  stm->notification_client = nullptr;
  stm->device_enumerator = nullptr;

  return S_OK;
}

HRESULT get_endpoint(com_ptr<IMMDevice> & device, LPCWSTR devid)
{
  com_ptr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return hr;
  }

  hr = enumerator->GetDevice(devid, device.receive());
  if (FAILED(hr)) {
    LOG("Could not get device: %lx", hr);
    return hr;
  }

  return S_OK;
}

HRESULT get_default_endpoint(com_ptr<IMMDevice> & device, EDataFlow direction)
{
  com_ptr<IMMDeviceEnumerator> enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return hr;
  }
  hr = enumerator->GetDefaultAudioEndpoint(direction, eConsole, device.receive());
  if (FAILED(hr)) {
    LOG("Could not get default audio endpoint: %lx", hr);
    return hr;
  }

  return ERROR_SUCCESS;
}

double
current_stream_delay(cubeb_stream * stm)
{
  stm->stream_reset_lock.assert_current_thread_owns();

  /* If the default audio endpoint went away during playback and we weren't
     able to configure a new one, it's possible the caller may call this
     before the error callback has propogated back. */
  if (!stm->audio_clock) {
    return 0;
  }

  UINT64 freq;
  HRESULT hr = stm->audio_clock->GetFrequency(&freq);
  if (FAILED(hr)) {
    LOG("GetFrequency failed: %lx", hr);
    return 0;
  }

  UINT64 pos;
  hr = stm->audio_clock->GetPosition(&pos, NULL);
  if (FAILED(hr)) {
    LOG("GetPosition failed: %lx", hr);
    return 0;
  }

  double cur_pos = static_cast<double>(pos) / freq;
  double max_pos = static_cast<double>(stm->frames_written)  / stm->output_mix_params.rate;
  double delay = max_pos - cur_pos;
  XASSERT(delay >= 0);

  return delay;
}

int
stream_set_volume(cubeb_stream * stm, float volume)
{
  stm->stream_reset_lock.assert_current_thread_owns();

  if (!stm->audio_stream_volume) {
    return CUBEB_ERROR;
  }

  uint32_t channels;
  HRESULT hr = stm->audio_stream_volume->GetChannelCount(&channels);
  if (hr != S_OK) {
    LOG("could not get the channel count: %lx", hr);
    return CUBEB_ERROR;
  }

  /* up to 9.1 for now */
  if (channels > 10) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  float volumes[10];
  for (uint32_t i = 0; i < channels; i++) {
    volumes[i] = volume;
  }

  hr = stm->audio_stream_volume->SetAllVolumes(channels,  volumes);
  if (hr != S_OK) {
    LOG("could not set the channels volume: %lx", hr);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}
} // namespace anonymous

extern "C" {
int wasapi_init(cubeb ** context, char const * context_name)
{
  HRESULT hr;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  /* We don't use the device yet, but need to make sure we can initialize one
     so that this backend is not incorrectly enabled on platforms that don't
     support WASAPI. */
  com_ptr<IMMDevice> device;
  hr = get_default_endpoint(device, eRender);
  if (FAILED(hr)) {
    LOG("Could not get device: %lx", hr);
    return CUBEB_ERROR;
  }

  cubeb * ctx = new cubeb();

  ctx->ops = &wasapi_ops;

  *context = ctx;

  return CUBEB_OK;
}
}

namespace {
bool stop_and_join_render_thread(cubeb_stream * stm)
{
  bool rv = true;
  LOG("Stop and join render thread.");
  if (!stm->thread) {
    LOG("No thread present.");
    return true;
  }

  // If we've already leaked the thread, just return,
  // there is not much we can do.
  if (!stm->emergency_bailout.load()) {
    return false;
  }

  BOOL ok = SetEvent(stm->shutdown_event);
  if (!ok) {
    LOG("Destroy SetEvent failed: %lx", GetLastError());
  }

  /* Wait five seconds for the rendering thread to return. It's supposed to
   * check its event loop very often, five seconds is rather conservative. */
  DWORD r = WaitForSingleObject(stm->thread, 5000);
  if (r == WAIT_TIMEOUT) {
    /* Something weird happened, leak the thread and continue the shutdown
     * process. */
    *(stm->emergency_bailout) = true;
    // We give the ownership to the rendering thread.
    stm->emergency_bailout = nullptr;
    LOG("Destroy WaitForSingleObject on thread timed out,"
        " leaking the thread: %lx", GetLastError());
    rv = false;
  }
  if (r == WAIT_FAILED) {
    *(stm->emergency_bailout) = true;
    // We give the ownership to the rendering thread.
    stm->emergency_bailout = nullptr;
    LOG("Destroy WaitForSingleObject on thread failed: %lx", GetLastError());
    rv = false;
  }


  // Only attempts to close and null out the thread and event if the
  // WaitForSingleObject above succeeded, so that calling this function again
  // attemps to clean up the thread and event each time.
  if (rv) {
    LOG("Closing thread.");
    CloseHandle(stm->thread);
    stm->thread = NULL;

    CloseHandle(stm->shutdown_event);
    stm->shutdown_event = 0;
  }

  return rv;
}

void wasapi_destroy(cubeb * context)
{
  delete context;
}

char const * wasapi_get_backend_id(cubeb * context)
{
  return "wasapi";
}

int
wasapi_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  HRESULT hr;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  XASSERT(ctx && max_channels);

  com_ptr<IMMDevice> device;
  hr = get_default_endpoint(device, eRender);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  com_ptr<IAudioClient> client;
  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, client.receive_vpp());
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  WAVEFORMATEX * tmp = nullptr;
  hr = client->GetMixFormat(&tmp);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }
  com_heap_ptr<WAVEFORMATEX> mix_format(tmp);

  *max_channels = mix_format->nChannels;

  return CUBEB_OK;
}

int
wasapi_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency_frames)
{
  HRESULT hr;
  REFERENCE_TIME default_period;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  if (params.format != CUBEB_SAMPLE_FLOAT32NE && params.format != CUBEB_SAMPLE_S16NE) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  com_ptr<IMMDevice> device;
  hr = get_default_endpoint(device, eRender);
  if (FAILED(hr)) {
    LOG("Could not get default endpoint: %lx", hr);
    return CUBEB_ERROR;
  }

  com_ptr<IAudioClient> client;
  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, client.receive_vpp());
  if (FAILED(hr)) {
    LOG("Could not activate device for latency: %lx", hr);
    return CUBEB_ERROR;
  }

  /* The second parameter is for exclusive mode, that we don't use. */
  hr = client->GetDevicePeriod(&default_period, NULL);
  if (FAILED(hr)) {
    LOG("Could not get device period: %lx", hr);
    return CUBEB_ERROR;
  }

  LOG("default device period: %I64d", default_period);

  /* According to the docs, the best latency we can achieve is by synchronizing
     the stream and the engine.
     http://msdn.microsoft.com/en-us/library/windows/desktop/dd370871%28v=vs.85%29.aspx */

  *latency_frames = hns_to_frames(params.rate, default_period);

  LOG("Minimum latency in frames: %u", *latency_frames);

  return CUBEB_OK;
}

int
wasapi_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  HRESULT hr;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  com_ptr<IMMDevice> device;
  hr = get_default_endpoint(device, eRender);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  com_ptr<IAudioClient> client;
  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, client.receive_vpp());
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  WAVEFORMATEX * tmp = nullptr;
  hr = client->GetMixFormat(&tmp);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }
  com_heap_ptr<WAVEFORMATEX> mix_format(tmp);

  *rate = mix_format->nSamplesPerSec;

  LOG("Preferred sample rate for output: %u", *rate);

  return CUBEB_OK;
}

int
wasapi_get_preferred_channel_layout(cubeb * context, cubeb_channel_layout * layout)
{
  HRESULT hr;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  com_ptr<IMMDevice> device;
  hr = get_default_endpoint(device, eRender);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  com_ptr<IAudioClient> client;
  hr = device->Activate(__uuidof(IAudioClient),
                        CLSCTX_INPROC_SERVER,
                        NULL, client.receive_vpp());
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  WAVEFORMATEX * tmp = nullptr;
  hr = client->GetMixFormat(&tmp);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }
  com_heap_ptr<WAVEFORMATEX> mix_format(tmp);

  WAVEFORMATEXTENSIBLE * format_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix_format.get());
  *layout = mask_to_channel_layout(format_pcm->dwChannelMask);

  LOG("Preferred channel layout: %s", CUBEB_CHANNEL_LAYOUT_MAPS[*layout].name);

  return CUBEB_OK;
}

void wasapi_stream_destroy(cubeb_stream * stm);

static void
waveformatex_update_derived_properties(WAVEFORMATEX * format)
{
  format->nBlockAlign = format->wBitsPerSample * format->nChannels / 8;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    WAVEFORMATEXTENSIBLE * format_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(format);
    format_pcm->Samples.wValidBitsPerSample = format->wBitsPerSample;
  }
}

/* Based on the mix format and the stream format, try to find a way to play
   what the user requested. */
static void
handle_channel_layout(cubeb_stream * stm,  com_heap_ptr<WAVEFORMATEX> & mix_format, const cubeb_stream_params * stream_params)
{
  XASSERT(stream_params->layout != CUBEB_LAYOUT_UNDEFINED);
  /* The docs say that GetMixFormat is always of type WAVEFORMATEXTENSIBLE [1],
     so the reinterpret_cast below should be safe. In practice, this is not
     true, and we just want to bail out and let the rest of the code find a good
     conversion path instead of trying to make WASAPI do it by itself.
     [1]: http://msdn.microsoft.com/en-us/library/windows/desktop/dd370811%28v=vs.85%29.aspx*/
  if (mix_format->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
    return;
  }

  WAVEFORMATEXTENSIBLE * format_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix_format.get());

  /* Stash a copy of the original mix format in case we need to restore it later. */
  WAVEFORMATEXTENSIBLE hw_mix_format = *format_pcm;

  /* Get the channel mask by the channel layout.
     If the layout is not supported, we will get a closest settings below. */
  format_pcm->dwChannelMask = channel_layout_to_mask(stream_params->layout);
  mix_format->nChannels = stream_params->channels;
  waveformatex_update_derived_properties(mix_format.get());

  /* Check if wasapi will accept our channel layout request. */
  WAVEFORMATEX * closest;
  HRESULT hr = stm->output_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                                     mix_format.get(),
                                                     &closest);
  if (hr == S_FALSE) {
    /* Channel layout not supported, but WASAPI gives us a suggestion. Use it,
       and handle the eventual upmix/downmix ourselves. Ignore the subformat of
       the suggestion, since it seems to always be IEEE_FLOAT. */
    LOG("Using WASAPI suggested format: channels: %d", closest->nChannels);
    WAVEFORMATEXTENSIBLE * closest_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(closest);
    format_pcm->dwChannelMask = closest_pcm->dwChannelMask;
    mix_format->nChannels = closest->nChannels;
    waveformatex_update_derived_properties(mix_format.get());
  } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
    /* Not supported, no suggestion. This should not happen, but it does in the
       field with some sound cards. We restore the mix format, and let the rest
       of the code figure out the right conversion path. */
    *reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix_format.get()) = hw_mix_format;
  } else if (hr == S_OK) {
    LOG("Requested format accepted by WASAPI.");
  } else {
    LOG("IsFormatSupported unhandled error: %lx", hr);
  }
}

#define DIRECTION_NAME (direction == eCapture ? "capture" : "render")

template<typename T>
int setup_wasapi_stream_one_side(cubeb_stream * stm,
                                 cubeb_stream_params * stream_params,
                                 wchar_t const * devid,
                                 EDataFlow direction,
                                 REFIID riid,
                                 com_ptr<IAudioClient> & audio_client,
                                 uint32_t * buffer_frame_count,
                                 HANDLE & event,
                                 T & render_or_capture_client,
                                 cubeb_stream_params * mix_params)
{
  com_ptr<IMMDevice> device;
  HRESULT hr;

  stm->stream_reset_lock.assert_current_thread_owns();
  bool try_again = false;
  // This loops until we find a device that works, or we've exhausted all
  // possibilities.
  do {
    if (devid) {
      hr = get_endpoint(device, devid);
      if (FAILED(hr)) {
        LOG("Could not get %s endpoint, error: %lx\n", DIRECTION_NAME, hr);
        return CUBEB_ERROR;
      }
    } else {
      hr = get_default_endpoint(device, direction);
      if (FAILED(hr)) {
        LOG("Could not get default %s endpoint, error: %lx\n", DIRECTION_NAME, hr);
        return CUBEB_ERROR;
      }
    }

    /* Get a client. We will get all other interfaces we need from
     * this pointer. */
    hr = device->Activate(__uuidof(IAudioClient),
                          CLSCTX_INPROC_SERVER,
                          NULL, audio_client.receive_vpp());
    if (FAILED(hr)) {
      LOG("Could not activate the device to get an audio"
          " client for %s: error: %lx\n", DIRECTION_NAME, hr);
      // A particular device can't be activated because it has been
      // unplugged, try fall back to the default audio device.
      if (devid && hr == AUDCLNT_E_DEVICE_INVALIDATED) {
        LOG("Trying again with the default %s audio device.", DIRECTION_NAME);
        devid = nullptr;
        device = nullptr;
        try_again = true;
      } else {
        return CUBEB_ERROR;
      }
    } else {
      try_again = false;
    }
  } while (try_again);

  /* We have to distinguish between the format the mixer uses,
   * and the format the stream we want to play uses. */
  WAVEFORMATEX * tmp = nullptr;
  hr = audio_client->GetMixFormat(&tmp);
  if (FAILED(hr)) {
    LOG("Could not fetch current mix format from the audio"
        " client for %s: error: %lx", DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }
  com_heap_ptr<WAVEFORMATEX> mix_format(tmp);

  WAVEFORMATEXTENSIBLE * format_pcm = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix_format.get());
  mix_format->wBitsPerSample = stm->bytes_per_sample * 8;
  format_pcm->SubFormat = stm->waveformatextensible_sub_format;
  waveformatex_update_derived_properties(mix_format.get());

  /* Set channel layout only when there're more than two channels. Otherwise,
   * use the default setting retrieved from the stream format of the audio
   * engine's internal processing by GetMixFormat. */
  if (mix_format->nChannels > 2) {
    /* Currently, we only support mono and stereo for capture stream. */
    if (direction == eCapture) {
      XASSERT(false && "Multichannel recording is not supported.");
    }

    handle_channel_layout(stm, mix_format, stream_params);
  }

  mix_params->format = stream_params->format;
  mix_params->rate = mix_format->nSamplesPerSec;
  mix_params->channels = mix_format->nChannels;
  mix_params->layout = mask_to_channel_layout(format_pcm->dwChannelMask);
  if (mix_params->layout == CUBEB_LAYOUT_UNDEFINED) {
    LOG("Output using undefined layout!\n");
  } else if (mix_format->nChannels != CUBEB_CHANNEL_LAYOUT_MAPS[mix_params->layout].channels) {
    // The CUBEB_CHANNEL_LAYOUT_MAPS[mix_params->layout].channels may be
    // different from the mix_params->channels. 6 channel ouput with stereo
    // layout is acceptable in Windows. If this happens, it should not downmix
    // audio according to layout.
    LOG("Channel count is different from the layout standard!\n");
  }
  LOG("Setup requested=[f=%d r=%u c=%u l=%s] mix=[f=%d r=%u c=%u l=%s]",
      stream_params->format, stream_params->rate, stream_params->channels,
      CUBEB_CHANNEL_LAYOUT_MAPS[stream_params->layout].name,
      mix_params->format, mix_params->rate, mix_params->channels,
      CUBEB_CHANNEL_LAYOUT_MAPS[mix_params->layout].name);

  hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                AUDCLNT_STREAMFLAGS_NOPERSIST,
                                frames_to_hns(stm, stm->latency),
                                0,
                                mix_format.get(),
                                NULL);
  if (FAILED(hr)) {
    LOG("Unable to initialize audio client for %s: %lx.", DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }

  hr = audio_client->GetBufferSize(buffer_frame_count);
  if (FAILED(hr)) {
    LOG("Could not get the buffer size from the client"
        " for %s %lx.", DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }
  // Input is up/down mixed when depacketized in get_input_buffer.
  if (has_output(stm) && cubeb_should_mix(stream_params, mix_params)) {
    stm->mix_buffer.resize(frames_to_bytes_before_mix(stm, *buffer_frame_count));
  }

  hr = audio_client->SetEventHandle(event);
  if (FAILED(hr)) {
    LOG("Could set the event handle for the %s client %lx.",
        DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }

  hr = audio_client->GetService(riid, render_or_capture_client.receive_vpp());
  if (FAILED(hr)) {
    LOG("Could not get the %s client %lx.", DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

#undef DIRECTION_NAME

int setup_wasapi_stream(cubeb_stream * stm)
{
  HRESULT hr;
  int rv;

  stm->stream_reset_lock.assert_current_thread_owns();

  auto_com com;
  if (!com.ok()) {
    LOG("Failure to initialize COM.");
    return CUBEB_ERROR;
  }

  XASSERT((!stm->output_client || !stm->input_client) && "WASAPI stream already setup, close it first.");

  if (has_input(stm)) {
    LOG("(%p) Setup capture: device=%p", stm, stm->input_device.get());
    rv = setup_wasapi_stream_one_side(stm,
                                      &stm->input_stream_params,
                                      stm->input_device.get(),
                                      eCapture,
                                      __uuidof(IAudioCaptureClient),
                                      stm->input_client,
                                      &stm->input_buffer_frame_count,
                                      stm->input_available_event,
                                      stm->capture_client,
                                      &stm->input_mix_params);

    // We initializing an input stream, buffer ahead two buffers worth of silence.
    // This delays the input side slightly, but allow to not glitch when no input
    // is available when calling into the resampler to call the callback: the input
    // refill event will be set shortly after to compensate for this lack of data.
    // In debug, four buffers are used, to avoid tripping up assertions down the line.
#if !defined(DEBUG)
    const int silent_buffer_count = 2;
#else
    const int silent_buffer_count = 4;
#endif
    stm->linear_input_buffer->push_silence(stm->input_buffer_frame_count *
                                          stm->input_stream_params.channels *
                                          silent_buffer_count);

    if (rv != CUBEB_OK) {
      LOG("Failure to open the input side.");
      return rv;
    }
  }

  if (has_output(stm)) {
    LOG("(%p) Setup render: device=%p", stm, stm->output_device.get());
    rv = setup_wasapi_stream_one_side(stm,
                                      &stm->output_stream_params,
                                      stm->output_device.get(),
                                      eRender,
                                      __uuidof(IAudioRenderClient),
                                      stm->output_client,
                                      &stm->output_buffer_frame_count,
                                      stm->refill_event,
                                      stm->render_client,
                                      &stm->output_mix_params);
    if (rv != CUBEB_OK) {
      LOG("Failure to open the output side.");
      return rv;
    }

    hr = stm->output_client->GetService(__uuidof(IAudioStreamVolume),
                                        stm->audio_stream_volume.receive_vpp());
    if (FAILED(hr)) {
      LOG("Could not get the IAudioStreamVolume: %lx", hr);
      return CUBEB_ERROR;
    }

    XASSERT(stm->frames_written == 0);
    hr = stm->output_client->GetService(__uuidof(IAudioClock),
                                        stm->audio_clock.receive_vpp());
    if (FAILED(hr)) {
      LOG("Could not get the IAudioClock: %lx", hr);
      return CUBEB_ERROR;
    }

    /* Restore the stream volume over a device change. */
    if (stream_set_volume(stm, stm->volume) != CUBEB_OK) {
      LOG("Could not set the volume.");
      return CUBEB_ERROR;
    }
  }

  /* If we have both input and output, we resample to
   * the highest sample rate available. */
  int32_t target_sample_rate;
  if (has_input(stm) && has_output(stm)) {
    XASSERT(stm->input_stream_params.rate == stm->output_stream_params.rate);
    target_sample_rate = stm->input_stream_params.rate;
  } else if (has_input(stm)) {
    target_sample_rate = stm->input_stream_params.rate;
  } else {
    XASSERT(has_output(stm));
    target_sample_rate = stm->output_stream_params.rate;
  }

  LOG("Target sample rate: %d", target_sample_rate);

  /* If we are playing/capturing a mono stream, we only resample one channel,
   and copy it over, so we are always resampling the number
   of channels of the stream, not the number of channels
   that WASAPI wants. */
  cubeb_stream_params input_params = stm->input_mix_params;
  input_params.channels = stm->input_stream_params.channels;
  cubeb_stream_params output_params = stm->output_mix_params;
  output_params.channels = stm->output_stream_params.channels;

  stm->resampler.reset(
    cubeb_resampler_create(stm,
                           has_input(stm) ? &input_params : nullptr,
                           has_output(stm) ? &output_params : nullptr,
                           target_sample_rate,
                           stm->data_callback,
                           stm->user_ptr,
                           CUBEB_RESAMPLER_QUALITY_DESKTOP));
  if (!stm->resampler) {
    LOG("Could not get a resampler");
    return CUBEB_ERROR;
  }

  XASSERT(has_input(stm) || has_output(stm));

  if (has_input(stm) && has_output(stm)) {
    stm->refill_callback = refill_callback_duplex;
  } else if (has_input(stm)) {
    stm->refill_callback = refill_callback_input;
  } else if (has_output(stm)) {
    stm->refill_callback = refill_callback_output;
  }

  return CUBEB_OK;
}

int
wasapi_stream_init(cubeb * context, cubeb_stream ** stream,
                   char const * stream_name,
                   cubeb_devid input_device,
                   cubeb_stream_params * input_stream_params,
                   cubeb_devid output_device,
                   cubeb_stream_params * output_stream_params,
                   unsigned int latency_frames, cubeb_data_callback data_callback,
                   cubeb_state_callback state_callback, void * user_ptr)
{
  HRESULT hr;
  int rv;
  auto_com com;
  if (!com.ok()) {
    return CUBEB_ERROR;
  }

  XASSERT(context && stream && (input_stream_params || output_stream_params));

  if (output_stream_params && input_stream_params &&
      output_stream_params->format != input_stream_params->format) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  std::unique_ptr<cubeb_stream, decltype(&wasapi_stream_destroy)> stm(new cubeb_stream(), wasapi_stream_destroy);

  stm->context = context;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  if (input_stream_params) {
    stm->input_stream_params = *input_stream_params;
    stm->input_device = utf8_to_wstr(reinterpret_cast<char const *>(input_device));
    // Make sure the layout matches the channel count.
    XASSERT(stm->input_stream_params.channels == CUBEB_CHANNEL_LAYOUT_MAPS[stm->input_stream_params.layout].channels);
  }
  if (output_stream_params) {
    stm->output_stream_params = *output_stream_params;
    stm->output_device = utf8_to_wstr(reinterpret_cast<char const *>(output_device));
    // Make sure the layout matches the channel count.
    XASSERT(stm->output_stream_params.channels == CUBEB_CHANNEL_LAYOUT_MAPS[stm->output_stream_params.layout].channels);
  }

  switch (output_stream_params ? output_stream_params->format : input_stream_params->format) {
    case CUBEB_SAMPLE_S16NE:
      stm->bytes_per_sample = sizeof(short);
      stm->waveformatextensible_sub_format = KSDATAFORMAT_SUBTYPE_PCM;
      stm->mixing.reset(new mixing_impl<short>(cubeb_downmix_short, cubeb_upmix_short));
      stm->linear_input_buffer.reset(new auto_array_wrapper_impl<short>);
      break;
    case CUBEB_SAMPLE_FLOAT32NE:
      stm->bytes_per_sample = sizeof(float);
      stm->waveformatextensible_sub_format = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
      stm->mixing.reset(new mixing_impl<float>(cubeb_downmix_float, cubeb_upmix_float));
      stm->linear_input_buffer.reset(new auto_array_wrapper_impl<float>);
      break;
    default:
      return CUBEB_ERROR_INVALID_FORMAT;
  }

  stm->latency = latency_frames;

  stm->reconfigure_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->reconfigure_event) {
    LOG("Can't create the reconfigure event, error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  /* Unconditionally create the two events so that the wait logic is simpler. */
  stm->refill_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->refill_event) {
    LOG("Can't create the refill event, error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  stm->input_available_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->input_available_event) {
    LOG("Can't create the input available event , error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  {
    /* Locking here is not strictly necessary, because we don't have a
       notification client that can reset the stream yet, but it lets us
       assert that the lock is held in the function. */
    auto_lock lock(stm->stream_reset_lock);
    rv = setup_wasapi_stream(stm.get());
  }
  if (rv != CUBEB_OK) {
    return rv;
  }

  hr = register_notification_client(stm.get());
  if (FAILED(hr)) {
    /* this is not fatal, we can still play audio, but we won't be able
       to keep using the default audio endpoint if it changes. */
    LOG("failed to register notification client, %lx", hr);
  }

  *stream = stm.release();

  return CUBEB_OK;
}

void close_wasapi_stream(cubeb_stream * stm)
{
  XASSERT(stm);

  stm->stream_reset_lock.assert_current_thread_owns();

  stm->output_client = nullptr;
  stm->render_client = nullptr;

  stm->input_client = nullptr;
  stm->capture_client = nullptr;

  stm->audio_stream_volume = nullptr;

  stm->audio_clock = nullptr;
  stm->total_frames_written += static_cast<UINT64>(round(stm->frames_written * stream_to_mix_samplerate_ratio(stm->output_stream_params, stm->output_mix_params)));
  stm->frames_written = 0;

  stm->resampler.reset();

  stm->mix_buffer.clear();
  stm->mixing.reset();
  stm->linear_input_buffer.reset();
}

void wasapi_stream_destroy(cubeb_stream * stm)
{
  XASSERT(stm);

  // Only free stm->emergency_bailout if we could join the thread.
  // If we could not join the thread, stm->emergency_bailout is true
  // and is still alive until the thread wakes up and exits cleanly.
  if (stop_and_join_render_thread(stm)) {
    delete stm->emergency_bailout.load();
    stm->emergency_bailout = nullptr;
  }

  unregister_notification_client(stm);

  CloseHandle(stm->reconfigure_event);
  CloseHandle(stm->refill_event);
  CloseHandle(stm->input_available_event);

  {
    auto_lock lock(stm->stream_reset_lock);
    close_wasapi_stream(stm);
  }

  delete stm;
}

enum StreamDirection {
  OUTPUT,
  INPUT
};

int stream_start_one_side(cubeb_stream * stm, StreamDirection dir)
{
  XASSERT((dir == OUTPUT && stm->output_client) ||
          (dir == INPUT && stm->input_client));

  HRESULT hr = dir == OUTPUT ? stm->output_client->Start() : stm->input_client->Start();
  if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
    LOG("audioclient invalidated for %s device, reconfiguring",
        dir == OUTPUT ? "output" : "input");

    BOOL ok = ResetEvent(stm->reconfigure_event);
    if (!ok) {
      LOG("resetting reconfig event failed for %s stream: %lx",
          dir == OUTPUT ? "output" : "input", GetLastError());
    }

    close_wasapi_stream(stm);
    int r = setup_wasapi_stream(stm);
    if (r != CUBEB_OK) {
      LOG("reconfigure failed");
      return r;
    }

    HRESULT hr2 = dir == OUTPUT ? stm->output_client->Start() : stm->input_client->Start();
    if (FAILED(hr2)) {
      LOG("could not start the %s stream after reconfig: %lx",
          dir == OUTPUT ? "output" : "input", hr);
      return CUBEB_ERROR;
    }
  } else if (FAILED(hr)) {
    LOG("could not start the %s stream: %lx.",
        dir == OUTPUT ? "output" : "input", hr);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

int wasapi_stream_start(cubeb_stream * stm)
{
  auto_lock lock(stm->stream_reset_lock);

  XASSERT(stm && !stm->thread && !stm->shutdown_event);
  XASSERT(stm->output_client || stm->input_client);

  stm->emergency_bailout = new std::atomic<bool>(false);

  if (stm->output_client) {
    int rv = stream_start_one_side(stm, OUTPUT);
    if (rv != CUBEB_OK) {
      return rv;
    }
  }

  if (stm->input_client) {
    int rv = stream_start_one_side(stm, INPUT);
    if (rv != CUBEB_OK) {
      return rv;
    }
  }

  stm->shutdown_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->shutdown_event) {
    LOG("Can't create the shutdown event, error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  stm->thread = (HANDLE) _beginthreadex(NULL, 512 * 1024, wasapi_stream_render_loop, stm, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
  if (stm->thread == NULL) {
    LOG("could not create WASAPI render thread.");
    return CUBEB_ERROR;
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);

  return CUBEB_OK;
}

int wasapi_stream_stop(cubeb_stream * stm)
{
  XASSERT(stm);
  HRESULT hr;

  {
    auto_lock lock(stm->stream_reset_lock);

    if (stm->output_client) {
      hr = stm->output_client->Stop();
      if (FAILED(hr)) {
        LOG("could not stop AudioClient (output)");
        return CUBEB_ERROR;
      }
    }

    if (stm->input_client) {
      hr = stm->input_client->Stop();
      if (FAILED(hr)) {
        LOG("could not stop AudioClient (input)");
        return CUBEB_ERROR;
      }
    }

    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
  }

  if (stop_and_join_render_thread(stm)) {
    // This is null if we've given the pointer to the other thread
    if (stm->emergency_bailout.load()) {
      delete stm->emergency_bailout.load();
      stm->emergency_bailout = nullptr;
    }
  }

  return CUBEB_OK;
}

int wasapi_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  XASSERT(stm && position);
  auto_lock lock(stm->stream_reset_lock);

  if (!has_output(stm)) {
    return CUBEB_ERROR;
  }

  /* Calculate how far behind the current stream head the playback cursor is. */
  uint64_t stream_delay = static_cast<uint64_t>(current_stream_delay(stm) * stm->output_stream_params.rate);

  /* Calculate the logical stream head in frames at the stream sample rate. */
  uint64_t max_pos = stm->total_frames_written +
                     static_cast<uint64_t>(round(stm->frames_written * stream_to_mix_samplerate_ratio(stm->output_stream_params, stm->output_mix_params)));

  *position = max_pos;
  if (stream_delay <= *position) {
    *position -= stream_delay;
  }

  if (*position < stm->prev_position) {
    *position = stm->prev_position;
  }
  stm->prev_position = *position;

  return CUBEB_OK;
}

int wasapi_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  XASSERT(stm && latency);

  if (!has_output(stm)) {
    return CUBEB_ERROR;
  }

  auto_lock lock(stm->stream_reset_lock);

  /* The GetStreamLatency method only works if the
     AudioClient has been initialized. */
  if (!stm->output_client) {
    return CUBEB_ERROR;
  }

  REFERENCE_TIME latency_hns;
  HRESULT hr = stm->output_client->GetStreamLatency(&latency_hns);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }
  *latency = hns_to_frames(stm, latency_hns);

  return CUBEB_OK;
}

int wasapi_stream_set_volume(cubeb_stream * stm, float volume)
{
  auto_lock lock(stm->stream_reset_lock);

  if (!has_output(stm)) {
    return CUBEB_ERROR;
  }

  if (stream_set_volume(stm, volume) != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  stm->volume = volume;

  return CUBEB_OK;
}

static char const *
wstr_to_utf8(LPCWSTR str)
{
  int size = ::WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, NULL, NULL);
  if (size <= 0) {
    return nullptr;
  }

  char * ret = static_cast<char *>(malloc(size));
  ::WideCharToMultiByte(CP_UTF8, 0, str, -1, ret, size, NULL, NULL);
  return ret;
}

static std::unique_ptr<wchar_t const []>
utf8_to_wstr(char const * str)
{
  int size = ::MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
  if (size <= 0) {
    return nullptr;
  }

  std::unique_ptr<wchar_t []> ret(new wchar_t[size]);
  ::MultiByteToWideChar(CP_UTF8, 0, str, -1, ret.get(), size);
  return std::move(ret);
}

static com_ptr<IMMDevice>
wasapi_get_device_node(IMMDeviceEnumerator * enumerator, IMMDevice * dev)
{
  com_ptr<IMMDevice> ret;
  com_ptr<IDeviceTopology> devtopo;
  com_ptr<IConnector> connector;

  if (SUCCEEDED(dev->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL, devtopo.receive_vpp())) &&
      SUCCEEDED(devtopo->GetConnector(0, connector.receive()))) {
    wchar_t * tmp = nullptr;
    if (SUCCEEDED(connector->GetDeviceIdConnectedTo(&tmp))) {
      com_heap_ptr<wchar_t> filterid(tmp);
      if (FAILED(enumerator->GetDevice(filterid.get(), ret.receive())))
        ret = NULL;
    }
  }

  return ret;
}

static BOOL
wasapi_is_default_device(EDataFlow flow, ERole role, LPCWSTR device_id,
                         IMMDeviceEnumerator * enumerator)
{
  BOOL ret = FALSE;
  com_ptr<IMMDevice> dev;
  HRESULT hr;

  hr = enumerator->GetDefaultAudioEndpoint(flow, role, dev.receive());
  if (SUCCEEDED(hr)) {
    wchar_t * tmp = nullptr;
    if (SUCCEEDED(dev->GetId(&tmp))) {
      com_heap_ptr<wchar_t> defdevid(tmp);
      ret = (wcscmp(defdevid.get(), device_id) == 0);
    }
  }

  return ret;
}

static cubeb_device_info *
wasapi_create_device(IMMDeviceEnumerator * enumerator, IMMDevice * dev)
{
  com_ptr<IMMEndpoint> endpoint;
  com_ptr<IMMDevice> devnode;
  com_ptr<IAudioClient> client;
  cubeb_device_info * ret = NULL;
  EDataFlow flow;
  DWORD state = DEVICE_STATE_NOTPRESENT;
  com_ptr<IPropertyStore> propstore;
  REFERENCE_TIME def_period, min_period;
  HRESULT hr;

  struct prop_variant : public PROPVARIANT {
    prop_variant() { PropVariantInit(this); }
    ~prop_variant() { PropVariantClear(this); }
    prop_variant(prop_variant const &) = delete;
    prop_variant & operator=(prop_variant const &) = delete;
  };

  hr = dev->QueryInterface(IID_PPV_ARGS(endpoint.receive()));
  if (FAILED(hr)) return nullptr;

  hr = endpoint->GetDataFlow(&flow);
  if (FAILED(hr)) return nullptr;

  wchar_t * tmp = nullptr;
  hr = dev->GetId(&tmp);
  if (FAILED(hr)) return nullptr;
  com_heap_ptr<wchar_t> device_id(tmp);

  hr = dev->OpenPropertyStore(STGM_READ, propstore.receive());
  if (FAILED(hr)) return nullptr;

  hr = dev->GetState(&state);
  if (FAILED(hr)) return nullptr;

  ret = (cubeb_device_info *)calloc(1, sizeof(cubeb_device_info));

  ret->device_id = wstr_to_utf8(device_id.get());
  ret->devid = reinterpret_cast<cubeb_devid>(ret->device_id);
  prop_variant namevar;
  hr = propstore->GetValue(PKEY_Device_FriendlyName, &namevar);
  if (SUCCEEDED(hr))
    ret->friendly_name = wstr_to_utf8(namevar.pwszVal);

  devnode = wasapi_get_device_node(enumerator, dev);
  if (devnode) {
    com_ptr<IPropertyStore> ps;
    hr = devnode->OpenPropertyStore(STGM_READ, ps.receive());
    if (FAILED(hr)) return ret;

    prop_variant instancevar;
    hr = ps->GetValue(PKEY_Device_InstanceId, &instancevar);
    if (SUCCEEDED(hr)) {
      ret->group_id = wstr_to_utf8(instancevar.pwszVal);
    }
  }

  ret->preferred = CUBEB_DEVICE_PREF_NONE;
  if (wasapi_is_default_device(flow, eConsole, device_id.get(), enumerator))
    ret->preferred = (cubeb_device_pref)(ret->preferred | CUBEB_DEVICE_PREF_MULTIMEDIA);
  if (wasapi_is_default_device(flow, eCommunications, device_id.get(), enumerator))
    ret->preferred = (cubeb_device_pref)(ret->preferred | CUBEB_DEVICE_PREF_VOICE);
  if (wasapi_is_default_device(flow, eConsole, device_id.get(), enumerator))
    ret->preferred = (cubeb_device_pref)(ret->preferred | CUBEB_DEVICE_PREF_NOTIFICATION);

  if (flow == eRender) ret->type = CUBEB_DEVICE_TYPE_OUTPUT;
  else if (flow == eCapture) ret->type = CUBEB_DEVICE_TYPE_INPUT;
  switch (state) {
    case DEVICE_STATE_ACTIVE:
      ret->state = CUBEB_DEVICE_STATE_ENABLED;
      break;
    case DEVICE_STATE_UNPLUGGED:
      ret->state = CUBEB_DEVICE_STATE_UNPLUGGED;
      break;
    default:
      ret->state = CUBEB_DEVICE_STATE_DISABLED;
      break;
  };

  ret->format = static_cast<cubeb_device_fmt>(CUBEB_DEVICE_FMT_F32NE | CUBEB_DEVICE_FMT_S16NE);
  ret->default_format = CUBEB_DEVICE_FMT_F32NE;
  prop_variant fmtvar;
  hr = propstore->GetValue(PKEY_AudioEngine_DeviceFormat, &fmtvar);
  if (SUCCEEDED(hr) && fmtvar.vt == VT_BLOB) {
    if (fmtvar.blob.cbSize == sizeof(PCMWAVEFORMAT)) {
      const PCMWAVEFORMAT * pcm = reinterpret_cast<const PCMWAVEFORMAT *>(fmtvar.blob.pBlobData);

      ret->max_rate = ret->min_rate = ret->default_rate = pcm->wf.nSamplesPerSec;
      ret->max_channels = pcm->wf.nChannels;
    } else if (fmtvar.blob.cbSize >= sizeof(WAVEFORMATEX)) {
      WAVEFORMATEX* wfx = reinterpret_cast<WAVEFORMATEX*>(fmtvar.blob.pBlobData);

      if (fmtvar.blob.cbSize >= sizeof(WAVEFORMATEX) + wfx->cbSize ||
          wfx->wFormatTag == WAVE_FORMAT_PCM) {
        ret->max_rate = ret->min_rate = ret->default_rate = wfx->nSamplesPerSec;
        ret->max_channels = wfx->nChannels;
      }
    }
  }

  if (SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, client.receive_vpp())) &&
      SUCCEEDED(client->GetDevicePeriod(&def_period, &min_period))) {
    ret->latency_lo = hns_to_frames(ret->default_rate, min_period);
    ret->latency_hi = hns_to_frames(ret->default_rate, def_period);
  } else {
    ret->latency_lo = 0;
    ret->latency_hi = 0;
  }

  return ret;
}

static int
wasapi_enumerate_devices(cubeb * context, cubeb_device_type type,
                         cubeb_device_collection ** out)
{
  auto_com com;
  com_ptr<IMMDeviceEnumerator> enumerator;
  com_ptr<IMMDeviceCollection> collection;
  cubeb_device_info * cur;
  HRESULT hr;
  UINT cc, i;
  EDataFlow flow;

  *out = NULL;

  if (!com.ok())
    return CUBEB_ERROR;

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return CUBEB_ERROR;
  }

  if (type == CUBEB_DEVICE_TYPE_OUTPUT) flow = eRender;
  else if (type == CUBEB_DEVICE_TYPE_INPUT) flow = eCapture;
  else if (type & (CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT)) flow = eAll;
  else return CUBEB_ERROR;

  hr = enumerator->EnumAudioEndpoints(flow, DEVICE_STATEMASK_ALL, collection.receive());
  if (FAILED(hr)) {
    LOG("Could not enumerate audio endpoints: %lx", hr);
    return CUBEB_ERROR;
  }

  hr = collection->GetCount(&cc);
  if (FAILED(hr)) {
    LOG("IMMDeviceCollection::GetCount() failed: %lx", hr);
    return CUBEB_ERROR;
  }
  *out = (cubeb_device_collection *) malloc(sizeof(cubeb_device_collection) +
      sizeof(cubeb_device_info*) * (cc > 0 ? cc - 1 : 0));
  if (!*out) {
    return CUBEB_ERROR;
  }
  (*out)->count = 0;
  for (i = 0; i < cc; i++) {
    com_ptr<IMMDevice> dev;
    hr = collection->Item(i, dev.receive());
    if (FAILED(hr)) {
      LOG("IMMDeviceCollection::Item(%u) failed: %lx", i-1, hr);
    } else if ((cur = wasapi_create_device(enumerator.get(), dev.get())) != NULL) {
      (*out)->device[(*out)->count++] = cur;
    }
  }

  return CUBEB_OK;
}

cubeb_ops const wasapi_ops = {
  /*.init =*/ wasapi_init,
  /*.get_backend_id =*/ wasapi_get_backend_id,
  /*.get_max_channel_count =*/ wasapi_get_max_channel_count,
  /*.get_min_latency =*/ wasapi_get_min_latency,
  /*.get_preferred_sample_rate =*/ wasapi_get_preferred_sample_rate,
  /*.get_preferred_channel_layout =*/ wasapi_get_preferred_channel_layout,
  /*.enumerate_devices =*/ wasapi_enumerate_devices,
  /*.destroy =*/ wasapi_destroy,
  /*.stream_init =*/ wasapi_stream_init,
  /*.stream_destroy =*/ wasapi_stream_destroy,
  /*.stream_start =*/ wasapi_stream_start,
  /*.stream_stop =*/ wasapi_stream_stop,
  /*.stream_get_position =*/ wasapi_stream_get_position,
  /*.stream_get_latency =*/ wasapi_stream_get_latency,
  /*.stream_set_volume =*/ wasapi_stream_set_volume,
  /*.stream_set_panning =*/ NULL,
  /*.stream_get_current_device =*/ NULL,
  /*.stream_device_destroy =*/ NULL,
  /*.stream_register_device_changed_callback =*/ NULL,
  /*.register_device_collection_changed =*/ NULL
};
} // namespace anonymous
