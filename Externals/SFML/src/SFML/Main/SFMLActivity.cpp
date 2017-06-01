////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2013 Jonathan De Wachter (dewachter.jonathan@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#include <SFML/Config.hpp>
#include <android/native_activity.h>
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <jni.h>

#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_INFO, "sfml-activity", __VA_ARGS__))

namespace {
    typedef void (*activityOnCreatePointer)(ANativeActivity*, void*, size_t);
}

const char *getLibraryName(JNIEnv* lJNIEnv, jobject& objectActivityInfo)
{
    // This function reads the value of meta-data "sfml.app.lib_name"
    // found in the Android Manifest file and returns it. It performs the
    // following Java code using the JNI interface:
    //
    // ai.metaData.getString("sfml.app.lib_name");
    static char name[256];

    // Get metaData instance from the ActivityInfo object
    jclass classActivityInfo = lJNIEnv->FindClass("android/content/pm/ActivityInfo");
    jfieldID fieldMetaData = lJNIEnv->GetFieldID(classActivityInfo, "metaData", "Landroid/os/Bundle;");
    jobject objectMetaData = lJNIEnv->GetObjectField(objectActivityInfo, fieldMetaData);

    // Create a java string object containing "sfml.app.lib_name"
    jobject objectName = lJNIEnv->NewStringUTF("sfml.app.lib_name");

    // Get the value of meta-data named "sfml.app.lib_name"
    jclass classBundle = lJNIEnv->FindClass("android/os/Bundle");
    jmethodID methodGetString = lJNIEnv->GetMethodID(classBundle, "getString", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring valueString = (jstring)lJNIEnv->CallObjectMethod(objectMetaData, methodGetString, objectName);

    // No meta-data "sfml.app.lib_name" was found so we abort and inform the user
    if (valueString == NULL)
    {
        LOGE("No meta-data 'sfml.app.lib_name' found in AndroidManifest.xml file");
        exit(1);
    }

    // Convert the application name to a C++ string and return it
    const jsize applicationNameLength = lJNIEnv->GetStringUTFLength(valueString);
    const char* applicationName = lJNIEnv->GetStringUTFChars(valueString, NULL);

    if (applicationNameLength >= 256)
    {
        LOGE("The value of 'sfml.app.lib_name' must not be longer than 255 characters.");
        exit(1);
    }

    strncpy(name, applicationName, applicationNameLength);
    name[applicationNameLength] = '\0';
    lJNIEnv->ReleaseStringUTFChars(valueString, applicationName);

    return name;
}

void* loadLibrary(const char* libraryName, JNIEnv* lJNIEnv, jobject& ObjectActivityInfo)
{
    // Find out the absolute path of the library
    jclass ClassActivityInfo = lJNIEnv->FindClass("android/content/pm/ActivityInfo");
    jfieldID FieldApplicationInfo = lJNIEnv->GetFieldID(ClassActivityInfo, "applicationInfo", "Landroid/content/pm/ApplicationInfo;");
    jobject ObjectApplicationInfo = lJNIEnv->GetObjectField(ObjectActivityInfo, FieldApplicationInfo);

    jclass ClassApplicationInfo = lJNIEnv->FindClass("android/content/pm/ApplicationInfo");
    jfieldID FieldNativeLibraryDir = lJNIEnv->GetFieldID(ClassApplicationInfo, "nativeLibraryDir", "Ljava/lang/String;");

    jobject ObjectDirPath = lJNIEnv->GetObjectField(ObjectApplicationInfo, FieldNativeLibraryDir);

    jclass ClassSystem = lJNIEnv->FindClass("java/lang/System");
    jmethodID StaticMethodMapLibraryName = lJNIEnv->GetStaticMethodID(ClassSystem, "mapLibraryName", "(Ljava/lang/String;)Ljava/lang/String;");

    jstring LibNameObject = lJNIEnv->NewStringUTF(libraryName);
    jobject ObjectName = lJNIEnv->CallStaticObjectMethod(ClassSystem, StaticMethodMapLibraryName, LibNameObject);

    jclass ClassFile = lJNIEnv->FindClass("java/io/File");
    jmethodID FileConstructor = lJNIEnv->GetMethodID(ClassFile, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
    jobject ObjectFile = lJNIEnv->NewObject(ClassFile, FileConstructor, ObjectDirPath, ObjectName);

    // Get the library absolute path and convert it
    jmethodID MethodGetPath = lJNIEnv->GetMethodID(ClassFile, "getPath", "()Ljava/lang/String;");
    jstring javaLibraryPath = static_cast<jstring>(lJNIEnv->CallObjectMethod(ObjectFile, MethodGetPath));
    const char* libraryPath = lJNIEnv->GetStringUTFChars(javaLibraryPath, NULL);

    // Manually load the library
    void * handle = dlopen(libraryPath, RTLD_NOW | RTLD_GLOBAL);
    if (!handle)
    {
        LOGE("dlopen(\"%s\"): %s", libraryPath, dlerror());
        exit(1);
    }

    // Release the Java string
    lJNIEnv->ReleaseStringUTFChars(javaLibraryPath, libraryPath);

    return handle;
}

void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
    // Before we can load a library, we need to find out its location. As
    // we're powerless here in C/C++, we need the JNI interface to communicate
    // with the attached Java virtual machine and perform some Java calls in
    // order to retrieve the absolute path of our libraries.
    //
    // Here's the snippet of Java code it performs:
    // --------------------------------------------
    // ai = getPackageManager().getActivityInfo(getIntent().getComponent(), PackageManager.GET_META_DATA);
    // File libraryFile = new File(ai.applicationInfo.nativeLibraryDir, System.mapLibraryName(libname));
    // String path = libraryFile.getPath();
    //
    // With libname being the library name such as "jpeg".

    // Retrieve JNI environment and JVM instance
    JavaVM* lJavaVM = activity->vm;
    JNIEnv* lJNIEnv = activity->env;

    // Retrieve the NativeActivity
    jobject ObjectNativeActivity = activity->clazz;
    jclass ClassNativeActivity = lJNIEnv->GetObjectClass(ObjectNativeActivity);

    // Retrieve the ActivityInfo
    jmethodID MethodGetPackageManager = lJNIEnv->GetMethodID(ClassNativeActivity, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject ObjectPackageManager = lJNIEnv->CallObjectMethod(ObjectNativeActivity, MethodGetPackageManager);

    jmethodID MethodGetIndent = lJNIEnv->GetMethodID(ClassNativeActivity, "getIntent", "()Landroid/content/Intent;");
    jobject ObjectIntent = lJNIEnv->CallObjectMethod(ObjectNativeActivity, MethodGetIndent);

    jclass ClassIntent = lJNIEnv->FindClass("android/content/Intent");
    jmethodID MethodGetComponent = lJNIEnv->GetMethodID(ClassIntent, "getComponent", "()Landroid/content/ComponentName;");

    jobject ObjectComponentName = lJNIEnv->CallObjectMethod(ObjectIntent, MethodGetComponent);

    jclass ClassPackageManager = lJNIEnv->FindClass("android/content/pm/PackageManager");

    jfieldID FieldGET_META_DATA = lJNIEnv->GetStaticFieldID(ClassPackageManager, "GET_META_DATA", "I");
    jint GET_META_DATA = lJNIEnv->GetStaticIntField(ClassPackageManager, FieldGET_META_DATA);

    jmethodID MethodGetActivityInfo = lJNIEnv->GetMethodID(ClassPackageManager, "getActivityInfo", "(Landroid/content/ComponentName;I)Landroid/content/pm/ActivityInfo;");
    jobject ObjectActivityInfo = lJNIEnv->CallObjectMethod(ObjectPackageManager, MethodGetActivityInfo, ObjectComponentName, GET_META_DATA);

    // Load our libraries in reverse order
#if defined(STL_LIBRARY)
#define _SFML_QS(s) _SFML_S(s)
#define _SFML_S(s) #s
    loadLibrary(_SFML_QS(STL_LIBRARY), lJNIEnv, ObjectActivityInfo);
#undef _SFML_S
#undef _SFML_QS
#endif
    loadLibrary("openal", lJNIEnv, ObjectActivityInfo);

#if !defined(SFML_DEBUG)
    loadLibrary("sfml-system", lJNIEnv, ObjectActivityInfo);
    loadLibrary("sfml-window", lJNIEnv, ObjectActivityInfo);
    loadLibrary("sfml-graphics", lJNIEnv, ObjectActivityInfo);
    loadLibrary("sfml-audio", lJNIEnv, ObjectActivityInfo);
    loadLibrary("sfml-network", lJNIEnv, ObjectActivityInfo);
#else
    loadLibrary("sfml-system-d", lJNIEnv, ObjectActivityInfo);
    loadLibrary("sfml-window-d", lJNIEnv, ObjectActivityInfo);
    loadLibrary("sfml-graphics-d", lJNIEnv, ObjectActivityInfo);
    loadLibrary("sfml-audio-d", lJNIEnv, ObjectActivityInfo);
    loadLibrary("sfml-network-d", lJNIEnv, ObjectActivityInfo);
#endif

    void* handle = loadLibrary(getLibraryName(lJNIEnv, ObjectActivityInfo), lJNIEnv, ObjectActivityInfo);

    // Call the original ANativeActivity_onCreate function
    activityOnCreatePointer ANativeActivity_onCreate = (activityOnCreatePointer)dlsym(handle, "ANativeActivity_onCreate");

    if (!ANativeActivity_onCreate)
    {
        LOGE("sfml-activity: Undefined symbol ANativeActivity_onCreate");
        exit(1);
    }

    ANativeActivity_onCreate(activity, savedState, savedStateSize);
}
