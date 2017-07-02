/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <pulse/pulseaudio.h>
#include <string.h>
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include "cubeb_mixer.h"
#include "cubeb_utils.h"
#include <stdio.h>

#ifdef DISABLE_LIBPULSE_DLOPEN
#define WRAP(x) x
#else
#define WRAP(x) cubeb_##x
#define LIBPULSE_API_VISIT(X)                   \
  X(pa_channel_map_can_balance)                 \
  X(pa_channel_map_init)                        \
  X(pa_context_connect)                         \
  X(pa_context_disconnect)                      \
  X(pa_context_drain)                           \
  X(pa_context_get_server_info)                 \
  X(pa_context_get_sink_info_by_name)           \
  X(pa_context_get_sink_info_list)              \
  X(pa_context_get_sink_input_info)             \
  X(pa_context_get_source_info_list)            \
  X(pa_context_get_state)                       \
  X(pa_context_new)                             \
  X(pa_context_rttime_new)                      \
  X(pa_context_set_sink_input_volume)           \
  X(pa_context_set_state_callback)              \
  X(pa_context_unref)                           \
  X(pa_cvolume_set)                             \
  X(pa_cvolume_set_balance)                     \
  X(pa_frame_size)                              \
  X(pa_operation_get_state)                     \
  X(pa_operation_unref)                         \
  X(pa_proplist_gets)                           \
  X(pa_rtclock_now)                             \
  X(pa_stream_begin_write)                      \
  X(pa_stream_cancel_write)                     \
  X(pa_stream_connect_playback)                 \
  X(pa_stream_cork)                             \
  X(pa_stream_disconnect)                       \
  X(pa_stream_get_channel_map)                  \
  X(pa_stream_get_index)                        \
  X(pa_stream_get_latency)                      \
  X(pa_stream_get_sample_spec)                  \
  X(pa_stream_get_state)                        \
  X(pa_stream_get_time)                         \
  X(pa_stream_new)                              \
  X(pa_stream_set_state_callback)               \
  X(pa_stream_set_write_callback)               \
  X(pa_stream_unref)                            \
  X(pa_stream_update_timing_info)               \
  X(pa_stream_write)                            \
  X(pa_sw_volume_from_linear)                   \
  X(pa_threaded_mainloop_free)                  \
  X(pa_threaded_mainloop_get_api)               \
  X(pa_threaded_mainloop_in_thread)             \
  X(pa_threaded_mainloop_lock)                  \
  X(pa_threaded_mainloop_new)                   \
  X(pa_threaded_mainloop_signal)                \
  X(pa_threaded_mainloop_start)                 \
  X(pa_threaded_mainloop_stop)                  \
  X(pa_threaded_mainloop_unlock)                \
  X(pa_threaded_mainloop_wait)                  \
  X(pa_usec_to_bytes)                           \
  X(pa_stream_set_read_callback)                \
  X(pa_stream_connect_record)                   \
  X(pa_stream_readable_size)                    \
  X(pa_stream_writable_size)                    \
  X(pa_stream_peek)                             \
  X(pa_stream_drop)                             \
  X(pa_stream_get_buffer_attr)                  \
  X(pa_stream_get_device_name)                  \
  X(pa_context_set_subscribe_callback)          \
  X(pa_context_subscribe)                       \
  X(pa_mainloop_api_once)                       \

#define MAKE_TYPEDEF(x) static typeof(x) * cubeb_##x;
LIBPULSE_API_VISIT(MAKE_TYPEDEF);
#undef MAKE_TYPEDEF
#endif

static struct cubeb_ops const pulse_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  void * libpulse;
  pa_threaded_mainloop * mainloop;
  pa_context * context;
  pa_sink_info * default_sink_info;
  char * context_name;
  int error;
  cubeb_device_collection_changed_callback collection_changed_callback;
  void * collection_changed_user_ptr;
};

struct cubeb_stream {
  cubeb * context;
  pa_stream * output_stream;
  pa_stream * input_stream;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * user_ptr;
  pa_time_event * drain_timer;
  pa_sample_spec output_sample_spec;
  pa_sample_spec input_sample_spec;
  int shutdown;
  float volume;
  cubeb_state state;
};

static const float PULSE_NO_GAIN = -1.0;

enum cork_state {
  UNCORK = 0,
  CORK = 1 << 0,
  NOTIFY = 1 << 1
};

static void
sink_info_callback(pa_context * context, const pa_sink_info * info, int eol, void * u)
{
  (void)context;
  cubeb * ctx = u;
  if (!eol) {
    free(ctx->default_sink_info);
    ctx->default_sink_info = malloc(sizeof(pa_sink_info));
    memcpy(ctx->default_sink_info, info, sizeof(pa_sink_info));
  }
  WRAP(pa_threaded_mainloop_signal)(ctx->mainloop, 0);
}

static void
server_info_callback(pa_context * context, const pa_server_info * info, void * u)
{
  pa_operation * o;
  o = WRAP(pa_context_get_sink_info_by_name)(context, info->default_sink_name, sink_info_callback, u);
  if (o) {
    WRAP(pa_operation_unref)(o);
  }
}

static void
context_state_callback(pa_context * c, void * u)
{
  cubeb * ctx = u;
  if (!PA_CONTEXT_IS_GOOD(WRAP(pa_context_get_state)(c))) {
    ctx->error = 1;
  }
  WRAP(pa_threaded_mainloop_signal)(ctx->mainloop, 0);
}

static void
context_notify_callback(pa_context * c, void * u)
{
  (void)c;
  cubeb * ctx = u;
  WRAP(pa_threaded_mainloop_signal)(ctx->mainloop, 0);
}

static void
stream_success_callback(pa_stream * s, int success, void * u)
{
  (void)s;
  (void)success;
  cubeb_stream * stm = u;
  WRAP(pa_threaded_mainloop_signal)(stm->context->mainloop, 0);
}

static void
stream_state_change_callback(cubeb_stream * stm, cubeb_state s)
{
  stm->state = s;
  stm->state_callback(stm, stm->user_ptr, s);
}

static void
stream_drain_callback(pa_mainloop_api * a, pa_time_event * e, struct timeval const * tv, void * u)
{
  (void)a;
  (void)tv;
  cubeb_stream * stm = u;
  assert(stm->drain_timer == e);
  stream_state_change_callback(stm, CUBEB_STATE_DRAINED);
  /* there's no pa_rttime_free, so use this instead. */
  a->time_free(stm->drain_timer);
  stm->drain_timer = NULL;
  WRAP(pa_threaded_mainloop_signal)(stm->context->mainloop, 0);
}

static void
stream_state_callback(pa_stream * s, void * u)
{
  cubeb_stream * stm = u;
  if (!PA_STREAM_IS_GOOD(WRAP(pa_stream_get_state)(s))) {
    stream_state_change_callback(stm, CUBEB_STATE_ERROR);
  }
  WRAP(pa_threaded_mainloop_signal)(stm->context->mainloop, 0);
}

static void
trigger_user_callback(pa_stream * s, void const * input_data, size_t nbytes, cubeb_stream * stm)
{
  void * buffer;
  size_t size;
  int r;
  long got;
  size_t towrite, read_offset;
  size_t frame_size;

  frame_size = WRAP(pa_frame_size)(&stm->output_sample_spec);
  assert(nbytes % frame_size == 0);

  towrite = nbytes;
  read_offset = 0;
  while (towrite) {
    size = towrite;
    r = WRAP(pa_stream_begin_write)(s, &buffer, &size);
    // Note: this has failed running under rr on occassion - needs investigation.
    assert(r == 0);
    assert(size > 0);
    assert(size % frame_size == 0);

    LOGV("Trigger user callback with output buffer size=%zd, read_offset=%zd", size, read_offset);
    got = stm->data_callback(stm, stm->user_ptr, (uint8_t const *)input_data + read_offset, buffer, size / frame_size);
    if (got < 0) {
      WRAP(pa_stream_cancel_write)(s);
      stm->shutdown = 1;
      return;
    }
    // If more iterations move offset of read buffer
    if (input_data) {
      size_t in_frame_size = WRAP(pa_frame_size)(&stm->input_sample_spec);
      read_offset += (size / frame_size) * in_frame_size;
    }

    if (stm->volume != PULSE_NO_GAIN) {
      uint32_t samples =  size * stm->output_sample_spec.channels / frame_size ;

      if (stm->output_sample_spec.format == PA_SAMPLE_S16BE ||
          stm->output_sample_spec.format == PA_SAMPLE_S16LE) {
        short * b = buffer;
        for (uint32_t i = 0; i < samples; i++) {
          b[i] *= stm->volume;
        }
      } else {
        float * b = buffer;
        for (uint32_t i = 0; i < samples; i++) {
          b[i] *= stm->volume;
        }
      }
    }

    r = WRAP(pa_stream_write)(s, buffer, got * frame_size, NULL, 0, PA_SEEK_RELATIVE);
    assert(r == 0);

    if ((size_t) got < size / frame_size) {
      pa_usec_t latency = 0;
      r = WRAP(pa_stream_get_latency)(s, &latency, NULL);
      if (r == -PA_ERR_NODATA) {
        /* this needs a better guess. */
        latency = 100 * PA_USEC_PER_MSEC;
      }
      assert(r == 0 || r == -PA_ERR_NODATA);
      /* pa_stream_drain is useless, see PA bug# 866. this is a workaround. */
      /* arbitrary safety margin: double the current latency. */
      assert(!stm->drain_timer);
      stm->drain_timer = WRAP(pa_context_rttime_new)(stm->context->context, WRAP(pa_rtclock_now)() + 2 * latency, stream_drain_callback, stm);
      stm->shutdown = 1;
      return;
    }

    towrite -= size;
  }

  assert(towrite == 0);
}

static int
read_from_input(pa_stream * s, void const ** buffer, size_t * size)
{
  size_t readable_size = WRAP(pa_stream_readable_size)(s);
  if (readable_size > 0) {
    if (WRAP(pa_stream_peek)(s, buffer, size) < 0) {
      return -1;
    }
  }
  return readable_size;
}

static void
stream_write_callback(pa_stream * s, size_t nbytes, void * u)
{
  LOGV("Output callback to be written buffer size %zd", nbytes);
  cubeb_stream * stm = u;
  if (stm->shutdown ||
      stm->state != CUBEB_STATE_STARTED) {
    return;
  }

  if (!stm->input_stream){
    // Output/playback only operation.
    // Write directly to output
    assert(!stm->input_stream && stm->output_stream);
    trigger_user_callback(s, NULL, nbytes, stm);
  }
}

static void
stream_read_callback(pa_stream * s, size_t nbytes, void * u)
{
  LOGV("Input callback buffer size %zd", nbytes);
  cubeb_stream * stm = u;
  if (stm->shutdown) {
    return;
  }

  void const * read_data = NULL;
  size_t read_size;
  while (read_from_input(s, &read_data, &read_size) > 0) {
    /* read_data can be NULL in case of a hole. */
    if (read_data) {
      size_t in_frame_size = WRAP(pa_frame_size)(&stm->input_sample_spec);
      size_t read_frames = read_size / in_frame_size;

      if (stm->output_stream) {
        // input/capture + output/playback operation
        size_t out_frame_size = WRAP(pa_frame_size)(&stm->output_sample_spec);
        size_t write_size = read_frames * out_frame_size;
        // Offer full duplex data for writing
        trigger_user_callback(stm->output_stream, read_data, write_size, stm);
      } else {
        // input/capture only operation. Call callback directly
        long got = stm->data_callback(stm, stm->user_ptr, read_data, NULL, read_frames);
        if (got < 0 || (size_t) got != read_frames) {
          WRAP(pa_stream_cancel_write)(s);
          stm->shutdown = 1;
          break;
        }
      }
    }
    if (read_size > 0) {
      WRAP(pa_stream_drop)(s);
    }

    if (stm->shutdown) {
      return;
    }
  }
}

static int
wait_until_context_ready(cubeb * ctx)
{
  for (;;) {
    pa_context_state_t state = WRAP(pa_context_get_state)(ctx->context);
    if (!PA_CONTEXT_IS_GOOD(state))
      return -1;
    if (state == PA_CONTEXT_READY)
      break;
    WRAP(pa_threaded_mainloop_wait)(ctx->mainloop);
  }
  return 0;
}

static int
wait_until_io_stream_ready(pa_stream * stream, pa_threaded_mainloop * mainloop)
{
  if (!stream || !mainloop){
    return -1;
  }
  for (;;) {
    pa_stream_state_t state = WRAP(pa_stream_get_state)(stream);
    if (!PA_STREAM_IS_GOOD(state))
      return -1;
    if (state == PA_STREAM_READY)
      break;
    WRAP(pa_threaded_mainloop_wait)(mainloop);
  }
  return 0;
}

static int
wait_until_stream_ready(cubeb_stream * stm)
{
  if (stm->output_stream &&
      wait_until_io_stream_ready(stm->output_stream, stm->context->mainloop) == -1) {
    return -1;
  }
  if(stm->input_stream &&
     wait_until_io_stream_ready(stm->input_stream, stm->context->mainloop) == -1) {
    return -1;
  }
  return 0;
}

static int
operation_wait(cubeb * ctx, pa_stream * stream, pa_operation * o)
{
  while (WRAP(pa_operation_get_state)(o) == PA_OPERATION_RUNNING) {
    WRAP(pa_threaded_mainloop_wait)(ctx->mainloop);
    if (!PA_CONTEXT_IS_GOOD(WRAP(pa_context_get_state)(ctx->context))) {
      return -1;
    }
    if (stream && !PA_STREAM_IS_GOOD(WRAP(pa_stream_get_state)(stream))) {
      return -1;
    }
  }
  return 0;
}

static void
cork_io_stream(cubeb_stream * stm, pa_stream * io_stream, enum cork_state state)
{
  pa_operation * o;
  if (!io_stream) {
    return;
  }
  o = WRAP(pa_stream_cork)(io_stream, state & CORK, stream_success_callback, stm);
  if (o) {
    operation_wait(stm->context, io_stream, o);
    WRAP(pa_operation_unref)(o);
  }
}

static void
stream_cork(cubeb_stream * stm, enum cork_state state)
{
  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
  cork_io_stream(stm, stm->output_stream, state);
  cork_io_stream(stm, stm->input_stream, state);
  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  if (state & NOTIFY) {
    stream_state_change_callback(stm, state & CORK ? CUBEB_STATE_STOPPED
                                                   : CUBEB_STATE_STARTED);
  }
}

static int
stream_update_timing_info(cubeb_stream * stm)
{
  int r = -1;
  pa_operation * o = NULL;
  if (stm->output_stream) {
    o = WRAP(pa_stream_update_timing_info)(stm->output_stream, stream_success_callback, stm);
    if (o) {
      r = operation_wait(stm->context, stm->output_stream, o);
      WRAP(pa_operation_unref)(o);
    }
    if (r != 0) {
      return r;
    }
  }

  if (stm->input_stream) {
    o = WRAP(pa_stream_update_timing_info)(stm->input_stream, stream_success_callback, stm);
    if (o) {
      r = operation_wait(stm->context, stm->input_stream, o);
      WRAP(pa_operation_unref)(o);
    }
  }

  return r;
}

static pa_channel_position_t
cubeb_channel_to_pa_channel(cubeb_channel channel)
{
  assert(channel != CHANNEL_INVALID);

  // This variable may be used for multiple times, so we should avoid to
  // allocate it in stack, or it will be created and removed repeatedly.
  // Use static to allocate this local variable in data space instead of stack.
  static pa_channel_position_t map[CHANNEL_MAX] = {
    // PA_CHANNEL_POSITION_INVALID,      // CHANNEL_INVALID
    PA_CHANNEL_POSITION_MONO,         // CHANNEL_MONO
    PA_CHANNEL_POSITION_FRONT_LEFT,   // CHANNEL_LEFT
    PA_CHANNEL_POSITION_FRONT_RIGHT,  // CHANNEL_RIGHT
    PA_CHANNEL_POSITION_FRONT_CENTER, // CHANNEL_CENTER
    PA_CHANNEL_POSITION_SIDE_LEFT,    // CHANNEL_LS
    PA_CHANNEL_POSITION_SIDE_RIGHT,   // CHANNEL_RS
    PA_CHANNEL_POSITION_REAR_LEFT,    // CHANNEL_RLS
    PA_CHANNEL_POSITION_REAR_CENTER,  // CHANNEL_RCENTER
    PA_CHANNEL_POSITION_REAR_RIGHT,   // CHANNEL_RRS
    PA_CHANNEL_POSITION_LFE           // CHANNEL_LFE
  };

  return map[channel];
}

static cubeb_channel
pa_channel_to_cubeb_channel(pa_channel_position_t channel)
{
  assert(channel != PA_CHANNEL_POSITION_INVALID);
  switch(channel) {
    case PA_CHANNEL_POSITION_MONO: return CHANNEL_MONO;
    case PA_CHANNEL_POSITION_FRONT_LEFT: return CHANNEL_LEFT;
    case PA_CHANNEL_POSITION_FRONT_RIGHT: return CHANNEL_RIGHT;
    case PA_CHANNEL_POSITION_FRONT_CENTER: return CHANNEL_CENTER;
    case PA_CHANNEL_POSITION_SIDE_LEFT: return CHANNEL_LS;
    case PA_CHANNEL_POSITION_SIDE_RIGHT: return CHANNEL_RS;
    case PA_CHANNEL_POSITION_REAR_LEFT: return CHANNEL_RLS;
    case PA_CHANNEL_POSITION_REAR_CENTER: return CHANNEL_RCENTER;
    case PA_CHANNEL_POSITION_REAR_RIGHT: return CHANNEL_RRS;
    case PA_CHANNEL_POSITION_LFE: return CHANNEL_LFE;
    default: return CHANNEL_INVALID;
  }
}

static void
layout_to_channel_map(cubeb_channel_layout layout, pa_channel_map * cm)
{
  assert(cm && layout != CUBEB_LAYOUT_UNDEFINED);

  WRAP(pa_channel_map_init)(cm);
  cm->channels = CUBEB_CHANNEL_LAYOUT_MAPS[layout].channels;
  for (uint8_t i = 0 ; i < cm->channels ; ++i) {
    cm->map[i] = cubeb_channel_to_pa_channel(CHANNEL_INDEX_TO_ORDER[layout][i]);
  }
}

static cubeb_channel_layout
channel_map_to_layout(pa_channel_map * cm)
{
  cubeb_channel_map cubeb_map;
  cubeb_map.channels = cm->channels;
  for (uint32_t i = 0 ; i < cm->channels ; ++i) {
    cubeb_map.map[i] = pa_channel_to_cubeb_channel(cm->map[i]);
  }
  return cubeb_channel_map_to_layout(&cubeb_map);
}

static void pulse_context_destroy(cubeb * ctx);
static void pulse_destroy(cubeb * ctx);

static int
pulse_context_init(cubeb * ctx)
{
  if (ctx->context) {
    assert(ctx->error == 1);
    pulse_context_destroy(ctx);
  }

  ctx->context = WRAP(pa_context_new)(WRAP(pa_threaded_mainloop_get_api)(ctx->mainloop),
                                      ctx->context_name);
  if (!ctx->context) {
    return -1;
  }
  WRAP(pa_context_set_state_callback)(ctx->context, context_state_callback, ctx);

  WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
  WRAP(pa_context_connect)(ctx->context, NULL, 0, NULL);

  if (wait_until_context_ready(ctx) != 0) {
    WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);
    pulse_context_destroy(ctx);
    ctx->context = NULL;
    return -1;
  }

  WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);

  ctx->error = 0;

  return 0;
}

/*static*/ int
pulse_init(cubeb ** context, char const * context_name)
{
  void * libpulse = NULL;
  cubeb * ctx;
  pa_operation * o;

  *context = NULL;

#ifndef DISABLE_LIBPULSE_DLOPEN
  libpulse = dlopen("libpulse.so.0", RTLD_LAZY);
  if (!libpulse) {
    return CUBEB_ERROR;
  }

#define LOAD(x) {                               \
    cubeb_##x = dlsym(libpulse, #x);            \
    if (!cubeb_##x) {                           \
      dlclose(libpulse);                        \
      return CUBEB_ERROR;                       \
    }                                           \
  }

  LIBPULSE_API_VISIT(LOAD);
#undef LOAD
#endif

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &pulse_ops;
  ctx->libpulse = libpulse;

  ctx->mainloop = WRAP(pa_threaded_mainloop_new)();
  ctx->default_sink_info = NULL;

  WRAP(pa_threaded_mainloop_start)(ctx->mainloop);

  ctx->context_name = context_name ? strdup(context_name) : NULL;
  if (pulse_context_init(ctx) != 0) {
    pulse_destroy(ctx);
    return CUBEB_ERROR;
  }

  /* server_info_callback performs a second async query, which is
     responsible for initializing default_sink_info and signalling the
     mainloop to end the wait. */
  WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
  o = WRAP(pa_context_get_server_info)(ctx->context, server_info_callback, ctx);
  if (o) {
    operation_wait(ctx, NULL, o);
    WRAP(pa_operation_unref)(o);
  }
  WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);
  assert(ctx->default_sink_info);

  *context = ctx;

  return CUBEB_OK;
}

static char const *
pulse_get_backend_id(cubeb * ctx)
{
  (void)ctx;
  return "pulse";
}

static int
pulse_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  (void)ctx;
  assert(ctx && max_channels);

  *max_channels = ctx->default_sink_info->channel_map.channels;

  return CUBEB_OK;
}

static int
pulse_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  assert(ctx && rate);
  (void)ctx;

  *rate = ctx->default_sink_info->sample_spec.rate;

  return CUBEB_OK;
}

static int
pulse_get_preferred_channel_layout(cubeb * ctx, cubeb_channel_layout * layout)
{
  assert(ctx && layout);
  (void)ctx;

  *layout = channel_map_to_layout(&ctx->default_sink_info->channel_map);

  return CUBEB_OK;
}

static int
pulse_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency_frames)
{
  (void)ctx;
  // According to PulseAudio developers, this is a safe minimum.
  *latency_frames = 25 * params.rate / 1000;

  return CUBEB_OK;
}

static void
pulse_context_destroy(cubeb * ctx)
{
  pa_operation * o;

  WRAP(pa_threaded_mainloop_lock)(ctx->mainloop);
  o = WRAP(pa_context_drain)(ctx->context, context_notify_callback, ctx);
  if (o) {
    operation_wait(ctx, NULL, o);
    WRAP(pa_operation_unref)(o);
  }
  WRAP(pa_context_set_state_callback)(ctx->context, NULL, NULL);
  WRAP(pa_context_disconnect)(ctx->context);
  WRAP(pa_context_unref)(ctx->context);
  WRAP(pa_threaded_mainloop_unlock)(ctx->mainloop);
}

static void
pulse_destroy(cubeb * ctx)
{
  free(ctx->context_name);
  if (ctx->context) {
    pulse_context_destroy(ctx);
  }

  if (ctx->mainloop) {
    WRAP(pa_threaded_mainloop_stop)(ctx->mainloop);
    WRAP(pa_threaded_mainloop_free)(ctx->mainloop);
  }

  if (ctx->libpulse) {
    dlclose(ctx->libpulse);
  }
  free(ctx->default_sink_info);
  free(ctx);
}

static void pulse_stream_destroy(cubeb_stream * stm);

static pa_sample_format_t
to_pulse_format(cubeb_sample_format format)
{
  switch (format) {
  case CUBEB_SAMPLE_S16LE:
    return PA_SAMPLE_S16LE;
  case CUBEB_SAMPLE_S16BE:
    return PA_SAMPLE_S16BE;
  case CUBEB_SAMPLE_FLOAT32LE:
    return PA_SAMPLE_FLOAT32LE;
  case CUBEB_SAMPLE_FLOAT32BE:
    return PA_SAMPLE_FLOAT32BE;
  default:
    return PA_SAMPLE_INVALID;
  }
}

static int
create_pa_stream(cubeb_stream * stm,
                 pa_stream ** pa_stm,
                 cubeb_stream_params * stream_params,
                 char const * stream_name)
{
  assert(stm && stream_params);
  assert(&stm->input_stream == pa_stm || (&stm->output_stream == pa_stm &&
         stream_params->layout != CUBEB_LAYOUT_UNDEFINED &&
         CUBEB_CHANNEL_LAYOUT_MAPS[stream_params->layout].channels == stream_params->channels));
  *pa_stm = NULL;
  pa_sample_spec ss;
  ss.format = to_pulse_format(stream_params->format);
  if (ss.format == PA_SAMPLE_INVALID)
    return CUBEB_ERROR_INVALID_FORMAT;
  ss.rate = stream_params->rate;
  ss.channels = stream_params->channels;

  if (stream_params->layout == CUBEB_LAYOUT_UNDEFINED) {
    *pa_stm = WRAP(pa_stream_new)(stm->context->context, stream_name, &ss, NULL);
  } else {
    pa_channel_map cm;
    layout_to_channel_map(stream_params->layout, &cm);
    *pa_stm = WRAP(pa_stream_new)(stm->context->context, stream_name, &ss, &cm);
  }
  return (*pa_stm == NULL) ? CUBEB_ERROR : CUBEB_OK;
}

static pa_buffer_attr
set_buffering_attribute(unsigned int latency_frames, pa_sample_spec * sample_spec)
{
  pa_buffer_attr battr;
  battr.maxlength = -1;
  battr.prebuf    = -1;
  battr.tlength   = latency_frames * WRAP(pa_frame_size)(sample_spec);
  battr.minreq    = battr.tlength / 4;
  battr.fragsize  = battr.minreq;

  LOG("Requested buffer attributes maxlength %u, tlength %u, prebuf %u, minreq %u, fragsize %u",
      battr.maxlength, battr.tlength, battr.prebuf, battr.minreq, battr.fragsize);

  return battr;
}

static int
pulse_stream_init(cubeb * context,
                  cubeb_stream ** stream,
                  char const * stream_name,
                  cubeb_devid input_device,
                  cubeb_stream_params * input_stream_params,
                  cubeb_devid output_device,
                  cubeb_stream_params * output_stream_params,
                  unsigned int latency_frames,
                  cubeb_data_callback data_callback,
                  cubeb_state_callback state_callback,
                  void * user_ptr)
{
  cubeb_stream * stm;
  pa_buffer_attr battr;
  int r;

  assert(context);

  // If the connection failed for some reason, try to reconnect
  if (context->error == 1 && pulse_context_init(context) != 0) {
    return CUBEB_ERROR;
  }

  *stream = NULL;

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = context;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->volume = PULSE_NO_GAIN;
  stm->state = -1;
  assert(stm->shutdown == 0);

  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
  if (output_stream_params) {
    r = create_pa_stream(stm, &stm->output_stream, output_stream_params, stream_name);
    if (r != CUBEB_OK) {
      WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);
      pulse_stream_destroy(stm);
      return r;
    }

    stm->output_sample_spec = *(WRAP(pa_stream_get_sample_spec)(stm->output_stream));

    WRAP(pa_stream_set_state_callback)(stm->output_stream, stream_state_callback, stm);
    WRAP(pa_stream_set_write_callback)(stm->output_stream, stream_write_callback, stm);

    battr = set_buffering_attribute(latency_frames, &stm->output_sample_spec);
    WRAP(pa_stream_connect_playback)(stm->output_stream,
                                     (char const *) output_device,
                                     &battr,
                                     PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING |
                                     PA_STREAM_START_CORKED | PA_STREAM_ADJUST_LATENCY,
                                     NULL, NULL);
  }

  // Set up input stream
  if (input_stream_params) {
    r = create_pa_stream(stm, &stm->input_stream, input_stream_params, stream_name);
    if (r != CUBEB_OK) {
      WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);
      pulse_stream_destroy(stm);
      return r;
    }

    stm->input_sample_spec = *(WRAP(pa_stream_get_sample_spec)(stm->input_stream));

    WRAP(pa_stream_set_state_callback)(stm->input_stream, stream_state_callback, stm);
    WRAP(pa_stream_set_read_callback)(stm->input_stream, stream_read_callback, stm);

    battr = set_buffering_attribute(latency_frames, &stm->input_sample_spec);
    WRAP(pa_stream_connect_record)(stm->input_stream,
                                   (char const *) input_device,
                                   &battr,
                                   PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING |
                                   PA_STREAM_START_CORKED | PA_STREAM_ADJUST_LATENCY);
  }

  r = wait_until_stream_ready(stm);
  if (r == 0) {
    /* force a timing update now, otherwise timing info does not become valid
       until some point after initialization has completed. */
    r = stream_update_timing_info(stm);
  }

  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  if (r != 0) {
    pulse_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  if (g_cubeb_log_level) {
    if (output_stream_params){
      const pa_buffer_attr * output_att;
      output_att = WRAP(pa_stream_get_buffer_attr)(stm->output_stream);
      LOG("Output buffer attributes maxlength %u, tlength %u, prebuf %u, minreq %u, fragsize %u",output_att->maxlength, output_att->tlength,
          output_att->prebuf, output_att->minreq, output_att->fragsize);
    }

    if (input_stream_params){
      const pa_buffer_attr * input_att;
      input_att = WRAP(pa_stream_get_buffer_attr)(stm->input_stream);
      LOG("Input buffer attributes maxlength %u, tlength %u, prebuf %u, minreq %u, fragsize %u",input_att->maxlength, input_att->tlength,
          input_att->prebuf, input_att->minreq, input_att->fragsize);
    }
  }

  *stream = stm;

  return CUBEB_OK;
}

static void
pulse_stream_destroy(cubeb_stream * stm)
{
  stream_cork(stm, CORK);

  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
  if (stm->output_stream) {

    if (stm->drain_timer) {
      /* there's no pa_rttime_free, so use this instead. */
      WRAP(pa_threaded_mainloop_get_api)(stm->context->mainloop)->time_free(stm->drain_timer);
    }

    WRAP(pa_stream_set_state_callback)(stm->output_stream, NULL, NULL);
    WRAP(pa_stream_set_write_callback)(stm->output_stream, NULL, NULL);
    WRAP(pa_stream_disconnect)(stm->output_stream);
    WRAP(pa_stream_unref)(stm->output_stream);
  }

  if (stm->input_stream) {
    WRAP(pa_stream_set_state_callback)(stm->input_stream, NULL, NULL);
    WRAP(pa_stream_set_read_callback)(stm->input_stream, NULL, NULL);
    WRAP(pa_stream_disconnect)(stm->input_stream);
    WRAP(pa_stream_unref)(stm->input_stream);
  }
  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  free(stm);
}

static void
pulse_defer_event_cb(pa_mainloop_api * a, void * userdata)
{
  (void)a;
  cubeb_stream * stm = userdata;
  if (stm->shutdown) {
    return;
  }
  size_t writable_size = WRAP(pa_stream_writable_size)(stm->output_stream);
  trigger_user_callback(stm->output_stream, NULL, writable_size, stm);
}

static int
pulse_stream_start(cubeb_stream * stm)
{
  stm->shutdown = 0;
  stream_cork(stm, UNCORK | NOTIFY);

  if (stm->output_stream && !stm->input_stream) {
    /* On output only case need to manually call user cb once in order to make
     * things roll. This is done via a defer event in order to execute it
     * from PA server thread. */
    WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
    WRAP(pa_mainloop_api_once)(WRAP(pa_threaded_mainloop_get_api)(stm->context->mainloop),
                               pulse_defer_event_cb, stm);
    WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);
  }

  return CUBEB_OK;
}

static int
pulse_stream_stop(cubeb_stream * stm)
{
  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
  stm->shutdown = 1;
  // If draining is taking place wait to finish
  while (stm->drain_timer) {
    WRAP(pa_threaded_mainloop_wait)(stm->context->mainloop);
  }
  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  stream_cork(stm, CORK | NOTIFY);
  return CUBEB_OK;
}

static int
pulse_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  int r, in_thread;
  pa_usec_t r_usec;
  uint64_t bytes;

  if (!stm || !stm->output_stream) {
    return CUBEB_ERROR;
  }

  in_thread = WRAP(pa_threaded_mainloop_in_thread)(stm->context->mainloop);

  if (!in_thread) {
    WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);
  }
  r = WRAP(pa_stream_get_time)(stm->output_stream, &r_usec);
  if (!in_thread) {
    WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);
  }

  if (r != 0) {
    return CUBEB_ERROR;
  }

  bytes = WRAP(pa_usec_to_bytes)(r_usec, &stm->output_sample_spec);
  *position = bytes / WRAP(pa_frame_size)(&stm->output_sample_spec);

  return CUBEB_OK;
}

static int
pulse_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  pa_usec_t r_usec;
  int negative, r;

  if (!stm || !stm->output_stream) {
    return CUBEB_ERROR;
  }

  r = WRAP(pa_stream_get_latency)(stm->output_stream, &r_usec, &negative);
  assert(!negative);
  if (r) {
    return CUBEB_ERROR;
  }

  *latency = r_usec * stm->output_sample_spec.rate / PA_USEC_PER_SEC;
  return CUBEB_OK;
}

static void
volume_success(pa_context *c, int success, void *userdata)
{
  (void)success;
  (void)c;
  cubeb_stream * stream = userdata;
  assert(success);
  WRAP(pa_threaded_mainloop_signal)(stream->context->mainloop, 0);
}

static int
pulse_stream_set_volume(cubeb_stream * stm, float volume)
{
  uint32_t index;
  pa_operation * op;
  pa_volume_t vol;
  pa_cvolume cvol;
  const pa_sample_spec * ss;

  if (!stm->output_stream) {
    return CUBEB_ERROR;
  }

  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);

  /* if the pulse daemon is configured to use flat volumes,
   * apply our own gain instead of changing the input volume on the sink. */
  if (stm->context->default_sink_info->flags & PA_SINK_FLAT_VOLUME) {
    stm->volume = volume;
  } else {
    ss = WRAP(pa_stream_get_sample_spec)(stm->output_stream);

    vol = WRAP(pa_sw_volume_from_linear)(volume);
    WRAP(pa_cvolume_set)(&cvol, ss->channels, vol);

    index = WRAP(pa_stream_get_index)(stm->output_stream);

    op = WRAP(pa_context_set_sink_input_volume)(stm->context->context,
                                                index, &cvol, volume_success,
                                                stm);
    if (op) {
      operation_wait(stm->context, stm->output_stream, op);
      WRAP(pa_operation_unref)(op);
    }
  }

  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  return CUBEB_OK;
}

struct sink_input_info_result {
  pa_cvolume * cvol;
  pa_threaded_mainloop * mainloop;
};

static void
sink_input_info_cb(pa_context * c, pa_sink_input_info const * i, int eol, void * u)
{
  struct sink_input_info_result * r = u;
  if (!eol) {
    *r->cvol = i->volume;
  }
  WRAP(pa_threaded_mainloop_signal)(r->mainloop, 0);
}

static int
pulse_stream_set_panning(cubeb_stream * stm, float panning)
{
  const pa_channel_map * map;
  pa_cvolume cvol;
  uint32_t index;
  pa_operation * op;

  if (!stm->output_stream) {
    return CUBEB_ERROR;
  }

  WRAP(pa_threaded_mainloop_lock)(stm->context->mainloop);

  map = WRAP(pa_stream_get_channel_map)(stm->output_stream);
  if (!WRAP(pa_channel_map_can_balance)(map)) {
    WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);
    return CUBEB_ERROR;
  }

  index = WRAP(pa_stream_get_index)(stm->output_stream);

  struct sink_input_info_result r = { &cvol, stm->context->mainloop };
  op = WRAP(pa_context_get_sink_input_info)(stm->context->context,
                                            index,
                                            sink_input_info_cb,
                                            &r);
  if (op) {
    operation_wait(stm->context, stm->output_stream, op);
    WRAP(pa_operation_unref)(op);
  }

  WRAP(pa_cvolume_set_balance)(&cvol, map, panning);

  op = WRAP(pa_context_set_sink_input_volume)(stm->context->context,
                                              index, &cvol, volume_success,
                                              stm);
  if (op) {
    operation_wait(stm->context, stm->output_stream, op);
    WRAP(pa_operation_unref)(op);
  }

  WRAP(pa_threaded_mainloop_unlock)(stm->context->mainloop);

  return CUBEB_OK;
}

typedef struct {
  char * default_sink_name;
  char * default_source_name;

  cubeb_device_info * devinfo;
  uint32_t max;
  uint32_t count;
  cubeb * context;
} pulse_dev_list_data;

static cubeb_device_fmt
pulse_format_to_cubeb_format(pa_sample_format_t format)
{
  switch (format) {
  case PA_SAMPLE_S16LE:
    return CUBEB_DEVICE_FMT_S16LE;
  case PA_SAMPLE_S16BE:
    return CUBEB_DEVICE_FMT_S16BE;
  case PA_SAMPLE_FLOAT32LE:
    return CUBEB_DEVICE_FMT_F32LE;
  case PA_SAMPLE_FLOAT32BE:
    return CUBEB_DEVICE_FMT_F32BE;
  default:
    return CUBEB_DEVICE_FMT_F32NE;
  }
}

static void
pulse_ensure_dev_list_data_list_size (pulse_dev_list_data * list_data)
{
  if (list_data->count == list_data->max) {
    list_data->max += 8;
    list_data->devinfo = realloc(list_data->devinfo,
        sizeof(cubeb_device_info) * list_data->max);
  }
}

static cubeb_device_state
pulse_get_state_from_sink_port(pa_sink_port_info * info)
{
  if (info != NULL) {
#if PA_CHECK_VERSION(2, 0, 0)
    if (info->available == PA_PORT_AVAILABLE_NO)
      return CUBEB_DEVICE_STATE_UNPLUGGED;
    else /*if (info->available == PA_PORT_AVAILABLE_YES) + UNKNOWN */
#endif
      return CUBEB_DEVICE_STATE_ENABLED;
  }

  return CUBEB_DEVICE_STATE_ENABLED;
}

static void
pulse_sink_info_cb(pa_context * context, const pa_sink_info * info,
                   int eol, void * user_data)
{
  pulse_dev_list_data * list_data = user_data;
  cubeb_device_info * devinfo;
  const char * prop;

  (void)context;

  if (eol || info == NULL)
    return;

  pulse_ensure_dev_list_data_list_size(list_data);
  devinfo = &list_data->devinfo[list_data->count];
  memset(devinfo, 0, sizeof(cubeb_device_info));

  devinfo->device_id = strdup(info->name);
  devinfo->devid = (cubeb_devid) devinfo->device_id;
  devinfo->friendly_name = strdup(info->description);
  prop = WRAP(pa_proplist_gets)(info->proplist, "sysfs.path");
  if (prop)
    devinfo->group_id = strdup(prop);
  prop = WRAP(pa_proplist_gets)(info->proplist, "device.vendor.name");
  if (prop)
    devinfo->vendor_name = strdup(prop);

  devinfo->type = CUBEB_DEVICE_TYPE_OUTPUT;
  devinfo->state = pulse_get_state_from_sink_port(info->active_port);
  devinfo->preferred = (strcmp(info->name, list_data->default_sink_name) == 0) ?
    CUBEB_DEVICE_PREF_ALL : CUBEB_DEVICE_PREF_NONE;

  devinfo->format = CUBEB_DEVICE_FMT_ALL;
  devinfo->default_format = pulse_format_to_cubeb_format(info->sample_spec.format);
  devinfo->max_channels = info->channel_map.channels;
  devinfo->min_rate = 1;
  devinfo->max_rate = PA_RATE_MAX;
  devinfo->default_rate = info->sample_spec.rate;

  devinfo->latency_lo = 0;
  devinfo->latency_hi = 0;

  list_data->count += 1;

  WRAP(pa_threaded_mainloop_signal)(list_data->context->mainloop, 0);
}

static cubeb_device_state
pulse_get_state_from_source_port(pa_source_port_info * info)
{
  if (info != NULL) {
#if PA_CHECK_VERSION(2, 0, 0)
    if (info->available == PA_PORT_AVAILABLE_NO)
      return CUBEB_DEVICE_STATE_UNPLUGGED;
    else /*if (info->available == PA_PORT_AVAILABLE_YES) + UNKNOWN */
#endif
      return CUBEB_DEVICE_STATE_ENABLED;
  }

  return CUBEB_DEVICE_STATE_ENABLED;
}

static void
pulse_source_info_cb(pa_context * context, const pa_source_info * info,
    int eol, void * user_data)
{
  pulse_dev_list_data * list_data = user_data;
  cubeb_device_info * devinfo;
  const char * prop;

  (void)context;

  if (eol)
    return;

  pulse_ensure_dev_list_data_list_size(list_data);
  devinfo = &list_data->devinfo[list_data->count];
  memset(devinfo, 0, sizeof(cubeb_device_info));

  devinfo->device_id = strdup(info->name);
  devinfo->devid = (cubeb_devid) devinfo->device_id;
  devinfo->friendly_name = strdup(info->description);
  prop = WRAP(pa_proplist_gets)(info->proplist, "sysfs.path");
  if (prop)
    devinfo->group_id = strdup(prop);
  prop = WRAP(pa_proplist_gets)(info->proplist, "device.vendor.name");
  if (prop)
    devinfo->vendor_name = strdup(prop);

  devinfo->type = CUBEB_DEVICE_TYPE_INPUT;
  devinfo->state = pulse_get_state_from_source_port(info->active_port);
  devinfo->preferred = (strcmp(info->name, list_data->default_source_name) == 0) ?
    CUBEB_DEVICE_PREF_ALL : CUBEB_DEVICE_PREF_NONE;

  devinfo->format = CUBEB_DEVICE_FMT_ALL;
  devinfo->default_format = pulse_format_to_cubeb_format(info->sample_spec.format);
  devinfo->max_channels = info->channel_map.channels;
  devinfo->min_rate = 1;
  devinfo->max_rate = PA_RATE_MAX;
  devinfo->default_rate = info->sample_spec.rate;

  devinfo->latency_lo = 0;
  devinfo->latency_hi = 0;

  list_data->count += 1;
  WRAP(pa_threaded_mainloop_signal)(list_data->context->mainloop, 0);
}

static void
pulse_server_info_cb(pa_context * c, const pa_server_info * i, void * userdata)
{
  pulse_dev_list_data * list_data = userdata;

  (void)c;

  free(list_data->default_sink_name);
  free(list_data->default_source_name);
  list_data->default_sink_name = strdup(i->default_sink_name);
  list_data->default_source_name = strdup(i->default_source_name);

  WRAP(pa_threaded_mainloop_signal)(list_data->context->mainloop, 0);
}

static int
pulse_enumerate_devices(cubeb * context, cubeb_device_type type,
                        cubeb_device_collection * collection)
{
  pulse_dev_list_data user_data = { NULL, NULL, NULL, 0, 0, context };
  pa_operation * o;

  WRAP(pa_threaded_mainloop_lock)(context->mainloop);

  o = WRAP(pa_context_get_server_info)(context->context,
      pulse_server_info_cb, &user_data);
  if (o) {
    operation_wait(context, NULL, o);
    WRAP(pa_operation_unref)(o);
  }

  if (type & CUBEB_DEVICE_TYPE_OUTPUT) {
    o = WRAP(pa_context_get_sink_info_list)(context->context,
        pulse_sink_info_cb, &user_data);
    if (o) {
      operation_wait(context, NULL, o);
      WRAP(pa_operation_unref)(o);
    }
  }

  if (type & CUBEB_DEVICE_TYPE_INPUT) {
    o = WRAP(pa_context_get_source_info_list)(context->context,
        pulse_source_info_cb, &user_data);
    if (o) {
      operation_wait(context, NULL, o);
      WRAP(pa_operation_unref)(o);
    }
  }

  WRAP(pa_threaded_mainloop_unlock)(context->mainloop);

  collection->device = user_data.devinfo;
  collection->count = user_data.count;

  free(user_data.default_sink_name);
  free(user_data.default_source_name);
  return CUBEB_OK;
}

static int
pulse_stream_get_current_device(cubeb_stream * stm, cubeb_device ** const device)
{
#if PA_CHECK_VERSION(0, 9, 8)
  *device = calloc(1, sizeof(cubeb_device));
  if (*device == NULL)
    return CUBEB_ERROR;

  if (stm->input_stream) {
    const char * name = WRAP(pa_stream_get_device_name)(stm->input_stream);
    (*device)->input_name = (name == NULL) ? NULL : strdup(name);
  }

  if (stm->output_stream) {
    const char * name = WRAP(pa_stream_get_device_name)(stm->output_stream);
    (*device)->output_name = (name == NULL) ? NULL : strdup(name);
  }

  return CUBEB_OK;
#else
  return CUBEB_ERROR_NOT_SUPPORTED;
#endif
}

static int
pulse_stream_device_destroy(cubeb_stream * stream,
                            cubeb_device * device)
{
  (void)stream;
  free(device->input_name);
  free(device->output_name);
  free(device);
  return CUBEB_OK;
}

static void
pulse_subscribe_callback(pa_context * ctx,
                         pa_subscription_event_type_t t,
                         uint32_t index, void * userdata)
{
  (void)ctx;
  cubeb * context = userdata;

  switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
  case PA_SUBSCRIPTION_EVENT_SOURCE:
  case PA_SUBSCRIPTION_EVENT_SINK:

    if (g_cubeb_log_level) {
      if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE &&
          (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
        LOG("Removing sink index %d", index);
      } else if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE &&
          (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
        LOG("Adding sink index %d", index);
      }
      if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK &&
          (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE) {
        LOG("Removing source index %d", index);
      } else if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK &&
          (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
        LOG("Adding source index %d", index);
      }
    }

    if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_REMOVE ||
        (t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
      context->collection_changed_callback(context, context->collection_changed_user_ptr);
    }
    break;
  }
}

static void
subscribe_success(pa_context *c, int success, void *userdata)
{
  (void)c;
  cubeb * context = userdata;
  assert(success);
  WRAP(pa_threaded_mainloop_signal)(context->mainloop, 0);
}

static int
pulse_register_device_collection_changed(cubeb * context,
                                         cubeb_device_type devtype,
                                         cubeb_device_collection_changed_callback collection_changed_callback,
                                         void * user_ptr)
{
  context->collection_changed_callback = collection_changed_callback;
  context->collection_changed_user_ptr = user_ptr;

  WRAP(pa_threaded_mainloop_lock)(context->mainloop);

  pa_subscription_mask_t mask;
  if (context->collection_changed_callback == NULL) {
    // Unregister subscription
    WRAP(pa_context_set_subscribe_callback)(context->context, NULL, NULL);
    mask = PA_SUBSCRIPTION_MASK_NULL;
  } else {
    WRAP(pa_context_set_subscribe_callback)(context->context, pulse_subscribe_callback, context);
    if (devtype == CUBEB_DEVICE_TYPE_INPUT)
      mask = PA_SUBSCRIPTION_MASK_SOURCE;
    else if (devtype == CUBEB_DEVICE_TYPE_OUTPUT)
      mask = PA_SUBSCRIPTION_MASK_SINK;
    else
      mask = PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE;
  }

  pa_operation * o;
  o = WRAP(pa_context_subscribe)(context->context, mask, subscribe_success, context);
  if (o == NULL) {
    LOG("Context subscribe failed");
    return CUBEB_ERROR;
  }
  operation_wait(context, NULL, o);
  WRAP(pa_operation_unref)(o);

  WRAP(pa_threaded_mainloop_unlock)(context->mainloop);

  return CUBEB_OK;
}

static struct cubeb_ops const pulse_ops = {
  .init = pulse_init,
  .get_backend_id = pulse_get_backend_id,
  .get_max_channel_count = pulse_get_max_channel_count,
  .get_min_latency = pulse_get_min_latency,
  .get_preferred_sample_rate = pulse_get_preferred_sample_rate,
  .get_preferred_channel_layout = pulse_get_preferred_channel_layout,
  .enumerate_devices = pulse_enumerate_devices,
  .device_collection_destroy = cubeb_utils_default_device_collection_destroy,
  .destroy = pulse_destroy,
  .stream_init = pulse_stream_init,
  .stream_destroy = pulse_stream_destroy,
  .stream_start = pulse_stream_start,
  .stream_stop = pulse_stream_stop,
  .stream_get_position = pulse_stream_get_position,
  .stream_get_latency = pulse_stream_get_latency,
  .stream_set_volume = pulse_stream_set_volume,
  .stream_set_panning = pulse_stream_set_panning,
  .stream_get_current_device = pulse_stream_get_current_device,
  .stream_device_destroy = pulse_stream_device_destroy,
  .stream_register_device_changed_callback = NULL,
  .register_device_collection_changed = pulse_register_device_collection_changed
};
