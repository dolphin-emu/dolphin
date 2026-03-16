plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.serialization)
    alias(libs.plugins.androidx.baselineprofile)
}

@Suppress("UnstableApiUsage")
android {
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    buildFeatures {
        viewBinding = true
        buildConfig = true
        resValues = true
    }

    compileOptions {
        // Flag to enable support for the new language APIs
        isCoreLibraryDesugaringEnabled = true

        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
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
        minSdk = 24
        targetSdk = 36

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
                getDefaultProguardFile("proguard-android-optimize.txt"),
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
                arguments(
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                    "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
                    // , "-DENABLE_GENERIC=ON"
                )
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
    coreLibraryDesugaring(libs.desugar.jdk.libs)

    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.cardview)
    implementation(libs.androidx.recyclerview)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.fragment.ktx)
    implementation(libs.androidx.slidingpanelayout)
    implementation(libs.material)
    implementation(libs.androidx.core.splashscreen)
    implementation(libs.androidx.preference.ktx)
    implementation(libs.androidx.profileinstaller)

    // Kotlin extensions for lifecycle components
    implementation(libs.androidx.lifecycle.viewmodel.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)

    // Android TV UI libraries.
    implementation(libs.androidx.leanback)
    implementation(libs.androidx.tvprovider)
    implementation(libs.androidx.swiperefreshlayout)

    // For loading game covers from disk and GameTDB
    implementation(libs.coil)

    // For loading custom GPU drivers
    implementation(libs.kotlinx.serialization.json)

    implementation(libs.kotlinx.coroutines.android)

    implementation(libs.filepicker)
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
        val commitCount = Integer.valueOf(
            ProcessBuilder("git", "rev-list", "--first-parent", "--count", "HEAD")
                .directory(project.rootDir)
                .redirectOutput(ProcessBuilder.Redirect.PIPE)
                .redirectError(ProcessBuilder.Redirect.PIPE)
                .start().inputStream.bufferedReader().use { it.readText() }
                .trim()
        )

        val isRelease = ProcessBuilder("git", "describe", "--exact-match", "HEAD")
            .directory(project.rootDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start()
            .waitFor() == 0

        return commitCount * 2 + (if (isRelease) 0 else 1)
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
