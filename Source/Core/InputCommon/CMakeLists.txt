set(SRCS InputConfig.cpp
  ControllerEmu/ControllerEmu.cpp
  ControllerEmu/Control/Control.cpp
  ControllerEmu/Control/Input.cpp
  ControllerEmu/Control/Output.cpp
  ControllerEmu/ControlGroup/AnalogStick.cpp
  ControllerEmu/ControlGroup/Buttons.cpp
  ControllerEmu/ControlGroup/ControlGroup.cpp
  ControllerEmu/ControlGroup/Cursor.cpp
  ControllerEmu/ControlGroup/Extension.cpp
  ControllerEmu/ControlGroup/Force.cpp
  ControllerEmu/ControlGroup/MixedTriggers.cpp
  ControllerEmu/ControlGroup/ModifySettingsButton.cpp
  ControllerEmu/ControlGroup/Slider.cpp
  ControllerEmu/ControlGroup/Tilt.cpp
  ControllerEmu/ControlGroup/Triggers.cpp
  ControllerEmu/Setting/BooleanSetting.cpp
  ControllerEmu/Setting/NumericSetting.cpp
  ControllerInterface/ControllerInterface.cpp
  ControllerInterface/Device.cpp
  ControlReference/ControlReference.cpp
  ControlReference/ExpressionParser.cpp
)
set(LIBS common)

if(WIN32)
  set(SRCS ${SRCS}
    ControllerInterface/DInput/DInput.cpp
    ControllerInterface/DInput/DInputJoystick.cpp
    ControllerInterface/DInput/DInputKeyboardMouse.cpp
    ControllerInterface/DInput/XInputFilter.cpp
    ControllerInterface/XInput/XInput.cpp
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
  )
elseif(APPLE)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(CARBON_LIBRARY Carbon)
  find_library(COCOA_LIBRARY Cocoa)
  set(SRCS ${SRCS}
    ControllerInterface/OSX/OSX.mm
    ControllerInterface/OSX/OSXJoystick.mm
    ControllerInterface/Quartz/Quartz.mm
    ControllerInterface/Quartz/QuartzKeyboardAndMouse.mm
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
  )
  set(LIBS ${LIBS} ${COREFOUNDATION_LIBRARY} ${CARBON_LIBRARY} ${COCOA_LIBRARY})
elseif(X11_FOUND)
  set(SRCS ${SRCS}
    ControllerInterface/Xlib/XInput2.cpp
  )
  set(LIBS ${LIBS} ${X11_LIBRARIES} ${X11_INPUT_LIBRARIES})
elseif(ANDROID)
  add_definitions(-DCIFACE_USE_ANDROID)
  set(SRCS ${SRCS}
    ControllerInterface/Android/Android.cpp
  )
endif()

if(ANDROID)
  set(SRCS ${SRCS} GCAdapter_Android.cpp)
else()
  set(SRCS ${SRCS} GCAdapter.cpp)
  set(LIBS ${LIBS} ${LIBUSB_LIBRARIES})
endif()

if(LIBEVDEV_FOUND AND LIBUDEV_FOUND)
  set(SRCS ${SRCS} ControllerInterface/evdev/evdev.cpp)
  set(LIBS ${LIBS} ${LIBEVDEV_LIBRARY} ${LIBUDEV_LIBRARY})
endif()

if(UNIX)
  set(SRCS ${SRCS} ControllerInterface/Pipes/Pipes.cpp)
endif()

if(ENABLE_SDL)
  find_package(SDL2)
  if(SDL2_FOUND)
    message(STATUS "Using shared SDL2")
    set(SDL_TARGET SDL2::SDL2)
  else()
    # SDL2 not found, try SDL
    find_package(SDL)
    if(SDL_FOUND)
      message(STATUS "Using shared SDL")
      add_library(System_SDL INTERFACE)
      target_include_directories(System_SDL INTERFACE ${SDL_INCLUDE_DIR})
      target_link_libraries(System_SDL INTERFACE ${SDL_LIBRARY})
      set(SDL_TARGET System_SDL)
    endif()
  endif()
  if(SDL_TARGET AND TARGET ${SDL_TARGET})
    set(SRCS ${SRCS} ControllerInterface/SDL/SDL.cpp)
    set(LIBS ${LIBS} ${SDL_TARGET})
    add_definitions(-DHAVE_SDL=1)
  else()
    message(STATUS "SDL NOT found, disabling SDL input")
  endif()
endif()

add_dolphin_library(inputcommon "${SRCS}" "${LIBS}")
