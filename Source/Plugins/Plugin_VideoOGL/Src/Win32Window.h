#ifndef _WIN32WINDOW_H
#define _WIN32WINDOW_H

#include "GLWindow.h"

#ifdef _WIN32
class Win32Window : public GLWindow 
{
public:
	virtual void SwapBuffers();
	virtual void SetWindowText(const char *text);
	virtual bool PeekMessages();
	virtual void Update();
	virtual bool MakeCurrent();
	HWND GetWnd(){return myHandle;};
	HWND GetParentWnd(){return myParent;};

	static bool valid() { return true; }
	~Win32Window();
	Win32Window();
	static sf::Key::Code VirtualKeyCodeToSF(WPARAM VirtualKey, LPARAM Flags);
	static sf::Key::Code GetShiftState(bool KeyDown);

private:
	struct VideoMode
	{
		unsigned int Width;
		unsigned int Height;
		unsigned int BitsPerPixel;
		unsigned int DepthBits;
		unsigned int StencilBits;
		unsigned int AntialiasingLevel;
	};

	virtual void ShowMouseCursor(bool Show);
	void RegisterWindowClass();
	void SwitchToFullscreen(const VideoMode& Mode);
	void CreateContext(VideoMode& Mode);
	void Cleanup();
	void ProcessEvent(UINT Message, WPARAM WParam, LPARAM LParam);
	static LRESULT CALLBACK GlobalOnEvent(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam);

	// Static member data
	static unsigned int		ourWindowCount;
	static const char*		ourClassName;
	static Win32Window*		ourFullscreenWindow;

	// Member data
	HWND		myHandle;
	HINSTANCE	myhInstance;
	HWND		myParent; // Possibly not wanted here
	long		myCallback;
	HCURSOR		myCursor;
	HICON		myIcon;
	bool		myKeyRepeatEnabled;
	bool		myIsCursorIn;
	HDC			myDeviceContext;
	HGLRC		myGLContext;
};

#else

class Win32Window : public GLWindow 
{
public:
    Win32Window() {}
};

#endif //_WIN32
#endif //_WIN32WINDOW_H
