#include "Common.h"
#include <wx/wx.h>
#include <wx/string.h>
namespace WxUtils {

	// Launch a file according to its mime type
	void Launch(const char *filename)
	{
		if (! ::wxLaunchDefaultBrowser(wxString::FromAscii(filename))) {
			// WARN_LOG
		}
	}

	// Launch an file explorer window on a certain path
	void Explore(const char *path)
	{
		wxString wxPath = wxString::FromAscii(path);
		
		// Default to file
		if (! wxPath.Contains(wxT("://"))) {
			wxPath = wxT("file://") + wxPath;
		}

		if (! ::wxLaunchDefaultBrowser(wxPath)) {
			// WARN_LOG
		}
	}
	
}
