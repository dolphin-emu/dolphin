set(SRCS	ControllerEmu.cpp
			InputConfig.cpp
			ControllerInterface/ControllerInterface.cpp
			ControllerInterface/Device.cpp
			ControllerInterface/ExpressionParser.cpp)


set(LIBS	common)

if(WIN32)
	set(SRCS	${SRCS}
				ControllerInterface/DInput/DInput.cpp
				ControllerInterface/DInput/DInputJoystick.cpp
				ControllerInterface/DInput/DInputKeyboardMouse.cpp
				ControllerInterface/XInput/XInput.cpp
				ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation)
	FIND_LIBRARY(CARBON_LIBRARY Carbon)
	FIND_LIBRARY(COCOA_LIBRARY Cocoa)
	set(SRCS	${SRCS}
				ControllerInterface/OSX/OSX.mm
				ControllerInterface/OSX/OSXKeyboard.mm
				ControllerInterface/OSX/OSXJoystick.mm
				ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp)
	set(LIBS ${LIBS} ${COREFOUNDATION_LIBRARY} ${CARBON_LIBRARY} ${COCOA_LIBRARY})
elseif(X11_FOUND)
	set(SRCS	${SRCS}
				ControllerInterface/Xlib/Xlib.cpp)
	if(XINPUT2_FOUND)
		set(SRCS	${SRCS}
					ControllerInterface/Xlib/XInput2.cpp)
	endif()
	set(LIBS ${LIBS} ${X11_LIBRARIES} ${XINPUT2_LIBRARIES})
elseif(ANDROID)
	set(SRCS	${SRCS}
				ControllerInterface/Android/Android.cpp)
endif()

if(SDL_FOUND OR SDL2_FOUND)
	set(SRCS ${SRCS} ControllerInterface/SDL/SDL.cpp)
	if (SDL2_FOUND)
		set(LIBS ${LIBS} ${SDL2_LIBRARY})
	elseif(SDL_FOUND)
		set(LIBS ${LIBS} ${SDL_LIBRARY})
	endif()
endif()

add_dolphin_library(inputcommon "${SRCS}" "${LIBS}")
