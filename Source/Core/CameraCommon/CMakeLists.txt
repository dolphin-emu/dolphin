add_library(cameracommon
  CameraOrchestrator.cpp
  CameraOrchestrator.h
  CameraQualifier.cpp
  CameraQualifier.h
  CameraBackend/CameraBackend.h
  CameraBackend/NullCameraBackend.cpp
  CameraBackend/NullCameraBackend.h
  CameraInterface/CameraInterface.h
  CameraInterface/NullCameraInterface.cpp
  CameraInterface/NullCameraInterface.h
)

target_link_libraries(cameracommon
PUBLIC
  common
)

if(ENABLE_SDL)
  target_sources(cameracommon PRIVATE
    CameraBackend/SDLCameraBackend.cpp
    CameraBackend/SDLCameraBackend.h
    CameraInterface/SDLCameraInterface.cpp
    CameraInterface/SDLCameraInterface.h
  )
  target_link_libraries(cameracommon PRIVATE SDL3::SDL3)
endif()
