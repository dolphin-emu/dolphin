# selectively extracted and adapted from the libpng CMakeLists.txt, which has the following copyright notice:


# Copyright (C) 2018 Cosmin Truta
# Copyright (C) 2007,2009-2018 Glenn Randers-Pehrson
# Written by Christian Ehrlicher, 2007
# Revised by Roger Lowman, 2009-2010
# Revised by Clifford Yapp, 2011-2012,2017
# Revised by Roger Leigh, 2016
# Revised by Andreas Franek, 2016
# Revised by Sam Serrels, 2017
# Revised by Vadim Barkov, 2017
# Revised by Vicky Pfau, 2018
# Revised by Cameron Cawley, 2018
# Revised by Cosmin Truta, 2018
# Revised by Kyle Bentley, 2018

# This code is released under the libpng license.
# For conditions of distribution and use, see the disclaimer
# and license in png.h



add_library(png STATIC
  png.c
  pngerror.c
  pngget.c
  pngmem.c
  pngpread.c
  pngread.c
  pngrio.c
  pngrtran.c
  pngrutil.c
  pngset.c
  pngtrans.c
  pngwio.c
  pngwrite.c
  pngwtran.c
  pngwutil.c
)

dolphin_disable_warnings_msvc(png)

option(PNG_HARDWARE_OPTIMIZATIONS "Enable hardware optimizations for libpng" OFF)

if(PNG_HARDWARE_OPTIMIZATIONS)

  message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")

  # set definitions and sources for arm
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" OR
    CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
    set(PNG_ARM_NEON_POSSIBLE_VALUES check on off)
    set(PNG_ARM_NEON "check" CACHE STRING "Enable ARM NEON optimizations:
       check: (default) use internal checking code;
       off: disable the optimizations;
       on: turn on unconditionally.")
    set_property(CACHE PNG_ARM_NEON PROPERTY STRINGS
       ${PNG_ARM_NEON_POSSIBLE_VALUES})
    list(FIND PNG_ARM_NEON_POSSIBLE_VALUES ${PNG_ARM_NEON} index)
    if(index EQUAL -1)
      message(FATAL_ERROR
              "PNG_ARM_NEON must be one of [${PNG_ARM_NEON_POSSIBLE_VALUES}]")
    elseif(NOT ${PNG_ARM_NEON} STREQUAL "off")
      target_sources(png PRIVATE
        arm/arm_init.c
        arm/filter_neon.S
        arm/filter_neon_intrinsics.c
        arm/palette_neon_intrinsics.c)

      if(${PNG_ARM_NEON} STREQUAL "on")
        target_compile_definitions(png PUBLIC -DPNG_ARM_NEON_OPT=2)
      elseif(${PNG_ARM_NEON} STREQUAL "check")
        target_compile_definitions(png PUBLIC -DPNG_ARM_NEON_CHECK_SUPPORTED)
      endif()
    else()
      target_compile_definitions(png PUBLIC -DPNG_ARM_NEON_OPT=0)
    endif()
  endif()

  # set definitions and sources for powerpc
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^powerpc*" OR
     CMAKE_SYSTEM_PROCESSOR MATCHES "^ppc64*")
    set(PNG_POWERPC_VSX_POSSIBLE_VALUES on off)
    set(PNG_POWERPC_VSX "on" CACHE STRING "Enable POWERPC VSX optimizations:
       off: disable the optimizations.")
    set_property(CACHE PNG_POWERPC_VSX PROPERTY STRINGS
       ${PNG_POWERPC_VSX_POSSIBLE_VALUES})
    list(FIND PNG_POWERPC_VSX_POSSIBLE_VALUES ${PNG_POWERPC_VSX} index)
    if(index EQUAL -1)
      message(FATAL_ERROR
              "PNG_POWERPC_VSX must be one of [${PNG_POWERPC_VSX_POSSIBLE_VALUES}]")
    elseif(NOT ${PNG_POWERPC_VSX} STREQUAL "off")
      target_sources(png PRIVATE
        powerpc/powerpc_init.c
        powerpc/filter_vsx_intrinsics.c)
      if(${PNG_POWERPC_VSX} STREQUAL "on")
        target_compile_definitions(png PUBLIC -DPNG_POWERPC_VSX_OPT=2)
      endif()
    else()
      target_compile_definitions(png PUBLIC -DPNG_POWERPC_VSX_OPT=0)
    endif()
  endif()

  # set definitions and sources for intel
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^i?86" OR
     CMAKE_SYSTEM_PROCESSOR MATCHES "^x86_64*")
    set(PNG_INTEL_SSE_POSSIBLE_VALUES on off)
    set(PNG_INTEL_SSE "on" CACHE STRING "Enable INTEL_SSE optimizations:
       off: disable the optimizations")
    set_property(CACHE PNG_INTEL_SSE PROPERTY STRINGS
       ${PNG_INTEL_SSE_POSSIBLE_VALUES})
    list(FIND PNG_INTEL_SSE_POSSIBLE_VALUES ${PNG_INTEL_SSE} index)
    if(index EQUAL -1)
      message(FATAL_ERROR
              "PNG_INTEL_SSE must be one of [${PNG_INTEL_SSE_POSSIBLE_VALUES}]")
    elseif(NOT ${PNG_INTEL_SSE} STREQUAL "off")
      target_sources(png PRIVATE
        intel/intel_init.c
        intel/filter_sse2_intrinsics.c)
      if(${PNG_INTEL_SSE} STREQUAL "on")
        target_compile_definitions(png PUBLIC -DPNG_INTEL_SSE_OPT=1)
      endif()
    else()
      target_compile_definitions(png PUBLIC -DPNG_INTEL_SSE_OPT=0)
    endif()
  endif()

  # set definitions and sources for MIPS
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "mipsel*" OR
     CMAKE_SYSTEM_PROCESSOR MATCHES "mips64el*")
    set(PNG_MIPS_MSA_POSSIBLE_VALUES on off)
    set(PNG_MIPS_MSA "on" CACHE STRING "Enable MIPS_MSA optimizations:
       off: disable the optimizations")
    set_property(CACHE PNG_MIPS_MSA PROPERTY STRINGS
       ${PNG_MIPS_MSA_POSSIBLE_VALUES})
    list(FIND PNG_MIPS_MSA_POSSIBLE_VALUES ${PNG_MIPS_MSA} index)
    if(index EQUAL -1)
      message(FATAL_ERROR
              "PNG_MIPS_MSA must be one of [${PNG_MIPS_MSA_POSSIBLE_VALUES}]")
    elseif(NOT ${PNG_MIPS_MSA} STREQUAL "off")
      target_sources(png PRIVATE
        mips/mips_init.c
        mips/filter_msa_intrinsics.c)
      if(${PNG_MIPS_MSA} STREQUAL "on")
        target_compile_definitions(png PUBLIC -DPNG_MIPS_MSA_OPT=2)
      endif()
    else()
      target_compile_definitions(png PUBLIC -DPNG_MIPS_MSA_OPT=0)
    endif()
  endif()

else(PNG_HARDWARE_OPTIMIZATIONS)

  # set definitions and sources for arm
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" OR
     CMAKE_SYSTEM_PROCESSOR MATCHES "^aarch64")
    target_compile_definitions(png PUBLIC -DPNG_ARM_NEON_OPT=0)
  endif()

  # set definitions and sources for powerpc
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^powerpc*" OR
     CMAKE_SYSTEM_PROCESSOR MATCHES "^ppc64*")
    target_compile_definitions(png PUBLIC -DPNG_POWERPC_VSX_OPT=0)
  endif()

  # set definitions and sources for intel
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^i?86" OR
     CMAKE_SYSTEM_PROCESSOR MATCHES "^x86_64*")
    target_compile_definitions(png PUBLIC -DPNG_INTEL_SSE_OPT=0)
  endif()

  # set definitions and sources for MIPS
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "mipsel*" OR
     CMAKE_SYSTEM_PROCESSOR MATCHES "mips64el*")
    target_compile_definitions(png PUBLIC -DPNG_MIPS_MSA_OPT=0)
  endif()

endif(PNG_HARDWARE_OPTIMIZATIONS)

target_sources(png PRIVATE
  # public headers
  png.h
  pngconf.h
  pnglibconf.h

  # private headers
  pngpriv.h
  pngdebug.h
  pnginfo.h
  pngstruct.h
)

target_include_directories(png PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(png PUBLIC ZLIB::ZLIB)

if(NOT MSVC)
  target_compile_options(png PRIVATE
    -Wno-self-assign
  )
endif()
