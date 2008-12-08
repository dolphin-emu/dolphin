#pragma once

#include <xstring>
#include <vector>


namespace W32Util
{
	std::string BrowseForFolder(HWND parent, char *title);
	bool BrowseForFileName (bool _bLoad, HWND _hParent, const char *_pTitle,
		const char *_pInitialFolder,const char *_pFilter,const char *_pExtension, 
		std::string& _strFileName);
	std::vector<std::string> BrowseForFileNameMultiSelect(bool _bLoad, HWND _hParent, const char *_pTitle,
		const char *_pInitialFolder,const char *_pFilter,const char *_pExtension);
}