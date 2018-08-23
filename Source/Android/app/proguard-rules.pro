# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in /home/sigma/apps/android-sdk/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Add any project specific keep options here:

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

-ignorewarnings
-dontskipnonpubliclibraryclasses
-dontskipnonpubliclibraryclassmembers
-verbose

-dontwarn com.squareup.**
-keep class com.squareup.** { *; }

-dontwarn sun.misc.Unsafe
-keep class sun.misc.Unsafe { *; }

-dontwarn rx.**
-keep class rx.** { *; }

-keep class org.dolphinemu.dolphinemu.NativeLibrary { *; }
-keep class org.dolphinemu.dolphinemu.utils.Java_GCAdapter { *; }
-keep class org.dolphinemu.dolphinemu.utils.Java_WiimoteAdapter { *; }
-keep class org.dolphinemu.dolphinemu.model.GameFile { *; }
-keepclassmembers class org.dolphinemu.dolphinemu.model.GameFile { private <fields>; }
-keepclassmembers class org.dolphinemu.dolphinemu.model.GameFileCache { private <fields>; }