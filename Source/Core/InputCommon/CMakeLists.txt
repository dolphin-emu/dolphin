add_library(inputcommon
  InputConfig.cpp
  InputConfig.h
  InputProfile.cpp
  InputProfile.h
  ControllerEmu/ControllerEmu.cpp
  ControllerEmu/ControllerEmu.h
  ControllerEmu/StickGate.cpp
  ControllerEmu/StickGate.h
  ControllerEmu/Control/Control.cpp
  ControllerEmu/Control/Control.h
  ControllerEmu/Control/Input.cpp
  ControllerEmu/Control/Input.h
  ControllerEmu/Control/Output.cpp
  ControllerEmu/Control/Output.h
  ControllerEmu/ControlGroup/AnalogStick.cpp
  ControllerEmu/ControlGroup/AnalogStick.h
  ControllerEmu/ControlGroup/Attachments.cpp
  ControllerEmu/ControlGroup/Attachments.h
  ControllerEmu/ControlGroup/Buttons.cpp
  ControllerEmu/ControlGroup/Buttons.h
  ControllerEmu/ControlGroup/ControlGroup.cpp
  ControllerEmu/ControlGroup/ControlGroup.h
  ControllerEmu/ControlGroup/Cursor.cpp
  ControllerEmu/ControlGroup/Cursor.h
  ControllerEmu/ControlGroup/Force.cpp
  ControllerEmu/ControlGroup/Force.h
  ControllerEmu/ControlGroup/MixedTriggers.cpp
  ControllerEmu/ControlGroup/MixedTriggers.h
  ControllerEmu/ControlGroup/ModifySettingsButton.cpp
  ControllerEmu/ControlGroup/ModifySettingsButton.h
  ControllerEmu/ControlGroup/Slider.cpp
  ControllerEmu/ControlGroup/Slider.h
  ControllerEmu/ControlGroup/Tilt.cpp
  ControllerEmu/ControlGroup/Tilt.h
  ControllerEmu/ControlGroup/Triggers.cpp
  ControllerEmu/ControlGroup/Triggers.h
  ControllerEmu/Setting/NumericSetting.cpp
  ControllerEmu/Setting/NumericSetting.h
  ControllerInterface/ControllerInterface.cpp
  ControllerInterface/ControllerInterface.h
  ControllerInterface/Device.cpp
  ControllerInterface/Device.h
  ControlReference/ControlReference.cpp
  ControlReference/ControlReference.h
  ControlReference/ExpressionParser.cpp
  ControlReference/ExpressionParser.h
)

target_link_libraries(inputcommon PUBLIC
  common
)

if(WIN32)
  target_sources(inputcommon PRIVATE
    ControllerInterface/DInput/DInput.cpp
    ControllerInterface/DInput/DInput.h
    ControllerInterface/DInput/DInput8.h
    ControllerInterface/DInput/DInputJoystick.cpp
    ControllerInterface/DInput/DInputJoystick.h
    ControllerInterface/DInput/DInputKeyboardMouse.cpp
    ControllerInterface/DInput/DInputKeyboardMouse.h
    ControllerInterface/DInput/NamedKeys.h
    ControllerInterface/DInput/XInputFilter.cpp
    ControllerInterface/DInput/XInputFilter.h
    ControllerInterface/Win32/Win32.cpp
    ControllerInterface/Win32/Win32.h
    ControllerInterface/XInput/XInput.cpp
    ControllerInterface/XInput/XInput.h
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.h
  )
elseif(APPLE)
  target_sources(inputcommon PRIVATE
    ControllerInterface/OSX/OSX.h
    ControllerInterface/OSX/OSX.mm
    ControllerInterface/OSX/OSXJoystick.h
    ControllerInterface/OSX/OSXJoystick.mm
    ControllerInterface/OSX/RunLoopStopper.h
    ControllerInterface/Quartz/Quartz.h
    ControllerInterface/Quartz/Quartz.mm
    ControllerInterface/Quartz/QuartzKeyboardAndMouse.h
    ControllerInterface/Quartz/QuartzKeyboardAndMouse.mm
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.h
  )
  target_link_libraries(inputcommon PRIVATE
    ${COREFOUNDATION_LIBRARY}
    ${CARBON_LIBRARY}
    ${COCOA_LIBRARY}
    ${FORCEFEEDBACK_LIBRARY}
    ${IOK_LIBRARY}
  )
elseif(X11_FOUND)
  target_sources(inputcommon PRIVATE
    ControllerInterface/Xlib/XInput2.cpp
    ControllerInterface/Xlib/XInput2.h
  )
  target_link_libraries(inputcommon PUBLIC
    ${X11_LIBRARIES}
    ${X11_INPUT_LIBRARIES}
  )
elseif(ANDROID)
  target_compile_definitions(inputcommon PRIVATE -DCIFACE_USE_ANDROID)
  target_sources(inputcommon PRIVATE
    ControllerInterface/Android/Android.cpp
    ControllerInterface/Android/Android.h
  )
endif()

if(ANDROID)
  target_sources(inputcommon PRIVATE GCAdapter_Android.cpp)
else()
  target_sources(inputcommon PRIVATE
    GCAdapter.cpp
    GCAdapter.h
  )
  target_link_libraries(inputcommon PUBLIC ${LIBUSB_LIBRARIES})
endif()

if(LIBEVDEV_FOUND AND LIBUDEV_FOUND)
  target_sources(inputcommon
  PRIVATE
    ControllerInterface/evdev/evdev.cpp
    ControllerInterface/evdev/evdev.h
  )
  target_include_directories(inputcommon
  PRIVATE
    ${LIBEVDEV_INCLUDE_DIR}
    ${LIBUDEV_INCLUDE_DIR}
  )
  target_link_libraries(inputcommon
  PRIVATE
    ${LIBEVDEV_LIBRARY}
    ${LIBUDEV_LIBRARY}
  )
endif()

if(UNIX)
  target_sources(inputcommon PRIVATE
    ControllerInterface/Pipes/Pipes.cpp
    ControllerInterface/Pipes/Pipes.h
  )
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
    target_sources(inputcommon PRIVATE
      ControllerInterface/SDL/SDL.cpp
      ControllerInterface/SDL/SDL.h
    )
    target_link_libraries(inputcommon PRIVATE ${SDL_TARGET})
    target_compile_definitions(inputcommon PRIVATE "CIFACE_USE_SDL=1")
  else()
    message(STATUS "SDL NOT found, disabling SDL input")
  endif()
endif()
