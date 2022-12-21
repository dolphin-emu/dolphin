# How to Set Up an Android Development Environment

If you'd like to contribute to the Android project, but do not currently have a development environment setup, follow the instructions in this guide.

## Prerequisites

* [Android Studio](https://developer.android.com/studio/)

If you downloaded Android Studio, install it with the default options and open the project located in `dolphin/Source/Android`

## Setting Up Android Studio

1. Wait for background tasks to complete on the bottom of the window.
2. Launch the Android SDK Manager by clicking on its icon in Android Studio's main toolbar:
![Android Studio Package Icon][package-icon]
3. Install or update the SDK Platform. Choose the API level as defined in the app module's [build.gradle](Source/Android/app/build.gradle#L7) file.
4. Install a CMake version as defined in the app module's [build.gradle](Source/Android/app/build.gradle#L99) file. The option won't appear until you select `Show Package Details`.
5. Select `Build Variants` on the left side of the window to choose the build variant and ABI you would like to compile for the `:app` module.
6. Select the green hammer icon in the main toolbar to build and create the apk in `Source/Android/app/build/outputs/apk`

## Compiling from the Command-Line

For command-line users, any task may be executed with `cd Source/Android` followed by `gradlew <task-name>`. In particular, `gradlew assemble` builds debug and release versions of the application (which are placed in `Source/Android/app/build/outputs/apk`).

[package-icon]: https://i.imgur.com/hgmMlsM.png
[code-style]: https://i.imgur.com/3b3UBhb.png
