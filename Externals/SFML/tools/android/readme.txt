Compiling external libraries for Android can be a tedious task, especially for
those who aren't familiar with the NDK, that's why we provide these scripts.

IMPORTANT: Please, be careful when using these scripts! They are unpolished at
the moment and you'll have to respect a simple rule: call these scripts from
where they are. So, in that case, head yourself to tools/android, then call
./make_all.sh.

Feel free to improve them or send patches.

HOW-TO-USE:
-----------
1) Some of these scripts need an environement variable to work ($NDK) as well
as the Android CMake toolchain you'll find in cmake/toolchains (android.toolchain.cmake)
export NDK=/path/to/your/ndk
export ANDROID_CMAKE_TOOLCHAIN=/path/to/android.toolchain.cmake

2) You'll need to make them executable, so:
chmod +x *.sh

3) Type: ./make_all.sh which should create standalone toolchains, download the
external libraries and compile them.

These scripts will be improved over time. Meanwhile, you'll have to play with
them if you want a customized behavior.

Good luck!
