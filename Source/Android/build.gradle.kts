// Top-level build file where you can add configuration options common to all sub-projects/modules.
plugins {
    id("com.android.application") version "9.0.1" apply false
    id("com.android.library") version "9.0.1" apply false
    id("com.android.test") version "9.0.1" apply false
    id("androidx.baselineprofile") version "1.5.0-alpha03" apply false
}

buildscript {
    repositories {
        google()
    }
}
