set(SRCS	Src/ControllerEmu.cpp
			Src/InputConfig.cpp
			Src/UDPWiimote.cpp
			Src/UDPWrapper.cpp
			Src/ControllerInterface/ControllerInterface.cpp
			Src/ControllerInterface/Device.cpp
			Src/ControllerInterface/ExpressionParser.cpp)

if(WIN32)
	set(SRCS	${SRCS}
				Src/ControllerInterface/DInput/DInput.cpp
				Src/ControllerInterface/DInput/DInputJoystick.cpp
				Src/ControllerInterface/DInput/DInputKeyboardMouse.cpp
				Src/ControllerInterface/SDL/SDL.cpp
				Src/ControllerInterface/XInput/XInput.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(SRCS	${SRCS}
				Src/ControllerInterface/OSX/OSX.mm
				Src/ControllerInterface/OSX/OSXKeyboard.mm
				Src/ControllerInterface/OSX/OSXJoystick.mm
				Src/ControllerInterface/SDL/SDL.cpp)
elseif(X11_FOUND)
	set(SRCS	${SRCS}
				Src/ControllerInterface/SDL/SDL.cpp
				Src/ControllerInterface/Xlib/Xlib.cpp)
	if(XINPUT2_FOUND)
		set(SRCS	${SRCS}
					Src/ControllerInterface/Xlib/XInput2.cpp)
	endif()
elseif(ANDROID)
	set(SRCS	${SRCS}
				Src/ControllerInterface/Android/Android.cpp)
endif()

add_library(inputcommon ${SRCS})
