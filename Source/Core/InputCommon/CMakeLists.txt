set(SRCS	ControllerEmu.cpp
			InputConfig.cpp
			UDPWiimote.cpp
			UDPWrapper.cpp
			ControllerInterface/ControllerInterface.cpp
			ControllerInterface/Device.cpp
			ControllerInterface/ExpressionParser.cpp)

if(WIN32)
	set(SRCS	${SRCS}
				ControllerInterface/DInput/DInput.cpp
				ControllerInterface/DInput/DInputJoystick.cpp
				ControllerInterface/DInput/DInputKeyboardMouse.cpp
				ControllerInterface/XInput/XInput.cpp
				ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(SRCS	${SRCS}
				ControllerInterface/OSX/OSX.mm
				ControllerInterface/OSX/OSXKeyboard.mm
				ControllerInterface/OSX/OSXJoystick.mm
				ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp)
elseif(X11_FOUND)
	set(SRCS	${SRCS}
				ControllerInterface/Xlib/Xlib.cpp)
	if(XINPUT2_FOUND)
		set(SRCS	${SRCS}
					ControllerInterface/Xlib/XInput2.cpp)
	endif()
elseif(ANDROID)
	set(SRCS	${SRCS}
				ControllerInterface/Android/Android.cpp)
endif()

if(SDL_FOUND OR SDL2_FOUND)
	set(SRCS ${SRCS} ControllerInterface/SDL/SDL.cpp)
endif()

add_dolphin_library(inputcommon "${SRCS}" "")
