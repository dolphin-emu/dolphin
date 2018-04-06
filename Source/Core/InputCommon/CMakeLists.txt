add_library(inputcommon
  InputConfig.cpp
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

target_link_libraries(inputcommon PUBLIC
  common
)

if(WIN32)
  target_sources(inputcommon PRIVATE
    ControllerInterface/DInput/DInput.cpp
    ControllerInterface/DInput/DInputJoystick.cpp
    ControllerInterface/DInput/DInputKeyboardMouse.cpp
    ControllerInterface/DInput/XInputFilter.cpp
    ControllerInterface/XInput/XInput.cpp
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
  )
elseif(APPLE)
  target_sources(inputcommon PRIVATE
    ControllerInterface/OSX/OSX.mm
    ControllerInterface/OSX/OSXJoystick.mm
    ControllerInterface/Quartz/Quartz.mm
    ControllerInterface/Quartz/QuartzKeyboardAndMouse.mm
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
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
  )
  target_link_libraries(inputcommon PUBLIC
    ${X11_LIBRARIES}
    ${X11_INPUT_LIBRARIES}
  )
elseif(ANDROID)
  target_compile_definitions(inputcommon PRIVATE -DCIFACE_USE_ANDROID)
  target_sources(inputcommon PRIVATE
    ControllerInterface/Android/Android.cpp
  )
endif()

if(ANDROID)
  target_sources(inputcommon PRIVATE GCAdapter_Android.cpp)
else()
  target_sources(inputcommon PRIVATE GCAdapter.cpp)
  target_link_libraries(inputcommon PUBLIC ${LIBUSB_LIBRARIES})
endif()

if(LIBEVDEV_FOUND AND LIBUDEV_FOUND)
  target_sources(inputcommon PRIVATE
    ControllerInterface/evdev/evdev.cpp
  )
  target_link_libraries(inputcommon PUBLIC
    ${LIBEVDEV_LIBRARY}
    ${LIBUDEV_LIBRARY}
  )
endif()

if(UNIX)
  target_sources(inputcommon PRIVATE
    ControllerInterface/Pipes/Pipes.cpp
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
    target_sources(inputcommon PRIVATE ControllerInterface/SDL/SDL.cpp)
    target_link_libraries(inputcommon PUBLIC ${SDL_TARGET})
    target_compile_definitions(inputcommon PRIVATE -DHAVE_SDL=1)
  else()
    message(STATUS "SDL NOT found, disabling SDL input")
  endif()
endif()
