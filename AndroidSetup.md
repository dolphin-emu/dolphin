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

### Step-By-Step Instructions

1. Download [Android Studio](https://developer.android.com/studio/)
2. From the splash screen, select "clone repository", and use the URL `https://github.com/dolphin-emu/dolphin` (it may take a while to clone)
3. Trust the project and open it
4. Wait for any background tasks to complete on the bottom of the window
5. Navigate to Source/Android/build.gradle.kts, or click the first link on the "Android Gradle build scripts found" popup <br /> ![Android Studio popup][android-studio-popup]
6. Click "Link Gradle project" on the banner near the top of the window, then choose "OK" on the popup <br /> ![Link Gradle project banner][link-gradle-project-banner]
7. Go to settings (the gear icon at the top of the window), then "SDK Manager", and navigate to the "SDK Tools" tab <br /> ![SDK Manager UI][sdk-manager-ui]  ![SDK Tools location][sdk-tools-location]
8. Ensure the following boxes are checked:
    * First, check "Show Package Details" at the bottom of the window
    * Under the "Android SDK Build-Tools" dropdown, choose an SDK of minimum version 21, preferably >= 34. The most recent version will usually be best.
    * Under the "NDK (Side by side)" dropdown, find version 27.0.12077973 and check it.
    * Under the "CMake" dropdown, choose a version past 3.22.1, where again the more recent versions are usually preferable.
9. Navigate to the project dropdown in the top left of the window, find the "Source/Android" directory, open it in the current window
10. Select "Build Variants" from the left toolbar (or find it in the search at the top right by checking "include non-project items), and configure build variant and ABI to your liking
11. Click the hammer icon with the green pip to create the apk in "Source/Android/app/build/outputs/apk". This may take a considerable amount of time <br /> ![Hammer icon][hammer-icon]

## Compiling from the Command-Line

For command-line users, any task may be executed with `cd Source/Android` followed by `gradlew <task-name>`. In particular, `gradlew assemble` builds debug and release versions of the application (which are placed in `Source/Android/app/build/outputs/apk`).

[package-icon]: https://i.imgur.com/hgmMlsM.png
[code-style]: https://i.imgur.com/3b3UBhb.png
[android-studio-popup]: https://i.imgur.com/RvBu5ON.png
[link-gradle-project-banner]: https://i.imgur.com/l02VeAV.png
[sdk-manager-ui]: https://i.imgur.com/1tYmApC.png
[sdk-tools-location]: https://i.imgur.com/FvVflia.png
[hammer-icon]: https://i.imgur.com/zD0Emhq.png
