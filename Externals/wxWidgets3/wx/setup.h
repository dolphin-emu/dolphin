#ifdef __APPLE__
#define __WXMAC__
#define __WXOSX__
#include "wx/wxcocoa.h"
#elif defined _WIN32
#include "wx/wxmsw.h"
#else
#include "wx/wxgtk.h"
#endif
