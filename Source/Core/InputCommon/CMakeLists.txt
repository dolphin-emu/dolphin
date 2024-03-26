add_library(inputcommon
  DynamicInputTextureManager.cpp
  DynamicInputTextureManager.h
  GCAdapter.cpp
  GCAdapter.h
  ImageOperations.cpp
  ImageOperations.h
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
  ControllerEmu/ControlGroup/IMUAccelerometer.cpp
  ControllerEmu/ControlGroup/IMUAccelerometer.h
  ControllerEmu/ControlGroup/IMUCursor.cpp
  ControllerEmu/ControlGroup/IMUCursor.h
  ControllerEmu/ControlGroup/IMUGyroscope.cpp
  ControllerEmu/ControlGroup/IMUGyroscope.h
  ControllerEmu/ControlGroup/IRPassthrough.cpp
  ControllerEmu/ControlGroup/IRPassthrough.h
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
  ControllerInterface/DualShockUDPClient/DualShockUDPClient.cpp
  ControllerInterface/DualShockUDPClient/DualShockUDPClient.h
  ControllerInterface/DualShockUDPClient/DualShockUDPProto.h
  ControllerInterface/ControllerInterface.cpp
  ControllerInterface/ControllerInterface.h
  ControllerInterface/CoreDevice.cpp
  ControllerInterface/CoreDevice.h
  ControllerInterface/InputBackend.cpp
  ControllerInterface/InputBackend.h
  ControllerInterface/MappingCommon.cpp
  ControllerInterface/MappingCommon.h
  ControllerInterface/Wiimote/WiimoteController.cpp
  ControllerInterface/Wiimote/WiimoteController.h
  ControllerInterface/WiiUAdapter/WiiUAdapter.cpp
  ControllerInterface/WiiUAdapter/WiiUAdapter.h
  ControlReference/ControlReference.cpp
  ControlReference/ControlReference.h
  ControlReference/ExpressionParser.cpp
  ControlReference/ExpressionParser.h
  ControlReference/FunctionExpression.cpp
  ControlReference/FunctionExpression.h
  DynamicInputTextures/DITConfiguration.cpp
  DynamicInputTextures/DITConfiguration.h
  DynamicInputTextures/DITData.h
  DynamicInputTextures/DITSpecification.cpp
  DynamicInputTextures/DITSpecification.h
)

target_link_libraries(inputcommon
PUBLIC
  common

PRIVATE
  fmt::fmt
  sfml-network
  spng::spng
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
    ControllerInterface/WGInput/WGInput.cpp
    ControllerInterface/WGInput/WGInput.h
    ControllerInterface/XInput/XInput.cpp
    ControllerInterface/XInput/XInput.h
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.cpp
    ControllerInterface/ForceFeedback/ForceFeedbackDevice.h
  )
elseif(APPLE)
  target_sources(inputcommon PRIVATE
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
  )
  target_compile_options(inputcommon PRIVATE
    -fobjc-arc
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
    ControllerInterface/Touch/InputOverrider.cpp
    ControllerInterface/Touch/InputOverrider.h
  )
  target_link_libraries(inputcommon PRIVATE
    androidcommon
  )
endif()

if(NOT ANDROID)
  target_link_libraries(inputcommon PUBLIC LibUSB::LibUSB)
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
  target_sources(inputcommon PRIVATE
    ControllerInterface/SDL/SDL.cpp
    ControllerInterface/SDL/SDL.h
  )
  target_link_libraries(inputcommon PRIVATE SDL2::SDL2)
  target_compile_definitions(inputcommon PUBLIC HAVE_SDL2=1)
endif()

if(MSVC)
  # Add precompiled header
  target_link_libraries(inputcommon PRIVATE use_pch)
endif()

if(TARGET Hidapi::Hidapi)
  target_sources(inputcommon
  PRIVATE
    ControllerInterface/SteamDeck/SteamDeck.cpp
    ControllerInterface/SteamDeck/SteamDeck.h
  )
  target_link_libraries(inputcommon PUBLIC Hidapi::Hidapi)
  target_compile_definitions(inputcommon PRIVATE -DHAVE_HIDAPI=1)
endif()
