# The PCH that dolphin uses for MSVC is non-standard;
# Instead of having one PCH per module, dolphin has one PCH shared between all modules.
# So we need to implement PCH manually, rather than using the PCH support built into cmake

# linking against this interface libary will cause targets to enable PCH
add_library(use_pch INTERFACE)

# Uncomment this return to disable PCH
#return()

add_library(build_pch pch.h pch.cpp)

# fmt/format.h is included in the PCH
target_link_libraries(build_pch PUBLIC fmt::fmt)

# pch.cpp should be compiled with the /Yc command, which creates the precompiled header
target_compile_options(build_pch PRIVATE /Ycpch.h)

# /Fp sets the location of the PCH. By forcing it to a fixed location, all modules
# will share this one PCH. We give it a fixed name so we can depend on it later
target_compile_options(build_pch PUBLIC /Fp$<TARGET_FILE_DIR:build_pch>/dolphin.pch )

# Sharing a PCH breaks pdb files. So we use the /Z7 option to inline the pdb into
# the binary. However MSVC gets noisy if you set both /Zi and /Z7
if (POLICY CMP0141)
    # CMake 3.25 has a policy that makes us control this somewhat sanely
    set_property(TARGET build_pch PROPERTY MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

    # Unfortnually, properties don't propagate. So we also set it globally via parent scope
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>" PARENT_SCOPE)
else()
    if (CMAKE_CXX_FLAGS_DEBUG MATCHES "/Zi")
        # Otherwise we do an ugly string replace to remove it from FLAGS_DEBUG
        string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")

        # and also overwrite the version in the parent scope
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" PARENT_SCOPE)
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" PARENT_SCOPE)

        target_compile_options(build_pch PUBLIC "$<$<CONFIG:Debug,RelWithDebInfo>:/Z7>")
    endif()
endif()
# Setting /Z7 also requires us to disable minimal rebuilds.
target_compile_options(build_pch PUBLIC "$<$<CONFIG:Debug,RelWithDebInfo>:/Gm->")

# To get this working with ninja, we need to tell it that compiling pch.cpp generates an extra output
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/pch.cpp PROPERTIES
    OBJECT_OUTPUTS $<TARGET_FILE_DIR:build_pch>/dolphin.pch
)

# and then create a custom target that depends on the pch output
# so that ninja won't start building anything that depends on this
# target before the pch is built
add_custom_target(force_build_pch
    DEPENDS $<TARGET_FILE_DIR:build_pch>/dolphin.pch
)

# link the pch into anything that depends on use_pch
target_link_libraries(use_pch INTERFACE build_pch)

# targets which use the pch need these compile options
# /Yu - Use precompiled header named "pch.h"
# /FI - Force include "pch.h" at top of every source file
target_compile_options(use_pch INTERFACE /Yupch.h /FIpch.h)

# For ninja, we need to depend on force_build_pch
add_dependencies(use_pch force_build_pch)
