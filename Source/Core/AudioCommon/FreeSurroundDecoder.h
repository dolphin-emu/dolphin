// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FREESURROUND_DECODER_H
#define FREESURROUND_DECODER_H
#include <complex>
#include <map>
#include <vector>

typedef std::complex<double> cplx;

// Identifiers for the supported output channels (from front to back, left to
// right). The ordering here also determines the ordering of interleaved
// samples in the output signal.

typedef enum channel_id
{
  ci_none = 0,
  ci_front_left = 1 << 1,
  ci_front_center_left = 1 << 2,
  ci_front_center = 1 << 3,
  ci_front_center_right = 1 << 4,
  ci_front_right = 1 << 5,
  ci_side_front_left = 1 << 6,
  ci_side_front_right = 1 << 7,
  ci_side_center_left = 1 << 8,
  ci_side_center_right = 1 << 9,
  ci_side_back_left = 1 << 10,
  ci_side_back_right = 1 << 11,
  ci_back_left = 1 << 12,
  ci_back_center_left = 1 << 13,
  ci_back_center = 1 << 14,
  ci_back_center_right = 1 << 15,
  ci_back_right = 1 << 16,
  ci_lfe = 1 << 31
} channel_id;

// The supported output channel setups. A channel setup is defined by the set
// of channels that are present. Here is a graphic of the cs_5point1 setup:
// http://en.wikipedia.org/wiki/File:5_1_channels_(surround_sound)_label.svg
typedef enum channel_setup
{
  cs_5point1 =
      ci_front_left | ci_front_center | ci_front_right | ci_back_left | ci_back_right | ci_lfe,

  cs_7point1 = ci_front_left | ci_front_center | ci_front_right | ci_side_center_left |
               ci_side_center_right | ci_back_left | ci_back_right | ci_lfe
} channel_setup;

extern "C" {

// we're using doubles here...
#define kiss_fft_scalar double

typedef struct
{
  kiss_fft_scalar r;
  kiss_fft_scalar i;
} kiss_fft_cpx;

typedef struct kiss_fftr_state* kiss_fftr_cfg;

kiss_fftr_cfg kiss_fftr_alloc(int nfft, int inverse_fft, void* mem, size_t* lenmem);
/*
 nfft must be even

 If you don't care to allocate space, use mem = lenmem = NULL
*/

void kiss_fftr(kiss_fftr_cfg cfg, const kiss_fft_scalar* timedata, kiss_fft_cpx* freqdata);
/*
 input timedata has nfft scalar points
 output freqdata has nfft/2+1 complex points
*/

void kiss_fftri(kiss_fftr_cfg cfg, const kiss_fft_cpx* freqdata, kiss_fft_scalar* timedata);
/*
 input freqdata has  nfft/2+1 complex points
 output timedata has nfft scalar points
*/
}

typedef std::vector<std::vector<float*>> alloc_lut;

constexpr int grid_res = 21;  // resolution of the lookup grid

// channel allocation maps (per setup)
extern std::map<unsigned, alloc_lut> chn_alloc;
// channel metadata maps (per setup)
extern std::map<unsigned, std::vector<float>> chn_angle;
extern std::map<unsigned, std::vector<float>> chn_xsf;
extern std::map<unsigned, std::vector<float>> chn_ysf;
extern std::map<unsigned, std::vector<channel_id>> chn_id;

// The FreeSurround decoder.

class DPL2FSDecoder
{
public:
  // Create an instance of the decoder.
  // @param setup The output channel setup -- determines the number of output
  // channels and their place in the sound field.
  // @param blocksize Granularity at which data is processed by the decode()
  // function. Must be a power of two and should correspond to ca. 10ms worth
  // of single-channel samples (default is 4096 for 44.1Khz data). Do not make
  // it shorter or longer than 5ms to 20ms since the granularity at which
  // locations are decoded changes with this.
  DPL2FSDecoder();
  ~DPL2FSDecoder();

  void Init(channel_setup chsetup = cs_5point1, unsigned int blocksize = 4096,
            unsigned int sample_rate = 48000);

  // Decode a chunk of stereo sound. The output is delayed by half of the
  // blocksize. This function is the only one needed for straightforward
  // decoding.
  // @param input Contains exactly blocksize (multiplexed) stereo samples, i.e.
  // 2*blocksize numbers.
  // @return A pointer to an internal buffer of exactly blocksize (multiplexed)
  // multichannel samples. The actual number of values depends on the number of
  // output channels in the chosen channel setup.
  float* decode(const float* input);

  // Flush the internal buffer.
  void flush();

private:
  // set soundfield & rendering parameters
  // for more information, see full FreeSurround source code
  void set_circular_wrap(float v);
  void set_shift(float v);
  void set_depth(float v);
  void set_focus(float v);
  void set_center_image(float v);
  void set_front_separation(float v);
  void set_rear_separation(float v);
  void set_low_cutoff(float v);
  void set_high_cutoff(float v);
  void set_bass_redirection(bool v);

  // number of samples per input/output block, number of output channels
  unsigned int N, C;
  unsigned int samplerate;

  // the channel setup
  channel_setup setup;
  bool initialized;

  // parameters
  // angle of the front soundstage around the listener (90\B0=default)
  float circular_wrap;

  // forward/backward offset of the soundstage
  float shift;

  // backward extension of the soundstage
  float depth;

  // localization of the sound events
  float focus;

  // presence of the center speaker
  float center_image;

  // front stereo separation
  float front_separation;

  // rear stereo separation
  float rear_separation;

  // LFE cutoff frequencies
  float lo_cut, hi_cut;

  // whether to use the LFE channel
  bool use_lfe;

  // FFT data structures
  // left total, right total (source arrays), time-domain destination buffer
  // array
  std::vector<double> lt, rt, dst;

  // left total / right total in frequency domain
  std::vector<cplx> lf, rf;

  // FFT buffers
  kiss_fftr_cfg forward, inverse;

  // buffers
  // whether the buffer is currently empty or dirty
  bool buffer_empty;

  // stereo input buffer (multiplexed)
  std::vector<float> inbuf;

  // multichannel output buffer (multiplexed)
  std::vector<float> outbuf;

  // the window function, precomputed
  std::vector<double> wnd;

  // the signal to be constructed in every channel, in the frequency domain
  // instantiate the decoder with a given channel setup and processing block
  // size (in samples)
  std::vector<std::vector<cplx>> signal;

  // helper functions
  static inline float sqr(double x);
  static inline double amplitude(const cplx& x);
  static inline double phase(const cplx& x);
  static inline cplx polar(double a, double p);
  static inline float min(double a, double b);
  static inline float max(double a, double b);
  static inline float clamp(double x);
  static inline float sign(double x);

  // get the distance of the soundfield edge, along a given angle
  static inline double edgedistance(double a);

  // get the index (and fractional offset!) in a piecewise-linear channel
  // allocation grid
  static int map_to_grid(double& x);

  // decode a block of data and overlap-add it into outbuf
  void buffered_decode(const float* input);

  // transform amp/phase difference space into x/y soundfield space
  static void transform_decode(double a, double p, double& x, double& y);

  // apply a circular_wrap transformation to some position
  static void transform_circular_wrap(double& x, double& y, double refangle);

  // apply a focus transformation to some position
  static void transform_focus(double& x, double& y, double focus);
};
#endif
