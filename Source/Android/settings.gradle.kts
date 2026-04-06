pluginManagement {
    buildscript {
        repositories {
            mavenCentral()
            maven {
                url = uri("https://storage.googleapis.com/r8-releases/raw")
            }
        }
        dependencies {
            // Temporary override for AGP-bundled R8 crash in release minification.
            // https://issuetracker.google.com/issues/495458806
            // TODO: Re-test without this override when upgrading AGP and remove if fixed upstream.
            classpath("com.android.tools:r8:9.1.34")
        }
    }

    repositories {
        gradlePluginPortal()
        google()
        mavenCentral()
    }
}

@Suppress("UnstableApiUsage")
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

include(":app")
include(":benchmark")
