#include <stdio.h>
#include <stdlib.h>

#include "Misc.h"

namespace W32Util
{
	void CenterWindow(HWND hwnd)
	{
		HWND hwndParent;
		RECT rect, rectP;
		int width, height;      
		int screenwidth, screenheight;
		int x, y;

		// make the window relative to its parent
		hwndParent = GetParent(hwnd);
		if (!hwndParent)
			return;

		GetWindowRect(hwnd, &rect);
		GetWindowRect(hwndParent, &rectP);
        
		width  = rect.right  - rect.left;
		height = rect.bottom - rect.top;

		x = ((rectP.right-rectP.left) -  width) / 2 + rectP.left;
		y = ((rectP.bottom-rectP.top) - height) / 2 + rectP.top;

		screenwidth  = GetSystemMetrics(SM_CXSCREEN);
		screenheight = GetSystemMetrics(SM_CYSCREEN);
    
		// make sure that the dialog box never moves outside of
		// the screen
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x + width  > screenwidth)  x = screenwidth  - width;
		if (y + height > screenheight) y = screenheight - height;

		MoveWindow(hwnd, x, y, width, height, FALSE);
	}
}