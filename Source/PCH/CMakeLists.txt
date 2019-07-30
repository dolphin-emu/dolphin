add_library(pch pch.h pch.cpp)
set(PCH_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(PCH_NAME ${PCH.pch})
target_compile_options(pch PUBLIC /Ycpch.h /Fp${PCH_DIRECTORY}/${PCH_NAME}) 