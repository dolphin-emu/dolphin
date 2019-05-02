add_dolphin_test(MMIOTest MMIOTest.cpp)
add_dolphin_test(PageFaultTest PageFaultTest.cpp)
add_dolphin_test(CoreTimingTest CoreTimingTest.cpp)

add_dolphin_test(DSPAcceleratorTest DSP/DSPAcceleratorTest.cpp)
add_dolphin_test(DSPAssemblyTest
  DSP/DSPAssemblyTest.cpp
  DSP/DSPTestBinary.cpp
  DSP/DSPTestText.cpp
  DSP/HermesBinary.cpp
)

add_dolphin_test(ESFormatsTest IOS/ES/FormatsTest.cpp IOS/ES/TestBinaryData.cpp)

add_dolphin_test(FileSystemTest IOS/FS/FileSystemTest.cpp)

if(_M_X86)
  add_dolphin_test(PowerPCTest PowerPC/Jit64Common/Frsqrte.cpp)
endif()
