#ifndef WINAMP_DSP_H
#define WINAMP_DSP_H


#include <windows.h>

// DSP plugin interface

// notes:
// any window that remains in foreground should optimally pass unused
// keystrokes to the parent (winamp's) window, so that the user
// can still control it. As for storing configuration,
// Configuration data should be stored in <dll directory>\plugin.ini
// (look at the vis plugin for configuration code)

typedef struct winampDSPModule {
  char *description;		// description
  HWND hwndParent;			// parent window (filled in by calling app)
  HINSTANCE hDllInstance;	// instance handle to this DLL (filled in by calling app)

  void (*Config)(struct winampDSPModule *this_mod);  // configuration dialog (if needed)
  int (*Init)(struct winampDSPModule *this_mod);     // 0 on success, creates window, etc (if needed)

  // modify waveform samples: returns number of samples to actually write
  // (typically numsamples, but no more than twice numsamples, and no less than half numsamples)
  // numsamples should always be at least 128. should, but I'm not sure
  int (*ModifySamples)(struct winampDSPModule *this_mod, short int *samples, int numsamples, int bps, int nch, int srate);
			
  void (*Quit)(struct winampDSPModule *this_mod);    // called when unloading

  void *userData; // user data, optional
} winampDSPModule;

typedef struct {
  int version;       // DSP_HDRVER
  char *description; // description of library
  winampDSPModule* (*getModule)(int);	// module retrieval function
} winampDSPHeader;

// exported symbols
typedef winampDSPHeader* (*winampDSPGetHeaderType)();

// header version: 0x20 == 0.20 == winamp 2.0
#define DSP_HDRVER 0x20


#endif // WINAMP_DSP_H
