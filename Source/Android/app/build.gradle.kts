plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    kotlin("plugin.serialization") version "1.8.21"
    id("androidx.baselineprofile")
}

@Suppress("UnstableApiUsage")
android {
    compileSdkVersion = "android-34"
    ndkVersion = "26.1.10909125"

    buildFeatures {
        viewBinding = true
        buildConfig = true
    }

    compileOptions {
        // Flag to enable support for the new language APIs
        isCoreLibraryDesugaringEnabled = true

        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    lint {
        // This is important as it will run lint but not abort on error
        // Lint has some overly obnoxious "errors" that should really be warnings
        abortOnError = false

        //Uncomment disable lines for test builds...
        //disable "MissingTranslation"
        //disable "ExtraTranslation"
    }

    defaultConfig {
        applicationId = "org.dolphinemu.dolphinemu"
        minSdk = 21
        targetSdk = 34

        versionCode = getBuildVersionCode()

        versionName = getGitVersion()

        buildConfigField("String", "GIT_HASH", "\"${getGitHash()}\"")
        buildConfigField("String", "BRANCH", "\"${getBranch()}\"")

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    signingConfigs {
        create("release") {
            if (project.hasProperty("keystore")) {
                storeFile = file(project.property("keystore")!!)
                storePassword = project.property("storepass").toString()
                keyAlias = project.property("keyalias").toString()
                keyPassword = project.property("keypass").toString()
            }
        }
    }

    // Define build types, which are orthogonal to product flavors.
    buildTypes {
        // Signed by release key, allowing for upload to Play Store.
        release {
            if (project.hasProperty("keystore")) {
                signingConfig = signingConfigs.getByName("release")
            }

            resValue("string", "app_name_suffixed", "Dolphin Emulator")
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android.txt"),
                "proguard-rules.pro"
            )
        }

        // Signed by debug key disallowing distribution on Play Store.
        // Attaches "debug" suffix to version and package name, allowing installation alongside the release build.
        debug {
            resValue("string", "app_name_suffixed", "Dolphin Debug")
            applicationIdSuffix = ".debug"
            versionNameSuffix = "-debug"
            isJniDebuggable = true
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../../CMakeLists.txt")
            version = "3.22.1+"
        }
    }
    namespace = "org.dolphinemu.dolphinemu"

    defaultConfig {
        externalNativeBuild {
            cmake {
                arguments("-DANDROID_STL=c++_static", "-DCMAKE_BUILD_TYPE=RelWithDebInfo")
                // , "-DENABLE_GENERIC=ON"
                abiFilters("arm64-v8a", "x86_64") //, "armeabi-v7a", "x86"

                // Uncomment the line below if you don't want to build the C++ unit tests
                //targets("main", "hook_impl", "main_hook", "gsl_alloc_hook", "file_redirect_hook")
            }
        }
    }

    packaging {
        jniLibs.useLegacyPackaging = true
    }
}

dependencies {
    baselineProfile(project(":benchmark"))
    coreLibraryDesugaring("com.android.tools:desugar_jdk_libs:2.0.4")

    implementation("androidx.core:core-ktx:1.13.0")
    implementation("androidx.appcompat:appcompat:1.6.1")
    implementation("androidx.cardview:cardview:1.0.0")
    implementation("androidx.recyclerview:recyclerview:1.3.2")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.fragment:fragment-ktx:1.6.2")
    implementation("androidx.slidingpanelayout:slidingpanelayout:1.2.0")
    implementation("com.google.android.material:material:1.11.0")
    implementation("androidx.core:core-splashscreen:1.0.1")
    implementation("androidx.preference:preference-ktx:1.2.1")
    implementation("androidx.profileinstaller:profileinstaller:1.3.1")

    // Kotlin extensions for lifecycle components
    implementation("androidx.lifecycle:lifecycle-viewmodel-ktx:2.7.0")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.7.0")

    // Android TV UI libraries.
    implementation("androidx.leanback:leanback:1.0.0")
    implementation("androidx.tvprovider:tvprovider:1.0.0")

    // For REST calls
    implementation("com.android.volley:volley:1.2.1")

    // For loading game covers from disk and GameTDB
    implementation("io.coil-kt:coil:2.6.0")

    // For loading custom GPU drivers
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.6.3")

    implementation("com.nononsenseapps:filepicker:4.2.1")
}

fun getGitVersion(): String {
    try {
        return ProcessBuilder("git", "describe", "--always", "--long")
            .directory(project.rootDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start().inputStream.bufferedReader().use { it.readText() }
            .trim()
            .replace(Regex("(-0)?-[^-]+$"), "")
    } catch (e: Exception) {
        logger.error("Cannot find git, defaulting to dummy version number")
    }

    return "0.0"
}

fun getBuildVersionCode(): Int {
    try {
        return Integer.valueOf(
            ProcessBuilder("git", "rev-list", "--first-parent", "--count", "HEAD")
                .directory(project.rootDir)
                .redirectOutput(ProcessBuilder.Redirect.PIPE)
                .redirectError(ProcessBuilder.Redirect.PIPE)
                .start().inputStream.bufferedReader().use { it.readText() }
                .trim()
        )
    } catch (e: Exception) {
        logger.error("Cannot find git, defaulting to dummy version code")
    }

    return 1
}

fun getGitHash(): String {
    try {
        return ProcessBuilder("git", "rev-parse", "HEAD")
            .directory(project.rootDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start().inputStream.bufferedReader().use { it.readText() }
            .trim()
    } catch (e: Exception) {
        logger.error("Cannot find git, defaulting to dummy git hash")
    }

    return "0"
}

fun getBranch(): String {
    try {
        return ProcessBuilder("git", "rev-parse", "--abbrev-ref", "HEAD")
            .directory(project.rootDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start().inputStream.bufferedReader().use { it.readText() }
            .trim()
    } catch (e: Exception) {
        logger.error("Cannot find git, defaulting to dummy git hash")
    }

    return "master"
}
