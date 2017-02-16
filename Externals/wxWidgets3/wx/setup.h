#ifdef __APPLE__
#define __WXMAC__
#define __WXOSX__
#define __WXOSX_COCOA__
#include "wx/wxcocoa.h"
#elif defined _WIN32
#define __WXMSW__
#include "wx/wxmsw.h"
#else
#define __WXGTK__
#include "wx/wxgtk.h"
#endif
