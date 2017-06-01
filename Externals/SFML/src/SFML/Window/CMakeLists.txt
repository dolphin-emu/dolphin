
set(INCROOT ${PROJECT_SOURCE_DIR}/include/SFML/Window)
set(SRCROOT ${PROJECT_SOURCE_DIR}/src/SFML/Window)

# all source files
set(SRC
    ${SRCROOT}/Context.cpp
    ${INCROOT}/Context.hpp
    ${INCROOT}/Export.hpp
    ${SRCROOT}/GlContext.cpp
    ${SRCROOT}/GlContext.hpp
    ${SRCROOT}/GlResource.cpp
    ${INCROOT}/GlResource.hpp
    ${INCROOT}/ContextSettings.hpp
    ${INCROOT}/Event.hpp
    ${SRCROOT}/InputImpl.hpp
    ${INCROOT}/Joystick.hpp
    ${SRCROOT}/Joystick.cpp
    ${SRCROOT}/JoystickImpl.hpp
    ${SRCROOT}/JoystickManager.cpp
    ${SRCROOT}/JoystickManager.hpp
    ${INCROOT}/Keyboard.hpp
    ${SRCROOT}/Keyboard.cpp
    ${INCROOT}/Mouse.hpp
    ${SRCROOT}/Mouse.cpp
    ${INCROOT}/Touch.hpp
    ${SRCROOT}/Touch.cpp
    ${INCROOT}/Sensor.hpp
    ${SRCROOT}/Sensor.cpp
    ${SRCROOT}/SensorImpl.hpp
    ${SRCROOT}/SensorManager.cpp
    ${SRCROOT}/SensorManager.hpp
    ${SRCROOT}/VideoMode.cpp
    ${INCROOT}/VideoMode.hpp
    ${SRCROOT}/VideoModeImpl.hpp
    ${SRCROOT}/Window.cpp
    ${INCROOT}/Window.hpp
    ${INCROOT}/WindowHandle.hpp
    ${SRCROOT}/WindowImpl.cpp
    ${SRCROOT}/WindowImpl.hpp
    ${INCROOT}/WindowStyle.hpp
)
if(SFML_OPENGL_ES AND NOT SFML_OS_IOS)
    list(APPEND SRC ${SRCROOT}/EGLCheck.cpp)
    list(APPEND SRC ${SRCROOT}/EGLCheck.hpp)
    list(APPEND SRC ${SRCROOT}/EglContext.cpp)
    list(APPEND SRC ${SRCROOT}/EglContext.hpp)
endif()
source_group("" FILES ${SRC})

# add platform specific sources
if(SFML_OS_WINDOWS)
    set(PLATFORM_SRC
        ${SRCROOT}/Win32/WglContext.cpp
        ${SRCROOT}/Win32/WglContext.hpp
        ${SRCROOT}/Win32/WglExtensions.cpp
        ${SRCROOT}/Win32/WglExtensions.hpp
        ${SRCROOT}/Win32/InputImpl.cpp
        ${SRCROOT}/Win32/InputImpl.hpp
        ${SRCROOT}/Win32/JoystickImpl.cpp
        ${SRCROOT}/Win32/JoystickImpl.hpp
        ${SRCROOT}/Win32/SensorImpl.hpp
        ${SRCROOT}/Win32/SensorImpl.cpp
        ${SRCROOT}/Win32/VideoModeImpl.cpp
        ${SRCROOT}/Win32/WindowImplWin32.cpp
        ${SRCROOT}/Win32/WindowImplWin32.hpp
    )
    source_group("windows" FILES ${PLATFORM_SRC})

    # make sure that we use the Unicode version of the Win API functions
    add_definitions(-DUNICODE -D_UNICODE)
elseif(SFML_OS_LINUX OR SFML_OS_FREEBSD)
    set(PLATFORM_SRC
        ${SRCROOT}/Unix/Display.cpp
        ${SRCROOT}/Unix/Display.hpp
        ${SRCROOT}/Unix/InputImpl.cpp
        ${SRCROOT}/Unix/InputImpl.hpp
        ${SRCROOT}/Unix/ScopedXcbPtr.hpp
        ${SRCROOT}/Unix/ScopedXcbPtr.inl
        ${SRCROOT}/Unix/SensorImpl.cpp
        ${SRCROOT}/Unix/SensorImpl.hpp
        ${SRCROOT}/Unix/VideoModeImpl.cpp
        ${SRCROOT}/Unix/WindowImplX11.cpp
        ${SRCROOT}/Unix/WindowImplX11.hpp
    )
    if(NOT SFML_OPENGL_ES)
        set(PLATFORM_SRC
            ${PLATFORM_SRC}
            ${SRCROOT}/Unix/GlxContext.cpp
            ${SRCROOT}/Unix/GlxContext.hpp
            ${SRCROOT}/Unix/GlxExtensions.cpp
            ${SRCROOT}/Unix/GlxExtensions.hpp
        )
    endif()
    if(SFML_OS_LINUX)
        set(PLATFORM_SRC
            ${PLATFORM_SRC}
            ${SRCROOT}/Unix/JoystickImpl.cpp
            ${SRCROOT}/Unix/JoystickImpl.hpp
        )
    elseif(SFML_OS_FREEBSD)
        set(PLATFORM_SRC
            ${PLATFORM_SRC}
            ${SRCROOT}/FreeBSD/JoystickImpl.cpp
            ${SRCROOT}/FreeBSD/JoystickImpl.hpp
        )
    endif()
    source_group("unix" FILES ${PLATFORM_SRC})
elseif(SFML_OS_MACOSX)
    set(PLATFORM_SRC
        ${SRCROOT}/OSX/cpp_objc_conversion.h
        ${SRCROOT}/OSX/cpp_objc_conversion.mm
        ${SRCROOT}/OSX/cg_sf_conversion.hpp
        ${SRCROOT}/OSX/cg_sf_conversion.mm
        ${SRCROOT}/OSX/InputImpl.mm
        ${SRCROOT}/OSX/InputImpl.hpp
        ${SRCROOT}/OSX/HIDInputManager.hpp
        ${SRCROOT}/OSX/HIDInputManager.mm
        ${SRCROOT}/OSX/HIDJoystickManager.hpp
        ${SRCROOT}/OSX/HIDJoystickManager.cpp
        ${SRCROOT}/OSX/JoystickImpl.cpp
        ${SRCROOT}/OSX/JoystickImpl.hpp
        ${SRCROOT}/OSX/NSImage+raw.h
        ${SRCROOT}/OSX/NSImage+raw.mm
        ${SRCROOT}/OSX/Scaling.h
        ${SRCROOT}/OSX/SensorImpl.cpp
        ${SRCROOT}/OSX/SensorImpl.hpp
        ${SRCROOT}/OSX/SFApplication.h
        ${SRCROOT}/OSX/SFApplication.m
        ${SRCROOT}/OSX/SFApplicationDelegate.h
        ${SRCROOT}/OSX/SFApplicationDelegate.m
        ${SRCROOT}/OSX/SFContext.hpp
        ${SRCROOT}/OSX/SFContext.mm
        ${SRCROOT}/OSX/SFKeyboardModifiersHelper.h
        ${SRCROOT}/OSX/SFKeyboardModifiersHelper.mm
        ${SRCROOT}/OSX/SFOpenGLView.h
        ${SRCROOT}/OSX/SFOpenGLView.mm
        ${SRCROOT}/OSX/SFOpenGLView+keyboard.mm
        ${SRCROOT}/OSX/SFOpenGLView+keyboard_priv.h
        ${SRCROOT}/OSX/SFOpenGLView+mouse.mm
        ${SRCROOT}/OSX/SFOpenGLView+mouse_priv.h
        ${SRCROOT}/OSX/SFSilentResponder.h
        ${SRCROOT}/OSX/SFSilentResponder.m
        ${SRCROOT}/OSX/SFWindow.h
        ${SRCROOT}/OSX/SFWindow.m
        ${SRCROOT}/OSX/SFWindowController.h
        ${SRCROOT}/OSX/SFWindowController.mm
        ${SRCROOT}/OSX/SFViewController.h
        ${SRCROOT}/OSX/SFViewController.mm
        ${SRCROOT}/OSX/VideoModeImpl.cpp
        ${SRCROOT}/OSX/WindowImplCocoa.hpp
        ${SRCROOT}/OSX/WindowImplCocoa.mm
        ${SRCROOT}/OSX/WindowImplDelegateProtocol.h
        ${SRCROOT}/OSX/AutoreleasePoolWrapper.h
        ${SRCROOT}/OSX/AutoreleasePoolWrapper.mm
    )
    source_group("mac" FILES ${PLATFORM_SRC})
elseif(SFML_OS_IOS)
    set(PLATFORM_SRC
        ${SRCROOT}/iOS/EaglContext.mm
        ${SRCROOT}/iOS/EaglContext.hpp
        ${SRCROOT}/iOS/InputImpl.mm
        ${SRCROOT}/iOS/InputImpl.hpp
        ${SRCROOT}/iOS/JoystickImpl.mm
        ${SRCROOT}/iOS/JoystickImpl.hpp
        ${SRCROOT}/iOS/SensorImpl.mm
        ${SRCROOT}/iOS/SensorImpl.hpp
        ${SRCROOT}/iOS/VideoModeImpl.mm
        ${SRCROOT}/iOS/WindowImplUIKit.hpp
        ${SRCROOT}/iOS/WindowImplUIKit.mm
        ${SRCROOT}/iOS/ObjCType.hpp
        ${SRCROOT}/iOS/SFAppDelegate.hpp
        ${SRCROOT}/iOS/SFAppDelegate.mm
        ${SRCROOT}/iOS/SFView.hpp
        ${SRCROOT}/iOS/SFView.mm
        ${SRCROOT}/iOS/SFViewController.hpp
        ${SRCROOT}/iOS/SFViewController.mm
        ${SRCROOT}/iOS/SFMain.hpp
        ${SRCROOT}/iOS/SFMain.mm
    )
    source_group("ios" FILES ${PLATFORM_SRC})
elseif(SFML_OS_ANDROID)
    set(PLATFORM_SRC
        ${SRCROOT}/Android/WindowImplAndroid.hpp
        ${SRCROOT}/Android/WindowImplAndroid.cpp
        ${SRCROOT}/Android/VideoModeImpl.cpp
        ${SRCROOT}/Android/InputImpl.hpp
        ${SRCROOT}/Android/InputImpl.cpp
        ${SRCROOT}/Android/JoystickImpl.hpp
        ${SRCROOT}/Android/JoystickImpl.cpp
        ${SRCROOT}/Android/SensorImpl.hpp
        ${SRCROOT}/Android/SensorImpl.cpp
    )
    source_group("android" FILES ${PLATFORM_SRC})
endif()

# find external libraries
if(SFML_OS_LINUX OR SFML_OS_FREEBSD)
    find_package(X11 REQUIRED)
    if(NOT X11_FOUND)
        message(FATAL_ERROR "X11 library not found")
    endif()
    include_directories(${X11_INCLUDE_DIR})
endif()
if(NOT SFML_OPENGL_ES)
    find_package(OpenGL REQUIRED)
    include_directories(${OPENGL_INCLUDE_DIR})
    if(SFML_OS_LINUX OR SFML_OS_FREEBSD)
        find_package(XCB COMPONENTS xlib_xcb image randr REQUIRED)
        if(NOT LIBXCB_FOUND)
            message(FATAL_ERROR "Xcb library not found")
        endif()
        include_directories(${LIBXCB_INCLUDE_DIRS})
    endif()
endif()
if(SFML_OPENGL_ES AND SFML_OS_LINUX)
    find_package(EGL REQUIRED)
    find_package(GLES REQUIRED)
    include_directories(${EGL_INCLUDE_DIR} ${GLES_INCLUDE_DIR})
endif()
if(SFML_OS_LINUX)
    find_package(UDev REQUIRED)
    if(NOT UDEV_FOUND)
        message(FATAL_ERROR "udev library not found")
    endif()
    include_directories(${UDEV_INCLUDE_DIR})
endif()


# build the list of external libraries to link
if(SFML_OS_WINDOWS)
    list(APPEND WINDOW_EXT_LIBS winmm gdi32)
elseif(SFML_OS_LINUX)
    list(APPEND WINDOW_EXT_LIBS ${X11_X11_LIB} ${LIBXCB_LIBRARIES} ${UDEV_LIBRARIES})
elseif(SFML_OS_FREEBSD)
    list(APPEND WINDOW_EXT_LIBS ${X11_X11_LIB} ${LIBXCB_LIBRARIES} usbhid)
elseif(SFML_OS_MACOSX)
    list(APPEND WINDOW_EXT_LIBS "-framework Foundation -framework AppKit -framework IOKit -framework Carbon")
elseif(SFML_OS_IOS)
    list(APPEND WINDOW_EXT_LIBS "-framework Foundation -framework UIKit -framework CoreGraphics -framework QuartzCore -framework CoreMotion")
elseif(SFML_OS_ANDROID)
    list(APPEND WINDOW_EXT_LIBS android)
endif()
if(SFML_OPENGL_ES)
    if(SFML_OS_LINUX)
        list(APPEND WINDOW_EXT_LIBS ${EGL_LIBRARY} ${GLES_LIBRARY})
    elseif(SFML_OS_IOS)
        list(APPEND WINDOW_EXT_LIBS "-framework OpenGLES")
    elseif(SFML_OS_ANDROID)
        list(APPEND WINDOW_EXT_LIBS EGL GLESv1_CM)
    endif()
else()
    list(APPEND WINDOW_EXT_LIBS ${OPENGL_gl_LIBRARY})
endif()

# define the sfml-window target
sfml_add_library(sfml-window
                 SOURCES ${SRC} ${PLATFORM_SRC}
                 DEPENDS sfml-system
                 EXTERNAL_LIBS ${WINDOW_EXT_LIBS})
