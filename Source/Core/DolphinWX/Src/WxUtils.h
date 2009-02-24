#ifndef WXUTILS_H
#define WXUTILS_H

namespace WxUtils {
	// Launch a file according to its mime type
	void Launch(const char *filename);
	
	// Launch an file explorer window on a certain path
	void Explore(const char *path);
		
} // NameSpace WxUtils
#endif // WXUTILS
