#include <stdio.h>
#include <stdlib.h>

#include "Misc.h"

namespace W32Util
{
	//shamelessly taken from http://www.catch22.org.uk/tuts/tips.asp
	void CenterWindow(HWND hwnd)
	{
		HWND hwndParent;
		RECT rect, rectP;
		int width, height;      
		int screenwidth, screenheight;
		int x, y;

		//make the window relative to its parent
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
    
		//make sure that the dialog box never moves outside of
		//the screen
		if(x < 0) x = 0;
		if(y < 0) y = 0;
		if(x + width  > screenwidth)  x = screenwidth  - width;
		if(y + height > screenheight) y = screenheight - height;

		MoveWindow(hwnd, x, y, width, height, FALSE);
	}

	HBITMAP CreateBitmapFromARGB(HWND someHwnd, DWORD *image, int w, int h)
	{
		BITMAPINFO *bitmap_header;
		static char bitmapbuffer[sizeof(BITMAPINFO)+16];
		memset(bitmapbuffer,0,sizeof(BITMAPINFO)+16);
		bitmap_header=(BITMAPINFO *)bitmapbuffer;
		bitmap_header->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmap_header->bmiHeader.biPlanes = 1;
		bitmap_header->bmiHeader.biBitCount = 32;
		bitmap_header->bmiHeader.biCompression = BI_RGB;
		bitmap_header->bmiHeader.biWidth = w;
		bitmap_header->bmiHeader.biHeight = -h;

		((unsigned long *)bitmap_header->bmiColors)[0] = 0x00FF0000;
		((unsigned long *)bitmap_header->bmiColors)[1] = 0x0000FF00;
		((unsigned long *)bitmap_header->bmiColors)[2] = 0x000000FF;

		HDC dc = GetDC(someHwnd);
		HBITMAP bitmap = CreateDIBitmap(dc,&bitmap_header->bmiHeader,CBM_INIT,image,bitmap_header,DIB_RGB_COLORS);
		ReleaseDC(someHwnd,dc);
		return bitmap;
	}	

	void NiceSizeFormat(size_t size, char *out)
	{
		char *sizes[] = {"b","KB","MB","GB","TB","PB","EB"};
		int s = 0;
		int frac = 0;
		while (size>1024)
		{
			s++;
			frac = (int)size & 1023;
			size /= 1024;
		}
		float f = (float)size + ((float)frac / 1024.0f);
		sprintf(out,"%3.1f %s",f,sizes[s]);
	}
	BOOL CopyTextToClipboard(HWND hwnd, TCHAR *text)
	{
		OpenClipboard(hwnd);
		EmptyClipboard();
		HANDLE hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (strlen(text) + 1) * sizeof(TCHAR)); 
		if (hglbCopy == NULL) 
		{ 
			CloseClipboard(); 
			return FALSE; 
		} 

		// Lock the handle and copy the text to the buffer. 

		TCHAR *lptstrCopy = (TCHAR *)GlobalLock(hglbCopy); 
		strcpy(lptstrCopy, text); 
		lptstrCopy[strlen(text)] = (TCHAR) 0;    // null character 
		GlobalUnlock(hglbCopy); 
		SetClipboardData(CF_TEXT,hglbCopy);
		CloseClipboard();
		return TRUE;
	}
}