set(SRCS	Src/ControllerEmu.cpp
			Src/InputConfig.cpp
			Src/UDPWiimote.cpp
			Src/UDPWrapper.cpp
			Src/ControllerInterface/ControllerInterface.cpp)

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
endif()

add_library(inputcommon ${SRCS})
