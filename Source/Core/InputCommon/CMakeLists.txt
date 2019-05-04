add_library(inputcommon
  InputConfig.h
  InputConfig.cpp
  InputProfile.h
  InputProfile.cpp
  ControllerEmu/ControllerEmu.h
  ControllerEmu/ControllerEmu.cpp
  ControllerEmu/StickGate.h
  ControllerEmu/StickGate.cpp
  ControllerEmu/Control/Control.h
  ControllerEmu/Control/Control.cpp
  ControllerEmu/Control/Input.h
  ControllerEmu/Control/Input.cpp
  ControllerEmu/Control/Output.h
  ControllerEmu/Control/Output.cpp
  ControllerEmu/ControlGroup/AnalogStick.h
  ControllerEmu/ControlGroup/AnalogStick.cpp
  ControllerEmu/ControlGroup/Attachments.h
  ControllerEmu/ControlGroup/Attachments.cpp
  ControllerEmu/ControlGroup/Buttons.h
  ControllerEmu/ControlGroup/Buttons.cpp
  ControllerEmu/ControlGroup/ControlGroup.h
  ControllerEmu/ControlGroup/ControlGroup.cpp
  ControllerEmu/ControlGroup/Cursor.h
  ControllerEmu/ControlGroup/Cursor.cpp
  ControllerEmu/ControlGroup/Force.h
  ControllerEmu/ControlGroup/Force.cpp
  ControllerEmu/ControlGroup/MixedTriggers.h
  ControllerEmu/ControlGroup/MixedTriggers.cpp
  ControllerEmu/ControlGroup/ModifySettingsButton.h
  ControllerEmu/ControlGroup/ModifySettingsButton.cpp
  ControllerEmu/ControlGroup/Slider.h
  ControllerEmu/ControlGroup/Slider.cpp
  ControllerEmu/ControlGroup/Tilt.h
  ControllerEmu/ControlGroup/Tilt.cpp
  ControllerEmu/ControlGroup/Triggers.h
  ControllerEmu/ControlGroup/Triggers.cpp
  ControllerEmu/Setting/NumericSetting.h
  ControllerEmu/Setting/NumericSetting.cpp
  ControllerInterface/ControllerInterface.h
  ControllerInterface/ControllerInterface.cpp
  ControllerInterface/Device.h
  ControllerInterface/Device.cpp
  ControlReference/ControlReference.h
  ControlReference/ControlReference.cpp
  ControlReference/ExpressionParser.h
  ControlReference/ExpressionParser.cpp
)

target_link_libraries(inputcommon PUBLIC
  common
)

set_target_properties(inputcommon PROPERTIES
  FOLDER "Dolphin Core"
  PROJECT_LABEL "InputCommon"
)

include(SourceGroupByFolder)
source_group_by_folder(inputcommon)

if(WIN32)
  target_sources(inputcommon PRIVATE
    ControllerInterface/DInput/DInput.h
    ControllerInterface/DInput/DInput.cpp
    ControllerInterface/DInput/DInput8.h
    ControllerInterface/DInput/DInputJoystick.h
    ControllerInterface/DInput/DInputJoystick.cpp
    ControllerInterface/DInput/DInputKeyboardMouse.h
    ControllerInterface/DInput/DInputKeyboardMouse.cpp
    ControllerInterface/DInput/XInputFilter.h
    ControllerInterface/DInput/XInputFilter.cpp
    ControllerInterface/Win32/Win32.h
    ControllerInterface/Win32/Win32.cpp
    ControllerInterface/XInput/XInput.h
    ControllerInterface/XInput/XInput.cpp
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.h
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
  )
elseif(APPLE)
  target_sources(inputcommon PRIVATE
    ControllerInterface/OSX/RunLoopStopper.h
    ControllerInterface/OSX/OSX.h
    ControllerInterface/OSX/OSX.mm
    ControllerInterface/OSX/OSXJoystick.h
    ControllerInterface/OSX/OSXJoystick.mm
    ControllerInterface/Quartz/Quartz.h
    ControllerInterface/Quartz/Quartz.mm
    ControllerInterface/Quartz/QuartzKeyboardAndMouse.h
    ControllerInterface/Quartz/QuartzKeyboardAndMouse.mm
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.h
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
    ControllerInterface/ForceFeedback/OSX/DirectInputAdapter.h
    ControllerInterface/ForceFeedback/OSX/DirectInputConstants.h
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
    ControllerInterface/Xlib/XInput2.h
    ControllerInterface/Xlib/XInput2.cpp
  )
  target_link_libraries(inputcommon PUBLIC
    ${X11_LIBRARIES}
    ${X11_INPUT_LIBRARIES}
  )
elseif(ANDROID)
  target_compile_definitions(inputcommon PRIVATE -DCIFACE_USE_ANDROID)
  target_sources(inputcommon PRIVATE
    ControllerInterface/Android/Android.h
    ControllerInterface/Android/Android.cpp
  )
endif()

if(ANDROID)
  target_sources(inputcommon PRIVATE GCAdapter_Android.cpp)
else()
  target_sources(inputcommon PRIVATE
    GCAdapter.h
    GCAdapter.cpp
  )
  target_link_libraries(inputcommon PUBLIC ${LIBUSB_LIBRARIES})
endif()

if(LIBEVDEV_FOUND AND LIBUDEV_FOUND)
  target_sources(inputcommon
  PRIVATE
    ControllerInterface/evdev/evdev.h
    ControllerInterface/evdev/evdev.cpp
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
    ControllerInterface/Pipes/Pipes.h
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
    target_sources(inputcommon PRIVATE
      ControllerInterface/SDL/SDL.h
      ControllerInterface/SDL/SDL.cpp
    )
    target_link_libraries(inputcommon PRIVATE ${SDL_TARGET})
    target_compile_definitions(inputcommon PRIVATE "CIFACE_USE_SDL=1")
  else()
    message(STATUS "SDL NOT found, disabling SDL input")
  endif()
endif()
