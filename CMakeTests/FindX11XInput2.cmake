# For finding the X11 XInput extension, version 2. Because the standard CMake
# FindX11.cmake may only know to look for version 1. Newer versions of CMake
# than 2.8.7 might not have this problem, I wouldn't know. Better to be safe.


if(USE_X11)
    
    IF(NOT X11_FOUND)
        INCLUDE(FindX11)
    ENDIF(NOT X11_FOUND)

    IF(X11_FOUND)
        FIND_PATH(X11_Xinput2_INCLUDE_PATH X11/extensions/XInput2.h ${X11_INC_SEARCH_PATH})
        
        FIND_LIBRARY(X11_Xinput2_LIB Xi ${X11_LIB_SEARCH_PATH})
        
        IF (X11_Xinput2_INCLUDE_PATH AND X11_Xinput2_LIB)
            SET(X11_Xinput2_FOUND TRUE)
            SET(X11_INCLUDE_DIR ${X11_INCLUDE_DIR} ${X11_Xinput2_INCLUDE_PATH})
            message("X11 Xinput2 found")
        ENDIF (X11_Xinput2_INCLUDE_PATH AND X11_Xinput2_LIB)
        
        MARK_AS_ADVANCED(
            X11_Xinput2_INCLUDE_PATH
            X11_Xinput2_LIB
        )
        
    ENDIF(X11_FOUND)

endif()
