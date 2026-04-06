# How to Set Up an Android Development Environment

If you'd like to contribute to the Android project, but do not currently have a development environment setup, follow the instructions in this guide.

## Prerequisites

* [Android Studio](https://developer.android.com/studio/)

Install Android Studio with the default options if you do not already have it.

## Setting Up Android Studio

1. Open the `Source/Android` project in Android Studio, let any background tasks finish, and Android Studio will automatically download the required SDK components and tooling.
2. Use the hammer icon or **Build > Assemble 'app' Run Configuration** to compile the `:app` module, then choose **Build > Generate App Bundles or APKs > Generate APKs** to produce the APK in `Source/Android/app/build/outputs/apk`.

## Import the Dolphin code style

The project maintains a custom IntelliJ/Android Studio style file to keep Java and Kotlin formatting consistent.

* Go to **File > Settings > Editor > Code Style** (or **Android Studio > Settings > Editor > Code Style** on macOS).
* Click the gear icon and choose **Import Scheme**.
* Select `Source/Android/code-style-java.xml` from the repository.
* Ensure that Dolphin-Java is selected.

## Format code before opening a pull request

Before committing or submitting a pull request, reformat any Java or Kotlin files you modified:

* Use **Code > Reformat Code** (or press `Ctrl+Alt+L`/`⌥⌘L` on macOS) with the Dolphin code style selected.
* Re-run formatting after edits to keep spacing, imports, and wrapping consistent with the rest of the project.

## Compiling from the Command-Line

For command-line users, any task may be executed with `cd Source/Android` followed by `gradlew <task-name>`. In particular, `gradlew assemble` builds debug and release versions of the application (which are placed in `Source/Android/app/build/outputs/apk`).
