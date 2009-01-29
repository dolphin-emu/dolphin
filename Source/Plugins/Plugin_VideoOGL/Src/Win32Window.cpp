#include "Win32Window.h"

// Static member data
unsigned int	Win32Window::ourWindowCount			= 0;
const char*		Win32Window::ourClassName			= _T("DolphinEmuWnd");
Win32Window*	Win32Window::ourFullscreenWindow	= NULL;


Win32Window::Win32Window() : GLWindow(),
myHandle			(NULL),
myCallback			(0),
myCursor			(NULL),
myIcon				(NULL),
myKeyRepeatEnabled	(true),
myIsCursorIn		(false)
{
	// Register the window class at first call
	if (ourWindowCount == 0)
		RegisterWindowClass();

	// Use small dimensions
	//SetWinSize(1, 1);

	// Create the rendering window
	if (!(myHandle = CreateWindow(ourClassName, _T(""), WS_OVERLAPPEDWINDOW, 
		CW_USEDEFAULT, CW_USEDEFAULT, 
		GetXwin(), GetYwin(), NULL, NULL, myhInstance, this))) {
		PanicAlert("Error creating GL Window");	
	}

	ShowWindow(myHandle, SW_SHOW);

	// Create the rendering context
	if (myHandle)
	{
		VideoMode Mode;
		Mode.Width				= GetXwin();
		Mode.Height				= GetYwin();
		Mode.BitsPerPixel		= 32;
		Mode.DepthBits			= 24;
		Mode.StencilBits		= 8;
		Mode.AntialiasingLevel	= 0;
		CreateContext(Mode);

		// Don't activate by default
		//SetNotActive();
	}
}

Win32Window::~Win32Window()
{
	// Destroy the custom icon, if any
	if (myIcon)
		DestroyIcon(myIcon);

	if (!myCallback)
	{
		// Destroy the window
		if (myHandle)
			DestroyWindow(myHandle);

		// Decrement the window count
		ourWindowCount--;

		// Unregister window class if we were the last window
		if (ourWindowCount == 0)
			UnregisterClass(ourClassName, GetModuleHandle(NULL));
	}
	else
	{
		// The window is external : remove the hook on its message callback
		SetWindowLongPtr(myHandle, GWLP_WNDPROC, myCallback);
	}
}

void Win32Window::SwapBuffers()
{
	if (myDeviceContext && myGLContext)
		::SwapBuffers(myDeviceContext);
}

void Win32Window::SetWindowText(const char *text)
{
	::SetWindowText(GetWnd(), text);
}

void Win32Window::ShowMouseCursor(bool Show)
{
	if (Show)
		myCursor = LoadCursor(NULL, IDC_ARROW);
	else
		myCursor = NULL;

	SetCursor(myCursor);
}

bool Win32Window::PeekMessages()
{
	// We update the window only if we own it
	if (!myCallback)
	{
		MSG Message;
		while (PeekMessage(&Message, myHandle, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
	}
	return true; // I don't know why this is bool...
}

void Win32Window::Update()
{
	// We just check all of our events here
	// TODO
	PeekMessages();
	eventHandler->Update();
	updateDim();
}

bool Win32Window::MakeCurrent()
{
	if (myDeviceContext && myGLContext && (wglGetCurrentContext() != myGLContext))
	{
		wglMakeCurrent(myDeviceContext, myGLContext);
		return true;
	}
	else
		return false;
}

void Win32Window::RegisterWindowClass()
{
	WNDCLASS WindowClass;
	myhInstance				  = GetModuleHandle(NULL);
	WindowClass.style         = CS_OWNDC;
	WindowClass.lpfnWndProc   = &Win32Window::GlobalOnEvent;
	WindowClass.cbClsExtra    = 0;
	WindowClass.cbWndExtra    = 0;
	WindowClass.hInstance     = myhInstance;
	WindowClass.hIcon         = NULL;
	WindowClass.hCursor       = 0;
	WindowClass.hbrBackground = 0;
	WindowClass.lpszMenuName  = NULL;
	WindowClass.lpszClassName = ourClassName;
	if (!RegisterClass(&WindowClass))
		ERROR_LOG("Failed To Register The Window Class\n");
}

void Win32Window::SwitchToFullscreen(const VideoMode& Mode)
{
	DEVMODE DevMode;
	DevMode.dmSize       = sizeof(DEVMODE);
	DevMode.dmPelsWidth  = Mode.Width;
	DevMode.dmPelsHeight = Mode.Height;
	DevMode.dmBitsPerPel = Mode.BitsPerPixel;
	DevMode.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

	// Apply fullscreen mode
	if (ChangeDisplaySettings(&DevMode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
	{
		ERROR_LOG("Failed to change display mode for fullscreen\n");
		return;
	}

	// Change window style (no border, no titlebar, ...)
	SetWindowLong(myHandle, GWL_STYLE,   WS_POPUP);
	SetWindowLong(myHandle, GWL_EXSTYLE, WS_EX_APPWINDOW);

	// And resize it so that it fits the entire screen
	SetWindowPos(myHandle, HWND_TOP, 0, 0, Mode.Width, Mode.Height, SWP_FRAMECHANGED);
	ShowWindow(myHandle, SW_SHOW);

	// Set "this" as the current fullscreen window
	ourFullscreenWindow = this;

	// SetPixelFormat can fail (really ?) if window style doesn't contain these flags
	long Style = GetWindowLong(myHandle, GWL_STYLE);
	SetWindowLong(myHandle, GWL_STYLE, Style | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
}

//VideoMode Win32Window::GetSupportedVideoModes(std::vector<VideoMode>& Modes)
//{
//	// First, clear array to fill
//	Modes.clear();
//
//	// Enumerate all available video modes for primary display adapter
//	DEVMODE Win32Mode;
//	Win32Mode.dmSize = sizeof(DEVMODE);
//	for (int Count = 0; EnumDisplaySettings(NULL, Count, &Win32Mode); ++Count)
//	{
//		// Convert to sfVideoMode
//		VideoMode Mode(Win32Mode.dmPelsWidth, Win32Mode.dmPelsHeight, Win32Mode.dmBitsPerPel);
//
//		// Add it only if it is not already in the array
//		if (std::find(Modes.begin(), Modes.end(), Mode) == Modes.end())
//			Modes.push_back(Mode);
//	}
//}

void Win32Window::CreateContext(VideoMode& Mode)
{
	// TESTING
	Mode.DepthBits = 24;
	Mode.StencilBits = 8;
	//Mode.AntialiasingLevel = 4;

	// Get the device context attached to the window
	myDeviceContext = GetDC(myHandle);
	if (myDeviceContext == NULL)
	{
		ERROR_LOG("Failed to get device context of window -- cannot create OpenGL context\n");
		return;
	}

	// Let's find a suitable pixel format -- first try with antialiasing
	int BestFormat = 0;
	if (Mode.AntialiasingLevel > 0)
	{
		// Get the wglChoosePixelFormatARB function (it is an extension)
		PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));

		// Define the basic attributes we want for our window
		int IntAttributes[] =
		{
			WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
			WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
			WGL_SAMPLE_BUFFERS_ARB, (Mode.AntialiasingLevel ? GL_TRUE : GL_FALSE),
			WGL_SAMPLES_ARB,        Mode.AntialiasingLevel,
			0,                      0
		};

		// Let's check how many formats are supporting our requirements
		int   Formats[128];
		UINT  NbFormats;
		float FloatAttributes[] = {0, 0};
		bool  IsValid = wglChoosePixelFormatARB(myDeviceContext, IntAttributes, FloatAttributes, sizeof(Formats) / sizeof(*Formats), Formats, &NbFormats) != 0;
		if (!IsValid || (NbFormats == 0))
		{
			if (Mode.AntialiasingLevel > 2)
			{
				// No format matching our needs : reduce the multisampling level
				char errMsg[256];
				sprintf(errMsg, "Failed to find a pixel format supporting %u antialiasing levels ; trying with 2 levels\n", Mode.AntialiasingLevel);
				ERROR_LOG(errMsg);

				Mode.AntialiasingLevel = IntAttributes[1] = 2;
				IsValid = wglChoosePixelFormatARB(myDeviceContext, IntAttributes, FloatAttributes, sizeof(Formats) / sizeof(*Formats), Formats, &NbFormats) != 0;
			}

			if (!IsValid || (NbFormats == 0))
			{
				// Cannot find any pixel format supporting multisampling ; disabling antialiasing
				ERROR_LOG("Failed to find a pixel format supporting antialiasing ; antialiasing will be disabled\n");
				Mode.AntialiasingLevel = 0;
			}
		}

		// Get the best format among the returned ones
		if (IsValid && (NbFormats > 0))
		{
			int BestScore = 0xFFFF;
			for (UINT i = 0; i < NbFormats; ++i)
			{
				// Get the current format's attributes
				PIXELFORMATDESCRIPTOR Attribs;
				Attribs.nSize    = sizeof(PIXELFORMATDESCRIPTOR);
				Attribs.nVersion = 1;
				DescribePixelFormat(myDeviceContext, Formats[i], sizeof(PIXELFORMATDESCRIPTOR), &Attribs);

				// Evaluate the current configuration
				int Color = Attribs.cRedBits + Attribs.cGreenBits + Attribs.cBlueBits + Attribs.cAlphaBits;
				int Score = abs(static_cast<int>(Mode.BitsPerPixel - Color)) + // The EvaluateConfig function
					abs(static_cast<int>(Mode.DepthBits - Attribs.cDepthBits)) +
					abs(static_cast<int>(Mode.StencilBits - Attribs.cStencilBits));

				// Keep it if it's better than the current best
				if (Score < BestScore)
				{
					BestScore  = Score;
					BestFormat = Formats[i];
				}
			}
		}
	}

	// Find a pixel format with no antialiasing, if not needed or not supported
	if (BestFormat == 0)
	{
		// Setup a pixel format descriptor from the rendering settings
		PIXELFORMATDESCRIPTOR PixelDescriptor;
		ZeroMemory(&PixelDescriptor, sizeof(PIXELFORMATDESCRIPTOR));
		PixelDescriptor.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
		PixelDescriptor.nVersion     = 1;
		PixelDescriptor.iLayerType   = PFD_MAIN_PLANE;
		PixelDescriptor.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		PixelDescriptor.iPixelType   = PFD_TYPE_RGBA;
		PixelDescriptor.cColorBits   = static_cast<BYTE>(Mode.BitsPerPixel);
		PixelDescriptor.cDepthBits   = static_cast<BYTE>(Mode.DepthBits);
		PixelDescriptor.cStencilBits = static_cast<BYTE>(Mode.StencilBits);

		// Get the pixel format that best matches our requirements
		BestFormat = ChoosePixelFormat(myDeviceContext, &PixelDescriptor);
		if (BestFormat == 0)
		{
			ERROR_LOG("Failed to find a suitable pixel format for device context -- cannot create OpenGL context\n");
			return;
		}
	}

	// Extract the depth and stencil bits from the chosen format
	PIXELFORMATDESCRIPTOR ActualFormat;
	ActualFormat.nSize    = sizeof(PIXELFORMATDESCRIPTOR);
	ActualFormat.nVersion = 1;
	DescribePixelFormat(myDeviceContext, BestFormat, sizeof(PIXELFORMATDESCRIPTOR), &ActualFormat);
	Mode.DepthBits   = ActualFormat.cDepthBits;
	Mode.StencilBits = ActualFormat.cStencilBits;

	// Set the chosen pixel format
	if (!SetPixelFormat(myDeviceContext, BestFormat, &ActualFormat))
	{
		ERROR_LOG("Failed to set pixel format for device context -- cannot create OpenGL context\n");
		return;
	}

	// Create the OpenGL context from the device context
	myGLContext = wglCreateContext(myDeviceContext);
	if (myGLContext == NULL)
	{
		ERROR_LOG("Failed to create an OpenGL context for this window\n");
		return;
	}

	// Share display lists with other contexts
	HGLRC CurrentContext = wglGetCurrentContext();
	if (CurrentContext)
		wglShareLists(CurrentContext, myGLContext);

	// Activate the context
	MakeCurrent();

	// Enable multisampling
	if (Mode.AntialiasingLevel > 0)
		glEnable(GL_MULTISAMPLE_ARB);
}

void Win32Window::Cleanup()
{
	// Restore the previous video mode (in case we were running in fullscreen)
	if (ourFullscreenWindow == this)
	{
		ChangeDisplaySettings(NULL, 0);
		ourFullscreenWindow = NULL;
	}

	// Unhide the mouse cursor (in case it was hidden)
	ShowMouseCursor(true);
	//GetProperty(OGL_HIDECURSOR)

	// Destroy the OpenGL context
	if (myGLContext)
	{
		// Unbind the context before destroying it
		//SetNotActive();

		wglDeleteContext(myGLContext);
		myGLContext = NULL;
	}
	if (myDeviceContext)
	{
		ReleaseDC(myHandle, myDeviceContext);
		myDeviceContext = NULL;
	}
}

////////////////////////////////////////////////////////////
/// Process a Win32 Event
////////////////////////////////////////////////////////////
void Win32Window::ProcessEvent(UINT Message, WPARAM WParam, LPARAM LParam)
{
	// Don't process any message until window is created
	if (myHandle == NULL)
		return;

	switch (Message)
	{
		// Destroy Event
	case WM_DESTROY :
		{
			// Here we must cleanup resources !
			Cleanup();
			break;
		}

		// Set cursor Event
	case WM_SETCURSOR :
		{
			// The mouse has moved, if the cursor is in our window we must refresh the cursor
			if (LOWORD(LParam) == HTCLIENT)
				SetCursor(myCursor);
			break;
		}
	case WM_CLOSE :
		{
			sf::Event Evt;
			Evt.Type = sf::Event::Closed;
			eventHandler->addEvent(&Evt);
			break;
		}

		// Resize Event
	case WM_SIZE :
		{
			// Update window size
			RECT Rect;
			GetClientRect(myHandle, &Rect);
			SetWinSize(Rect.right - Rect.left, Rect.bottom - Rect.top);

			sf::Event Evt;
			Evt.Type        = sf::Event::Resized;
			Evt.Size.Width  = GetXwin();
			Evt.Size.Height = GetYwin();
			eventHandler->addEvent(&Evt);
			break;
		}
		// Gain focus Event
	case WM_SETFOCUS :
		{
			sf::Event Evt;
			Evt.Type = sf::Event::GainedFocus;
			eventHandler->addEvent(&Evt);
			break;
		}

		// Lost focus Event
	case WM_KILLFOCUS :
		{
			sf::Event Evt;
			Evt.Type = sf::Event::LostFocus;
			eventHandler->addEvent(&Evt);
			break;
		}

		// Text Event
	case WM_CHAR :
		{
			sf::Event Evt;
			Evt.Type = sf::Event::TextEntered;
			Evt.Text.Unicode = static_cast<u32>(WParam);
			eventHandler->addEvent(&Evt);
			break;
		}
		// Keydown Event
	case WM_KEYDOWN :
	case WM_SYSKEYDOWN :
		{
			if (myKeyRepeatEnabled || ((LParam & (1 << 30)) == 0))
			{
				sf::Event Evt;
				Evt.Type        = sf::Event::KeyPressed;
				Evt.Key.Code    = (WParam == VK_SHIFT) ? GetShiftState(true) : VirtualKeyCodeToSF(WParam, LParam);
				Evt.Key.Alt     = HIWORD(GetAsyncKeyState(VK_MENU))    != 0;
				Evt.Key.Control = HIWORD(GetAsyncKeyState(VK_CONTROL)) != 0;
				Evt.Key.Shift   = HIWORD(GetAsyncKeyState(VK_SHIFT))   != 0;
				eventHandler->addEvent(&Evt);
			}
			break;
		}

		// Keyup Event
	case WM_KEYUP :
	case WM_SYSKEYUP :
		{
			sf::Event Evt;
			Evt.Type        = sf::Event::KeyReleased;
			Evt.Key.Code    = (WParam == VK_SHIFT) ? GetShiftState(false) : VirtualKeyCodeToSF(WParam, LParam);
			Evt.Key.Alt     = HIWORD(GetAsyncKeyState(VK_MENU))    != 0;
			Evt.Key.Control = HIWORD(GetAsyncKeyState(VK_CONTROL)) != 0;
			Evt.Key.Shift   = HIWORD(GetAsyncKeyState(VK_SHIFT))   != 0;
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse wheel Event
	case WM_MOUSEWHEEL :
		{
			sf::Event Evt;
			Evt.Type = sf::Event::MouseWheelMoved;
			Evt.MouseWheel.Delta = static_cast<s16>(HIWORD(WParam)) / 120;
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse left button down Event
	case WM_LBUTTONDOWN :
		{
			sf::Event Evt;
			Evt.Type               = sf::Event::MouseButtonPressed;
			Evt.MouseButton.Button = sf::Mouse::Left;
			Evt.MouseButton.X      = LOWORD(LParam);
			Evt.MouseButton.Y      = HIWORD(LParam);
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse left button up Event
	case WM_LBUTTONUP :
		{
			sf::Event Evt;
			Evt.Type               = sf::Event::MouseButtonReleased;
			Evt.MouseButton.Button = sf::Mouse::Left;
			Evt.MouseButton.X      = LOWORD(LParam);
			Evt.MouseButton.Y      = HIWORD(LParam);
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse right button down Event
	case WM_RBUTTONDOWN :
		{
			sf::Event Evt;
			Evt.Type               = sf::Event::MouseButtonPressed;
			Evt.MouseButton.Button = sf::Mouse::Right;
			Evt.MouseButton.X      = LOWORD(LParam);
			Evt.MouseButton.Y      = HIWORD(LParam);
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse right button up Event
	case WM_RBUTTONUP :
		{
			sf::Event Evt;
			Evt.Type               = sf::Event::MouseButtonReleased;
			Evt.MouseButton.Button = sf::Mouse::Right;
			Evt.MouseButton.X      = LOWORD(LParam);
			Evt.MouseButton.Y      = HIWORD(LParam);
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse wheel button down Event
	case WM_MBUTTONDOWN :
		{
			sf::Event Evt;
			Evt.Type               = sf::Event::MouseButtonPressed;
			Evt.MouseButton.Button = sf::Mouse::Middle;
			Evt.MouseButton.X      = LOWORD(LParam);
			Evt.MouseButton.Y      = HIWORD(LParam);
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse wheel button up Event
	case WM_MBUTTONUP :
		{
			sf::Event Evt;
			Evt.Type               = sf::Event::MouseButtonReleased;
			Evt.MouseButton.Button = sf::Mouse::Middle;
			Evt.MouseButton.X      = LOWORD(LParam);
			Evt.MouseButton.Y      = HIWORD(LParam);
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse X button down Event
	case WM_XBUTTONDOWN :
		{
			sf::Event Evt;
			Evt.Type               = sf::Event::MouseButtonPressed;
			Evt.MouseButton.Button = HIWORD(WParam) == XBUTTON1 ? sf::Mouse::XButton1 : sf::Mouse::XButton2;
			Evt.MouseButton.X      = LOWORD(LParam);
			Evt.MouseButton.Y      = HIWORD(LParam);
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse X button up Event
	case WM_XBUTTONUP :
		{
			sf::Event Evt;
			Evt.Type               = sf::Event::MouseButtonReleased;
			Evt.MouseButton.Button = HIWORD(WParam) == XBUTTON1 ? sf::Mouse::XButton1 : sf::Mouse::XButton2;
			Evt.MouseButton.X      = LOWORD(LParam);
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse move Event
	case WM_MOUSEMOVE :
		{
			// Check if we need to generate a MouseEntered Event
			if (!myIsCursorIn)
			{
				TRACKMOUSEEVENT MouseEvent;
				MouseEvent.cbSize    = sizeof(TRACKMOUSEEVENT);
				MouseEvent.hwndTrack = myHandle;
				MouseEvent.dwFlags   = TME_LEAVE;
				TrackMouseEvent(&MouseEvent);

				myIsCursorIn = true;

				sf::Event Evt;
				Evt.Type = sf::Event::MouseEntered;
				eventHandler->addEvent(&Evt);
			}

			sf::Event Evt;
			Evt.Type        = sf::Event::MouseMoved;
			Evt.MouseMove.X = LOWORD(LParam);
			Evt.MouseMove.Y = HIWORD(LParam); // (shuffle2) Added this, check to see if it's good
			eventHandler->addEvent(&Evt);
			break;
		}

		// Mouse leave Event
	case WM_MOUSELEAVE :
		{
			myIsCursorIn = false;

			sf::Event Evt;
			Evt.Type = sf::Event::MouseLeft;
			eventHandler->addEvent(&Evt);
			break;
		}
	}
}

///////////////////////////////////////////////////////////
/// Check the state of the shift keys on a key sf::Event,
/// and return the corresponding SF key code
////////////////////////////////////////////////////////////
sf::Key::Code Win32Window::GetShiftState(bool KeyDown)
{
	static bool LShiftPrevDown = false;
	static bool RShiftPrevDown = false;

	bool LShiftDown = (HIWORD(GetAsyncKeyState(VK_LSHIFT)) != 0);
	bool RShiftDown = (HIWORD(GetAsyncKeyState(VK_RSHIFT)) != 0);

	sf::Key::Code Code = sf::Key::Code(0);
	if (KeyDown)
	{
		if      (!LShiftPrevDown && LShiftDown) Code = sf::Key::LShift;
		else if (!RShiftPrevDown && RShiftDown) Code = sf::Key::RShift;
	}
	else
	{
		if      (LShiftPrevDown && !LShiftDown) Code = sf::Key::LShift;
		else if (RShiftPrevDown && !RShiftDown) Code = sf::Key::RShift;
	}

	LShiftPrevDown = LShiftDown;
	RShiftPrevDown = RShiftDown;

	return Code;
}

///////////////////////////////////////////////////////////
/// Convert a Win32 virtual key code to a SFML key code
////////////////////////////////////////////////////////////
sf::Key::Code Win32Window::VirtualKeyCodeToSF(WPARAM VirtualKey, LPARAM Flags)
{
	switch (VirtualKey)
	{
		// VK_SHIFT is handled by the GetShiftState function
	case VK_MENU :       return (Flags & (1 << 24)) ? sf::Key::RAlt     : sf::Key::LAlt;
	case VK_CONTROL :    return (Flags & (1 << 24)) ? sf::Key::RControl : sf::Key::LControl;
	case VK_LWIN :       return sf::Key::LSystem;
	case VK_RWIN :       return sf::Key::RSystem;
	case VK_APPS :       return sf::Key::Menu;
	case VK_OEM_1 :      return sf::Key::SemiColon;
	case VK_OEM_2 :      return sf::Key::Slash;
	case VK_OEM_PLUS :   return sf::Key::Equal;
	case VK_OEM_MINUS :  return sf::Key::Dash;
	case VK_OEM_4 :      return sf::Key::LBracket;
	case VK_OEM_6 :      return sf::Key::RBracket;
	case VK_OEM_COMMA :  return sf::Key::Comma;
	case VK_OEM_PERIOD : return sf::Key::Period;
	case VK_OEM_7 :      return sf::Key::Quote;
	case VK_OEM_5 :      return sf::Key::BackSlash;
	case VK_OEM_3 :      return sf::Key::Tilde;
	case VK_ESCAPE :     return sf::Key::Escape;
	case VK_SPACE :      return sf::Key::Space;
	case VK_RETURN :     return sf::Key::Return;
	case VK_BACK :       return sf::Key::Back;
	case VK_TAB :        return sf::Key::Tab;
	case VK_PRIOR :      return sf::Key::PageUp;
	case VK_NEXT :       return sf::Key::PageDown;
	case VK_END :        return sf::Key::End;
	case VK_HOME :       return sf::Key::Home;
	case VK_INSERT :     return sf::Key::Insert;
	case VK_DELETE :     return sf::Key::Delete;
	case VK_ADD :        return sf::Key::Add;
	case VK_SUBTRACT :   return sf::Key::Subtract;
	case VK_MULTIPLY :   return sf::Key::Multiply;
	case VK_DIVIDE :     return sf::Key::Divide;
	case VK_PAUSE :      return sf::Key::Pause;
	case VK_F1 :         return sf::Key::F1;
	case VK_F2 :         return sf::Key::F2;
	case VK_F3 :         return sf::Key::F3;
	case VK_F4 :         return sf::Key::F4;
	case VK_F5 :         return sf::Key::F5;
	case VK_F6 :         return sf::Key::F6;
	case VK_F7 :         return sf::Key::F7;
	case VK_F8 :         return sf::Key::F8;
	case VK_F9 :         return sf::Key::F9;
	case VK_F10 :        return sf::Key::F10;
	case VK_F11 :        return sf::Key::F11;
	case VK_F12 :        return sf::Key::F12;
	case VK_F13 :        return sf::Key::F13;
	case VK_F14 :        return sf::Key::F14;
	case VK_F15 :        return sf::Key::F15;
	case VK_LEFT :      return sf::Key::Left;
	case VK_RIGHT :      return sf::Key::Right;
	case VK_UP :         return sf::Key::Up;
	case VK_DOWN :       return sf::Key::Down;
	case VK_NUMPAD0 :    return sf::Key::Numpad0;
	case VK_NUMPAD1 :    return sf::Key::Numpad1;
	case VK_NUMPAD2 :    return sf::Key::Numpad2;
	case VK_NUMPAD3 :    return sf::Key::Numpad3;
	case VK_NUMPAD4 :    return sf::Key::Numpad4;
	case VK_NUMPAD5 :    return sf::Key::Numpad5;
	case VK_NUMPAD6 :    return sf::Key::Numpad6;
	case VK_NUMPAD7 :    return sf::Key::Numpad7;
	case VK_NUMPAD8 :    return sf::Key::Numpad8;
	case VK_NUMPAD9 :    return sf::Key::Numpad9;
	case 'A' :           return sf::Key::A;
	case 'Z' :           return sf::Key::Z;
	case 'E' :           return sf::Key::E;
	case 'R' :           return sf::Key::R;
	case 'T' :           return sf::Key::T;
	case 'Y' :           return sf::Key::Y;
	case 'U' :           return sf::Key::U;
	case 'I' :           return sf::Key::I;
	case 'O' :           return sf::Key::O;
	case 'P' :           return sf::Key::P;
	case 'Q' :           return sf::Key::Q;
	case 'S' :           return sf::Key::S;
	case 'D' :           return sf::Key::D;
	case 'F' :           return sf::Key::F;
	case 'G' :           return sf::Key::G;
	case 'H' :           return sf::Key::H;
	case 'J' :           return sf::Key::J;
	case 'K' :           return sf::Key::K;
	case 'L' :           return sf::Key::L;
	case 'M' :           return sf::Key::M;
	case 'W' :           return sf::Key::W;
	case 'X' :           return sf::Key::X;
	case 'C' :           return sf::Key::C;
	case 'V' :           return sf::Key::V;
	case 'B' :           return sf::Key::B;
	case 'N' :           return sf::Key::N;
	case '0' :           return sf::Key::Num0;
	case '1' :           return sf::Key::Num1;
	case '2' :           return sf::Key::Num2;
	case '3' :           return sf::Key::Num3;
	case '4' :           return sf::Key::Num4;
	case '5' :           return sf::Key::Num5;
	case '6' :           return sf::Key::Num6;
	case '7' :           return sf::Key::Num7;
	case '8' :           return sf::Key::Num8;
	case '9' :           return sf::Key::Num9;
	}

	return sf::Key::Code(0);
}

///////////////////////////////////////////////////////////
/// Win32 Callback for the window class
////////////////////////////////////////////////////////////
LRESULT CALLBACK Win32Window::GlobalOnEvent(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
	// Associate handle and Window instance when the creation message is received
	if (Message == WM_CREATE)
	{
		// Get Win32Window instance (it was passed as the last argument of CreateWindow)
		long This = reinterpret_cast<long>(reinterpret_cast<CREATESTRUCT*>(LParam)->lpCreateParams);

		// Set as the "user data" parameter of the window
		SetWindowLongPtr(Handle, GWLP_USERDATA, This);
	}

	// Get the GLWindow instance corresponding to the window handle
	Win32Window* Window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(Handle, GWLP_USERDATA));

	// Forward the event to the appropriate function
	if (Window)
	{
		Window->ProcessEvent(Message, WParam, LParam);

		if (Window->myCallback)
			return CallWindowProc(reinterpret_cast<WNDPROC>(Window->myCallback), Handle, Message, WParam, LParam);
	}

	// We don't forward the WM_CLOSE message to prevent the OS from automatically destroying the window
	if (Message == WM_CLOSE)
		return 0;

	return DefWindowProc(Handle, Message, WParam, LParam);
}
