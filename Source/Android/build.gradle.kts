// Top-level build file where you can add configuration options common to all sub-projects/modules.
plugins {
    id("com.android.application") version "8.13.1" apply false
    id("com.android.library") version "8.13.1" apply false
    id("org.jetbrains.kotlin.android") version "2.2.21" apply false
    id("com.android.test") version "8.13.1" apply false
    id("androidx.baselineprofile") version "1.3.3" apply false
}

buildscript {
    repositories {
        google()
    }
}
