../../Binary/x64/DSPTool.exe -h dsp_code tests/mul_test.ds
mkdir emu
cp ../Main/Core/DSP/*.cpp emu
cp ../Main/Core/DSP/*.h emu
make

